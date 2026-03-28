#pragma once

#include <string>

#include <fcitx/addonfactory.h>
#include <fcitx-config/configuration.h>
#include <fcitx/inputmethodengine.h>

#include "engine.h"

class VnkeyEngine : public fcitx::InputMethodEngineV2 {
public:
    VnkeyEngine();

    void keyEvent(const fcitx::InputMethodEntry &entry,
                  fcitx::KeyEvent &keyEvent) override;

    void reset(const fcitx::InputMethodEntry &entry,
               fcitx::InputContextEvent &event) override;

    void reloadConfig() override;
    const fcitx::Configuration *getConfig() const override;
    void setConfig(const fcitx::RawConfig &config) override;

private:
    FCITX_CONFIGURATION(
        VnkeyConfig,
        fcitx::Option<bool> directCommitRollback{
            this,
            "DirectCommitRollback",
            "Bật bộ gõ không gạch chân (preedit) - Hỗ trợ một số ứng dụng",
            true
        };
    );

    static constexpr char configFile[] = "conf/vnkey.conf";

    EngineVietCpp engine_;

    VnkeyConfig config_;

    // Experimental mode state: raw ASCII typed since last word boundary,
    // and the currently committed display string in the client.
    std::string rollbackRawAscii_;
    std::string rollbackDisplay_;

    std::string lastPreedit_;

    void updatePreedit(fcitx::InputContext *ic);

    void keyEventPreedit(fcitx::InputContext *ic, fcitx::KeyEvent &keyEvent);
    void keyEventDirectRollback(fcitx::InputContext *ic, fcitx::KeyEvent &keyEvent);
    void rollbackClearState();
};

class VnkeyEngineFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        FCITX_UNUSED(manager);
        return new VnkeyEngine;
    }
};

