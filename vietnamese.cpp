// Vietnamese input engine: Âm tiết = Âm đầu + Vần + Thanh (plan-based).
// On each commit: split into onset (âm đầu) + rime (vần), apply tone (thanh) to main vowel of rime.

#include "vietnamese.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// Telex tone keys: s=sắc(1), f=huyền(2), r=hỏi(3), x=ngã(4), j=nặng(5). No key = ngang(0).
const std::unordered_map<char, int> TONE_KEY = {
    {'s', 1}, {'f', 2}, {'r', 3}, {'x', 4}, {'j', 5},
};

// Internal placeholders: A=ă, B=â, E=ê, O=ô, Q=ơ, U=ư, D=đ.
const std::string INTERNAL_VOWELS = "aeiouyABEOQU";

// Âm đầu (onsets) - longest first for matching. Plan + standard orthography.
static const std::vector<std::string>& get_onsets() {
    static const std::vector<std::string> ONSETS = {
        "ngh", "ng", "nh", "gh", "gi", "ch", "kh", "ph", "qu", "th", "tr",
        "b", "c", "d", "g", "h", "k", "l", "m", "n", "p", "q", "r", "s", "t", "v", "x",
        ""  // vowel-initial
    };
    return ONSETS;
}

// Rime (vần) in internal form -> main vowel index (0-based). From plan.md groups.
static const std::unordered_map<std::string, int>& get_rime_main_vowel_table() {
    static const std::unordered_map<std::string, int> RIME_MAIN = []() {
        std::unordered_map<std::string, int> m;
        auto add = [&m](const std::string& rime, int main_idx) { m[rime] = main_idx; };
        // Đơn & nguyên âm ghép
        add("a", 0); add("ai", 0); add("ay", 0); add("ao", 0); add("au", 0);
        add("e", 0); add("eo", 0); add("i", 0); add("ia", 0); add("iu", 0);
        add("o", 0); add("oi", 0); add("oa", 0); add("oe", 0);
        add("u", 0); add("ua", 0); add("ui", 0); add("uy", 0);
        add("y", 0);
        add("Bu", 0); add("By", 0);   // âu, ây
        add("Eu", 0);                  // êu
        add("iEu", 1); add("yEu", 1);  // iêu, yêu
        add("Oi", 0); add("Qi", 0);    // ôi, ơi
        add("Ui", 0); add("Ua", 0);   // ưi, ưa
        add("uOi", 1); add("UQi", 1); // uôi, ươi (tone on ơ for ươi) e.g. bưởi, mười
        add("uE", 1); add("uQ", 1); add("uO", 1);  // uê, uơ, uô
        add("uya", 1);
        // (UQi already added above)
        // m/p
        add("am", 0); add("ap", 0); add("Am", 0); add("Ap", 0);
        add("Bm", 0); add("Bp", 0); add("em", 0); add("ep", 0);
        add("Em", 0); add("Ep", 0); add("im", 0); add("ip", 0);
        add("iEm", 1); add("iEp", 1);  // iêm, iếp
        add("om", 0); add("op", 0); add("Om", 0); add("Op", 0);
        add("Qm", 0); add("Qp", 0); add("um", 0); add("up", 0);
        add("UQm", 1); add("UQp", 1);  // ươm, ướp
        // n/t
        add("an", 0); add("at", 0); add("An", 0); add("At", 0);
        add("Bn", 0); add("Bt", 0); add("en", 0); add("et", 0);
        add("En", 0); add("Et", 0); add("in", 0); add("it", 0);
        add("iEn", 1); add("iEt", 1);  // iên, iết
        add("on", 0); add("ot", 0); add("On", 0); add("Ot", 0);
        add("Qn", 0); add("Qt", 0); add("un", 0); add("ut", 0);
        add("uOn", 1); add("uOt", 1); add("UQn", 1); add("UQt", 1);
        add("oan", 1); add("oat", 1); add("oAn", 1); add("oAt", 1);
        add("uBn", 1); add("uBt", 1); add("oen", 1); add("oet", 1);
        add("uyEn", 2); add("uyEt", 2);  // uyên, uyết
        // ng/c
        add("ang", 0); add("ac", 0); add("Ang", 0); add("Ac", 0);
        add("Bng", 0); add("Bc", 0); add("eng", 0); add("ec", 0);
        add("iEng", 1); add("iEc", 1);  // iêng, iếc
        add("ong", 0); add("oc", 0); add("Ong", 0); add("Oc", 0);
        add("ung", 0); add("uc", 0); add("Ung", 0); add("Uc", 0);
        add("uOng", 1); add("uOc", 1); add("UQng", 1); add("UQc", 1);
        add("oang", 1); add("oac", 1); add("oAng", 1); add("oAc", 1);
        add("uBng", 1); add("uBc", 1);
        // nh/ch
        add("anh", 0); add("ach", 0); add("Enh", 0); add("Ech", 0);
        add("inh", 0); add("ich", 0); add("oanh", 1); add("oach", 1);
        add("uynh", 0); add("uych", 1);
        // uyn/uyt
        add("uyn", 0); add("uyt", 1);
        return m;
    }();
    return RIME_MAIN;
}

