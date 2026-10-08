#pragma once

// No Geode dependencies: this is the deterministic, testable input core.
// P1 and P2 are separate channels even if they press on the same tick.
#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace swift {
inline constexpr int kTPS = 240;  // Target metadata only, not a TPS bypass.
inline constexpr std::size_t kMaxEvents = 200000;

enum class Mode { Idle, Record, Playback };

struct InputEvent {
    std::uint64_t tick = 0;
    int button = 1;
    bool player2 = false;
    bool down = false;
    bool operator==(InputEvent const&) const = default;
};

struct PlayerStats {
    std::size_t events = 0;
    std::size_t presses = 0;
    std::size_t releases = 0;
};

class ReplayCore {
public:
    void beginRecord() {
        m_events.clear();
        m_mode = Mode::Record;
        resetAttempt();
    }

    void beginPlayback(std::vector<InputEvent> events) {
        m_events = std::move(events);
        m_mode = Mode::Playback;
        resetAttempt();
    }

    void stop() { m_mode = Mode::Idle; }

    void resetAttempt() {
        m_tick = 0;
        m_cursor = 0;
        m_held = {};
        // GD respawns/reset restarts the attempt, not just the clock.
        // Never append a second attempt onto a previous attempt's events.
        if (m_mode == Mode::Record) m_events.clear();
    }

    // Returns false when physical input must not reach GD during playback.
    // Deliberately records every valid transition, including repeated down
    // events, so input chatter can be diagnosed rather than silently erased.
    bool physicalInput(bool down, int button, bool player2) {
        if (m_mode == Mode::Playback) return false;
        if (!validButton(button)) return true;
        m_held[player2 ? 1 : 0][button - 1] = down;
        if (m_mode == Mode::Record && m_events.size() < kMaxEvents) {
            m_events.push_back({m_tick, button, player2, down});
        }
        return true;
    }

    void injectedInput(InputEvent const& event) {
        if (validButton(event.button))
            m_held[event.player2 ? 1 : 0][event.button - 1] = event.down;
    }

    template <class Callback>
    void dispatchCurrentTick(Callback&& inject) {
        if (m_mode != Mode::Playback) return;
        while (m_cursor < m_events.size() &&
               m_events[m_cursor].tick <= m_tick) {
            // Use a copy, as callbacks can reenter the native input hook.
            auto event = m_events[m_cursor++];
            injectedInput(event);
            inject(event);
        }
    }

    void finishedPhysicsStep() {
        if (m_mode != Mode::Idle && m_tick < std::numeric_limits<std::uint64_t>::max())
            ++m_tick;
    }

    [[nodiscard]] Mode mode() const { return m_mode; }
    [[nodiscard]] std::uint64_t tick() const { return m_tick; }
    [[nodiscard]] std::size_t cursor() const { return m_cursor; }
    [[nodiscard]] bool held(bool player2, int button) const {
        return validButton(button) ? m_held[player2 ? 1 : 0][button - 1] : false;
    }
    [[nodiscard]] std::vector<InputEvent> const& events() const { return m_events; }
    [[nodiscard]] PlayerStats stats(bool player2) const {
        PlayerStats result;
        for (auto const& e : m_events) {
            if (e.player2 != player2) continue;
            ++result.events;
            if (e.down) ++result.presses;
            else ++result.releases;
        }
        return result;
    }
    [[nodiscard]] bool reachedLimit() const { return m_events.size() >= kMaxEvents; }

    static bool validButton(int button) { return button >= 1 && button <= 3; }

    static void write(std::ostream& out, std::vector<InputEvent> const& events) {
        out << "SWIFT1 " << kTPS << '\n';
        for (auto const& e : events)
            out << e.tick << ' ' << e.button << ' ' << int(e.player2)
                << ' ' << int(e.down) << '\n';
    }

    // Strict, atomic parser: invalid/malformed files never replace a recording.
    static bool read(std::istream& in, std::vector<InputEvent>& output) {
        std::string line, magic, extra;
        int tps = 0;
        if (!std::getline(in, line)) return false;
        std::istringstream header(line);
        if (!(header >> magic >> tps) || (header >> extra) ||
            magic != "SWIFT1" || tps != kTPS) return false;

        std::vector<InputEvent> parsed;
        while (std::getline(in, line)) {
            // Accept blank lines, not comments or truncated event rows.
            if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
            std::istringstream row(line);
            std::uint64_t tick = 0;
            int button = 0, player2 = -1, down = -1;
            if (!(row >> tick >> button >> player2 >> down) || (row >> extra))
                return false;
            if (!validButton(button) || (player2 != 0 && player2 != 1) ||
                (down != 0 && down != 1) || parsed.size() >= kMaxEvents ||
                (!parsed.empty() && tick < parsed.back().tick)) return false;
            parsed.push_back({tick, button, player2 == 1, down == 1});
        }
        if (!in.eof() || in.bad()) return false;
        output = std::move(parsed);
        return true;
    }

private:
    Mode m_mode = Mode::Idle;
    std::uint64_t m_tick = 0;
    std::size_t m_cursor = 0;
    std::array<std::array<bool, 3>, 2> m_held{};
    std::vector<InputEvent> m_events;
};
} // namespace swift
