// Tests for UserSettings — everything that survives a restart.
//
// This class had no coverage at all, and two of its properties are the kind that
// only bite in the field: the save must be atomic, because a crash mid-write
// used to lose every setting silently; and it is written from more than one
// thread, because GameThread::setFixedHandicap() calls setKomi() from the game
// thread while GobanView saves the camera from the UI thread.
//
// The load cases matter for the same reason. A malformed or truncated file is
// not hypothetical — it is exactly what the old non-atomic save produced when
// interrupted — and the recovery has to be "fall back to defaults and keep
// running", never a crash or a half-applied state.
#include <doctest/doctest.h>

#include <spdlog/spdlog.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "UserSettings.h"

namespace {

void quietLogging() {
    static bool done = false;
    if (!done) {
        spdlog::set_level(spdlog::level::off);
        done = true;
    }
}

/// A scratch settings file, and a singleton pointed at it. UserSettings is
/// process-wide, so each case must redirect it and reset the fields it checks.
struct SettingsFixture {
    std::filesystem::path dir;

    SettingsFixture() {
        quietLogging();
        static int counter = 0;
        const auto base = std::filesystem::temp_directory_path();
        do {
            dir = base / ("goban_settings_test_" + std::to_string(counter++));
        } while (std::filesystem::exists(dir));
        std::filesystem::create_directories(dir);
        UserSettings::instance().setSettingsFile(path().string());
    }

    ~SettingsFixture() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    [[nodiscard]] std::filesystem::path path() const { return dir / "user.json"; }

    void write(const std::string& contents) const {
        std::ofstream out(path());
        out << contents;
    }

    [[nodiscard]] std::string read() const {
        std::ifstream in(path());
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }

    UserSettings& settings() const { return UserSettings::instance(); }
};

}  // namespace

TEST_CASE("a missing settings file leaves the defaults in place") {
    SettingsFixture f;
    f.settings().load();

    // No file is the first-run case, not an error.
    CHECK(f.settings().getBoardSize() >= 9);
    CHECK_FALSE(f.settings().hasSettings());
}

TEST_CASE("settings round-trip through save and load") {
    SettingsFixture f;
    f.settings().setGameSettings(13, 7.5f, 4, "Alice", "Bob");
    f.settings().setLastConfig("./config/cs.json");

    CameraState cam;
    cam.rotX = -0.9f; cam.rotY = 0.5f; cam.rotZ = 0.0f; cam.rotW = 0.0f;
    cam.panX = 0.25f; cam.panY = -0.2f; cam.distance = 3.1f;
    f.settings().setSavedCamera(cam);
    f.settings().save();

    f.settings().load();
    CHECK(f.settings().getBoardSize() == 13);
    CHECK(f.settings().getKomi() == doctest::Approx(7.5f));
    CHECK(f.settings().getHandicap() == 4);
    CHECK(f.settings().getBlackPlayer() == "Alice");
    CHECK(f.settings().getWhitePlayer() == "Bob");
    CHECK(f.settings().getLastConfig() == "./config/cs.json");

    REQUIRE(f.settings().hasSavedCamera());
    const CameraState back = f.settings().getSavedCamera();
    CHECK(back.rotX == doctest::Approx(-0.9f));
    CHECK(back.panY == doctest::Approx(-0.2f));
    CHECK(back.distance == doctest::Approx(3.1f));
}

TEST_CASE("a malformed settings file does not throw and does not half-apply") {
    SettingsFixture f;
    f.settings().setBoardSize(9);
    f.settings().save();

    // Truncated mid-object: precisely what the old non-atomic save produced if
    // the process died between the truncate and the write.
    f.write(R"({"game": {"board_size": 19, "komi": )");

    CHECK_NOTHROW(f.settings().load());
    // The parse threw before reaching board_size, so the previous value stands
    // rather than a partially-applied mixture.
    CHECK(f.settings().getBoardSize() == 9);
}

TEST_CASE("an empty settings file is survivable") {
    SettingsFixture f;
    f.write("");
    CHECK_NOTHROW(f.settings().load());
}

TEST_CASE("a file of the wrong shape is survivable") {
    SettingsFixture f;
    // Valid JSON, wrong types throughout. value() falls back on a type mismatch;
    // what must not happen is a throw escaping into startup.
    f.write(R"({"game": "not an object", "camera": 42, "session": [1,2,3]})");
    CHECK_NOTHROW(f.settings().load());
}

TEST_CASE("session state round-trips, including the branch choices") {
    SettingsFixture f;
    f.settings().setSessionFile("./games/2026-08-14.sgf");
    f.settings().setSessionGameIndex(2);
    f.settings().setSessionTreePathLength(27);
    f.settings().setSessionTreePath({0, 1, 0});
    f.settings().setSessionGameMode(GameMode::TSUMEGO);
    f.settings().save();

    f.settings().load();
    CHECK(f.settings().getSessionFile() == "./games/2026-08-14.sgf");
    CHECK(f.settings().getSessionGameIndex() == 2);
    CHECK(f.settings().getSessionTreePathLength() == 27);
    CHECK(f.settings().getSessionTreePath() == std::vector<int>{0, 1, 0});
    CHECK(f.settings().getSessionGameMode() == GameMode::TSUMEGO);

    f.settings().clearSessionState();
    CHECK_FALSE(f.settings().hasSessionState());
    CHECK(f.settings().getSessionTreePath().empty());
    CHECK(f.settings().getSessionGameMode() == GameMode::MATCH);
}

