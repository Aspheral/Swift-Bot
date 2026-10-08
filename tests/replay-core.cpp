#include "../src/ReplayCore.hpp"
#include <cassert>
#include <sstream>
#include <vector>
#include <iostream>
using swift::ReplayCore;
using swift::InputEvent;
int main() {
    ReplayCore c;
    c.beginRecord();
    assert(c.physicalInput(true, 1, false));
    assert(c.physicalInput(true, 1, true)); // Same tick, different player.
    assert(c.held(false, 1) && c.held(true, 1));
    c.finishedPhysicsStep();
    c.physicalInput(false, 1, false);
    assert(!c.held(false, 1) && c.held(true, 1)); // Independent release.
    c.finishedPhysicsStep();
    c.physicalInput(false, 1, true);
    assert(c.events().size() == 4);
    assert(c.stats(false).events == 2 && c.stats(true).events == 2);
    assert(c.stats(false).presses == 1 && c.stats(true).releases == 1);
    std::stringstream saved;
    ReplayCore::write(saved, c.events());
    assert(saved.str() == "SWIFT1 240\n0 1 0 1\n0 1 1 1\n1 1 0 0\n2 1 1 0\n");

    std::vector<InputEvent> events;
    assert(ReplayCore::read(saved, events));
    assert(events == c.events());

    ReplayCore replay;
    replay.beginPlayback(events);
    assert(!replay.physicalInput(true, 2, false)); // Physical override blocked.
    std::vector<InputEvent> dispatched;
    auto drain=[&](InputEvent e){ dispatched.push_back(e); };
    replay.dispatchCurrentTick(drain);
    assert(dispatched.size() == 2 && !dispatched[0].player2 && dispatched[1].player2);
    assert(replay.held(false, 1) && replay.held(true, 1));
    replay.finishedPhysicsStep();
    replay.dispatchCurrentTick(drain);
    assert(!replay.held(false, 1) && replay.held(true, 1));
    replay.finishedPhysicsStep();
    replay.dispatchCurrentTick(drain);
    assert(!replay.held(false, 1) && !replay.held(true, 1));
    assert(dispatched == events);

    // Repeated input is retained rather than merged: vital for diagnosing chatter.
    c.beginRecord();
    c.physicalInput(true, 1, false);
    c.physicalInput(true, 1, false);
    assert(c.events().size() == 2);
    assert(c.stats(false).presses == 2);
    assert(c.stats(true).events == 0);

    // A new attempt must not mix with the previous attempt.
    c.finishedPhysicsStep();
    c.resetAttempt();
    assert(c.tick() == 0 && c.events().empty());
    c.physicalInput(true, 2, true);
    assert(c.events().size() == 1 && c.events()[0].player2);
    assert(c.events()[0].tick == 0);

    // Rejected input never mutates an existing file's parsed macro.
    auto prior = events;
    std::stringstream invalid("SWIFT1 240\n0 1 0 1\n1 1 2 1\n");
    assert(!ReplayCore::read(invalid, prior));
    assert(prior == events);
    std::stringstream garbage("SWIFT1 240\n0 1 0 1\nnot an event\n");
    assert(!ReplayCore::read(garbage, prior));
    assert(prior == events);
    std::stringstream wrongTps("SWIFT1 360\n0 1 0 1\n");
    assert(!ReplayCore::read(wrongTps, prior));

    // Same-tick ordering also works with all six player/button channels.
    c.beginRecord();
    for (int b=1;b<=3;++b) {
        c.physicalInput(true,b,false);
        c.physicalInput(true,b,true);
    }
    assert(c.events().size() == 6);
    for(int b=1;b<=3;++b) {
        assert(c.held(false,b) && c.held(true,b));
    }
    std::cout << "PASS: independent P1/P2 capture, simultaneous events, replay, reset, and parser\n";
}