bool is_ascii_vowel(char c) {
    return std::string("aeiouy").find(static_cast<char>(std::tolower(static_cast<unsigned char>(c)))) != std::string::npos;
}

// Any-position hat key: move a/e/o to immediately after the previous same vowel
// when it was typed later after a consonant cluster (e.g. hongof -> hoongf).
static void canonicalize_hat_position(std::string& s) {
    if (s.size() < 3) return;
    auto is_hat_vowel = [](char c) { return c == 'a' || c == 'e' || c == 'o'; };

    for (std::size_t i = 1; i < s.size(); ++i) {
        char v = s[i];
        if (!is_hat_vowel(v)) continue;

        // Special-case "aya" pattern for ây: move trailing 'a' next to the first 'a'.
        // Example: "vayaj" -> "vaayj" (ây + tone) -> "vậy".
        if (v == 'a' && i >= 2 && s[i - 1] == 'y') {
            // Find previous 'a' before the 'y'.
            int prev_a = -1;
            for (int k = static_cast<int>(i) - 2; k >= 0; --k) {
                char c = s[static_cast<std::size_t>(k)];
                if (c == 'a') { prev_a = k; break; }
                // Stop if we encounter another vowel before seeing an 'a'.
                if (is_ascii_vowel(c)) break;
            }
            if (prev_a >= 0) {
                s.erase(i, 1); // remove this 'a'
                s.insert(static_cast<std::size_t>(prev_a + 1), 1, 'a');
                i = static_cast<std::size_t>(prev_a + 1);
                continue;
            }
        }

        // Find previous same vowel.
        int prev = -1;
        for (int k = static_cast<int>(i) - 1; k >= 0; --k) {
            if (s[static_cast<std::size_t>(k)] == v) { prev = k; break; }
            // Stop if we hit another vowel: this is likely a diphthong (oa/oe/uy...), don't reorder.
            if (is_ascii_vowel(s[static_cast<std::size_t>(k)])) break;
        }
        if (prev < 0) continue;

        // Only reorder when the span between prev..i contains no vowels (consonant cluster),
        // meaning this is a delayed hat key rather than a vowel cluster.
        bool has_vowel_between = false;
        for (int k = prev + 1; k < static_cast<int>(i); ++k) {
            if (is_ascii_vowel(s[static_cast<std::size_t>(k)])) { has_vowel_between = true; break; }
        }
        if (has_vowel_between) continue;

        // If it's already adjacent, it's already a standard aa/ee/oo.
        if (static_cast<int>(i) == prev + 1) continue;

        // Move this vowel to right after prev.
        s.erase(i, 1);
        s.insert(static_cast<std::size_t>(prev + 1), 1, v);
        i = static_cast<std::size_t>(prev + 1);
    }
}

