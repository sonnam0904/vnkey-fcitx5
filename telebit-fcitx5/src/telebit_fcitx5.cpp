/*
 * telebit-fcitx5 - Vietnamese Telex input method for Fcitx5
 * Engine implementation that reuses EngineVietCpp from the core C++ logic.
 */

#include "telebit_fcitx5.h"
#include "vietnamese.h"

#include <fcitx-config/iniparser.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>
#include <fcitx-utils/keysym.h>

using namespace fcitx;

namespace {

// Count UTF-8 characters (codepoints) in a string.
int utf8CharCount(const std::string &s) {
    int count = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        // Leading bytes: 0xxxxxxx, 110xxxxx, 1110xxxx, 11110xxx
        if ((c & 0x80u) == 0 || (c & 0xC0u) == 0xC0u) {
            ++count;
        }
    }
    return count;
}

// Find common prefix length in bytes, stopping only at UTF-8 codepoint boundaries.
std::size_t commonPrefixBytes(const std::string &s1, const std::string &s2) {
    std::size_t n = std::min(s1.size(), s2.size());
    std::size_t i = 0;
    while (i < n && s1[i] == s2[i]) {
        ++i;
    }
    // Backtrack if we are in the middle of a multi-byte sequence.
    while (i > 0 && (static_cast<unsigned char>(s1[i]) & 0xC0u) == 0x80u) {
        --i;
    }
    return i;
}

} // namespace

TelebitFcitx5Engine::TelebitFcitx5Engine() {
    reloadConfig();
}

void TelebitFcitx5Engine::reloadConfig() {
    readAsIni(config_, configFile);
    // Clear state when toggling behavior.
    rollbackClearState();
    engine_.reset();
}

const Configuration *TelebitFcitx5Engine::getConfig() const {
    return &config_;
}

void TelebitFcitx5Engine::setConfig(const RawConfig &config) {
    config_.load(config, true);
    safeSaveAsIni(config_, configFile);
    rollbackClearState();
    engine_.reset();
}

void TelebitFcitx5Engine::rollbackClearState() {
    rollbackRawAscii_.clear();
    rollbackDisplay_.clear();
}

void TelebitFcitx5Engine::updatePreedit(InputContext *ic) {
    if (!ic) {
        return;
    }

    const std::string &buffer = engine_.buffer();
    if (buffer.empty()) {
        ic->inputPanel().setClientPreedit(Text());
        ic->updatePreedit();
        return;
    }

    std::string preeditStr = convert_buffer(buffer);
    Text preedit;
    preedit.append(preeditStr, TextFormatFlag::Underline);
    // Place cursor at the end of the preedit text (by byte).
    preedit.setCursor(static_cast<int>(preeditStr.size()));
    ic->inputPanel().setClientPreedit(preedit);
    ic->updatePreedit();
}

void TelebitFcitx5Engine::keyEvent(const InputMethodEntry &entry, KeyEvent &keyEvent) {
    FCITX_UNUSED(entry);

    auto *ic = keyEvent.inputContext();
    if (!ic) {
        return;
    }

    bool useDirectRollback = config_.directCommitRollback.value();

    // Firefox reports SurroundingText but its implementation is unreliable,
    // which breaks direct-rollback mode. Force preedit mode there.
    const std::string &program = ic->program();
    if (useDirectRollback && program == "firefox") {
        useDirectRollback = false;
    }

    if (useDirectRollback) {
        keyEventDirectRollback(ic, keyEvent);
    } else {
        keyEventPreedit(ic, keyEvent);
    }
}

