#include "MessageLog.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

#include <algorithm>
#include <ctime>

MessageLog& MessageLog::instance() {
    // See the header: leaked on purpose, because a spdlog sink can outlive
    // ordinary static destruction.
    static MessageLog* singleton = new MessageLog();
    return *singleton;
}

void MessageLog::add(MessageSeverity severity, std::string timestamp, std::string text) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.push_back(LogEntry{severity, std::move(timestamp), std::move(text)});
        while (entries_.size() > capacity_) {
            entries_.pop_front();
        }
    }
    // Outside the lock: a UI thread polling these must never wait on a writer,
    // and neither is ordered against the buffer contents in a way that matters —
    // a reader that sees a new version and copies one entry late simply shows it
    // on the next frame.
    version_.fetch_add(1, std::memory_order_relaxed);
    const int level = static_cast<int>(severity);
    int previous = unseen_.load(std::memory_order_relaxed);
    while (level > previous
           && !unseen_.compare_exchange_weak(previous, level, std::memory_order_relaxed)) {
        // previous was reloaded by compare_exchange_weak; retry only while this
        // entry is still the worse one. A concurrent markSeen() that lands here
        // loses the mark, which is the safe direction: the badge stays up.
    }
}

std::vector<LogEntry> MessageLog::entries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {entries_.begin(), entries_.end()};
}

size_t MessageLog::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

void MessageLog::clear() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }
    version_.fetch_add(1, std::memory_order_relaxed);
    markSeen();
}

void MessageLog::setCapacity(size_t capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    capacity_ = std::max<size_t>(1, capacity);
    while (entries_.size() > capacity_) {
        entries_.pop_front();
    }
}

size_t MessageLog::capacity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return capacity_;
}

namespace {

MessageSeverity toSeverity(spdlog::level::level_enum level) {
    if (level >= spdlog::level::err) return MessageSeverity::Error;
    if (level == spdlog::level::warn) return MessageSeverity::Warning;
    return MessageSeverity::Info;
}

/// Feeds MessageLog from spdlog. base_sink<std::mutex> already serialises
/// sink_it_, and MessageLog locks for itself, so this adds no locking of its own.
class MessageLogSink : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        // Format the payload only. The level is carried structurally so the
        // panel can colour it, and the logger name ("multi_sink") is noise to a
        // player. Never call spdlog from in here — it would re-enter this sink.
        std::string text(msg.payload.data(), msg.payload.size());

        auto asTime = std::chrono::system_clock::to_time_t(msg.time);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &asTime);
#else
        localtime_r(&asTime, &tm);
#endif
        char stamp[16];
        std::strftime(stamp, sizeof(stamp), "%H:%M:%S", &tm);

        MessageLog::instance().add(toSeverity(msg.level), stamp, std::move(text));
    }

    void flush_() override {}
};

}  // namespace

void installMessageLogSink() {
    auto logger = spdlog::default_logger();
    if (!logger) return;

    auto sink = std::make_shared<MessageLogSink>();
    // info and worse. The logger's own level may be debug during development,
    // and a debug-level firehose would evict everything worth reading within
    // seconds of startup.
    sink->set_level(spdlog::level::info);
    logger->sinks().push_back(std::move(sink));

    // The logger only forwards what passes its own level, so a run started with
    // --verbosity warn shows warnings and errors and no info. That is the
    // correct behaviour: the panel reflects the log the user asked for.
}