// Any-position horn key: move 'w' to the correct place inside the rime.
// Examples:
//  - hopwj: "opw" -> "owp" (ơ + p)
//  - hungws: "ungw" -> "uwng" (ư + ng)
//  - bapws: "apw" -> "awp" (ă + p)
//  - unws: "unw" -> "uwn" (ư + n)
// Special-case:
//  - huaws: "uaw" should become "uwa" (ưa), not "uă".
static void canonicalize_w_position(std::string& s) {
    if (s.size() < 2) return;

    // Walk left-to-right, and for each 'w' (not in 'ww'), attach it to the nearest valid vowel on the left.
    for (std::size_t i = 1; i < s.size(); ++i) {
        if (s[i] != 'w') continue;
        // If this is the second 'w' in a 'ww' sequence, don't move it (prevents oscillation on inputs like sunwworld).
        if (s[i - 1] == 'w') continue;
        if (i + 1 < s.size() && s[i + 1] == 'w') continue; // let apply_shapes_rime handle ww cases

        // If 'w' is right after a/o/u, it's already in the right spot.
        char prev = s[i - 1];
        if (prev == 'a' || prev == 'o' || prev == 'u') continue;

        // Special-case "uaw": move w after 'u' (between u and a) to form ưa.
        if (prev == 'a' && i >= 2 && s[i - 2] == 'u') {
            s.erase(i, 1);
            s.insert(i - 1, 1, 'w');
            // Re-scan from the insertion point.
            i = (i >= 2) ? (i - 2) : 0;
            continue;
        }

        // General: find last vowel before this 'w' and move 'w' right after it.
        int last_vowel = -1;
        for (int k = static_cast<int>(i) - 1; k >= 0; --k) {
            if (is_ascii_vowel(s[static_cast<std::size_t>(k)])) { last_vowel = k; break; }
        }
        if (last_vowel < 0) continue;

        // 'w' only modifies a/o/u (ă/ơ/ư). If the nearest vowel is i/y,
        // attach 'w' to the nearest a/o/u on the left in the same vowel cluster.
        char lv = s[static_cast<std::size_t>(last_vowel)];
        if (lv == 'i' || lv == 'y') {
            for (int k = last_vowel - 1; k >= 0; --k) {
                char c = s[static_cast<std::size_t>(k)];
                if (!is_ascii_vowel(c)) break; // stop at consonant boundary
                if (c == 'a' || c == 'o' || c == 'u') { last_vowel = k; break; }
            }
        }

        // Move w to after last_vowel if it is not already there.
        if (static_cast<int>(i) != last_vowel + 1) {
            // If the canonical position already has a 'w', keep this one in place (represents a literal 'ww' sequence).
            std::size_t target = static_cast<std::size_t>(last_vowel + 1);
            if (target < s.size() && s[target] == 'w') continue;
            s.erase(i, 1);
            s.insert(target, 1, 'w');
            i = static_cast<std::size_t>(last_vowel); // continue after the vowel
        }
    }
}

// Move vowel before n/m when pattern is (n|m) + vowel + tone_key -> vowel + (n|m) + tone_key (e.g. tuanas -> tuaans).
static void canonicalize_tone_position(std::string& s) {
    if (s.size() < 4) return;
    for (int i = static_cast<int>(s.size()) - 3; i >= 1; --i) {
        char c = s[static_cast<std::size_t>(i)];
        if ((c == 'n' || c == 'm') && is_ascii_vowel(s[static_cast<std::size_t>(i + 1)]) &&
            TONE_KEY.count(s[static_cast<std::size_t>(i + 2)])) {
            char v = s[static_cast<std::size_t>(i + 1)];
            s.erase(static_cast<std::size_t>(i + 1), 1);
            s.insert(static_cast<std::size_t>(i), 1, v);
            return;
        }
    }
}

// English triple vowels: eee->ee, aaa->aa, ooo->oo. Returns true if any change was made.
static bool normalize_triple_vowels(std::string& s) {
    bool changed = false;
    auto replace_all = [&s, &changed](const std::string& from, const std::string& to) {
        for (std::size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size()) {
            s.replace(pos, from.size(), to);
            changed = true;
        }
    };
    replace_all("eee", "ee");
    replace_all("aaa", "aa");
    replace_all("ooo", "oo");
    return changed;
}

