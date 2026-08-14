// Tests for MessageLog — the in-memory tail of the log the UI shows, and the
// badge that tells the user to look at it.
//
// The point of this class is that a diagnostic reaches the *user*, so the cases
// below are about what the interface can ask it: is there something new worth a
// badge, what is the worst thing that arrived, and does a talkative engine push
// the interesting line out of the buffer.
//
// It is filled from a spdlog sink, so the sink test matters as much as the
// buffer ones: the whole design rests on existing spdlog::warn/error call sites
// surfacing without being edited.
#include <doctest/doctest.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/null_sink.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "MessageLog.h"

namespace {

/// The log is a process-wide singleton, so every case starts from a known state.
MessageLog& freshLog(size_t capacity = 200) {
    auto& log = MessageLog::instance();
    log.clear();
    log.setCapacity(capacity);
    return log;
}

}  // namespace

TEST_CASE("an empty log has nothing to show and nothing to flag") {
    auto& log = freshLog();
    CHECK(log.size() == 0);
    CHECK(log.entries().empty());
    CHECK_FALSE(log.hasUnseen());
    CHECK(log.unseenSeverity() == MessageSeverity::Info);
}

TEST_CASE("entries come back oldest first") {
    auto& log = freshLog();
    log.add(MessageSeverity::Info, "10:00:00", "first");
    log.add(MessageSeverity::Warning, "10:00:01", "second");

    auto entries = log.entries();
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].text == "first");
    CHECK(entries[1].text == "second");
    CHECK(entries[1].severity == MessageSeverity::Warning);
    CHECK(entries[1].timestamp == "10:00:01");
}

TEST_CASE("info alone raises no badge, a warning does") {
    auto& log = freshLog();

    log.add(MessageSeverity::Info, "10:00:00", "loading GNU Go");
    CHECK_FALSE(log.hasUnseen());

    log.add(MessageSeverity::Warning, "10:00:01", "engine folder does not exist");
    CHECK(log.hasUnseen());
    CHECK(log.unseenSeverity() == MessageSeverity::Warning);
}

TEST_CASE("the badge shows the worst unseen severity, and info cannot lower it") {
    auto& log = freshLog();

    log.add(MessageSeverity::Warning, "10:00:00", "no engine claimed main");
    log.add(MessageSeverity::Error, "10:00:01", "failed to start GTP engine");
    CHECK(log.unseenSeverity() == MessageSeverity::Error);

    // A later, milder message must not downgrade the badge — the error is still
    // the thing the user has not seen.
    log.add(MessageSeverity::Warning, "10:00:02", "using fallback coach");
    log.add(MessageSeverity::Info, "10:00:03", "engine ready");
    CHECK(log.unseenSeverity() == MessageSeverity::Error);
}

TEST_CASE("markSeen clears the badge but keeps the entries") {
    auto& log = freshLog();
    log.add(MessageSeverity::Error, "10:00:00", "boom");
    REQUIRE(log.hasUnseen());

    log.markSeen();
    CHECK_FALSE(log.hasUnseen());
    CHECK(log.unseenSeverity() == MessageSeverity::Info);
    CHECK(log.size() == 1);          // opening the panel must not empty it

    log.add(MessageSeverity::Warning, "10:00:01", "again");
    CHECK(log.hasUnseen());          // and a new one flags again
}

TEST_CASE("the version counter changes on every add, so the panel knows to rebuild") {
    auto& log = freshLog();
    const uint64_t start = log.version();

    log.add(MessageSeverity::Info, "10:00:00", "one");
    const uint64_t afterFirst = log.version();
    CHECK(afterFirst != start);

    log.add(MessageSeverity::Info, "10:00:01", "two");
    CHECK(log.version() != afterFirst);
}

TEST_CASE("the buffer is bounded and drops the oldest first") {
    auto& log = freshLog(3);

    for (int i = 0; i < 5; ++i) {
        log.add(MessageSeverity::Info, "10:00:00", "line " + std::to_string(i));
    }

    auto entries = log.entries();
    REQUIRE(entries.size() == 3);
    CHECK(log.size() == 3);
    CHECK(entries[0].text == "line 2");   // 0 and 1 evicted
    CHECK(entries[2].text == "line 4");
}

