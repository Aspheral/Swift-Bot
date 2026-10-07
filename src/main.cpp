#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

using namespace geode::prelude;

namespace swift {
    enum class Mode { Idle, Record, Playback };
    struct Event {
        std::uint64_t tick;
        int button;
        bool player2;
        bool down;
    };
    static Mode mode = Mode::Idle;
    static bool active = false;
    static bool injecting = false;
    static std::uint64_t tick = 0;
    static size_t cursor = 0;
    static std::vector<Event> events;
    static constexpr int kTPS = 240; // Metadata only. Does not install a TPS bypass.

    static std::filesystem::path filePath() {
        return Mod::get()->getSaveDir() / "latest.swift";
    }

    static bool save() {
        auto path = filePath();
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) return false;
        std::ofstream out(path, std::ios::trunc);
        if (!out) return false;
        out << "SWIFT1 " << kTPS << '\n';
        for (auto const& e : events) {
            out << e.tick << ' ' << e.button << ' ' << int(e.player2) << ' ' << int(e.down) << '\n';
        }
        out.flush();
        return out.good();
    }

    static bool load() {
        std::ifstream in(filePath());
        std::string magic;
        int tps = 0;
        if (!(in >> magic >> tps) || magic != "SWIFT1" || tps != kTPS) return false;
        std::vector<Event> parsed;
        std::uint64_t t;
        int button, p2, down;
        while (in >> t >> button >> p2 >> down) {
            if (button < 1 || button > 3 || p2 < 0 || p2 > 1 || down < 0 || down > 1) return false;
            if (!parsed.empty() && t < parsed.back().tick) return false;
            parsed.push_back({t, button, bool(p2), bool(down)});
            if (parsed.size() > 2000000) return false;
        }
        if (!in.eof()) return false;
        events = std::move(parsed);
        cursor = 0;
        return true;
    }

    static void resetClock() {
        tick = 0;
        cursor = 0;
    }
}

// NOTE: Whole-tick recording only. The exact timing of input inside the
// current physics step is intentionally not fabricated as CBF data.
class $modify(SwiftInputLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool player2) {
        if (swift::active && this == PlayLayer::get()) {
            if (swift::mode == swift::Mode::Playback && !swift::injecting) return;
            if (swift::mode == swift::Mode::Record && !swift::injecting) {
                swift::events.push_back({swift::tick, button, player2, down});
            }
        }
        GJBaseGameLayer::handleButton(down, button, player2);
    }

    void processCommands(float dt, bool p1, bool p2) {
        if (swift::active && this == PlayLayer::get() && swift::mode == swift::Mode::Playback) {
            while (swift::cursor < swift::events.size() &&
                   swift::events[swift::cursor].tick <= swift::tick) {
                auto const event = swift::events[swift::cursor++];
                swift::injecting = true;
                this->handleButton(event.down, event.button, event.player2);
                swift::injecting = false;
            }
        }
        GJBaseGameLayer::processCommands(dt, p1, p2);
        if (swift::active && this == PlayLayer::get()) ++swift::tick;
    }
};

// Experiment only: render FPS is unaffected by passing a smaller elapsed
// simulation time through CCScheduler; GD may still enforce minimum steps.
// Audio sync and interactions with physics mods are NOT yet solved.
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
        swift::mode = swift::Mode::Record;
        swift::events.clear();
        swift::resetClock();
        this->resetLevel();
        log::info("[Swift] Recording 240 TPS whole-step inputs");
    }
    void onSwiftStop(CCObject*) {
        if (swift::mode == swift::Mode::Record) {
            log::info("[Swift] Saved recording: {}", swift::save());
        }
        swift::mode = swift::Mode::Idle;
    }
    void onSwiftPlay(CCObject*) {
        if (!swift::load()) {
            FLAlertLayer::create("Swift Bot", "No compatible latest.swift recording found.", "OK")->show();
            return;
        }
        swift::mode = swift::Mode::Playback;
        swift::resetClock();
        this->resetLevel();
        log::info("[Swift] Playing {} whole-step events", swift::events.size());
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        swift::active = true;
        swift::mode = swift::Mode::Idle;
        swift::resetClock();

        auto menu = CCMenu::create();
        auto makeButton = [&](char const* text, SEL_MenuHandler selector) {
            auto label = CCLabelBMFont::create(text, "goldFont.fnt");
            label->setScale(0.58f);
            auto item = CCMenuItemSpriteExtra::create(label, this, selector);
            menu->addChild(item);
        };
        makeButton("REC", menu_selector(SwiftPlayLayer::onSwiftRecord));
        makeButton("STOP", menu_selector(SwiftPlayLayer::onSwiftStop));
        makeButton("PLAY", menu_selector(SwiftPlayLayer::onSwiftPlay));
        menu->alignItemsVerticallyWithPadding(5.f);
        auto screen = CCDirector::sharedDirector()->getWinSize();
        menu->setPosition({screen.width - 32.f, screen.height - 88.f});
        menu->setZOrder(10000);
        this->addChild(menu, 10000);
        return true;
    }

    void resetLevel() {
        swift::resetClock();
        PlayLayer::resetLevel();
    }

    void onExit() {
        swift::active = false;
        swift::mode = swift::Mode::Idle;
        swift::injecting = false;
        PlayLayer::onExit();
    }
};