// Split word into (onset, rime) using longest matching onset. Word = body only (no tone suffix).
void split_onset_rime(const std::string& word, std::string& onset, std::string& rime) {
    onset.clear();
    rime.clear();
    if (word.empty()) return;

    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                  [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Special: "dd" or "dd..." (đ + optional rime) has no onset.
    if (lower.size() >= 2 && lower[0] == 'd' && lower[1] == 'd') {
        onset = "";
        rime = lower;
        return;
    }

    const auto& onsets = get_onsets();
    for (const std::string& o : onsets) {
        if (o.empty()) {
            // Vowel-initial: onset = "", rime = all
            if (lower.size() >= 1 && is_ascii_vowel(lower[0])) {
                onset = "";
                rime = lower;
                return;
            }
            continue;
        }
        if (lower.size() >= o.size() && lower.compare(0, o.size(), o) == 0) {
            std::string rest = lower.substr(o.size());
            if (rest.empty() || is_ascii_vowel(rest[0]) || rest[0] == 'w') {
                onset = o;
                rime = rest;
                return;
            }
        }
    }
    // No vowel at start and no matching onset: treat first char as onset (consonant)
    if (!lower.empty() && !is_ascii_vowel(lower[0])) {
        onset = lower.substr(0, 1);
        rime = lower.substr(1);
        return;
    }
    onset = "";
    rime = lower;
}

// Extract tone from word. Tone key (s/f/r/x/j) may appear after first vowel (any position).
// Returns (body_without_tone_keys, tone 0..5, strip_tone).
std::tuple<std::string, int, bool> extract_tone(const std::string& word) {
    if (word.empty()) return std::make_tuple(word, 0, false);

    int first_vowel = -1;
    int last_vowel = -1;
    for (std::size_t i = 0; i < word.size(); ++i) {
        if (is_ascii_vowel(word[i])) {
            if (first_vowel < 0) first_vowel = static_cast<int>(i);
            last_vowel = static_cast<int>(i);
        }
    }
    if (last_vowel < 0) return std::make_tuple(word, 0, false);

    std::string suffix = word.substr(static_cast<std::size_t>(last_vowel + 1));

    std::string body;

    // Any-position tone: last tone key after first vowel wins; remove all such tone keys.
    // Exception: 'r' followed by 'l' or 'n' is not tone (English "world", "burn").
    int tone = 0;
    for (int i = first_vowel + 1; i < static_cast<int>(word.size()); ++i) {
        char c = word[static_cast<std::size_t>(i)];
        if (TONE_KEY.count(c)) {
            if (c == 'r' && i + 1 < static_cast<int>(word.size())) {
                char next = word[static_cast<std::size_t>(i + 1)];
                if (next == 'l' || next == 'n') continue;
            }
            tone = TONE_KEY.at(c);
        }
    }
    if (tone == 0 && !suffix.empty() && TONE_KEY.count(suffix.back()))
        tone = TONE_KEY.at(suffix.back());

    for (int i = 0; i < static_cast<int>(word.size()); ++i) {
        char c = word[static_cast<std::size_t>(i)];
        if (i <= first_vowel) { body.push_back(c); continue; }
        if (TONE_KEY.count(c)) {
            if (c == 'r' && i + 1 < static_cast<int>(word.size())) {
                char next = word[static_cast<std::size_t>(i + 1)];
                if (next == 'l' || next == 'n') { body.push_back(c); continue; }
            }
            continue;
        }
        body.push_back(c);
    }
    return std::make_tuple(body, tone, false);
}

// Apply Telex shape to rime only: dd->D, aa->B, aw->A, ee->E, oo->O, ow->Q, uw->U, uow->UQ.
// uww->uw, oww->ow, aww->aw; other ww -> literal W (output as "w").
std::string apply_shapes_rime(std::string s) {
    if (s.empty()) return s;
    // First pass: collapse ww. uww->uW, oww->oW, aww->aW (one literal w); else ww->W.
    std::string step;
    step.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        if (i + 2 <= s.size() && s[i] == 'w' && s[i+1] == 'w') {
            if (!step.empty()) {
                char p = step.back();
                if (p == 'u' || p == 'o' || p == 'a') {
                    step.push_back('W');  // one literal w (uww -> uw)
                    i += 2;
                    continue;
                }
            }
            step.push_back('W');
            i += 2;
            continue;
        }
        step.push_back(s[i]);
        ++i;
    }
    s = std::move(step);
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        if (s[i] == 'W') { out.push_back('W'); ++i; continue; }
        if (i + 3 <= s.size() && s[i] == 'u' && s[i+1] == 'o' && s[i+2] == 'w') {
            out.push_back('U'); out.push_back('Q');
            i += 3;
            continue;
        }
        if (i + 1 < s.size()) {
            char a = s[i], b = s[i+1];
            if (b == 'W') { out.push_back(a); out.push_back('W'); i += 2; continue; }
            if (a == 'd' && b == 'd') { out.push_back('D'); i += 2; continue; }
            if (a == 'a' && b == 'a') { out.push_back('B'); i += 2; continue; }
            if (a == 'e' && b == 'e') { out.push_back('E'); i += 2; continue; }
            if (a == 'e' && b == 'w') { out.push_back('E'); i += 2; continue; }
            if (a == 'o' && b == 'o') { out.push_back('O'); i += 2; continue; }
            if (a == 'u' && b == 'w') { out.push_back('U'); i += 2; continue; }
            // uaw -> ưa (not uă): if prev is 'u', output U then 'a', skip 'w'.
            if (!out.empty() && out.back() == 'u' && a == 'a' && b == 'w') {
                out.back() = 'U';
                out.push_back('a');
                i += 2;
                continue;
            }
            if (a == 'a' && b == 'w') { out.push_back('A'); i += 2; continue; }
            if (a == 'o' && b == 'w') { out.push_back('Q'); i += 2; continue; }
        }
        if (s[i] == 'w') {
            if (!out.empty()) {
                char& p = out.back();
                if (p == 'a') { p = 'A'; ++i; continue; }
                if (p == 'o') { p = 'Q'; ++i; continue; }
                if (p == 'u') { p = 'U'; ++i; continue; }
            }
            out.push_back('U');
            ++i;
            continue;
        }
        out.push_back(s[i]);
        ++i;
    }
    return out;
}