TEST_CASE("shrinking the capacity trims what is already there") {
    auto& log = freshLog(10);
    for (int i = 0; i < 6; ++i) {
        log.add(MessageSeverity::Info, "10:00:00", "line " + std::to_string(i));
    }

    log.setCapacity(2);
    auto entries = log.entries();
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].text == "line 4");
    CHECK(entries[1].text == "line 5");
}

TEST_CASE("a capacity of zero is refused rather than dividing by it") {
    auto& log = freshLog(10);
    log.setCapacity(0);
    CHECK(log.capacity() >= 1);

    log.add(MessageSeverity::Error, "10:00:00", "still recorded");
    CHECK(log.size() == 1);
}

TEST_CASE("clear empties the buffer and the badge") {
    auto& log = freshLog();
    log.add(MessageSeverity::Error, "10:00:00", "boom");

    log.clear();
    CHECK(log.size() == 0);
    CHECK_FALSE(log.hasUnseen());
}

TEST_CASE("concurrent writers neither lose entries nor corrupt the buffer") {
    // The real writers are the game thread, the engine loader threads and one
    // stderr reader per engine, all of which can log at once during startup —
    // which is exactly when the interesting failures happen.
    auto& log = freshLog(10000);

    constexpr int kThreads = 4;
    constexpr int kPerThread = 250;
    std::vector<std::thread> writers;
    writers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        writers.emplace_back([&log, t]() {
            for (int i = 0; i < kPerThread; ++i) {
                log.add(t == 0 ? MessageSeverity::Error : MessageSeverity::Info,
                        "10:00:00", "thread " + std::to_string(t));
            }
        });
    }
    for (auto& w : writers) w.join();

    CHECK(log.size() == kThreads * kPerThread);
    CHECK(log.entries().size() == kThreads * kPerThread);
    CHECK(log.unseenSeverity() == MessageSeverity::Error);
}

TEST_CASE("a reader may copy the entries while writers are adding") {
    auto& log = freshLog(64);   // small, so eviction races the reader too

    std::atomic<bool> stop{false};
    std::thread writer([&log, &stop]() {
        while (!stop.load()) {
            log.add(MessageSeverity::Warning, "10:00:00", "engine said something");
        }
    });

    for (int i = 0; i < 200; ++i) {
        auto entries = log.entries();
        CHECK(entries.size() <= 64);
        for (const auto& e : entries) {
            CHECK_FALSE(e.text.empty());   // no torn string
        }
    }

    stop.store(true);
    writer.join();
}

TEST_CASE("the spdlog sink captures existing call sites without editing them") {
    // The whole design rests on this: every spdlog::warn/error already in the
    // codebase reaches the user because the sink is installed, not because
    // anyone changed the call.
    auto previous = spdlog::default_logger();
    auto logger = std::make_shared<spdlog::logger>(
        "messagelog_test", std::make_shared<spdlog::sinks::null_sink_mt>());
    logger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(logger);

    installMessageLogSink();
    auto& log = freshLog();

    spdlog::error("Failed to load engine '{}': {}", "GNU Go", "not found");
    spdlog::warn("Engine [{}] folder '{}' does not exist", "katago", "./engine");
    spdlog::info("Setting [{}] engine as coach and referee.", "GNU Go");
    spdlog::debug("gtp err = {}", "noise");   // below the sink's level

    auto entries = log.entries();
    REQUIRE(entries.size() == 3);
    CHECK(entries[0].severity == MessageSeverity::Error);
    CHECK(entries[0].text == "Failed to load engine 'GNU Go': not found");
    CHECK(entries[1].severity == MessageSeverity::Warning);
    CHECK(entries[2].severity == MessageSeverity::Info);
    CHECK(entries[2].text == "Setting [GNU Go] engine as coach and referee.");

    // Formatted payload only — no level, no logger name, and a timestamp the
    // panel can show.
    CHECK(entries[0].text.find("multi_sink") == std::string::npos);
    CHECK(entries[0].text.find("error") == std::string::npos);
    CHECK(entries[0].timestamp.size() == 8);   // HH:MM:SS

    CHECK(log.unseenSeverity() == MessageSeverity::Error);

    spdlog::set_default_logger(previous);
}
