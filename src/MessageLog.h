/** \file
 *  \brief The user-visible tail of the log, and the badge that says to look at it.
 *
 * Every diagnostic goban produces — an engine that would not start, a GTP
 * timeout, a rejected move, a failed save — went to `last_run.log` and nowhere
 * else. A user whose engine is missing saw a greyed toolbar and no reason for
 * it, and a bug report came back as "it does nothing".
 *
 * This is the in-memory tail of that log: a bounded ring buffer the UI can show
 * on demand, plus an *unseen* mark so the interface can raise a badge when
 * something worse than `info` arrives. A transient toast was considered and
 * rejected — a message the user happens not to be looking at is no better than
 * a file they never open, and engine failures land in the first seconds, while
 * they are still looking at the board.
 *
 * It is filled by a spdlog sink (`installMessageLogSink()`), not by call sites.
 * That is deliberate: every `spdlog::warn`/`error` already in the codebase
 * surfaces without being touched, and a new one surfaces by existing at all.
 * Nothing here may log, on pain of re-entering the sink that called it.
 */
#ifndef GOBAN_MESSAGELOG_H
#define GOBAN_MESSAGELOG_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

/// Severity as the UI cares about it. spdlog has six levels; the panel needs
/// three, and the badge only needs to know "worse than info".
enum class MessageSeverity { Info = 0, Warning = 1, Error = 2 };

struct LogEntry {
    MessageSeverity severity = MessageSeverity::Info;
    std::string timestamp;   ///< "16:04:12", local time, seconds are enough
    std::string text;        ///< The formatted payload, without level or logger name
};

/** \brief Bounded, thread-safe tail of the log with an unseen-since-opened mark.
 *
 * Written from any thread — the game thread, the engine loader threads, the
 * per-engine stderr readers — and read from the UI thread. The counters the UI
 * polls every frame (`version()`, `unseenSeverity()`) are atomics, so the common
 * case takes no lock at all; only `entries()`, which the panel calls when it is
 * open and something changed, does.
 */
class MessageLog {
public:
    /// Deliberately leaked. A spdlog sink outlives ordinary static destruction
    /// order — the registry flushes at exit — and a sink writing into a
    /// destroyed singleton is a crash on the way out, which is the worst
    /// possible time to have one. Leaking one ring buffer at process exit costs
    /// nothing the OS does not reclaim.
    static MessageLog& instance();

    void add(MessageSeverity severity, std::string timestamp, std::string text);

    /// Newest last. A copy, so the caller can walk it without holding the lock.
    [[nodiscard]] std::vector<LogEntry> entries() const;

    [[nodiscard]] size_t size() const;

    /// Bumped on every add. The UI rebuilds the panel only when this changes,
    /// rather than copying the whole buffer every frame.
    [[nodiscard]] uint64_t version() const { return version_.load(std::memory_order_relaxed); }

    /// Worst severity added since the last markSeen(), or Info if nothing worse
    /// than info has arrived. This is what decides whether the badge shows, and
    /// in which colour.
    [[nodiscard]] MessageSeverity unseenSeverity() const {
        return static_cast<MessageSeverity>(unseen_.load(std::memory_order_relaxed));
    }
    [[nodiscard]] bool hasUnseen() const {
        return unseen_.load(std::memory_order_relaxed) > static_cast<int>(MessageSeverity::Info);
    }
    /// The user has looked. Clears the badge, not the buffer.
    void markSeen() { unseen_.store(static_cast<int>(MessageSeverity::Info), std::memory_order_relaxed); }

    void clear();

    /// Entries kept before the oldest is dropped. Bounded because an engine in a
    /// retry loop can produce thousands of lines, and this lives in memory for
    /// the whole session.
    void setCapacity(size_t capacity);
    [[nodiscard]] size_t capacity() const;

private:
    MessageLog() = default;

    mutable std::mutex mutex_;
    std::deque<LogEntry> entries_;
    size_t capacity_ = 200;
    std::atomic<uint64_t> version_{0};
    std::atomic<int> unseen_{static_cast<int>(MessageSeverity::Info)};
};

/// Attach the log to spdlog's default logger. Captures `info` and worse: the
/// panel is a log, so info belongs in it, but only warnings and errors raise the
/// badge. Call once, after the default logger is installed.
void installMessageLogSink();

#endif // GOBAN_MESSAGELOG_H