// Get main vowel position in shaped rime (internal form). Returns index in string, or -1.
int get_main_vowel_index(const std::string& shaped) {
    if (shaped.empty()) return -1;
    int first_vowel = -1;
    std::vector<int> vowel_pos;
    for (std::size_t i = 0; i < shaped.size(); ++i) {
        if (INTERNAL_VOWELS.find(shaped[i]) != std::string::npos) {
            if (first_vowel < 0) first_vowel = static_cast<int>(i);
            vowel_pos.push_back(static_cast<int>(i));
        }
    }
    if (vowel_pos.empty()) return -1;

    std::string rime_from_vowel = shaped.substr(static_cast<std::size_t>(first_vowel));
    const auto& table = get_rime_main_vowel_table();
    auto it = table.find(rime_from_vowel);
    int main_idx = (it != table.end()) ? it->second : 0;
    if (main_idx >= static_cast<int>(vowel_pos.size())) main_idx = 0;
    return vowel_pos[static_cast<std::size_t>(main_idx)];
}

// Render single vowel (internal) to UTF-8, no tone.
std::string render_vowel_no_tone(char v) {
    switch (v) {
        case 'a': return "a"; case 'A': return "ă"; case 'B': return "â";
        case 'e': return "e"; case 'E': return "ê";
        case 'i': return "i"; case 'o': return "o"; case 'O': return "ô"; case 'Q': return "ơ";
        case 'u': return "u"; case 'U': return "ư"; case 'y': return "y";
        default: return std::string(1, v);
    }
}

// Render single vowel + tone (1..5) to UTF-8.
std::string render_vowel_with_tone(char v, int tone) {
    if (tone < 1 || tone > 5) return render_vowel_no_tone(v);
    switch (v) {
        case 'a': switch (tone) { case 1: return "á"; case 2: return "à"; case 3: return "ả"; case 4: return "ã"; case 5: return "ạ"; } break;
        case 'A': switch (tone) { case 1: return "ắ"; case 2: return "ằ"; case 3: return "ẳ"; case 4: return "ẵ"; case 5: return "ặ"; } break;
        case 'B': switch (tone) { case 1: return "ấ"; case 2: return "ầ"; case 3: return "ẩ"; case 4: return "ẫ"; case 5: return "ậ"; } break;
        case 'e': switch (tone) { case 1: return "é"; case 2: return "è"; case 3: return "ẻ"; case 4: return "ẽ"; case 5: return "ẹ"; } break;
        case 'E': switch (tone) { case 1: return "ế"; case 2: return "ề"; case 3: return "ể"; case 4: return "ễ"; case 5: return "ệ"; } break;
        case 'i': switch (tone) { case 1: return "í"; case 2: return "ì"; case 3: return "ỉ"; case 4: return "ĩ"; case 5: return "ị"; } break;
        case 'o': switch (tone) { case 1: return "ó"; case 2: return "ò"; case 3: return "ỏ"; case 4: return "õ"; case 5: return "ọ"; } break;
        case 'O': switch (tone) { case 1: return "ố"; case 2: return "ồ"; case 3: return "ổ"; case 4: return "ỗ"; case 5: return "ộ"; } break;
        case 'Q': switch (tone) { case 1: return "ớ"; case 2: return "ờ"; case 3: return "ở"; case 4: return "ỡ"; case 5: return "ợ"; } break;
        case 'u': switch (tone) { case 1: return "ú"; case 2: return "ù"; case 3: return "ủ"; case 4: return "ũ"; case 5: return "ụ"; } break;
        case 'U': switch (tone) { case 1: return "ứ"; case 2: return "ừ"; case 3: return "ử"; case 4: return "ữ"; case 5: return "ự"; } break;
        case 'y': switch (tone) { case 1: return "ý"; case 2: return "ỳ"; case 3: return "ỷ"; case 4: return "ỹ"; case 5: return "ỵ"; } break;
        default: break;
    }
    return render_vowel_no_tone(v);
}

