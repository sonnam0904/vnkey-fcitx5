#pragma once

#include <string>

#include <fcitx/addonfactory.h>
#include <fcitx-config/configuration.h>
#include <fcitx/inputmethodengine.h>

#include "engine.h"

class TelebitFcitx5Engine : public fcitx::InputMethodEngineV2 {
public:
    TelebitFcitx5Engine();

    void keyEvent(const fcitx::InputMethodEntry &entry,
                  fcitx::KeyEvent &keyEvent) override;

    void reset(const fcitx::InputMethodEntry &entry,
               fcitx::InputContextEvent &event) override;

    void reloadConfig() override;
    const fcitx::Configuration *getConfig() const override;
    void setConfig(const fcitx::RawConfig &config) override;

private:
    FCITX_CONFIGURATION(
        TelebitFcitx5Config,
        fcitx::Option<bool> directCommitRollback{
            this,
            "DirectCommitRollback",
            "Bật bộ gõ không gạch chân (preedit) - Hỗ trợ một số ứng dụng",
            true
        };
    );

    static constexpr char configFile[] = "conf/telebit-fcitx5.conf";

    EngineVietCpp engine_;

    TelebitFcitx5Config config_;

    // Experimental mode state: raw ASCII typed since last word boundary,
    // and the currently committed display string in the client.
    std::string rollbackRawAscii_;
    std::string rollbackDisplay_;

    void updatePreedit(fcitx::InputContext *ic);

    void keyEventPreedit(fcitx::InputContext *ic, fcitx::KeyEvent &keyEvent);
    void keyEventDirectRollback(fcitx::InputContext *ic, fcitx::KeyEvent &keyEvent);
    void rollbackClearState();
};

class TelebitFcitx5EngineFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        FCITX_UNUSED(manager);
        return new TelebitFcitx5Engine;
    }
};