TEST_CASE("the two mode booleans migrate to the one game_mode key") {
    // `tsumego_mode` and `analysis_mode` were independent, so a file written by
    // an older build can carry either — or, since nothing stopped it, both. The
    // enum has no such value; tsumego wins, which is the order finalizeGameLoad()
    // resolves them in and the stricter of the two (a puzzle answers itself).
    SettingsFixture f;
    f.write(R"({"session": {"file": "./x.sgf", "tsumego_mode": true, "analysis_mode": true}})");
    f.settings().load();
    CHECK(f.settings().getSessionGameMode() == GameMode::TSUMEGO);

    f.write(R"({"session": {"file": "./x.sgf", "analysis_mode": true}})");
    f.settings().load();
    CHECK(f.settings().getSessionGameMode() == GameMode::EXPLORE);

    // The new key wins outright, so a stale boolean beside it cannot resurrect
    // a mode the user has since left.
    f.write(R"({"session": {"file": "./x.sgf", "game_mode": "match", "tsumego_mode": true}})");
    f.settings().load();
    CHECK(f.settings().getSessionGameMode() == GameMode::MATCH);

    // An unreadable name is not a mode. Falling back beats throwing out of
    // startup, and beats silently picking whichever value is listed first —
    // which is what NLOHMANN_JSON_SERIALIZE_ENUM would have done here.
    f.write(R"({"session": {"file": "./x.sgf", "game_mode": "analysis"}})");
    CHECK_NOTHROW(f.settings().load());
    CHECK(f.settings().getSessionGameMode() == GameMode::MATCH);
}

TEST_CASE("the shipped default camera is never written to the settings file") {
    // It belongs to config/base.json, which the application does not write.
    // Round-tripping it here would recreate the coupling that leaked local paths
    // and language into the repository once already (5fe4d48).
    SettingsFixture f;
    CameraState def;
    def.distance = 3.1f;
    def.panY = -0.2f;
    f.settings().setDefaultCamera(def);
    f.settings().save();

    const std::string contents = f.read();
    CHECK(contents.find("camera_default") == std::string::npos);

    // And it survives a load, because loading does not touch it.
    f.settings().load();
    CHECK(f.settings().hasDefaultCamera());
    CHECK(f.settings().getDefaultCamera().distance == doctest::Approx(3.1f));
}

TEST_CASE("saving leaves no temp file behind") {
    SettingsFixture f;
    f.settings().setBoardSize(19);
    f.settings().save();

    CHECK(std::filesystem::exists(f.path()));
    CHECK_FALSE(std::filesystem::exists(f.path().string() + ".tmp"));
}

TEST_CASE("a save never leaves the file empty or unparseable") {
    // The atomicity property, stated as the thing a reader can check: at no
    // point does the settings file exist in a truncated state. The old
    // implementation opened the target with std::ofstream, which truncates, so
    // any interruption between open and write lost everything.
    SettingsFixture f;
    f.settings().setGameSettings(19, 6.5f, 0, "Human", "Human");
    f.settings().save();

    std::atomic<bool> stop{false};
    std::atomic<int> reads{0};
    std::atomic<int> bad{0};

    std::thread reader([&]() {
        while (!stop.load()) {
            const std::string contents = f.read();
            if (contents.empty()) { ++bad; continue; }
            try {
                auto parsed = nlohmann::json::parse(contents);
                if (!parsed.contains("game")) ++bad;
            } catch (const std::exception&) {
                ++bad;
            }
            ++reads;
        }
    });

    for (int i = 0; i < 200; ++i) {
        f.settings().setBoardSize(i % 2 ? 19 : 13);   // each setter rewrites the file
    }
    stop.store(true);
    reader.join();

    CHECK(reads.load() > 0);
    CHECK(bad.load() == 0);
}

TEST_CASE("concurrent writers do not corrupt the file or each other") {
    // The real pairing: GameThread::setFixedHandicap() calls setKomi() on the
    // game thread while GobanView saves the camera on the UI thread, and both
    // rewrite the whole file.
    SettingsFixture f;
    f.settings().setGameSettings(19, 6.5f, 0, "Human", "Human");

    std::thread komiWriter([&]() {
        for (int i = 0; i < 150; ++i) f.settings().setKomi(i % 2 ? 0.5f : 7.5f);
    });
    std::thread cameraWriter([&]() {
        for (int i = 0; i < 150; ++i) {
            CameraState cam;
            cam.distance = 3.0f + static_cast<float>(i % 10) * 0.1f;
            f.settings().setCurrentCamera(cam);
            f.settings().save();
        }
    });
    komiWriter.join();
    cameraWriter.join();

    const std::string contents = f.read();
    CHECK_FALSE(contents.empty());
    CHECK_NOTHROW(nlohmann::json::parse(contents));

    CHECK_NOTHROW(f.settings().load());
    CHECK(f.settings().hasGameSettings());
}