// Render shaped rime to UTF-8; tone applied to main vowel. tone 0 = no tone.
[[maybe_unused]] std::string render_rime(const std::string& shaped, int tone) {
    std::string out;
    out.reserve(shaped.size() * 3);
    int main_idx = (tone != 0) ? get_main_vowel_index(shaped) : -1;

    for (int i = 0; i < static_cast<int>(shaped.size()); ++i) {
        char c = shaped[static_cast<std::size_t>(i)];
        if (c == 'W') {
            out.push_back('w');
            continue;
        }
        if (c == 'D') {
            out.append("đ");
            continue;
        }
        if (INTERNAL_VOWELS.find(c) != std::string::npos) {
            if (i == main_idx)
                out.append(render_vowel_with_tone(c, tone));
            else
                out.append(render_vowel_no_tone(c));
            continue;
        }
        out.push_back(c);
    }
    return out;
}

// UTF-8 uppercase for Vietnamese.
std::string utf8_to_upper(const std::string& s) {
    static const std::unordered_map<std::string, std::string> VN_UPPER = {
        {"ă","Ă"},{"â","Â"},{"ê","Ê"},{"ô","Ô"},{"ơ","Ơ"},{"ư","Ư"},{"đ","Đ"},
        {"á","Á"},{"à","À"},{"ả","Ả"},{"ã","Ã"},{"ạ","Ạ"},
        {"ắ","Ắ"},{"ằ","Ằ"},{"ẳ","Ẳ"},{"ẵ","Ẵ"},{"ặ","Ặ"},
        {"ấ","Ấ"},{"ầ","Ầ"},{"ẩ","Ẩ"},{"ẫ","Ẫ"},{"ậ","Ậ"},
        {"é","É"},{"è","È"},{"ẻ","Ẻ"},{"ẽ","Ẽ"},{"ẹ","Ẹ"},
        {"ế","Ế"},{"ề","Ề"},{"ể","Ể"},{"ễ","Ễ"},{"ệ","Ệ"},
        {"í","Í"},{"ì","Ì"},{"ỉ","Ỉ"},{"ĩ","Ĩ"},{"ị","Ị"},
        {"ó","Ó"},{"ò","Ò"},{"ỏ","Ỏ"},{"õ","Õ"},{"ọ","Ọ"},
        {"ố","Ố"},{"ồ","Ồ"},{"ổ","Ổ"},{"ỗ","Ỗ"},{"ộ","Ộ"},
        {"ớ","Ớ"},{"ờ","Ờ"},{"ở","Ở"},{"ỡ","Ỡ"},{"ợ","Ợ"},
        {"ú","Ú"},{"ù","Ù"},{"ủ","Ủ"},{"ũ","Ũ"},{"ụ","Ụ"},
        {"ứ","Ứ"},{"ừ","Ừ"},{"ử","Ử"},{"ữ","Ữ"},{"ự","Ự"},
        {"ý","Ý"},{"ỳ","Ỳ"},{"ỷ","Ỷ"},{"ỹ","Ỹ"},{"ỵ","Ỵ"},
    };
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80u) {
            out.push_back(static_cast<char>(std::toupper(c)));
            ++i;
            continue;
        }
        std::size_t len = (c & 0xE0u) == 0xC0u ? 2 : (c & 0xF0u) == 0xE0u ? 3 : 4;
        if (i + len > s.size()) { out.append(s.substr(i)); break; }
        std::string cp = s.substr(i, len);
        auto it = VN_UPPER.find(cp);
        out.append(it != VN_UPPER.end() ? it->second : cp);
        i += len;
    }
    return out;
}

