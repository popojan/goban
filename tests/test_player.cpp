// Tests for the handshake between the UI thread and the game loop over a human
// player's move.
//
// This is the narrowest of the concurrency boundaries in the program and the
// least obvious: LocalHumanPlayer::genmove() blocks on a condition variable
// until someone calls suggestMove(). The subtlety is that the game loop calls
// suggestMove() too, with whatever it found in `queuedMove` — which is normally
// nothing at all.
#include <doctest/doctest.h>

#include "player.h"

#include <atomic>
#include <chrono>
#include <thread>

TEST_CASE("suggesting nothing does not discard a move already suggested") {
    // The regression. The game loop does, in order:
    //
    //   lock; suggested = queuedMove; queuedMove = INVALID; playerToMove = p
    //   p->suggestMove(suggested)          <-- usually INVALID
    //   p->genmove()
    //
    // A board click landing between the second and third lines reaches
    // GameThread::playLocalMove(), which sees playerToMove already set and
    // delivers the move straight to the player. The loop's own
    // suggestMove(INVALID) then overwrote it, and genmove() went back to waiting
    // for a move the user had already made. The stone simply never appeared.
    LocalHumanPlayer player("Human");

    player.suggestMove(Move(Position(3, 3), Color::BLACK));   // the click
    player.suggestMove(Move(Move::INVALID, Color::BLACK));    // the loop, right after

    const Move got = player.genmove(Color::BLACK);
    CHECK(static_cast<bool>(got));
    CHECK(got == Move::NORMAL);
    CHECK(got.pos == Position(3, 3));
}

TEST_CASE("genmove returns the move that was suggested, once") {
    LocalHumanPlayer player("Human");
    player.suggestMove(Move(Position(2, 5), Color::WHITE));

    const Move first = player.genmove(Color::WHITE);
    CHECK(first.pos == Position(2, 5));

    // The move is consumed: a second genmove must block rather than replay it,
    // or every move would be played twice.
    std::atomic<bool> returned{false};
    std::thread waiter([&]() {
        player.genmove(Color::WHITE);
        returned = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK_FALSE(returned.load());

    player.suggestMove(Move(Move::INTERRUPT, Color::WHITE));
    waiter.join();
    CHECK(returned.load());
}

TEST_CASE("a blocked genmove is released by a move, a pass or an interrupt") {
    // The three things the game loop and the UI actually send. INTERRUPT is how
    // setGameMode() unblocks a human so the loop can re-evaluate, so it must
    // still get through the INVALID guard above.
    const Move::Special releases[] = {
        Move::NORMAL, Move::PASS, Move::RESIGN, Move::INTERRUPT, Move::KIBITZED,
    };

    for (const Move::Special special : releases) {
        const Move release = (special == Move::NORMAL)
            ? Move(Position(0, 0), Color::BLACK)
            : Move(special, Color::BLACK);
        LocalHumanPlayer player("Human");
        std::atomic<bool> done{false};
        Move got;
        std::thread waiter([&]() {
            got = player.genmove(Color::BLACK);
            done = true;
        });

        // Give the waiter time to actually reach the wait, so this exercises the
        // notify path rather than a value that was already sitting there.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        CHECK_FALSE(done.load());

        player.suggestMove(release);
        waiter.join();
        CHECK(done.load());
        CHECK(got == special);
    }
}

TEST_CASE("an interrupt is not swallowed by a suggestion that follows it") {
    // Ordering the other way round: the loop's INVALID must not undo an
    // interrupt either, or a mode switch would leave the loop wedged.
    LocalHumanPlayer player("Human");
    player.suggestMove(Move(Move::INTERRUPT, Color::BLACK));
    player.suggestMove(Move(Move::INVALID, Color::BLACK));

    const Move got = player.genmove(Color::BLACK);
    CHECK(got == Move::INTERRUPT);
}
