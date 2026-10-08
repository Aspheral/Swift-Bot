#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include "ReplayCore.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace swift {
static ReplayCore core;
static bool active = false;
static bool injecting = false;
static bool resetting = false;
static std::array<std::size_t, 2> counts{};
static cocos2d::CCLabelBMFont* statusLabel = nullptr;

static std::filesystem::path macroPath() {
    return Mod::get()->getSaveDir() / "latest.swift";
}

static bool save() {
    auto path = macroPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;
    std::ofstream file(path, std::ios::trunc);
    if (!file) return false;
    ReplayCore::write(file, core.events());
    file.flush();
    return file.good();
}

static bool load(std::vector<InputEvent>& events) {
    std::ifstream file(macroPath());
    if (!file) return false;
    return ReplayCore::read(file, events);
}

static void refreshStatus() {
    if (!statusLabel) return;
    const char* mode = core.mode() == Mode::Record ? "REC" :
                       core.mode() == Mode::Playback ? "PLAY" : "IDLE";
    const std::string text = std::string(mode) + "  P1: " +
        std::to_string(counts[0]) + " | P2: " + std::to_string(counts[1]);
    statusLabel->setString(text.c_str());
}

static void resetCounters() {
    counts = {};
    refreshStatus();
}

static void countInput(bool player2, int button) {
    if (!ReplayCore::validButton(button)) return;
    ++counts[player2 ? 1 : 0];
    refreshStatus();
}
} // namespace swift

// IMPORTANT: GJBaseGameLayer's player2 flag identifies the physical
// player channel. Never infer the player from 'dual mode' or the keyboard key.
// The input log preserves per-player order even for same-tick events.
class $modify(SwiftInputLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool player2) {
        if (swift::active && this == PlayLayer::get() && !swift::resetting) {
            if (!swift::injecting) {
                if (!swift::core.physicalInput(down, button, player2)) return;
                if (swift::core.mode() == swift::Mode::Record)
                    swift::countInput(player2, button);
            }
            // Injected replay events update the core held state in
            // dispatchCurrentTick, without being re-recorded here.
        }
        GJBaseGameLayer::handleButton(down, button, player2);
    }

    void processCommands(float dt, bool p1, bool p2) {
        const bool inLevel = swift::active && this == PlayLayer::get();
        if (inLevel && !swift::resetting) {
            swift::core.dispatchCurrentTick([&](swift::InputEvent const& event) {
                swift::injecting = true;
                this->handleButton(event.down, event.button, event.player2);
                swift::injecting = false;
                swift::countInput(event.player2, event.button);
            });
        }
        GJBaseGameLayer::processCommands(dt, p1, p2);
        if (inLevel && !swift::resetting)
            swift::core.finishedPhysicsStep();
    }
};

// Continuous delta scaling is an EXPERIMENT, not an independently verified
// TPS bypass or true sub-tick physics slow-motion. Default is 1.0x.
class $modify(SwiftScheduler, cocos2d::CCScheduler) {
    void update(float dt) {
        if (swift::active) {
            double multiplier = Mod::get()->getSettingValue<double>("slow-motion");
            dt *= static_cast<float>(std::clamp(multiplier, 0.001, 1.0));
        }
        cocos2d::CCScheduler::update(dt);
    }
};

class $modify(SwiftPlayLayer, PlayLayer) {
    void onSwiftRecord(CCObject*) {
        swift::core.stop();
        this->resetLevel();
        swift::core.beginRecord();
        swift::resetCounters();
        log::info("[Swift] Recording independent P1/P2 button events");
    }

    void onSwiftStop(CCObject*) {
        if (swift::core.mode() == swift::Mode::Record) {
            const auto p1 = swift::core.stats(false);
            const auto p2 = swift::core.stats(true);
            const bool saved = swift::save();
            log::info("[Swift] Saved: {} | P1: {} presses / {} releases; P2: {} presses / {} releases",
                saved, p1.presses, p1.releases, p2.presses, p2.releases);
            if (!saved) {
                FLAlertLayer::create("Swift Bot", "Could not save latest.swift.", "OK")->show();
            }
        }
        swift::core.stop();
        swift::refreshStatus();
    }

    void onSwiftPlay(CCObject*) {
        std::vector<swift::InputEvent> events;
        if (!swift::load(events)) {
            FLAlertLayer::create("Swift Bot", "Missing or invalid latest.swift (requires SWIFT1 240).", "OK")->show();
            return;
        }
        swift::core.stop();
        this->resetLevel();
        swift::core.beginPlayback(std::move(events));
        swift::resetCounters();
        log::info("[Swift] Playback ready: P1 {} events; P2 {} events",
            swift::core.stats(false).events, swift::core.stats(true).events);
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        swift::core.stop();
        swift::core.resetAttempt();
        swift::active = true;
        swift::resetting = false;
        swift::injecting = false;
        swift::counts = {};

        auto menu = CCMenu::create();
        auto makeButton = [&](char const* title, SEL_MenuHandler selector) {
            auto label = CCLabelBMFont::create(title, "goldFont.fnt");
            label->setScale(0.58f);
            menu->addChild(CCMenuItemSpriteExtra::create(label, this, selector));
        };
        makeButton("REC", menu_selector(SwiftPlayLayer::onSwiftRecord));
        makeButton("STOP", menu_selector(SwiftPlayLayer::onSwiftStop));
        makeButton("PLAY", menu_selector(SwiftPlayLayer::onSwiftPlay));
        menu->alignItemsVerticallyWithPadding(5.f);

        auto screen = CCDirector::sharedDirector()->getWinSize();
        menu->setPosition({screen.width - 34.f, screen.height - 91.f});
        this->addChild(menu, 10000);

        swift::statusLabel = CCLabelBMFont::create("IDLE  P1: 0 | P2: 0", "chatFont.fnt");
        swift::statusLabel->setScale(0.58f);
        swift::statusLabel->setAnchorPoint({1.f, 0.5f});
        swift::statusLabel->setPosition({screen.width - 8.f, screen.height - 143.f});
        this->addChild(swift::statusLabel, 10000);
        swift::refreshStatus();
        return true;
    }

    void resetLevel() {
        swift::resetting = true;
        swift::core.resetAttempt();
        swift::resetCounters();
        PlayLayer::resetLevel();
        swift::resetting = false;
    }

    void onExit() {
        if (swift::core.mode() == swift::Mode::Record)
            log::info("[Swift] Auto-save on exit: {}", swift::save());
        swift::active = false;
        swift::injecting = false;
        swift::resetting = false;
        swift::statusLabel = nullptr;
        swift::core.stop();
        PlayLayer::onExit();
    }
};