std::string apply_word_case(const std::string& conv, const std::string& word) {
    if (conv.empty() || word.empty()) return conv;
    bool all_upper = true;
    for (char c : word) {
        if (std::isalpha(static_cast<unsigned char>(c)) && !std::isupper(static_cast<unsigned char>(c))) { all_upper = false; break; }
    }
    if (all_upper) return utf8_to_upper(conv);
    if (std::isupper(static_cast<unsigned char>(word[0]))) {
        std::string upper = utf8_to_upper(conv);
        unsigned char c0 = static_cast<unsigned char>(conv[0]);
        std::size_t first_len = (c0 & 0x80u) == 0 ? 1 : (c0 & 0xE0u) == 0xC0u ? 2 : (c0 & 0xF0u) == 0xE0u ? 3 : 4;
        if (first_len <= conv.size() && first_len <= upper.size())
            return upper.substr(0, first_len) + conv.substr(first_len);
    }
    return conv;
}

// Convert one word: Âm đầu + Vần + Thanh. Returns UTF-8 syllable.
std::string convert_word(const std::string& word) {
    if (word.empty()) return word;

    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                  [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Escape rule:
    // If the user types ss/ff/rr/xx/jj/ww or aaa/eee/ooo anywhere in the word, we:
    // 1) Collapse the sequence (ss->s, ww->w, aaa->aa, ...).
    // 2) Return the WHOLE word (with that collapse) in raw form.
    // 3) Do NOT apply any IME processing (no Telex conversion) on this word.
    {
        struct Esc {
            const char* pat;
            int pat_len;
            int out_len; // pat_len-1
        };
        const Esc escapes[] = {
            {"ss", 2, 1}, {"ff", 2, 1}, {"rr", 2, 1}, {"xx", 2, 1}, {"jj", 2, 1}, {"ww", 2, 1},
            {"aaa", 3, 2}, {"eee", 3, 2}, {"ooo", 3, 2},
        };

        std::size_t esc_pos = std::string::npos;
        const Esc* esc = nullptr;
        for (const auto& e : escapes) {
            std::size_t p = lower.find(e.pat);
            if (p != std::string::npos && (esc_pos == std::string::npos || p < esc_pos)) {
                esc_pos = p;
                esc = &e;
            }
        }
        if (esc_pos != std::string::npos && esc != nullptr) {
            // Rebuild from the ORIGINAL typed word to preserve case.
            std::string out;
            out.reserve(word.size() - 1);
            out.append(word.substr(0, esc_pos));
            // Keep the first out_len characters as typed (e.g. "aaa" -> "aa").
            out.append(word.substr(esc_pos, static_cast<std::size_t>(esc->out_len)));
            // Everything after the pattern.
            std::size_t after = esc_pos + static_cast<std::size_t>(esc->pat_len);
            if (after < word.size()) out.append(word.substr(after));
            return out;
        }
    }

    canonicalize_hat_position(lower);
    canonicalize_w_position(lower);
    canonicalize_tone_position(lower);

    std::string body;
    int tone = 0;
    bool strip = false;
    std::tie(body, tone, strip) = extract_tone(lower);

    if (normalize_triple_vowels(body)) {
        // English triple-vowel collapse: output normalized form without Telex.
        return apply_word_case(body, word);
    }

    std::string onset, rime_raw;
    split_onset_rime(body, onset, rime_raw);

    std::string rime_shaped = apply_shapes_rime(rime_raw);
    std::string rime_utf8 = render_rime(rime_shaped, strip ? 0 : tone);

    std::string conv = onset + rime_utf8;
    return apply_word_case(conv, word);
}

}  // namespace

std::string telex_to_unicode(const std::string& raw) {
    if (raw.empty()) return raw;

    std::string result;
    result.reserve(raw.size());

    std::size_t i = 0;
    while (i < raw.size()) {
        if (std::isspace(static_cast<unsigned char>(raw[i]))) {
            result.push_back(raw[i]);
            ++i;
            continue;
        }
        std::size_t start = i;
        while (i < raw.size() && !std::isspace(static_cast<unsigned char>(raw[i]))) ++i;
        std::string word = raw.substr(start, i - start);

        bool all_alpha = true;
        for (char c : word) {
            if (!std::isalpha(static_cast<unsigned char>(c))) { all_alpha = false; break; }
        }
        if (!all_alpha) {
            result += word;
            continue;
        }

        std::string conv = convert_word(word);
        result += conv;
    }
    return result;
}