void TelebitFcitx5Engine::keyEventPreedit(InputContext *ic, KeyEvent &keyEvent) {
    if (keyEvent.isRelease()) {
        return;
    }

    // If user used direct rollback mode before, ensure we don't keep stale state.
    rollbackClearState();

    const Key &key = keyEvent.key();
    std::uint32_t state = 0;
    if (key.states().test(KeyState::Ctrl)) {
        state |= MOD_CONTROL;
    }
    if (key.states().test(KeyState::Alt)) {
        state |= MOD_ALT;
    }

    std::uint32_t keyval = static_cast<std::uint32_t>(key.sym());
    std::uint32_t keycode = static_cast<std::uint32_t>(key.code());

    KeyResult result = engine_.process_key_event(keyval, keycode, state);

    if (result.handled) {
        keyEvent.filterAndAccept();
        if (!result.commit_text.empty()) {
            ic->commitString(result.commit_text);
        }
        updatePreedit(ic);
    } else {
        if (!result.commit_text.empty()) {
            ic->commitString(result.commit_text);
            updatePreedit(ic);
        }
    }
}

void TelebitFcitx5Engine::keyEventDirectRollback(InputContext *ic, KeyEvent &keyEvent) {
    if (keyEvent.isRelease()) {
        return;
    }

    // If client does not support surrounding text, fall back to preedit mode.
    if (!ic->capabilityFlags().test(CapabilityFlag::SurroundingText)) {
        rollbackClearState();
        keyEventPreedit(ic, keyEvent);
        return;
    }

    const Key &key = keyEvent.key();
    // Do not interfere with shortcuts.
    if (key.states().test(KeyState::Ctrl) || key.states().test(KeyState::Alt)) {
        rollbackClearState();
        return;
    }

    auto sym = key.sym();

    // Backspace: update our internal buffers and rewrite the current word.
    if (sym == FcitxKey_BackSpace) {
        if (!rollbackRawAscii_.empty()) {
            rollbackRawAscii_.pop_back();
            std::string newDisplay = telex_to_unicode(rollbackRawAscii_);

            std::size_t prefixLen = commonPrefixBytes(rollbackDisplay_, newDisplay);
            int charsToDelete = utf8CharCount(rollbackDisplay_.substr(prefixLen));
            if (charsToDelete > 0) {
                ic->deleteSurroundingText(-charsToDelete,
                                          static_cast<unsigned int>(charsToDelete));
            }
            if (newDisplay.size() > prefixLen) {
                ic->commitString(newDisplay.substr(prefixLen));
            }

            rollbackDisplay_ = newDisplay;
            keyEvent.filterAndAccept();
            return;
        }
        // Nothing tracked: let app handle backspace.
        rollbackClearState();
        return;
    }

    // Word boundary: space / enter / tab / punctuation / digits.
    // Clear state and allow the key to reach the application.
    if (!(('a' <= sym && sym <= 'z') || ('A' <= sym && sym <= 'Z'))) {
        rollbackClearState();
        return;
    }

    // Letter key: rewrite the word in-place.
    rollbackRawAscii_.push_back(static_cast<char>(sym));
    std::string newDisplay = telex_to_unicode(rollbackRawAscii_);

    std::size_t prefixLen = commonPrefixBytes(rollbackDisplay_, newDisplay);
    int charsToDelete = utf8CharCount(rollbackDisplay_.substr(prefixLen));
    if (charsToDelete > 0) {
        ic->deleteSurroundingText(-charsToDelete,
                                  static_cast<unsigned int>(charsToDelete));
    }
    if (newDisplay.size() > prefixLen) {
        ic->commitString(newDisplay.substr(prefixLen));
    }
    rollbackDisplay_ = newDisplay;

    // We commit ourselves, so do not forward the original key.
    keyEvent.filterAndAccept();
}

void TelebitFcitx5Engine::reset(const InputMethodEntry &entry, InputContextEvent &event) {
    FCITX_UNUSED(entry);
    auto *ic = event.inputContext();
    engine_.reset();
    rollbackClearState();
    if (ic) {
        ic->inputPanel().setClientPreedit(Text());
        ic->updatePreedit();
    }
}

FCITX_ADDON_FACTORY(TelebitFcitx5EngineFactory);

