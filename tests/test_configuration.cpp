// Tests for Configuration — the application config and the keybinding table.
//
// Two things here had no coverage and both are load-bearing. `$include` is how
// every language config is built (config/cs.json includes base.json and
// overrides a handful of keys), so a merge that dropped or clobbered the wrong
// field would break all five interfaces at once. And the keybinding table
// decides what every key does.
//
// The chord form is new: bindings used to be a bare KeyIdentifier, which made
// Ctrl+S impossible to express. The cases below pin that an entry with no
// modifiers still means exactly what it always did.
#include <doctest/doctest.h>

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Configuration.h"

namespace {

void quietLogging() {
    static bool done = false;
    if (!done) {
        spdlog::set_level(spdlog::level::off);
        done = true;
    }
}

struct ConfigFixture {
    std::filesystem::path dir;

    ConfigFixture() {
        quietLogging();
        static int counter = 0;
        const auto base = std::filesystem::temp_directory_path();
        do {
            dir = base / ("goban_config_test_" + std::to_string(counter++));
        } while (std::filesystem::exists(dir));
        std::filesystem::create_directories(dir);
    }

    ~ConfigFixture() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    std::string write(const std::string& name, const std::string& contents) const {
        const auto path = dir / name;
        std::ofstream out(path);
        out << contents;
        return path.string();
    }
};

// KI_S is 30, KI_O is 26, KI_U is 32 — the codes config/base.json uses.
constexpr auto KEY_S = Rml::Input::KI_S;
constexpr auto KEY_O = Rml::Input::KI_O;
constexpr auto KEY_U = Rml::Input::KI_U;

}  // namespace

TEST_CASE("a missing config file is invalid rather than fatal") {
    ConfigFixture f;
    Configuration config((f.dir / "nope.json").string());
    CHECK_FALSE(config.valid);
}

TEST_CASE("malformed JSON is reported rather than thrown") {
    ConfigFixture f;
    const auto path = f.write("bad.json", R"({"controls": [ )");
    CHECK_NOTHROW(Configuration{path});
    Configuration config(path);
    CHECK_FALSE(config.valid);
}

TEST_CASE("an unmodified binding keeps meaning exactly what it did") {
    ConfigFixture f;
    const auto path = f.write("c.json", R"({
        "controls": [
            {"key": 32, "command": "undo move"},
            {"key": 30, "command": "zoom camera"}
        ]
    })");
    Configuration config(path);
    REQUIRE(config.valid);

    // The historical form, and the default argument, are the same thing.
    CHECK(config.getCommand(KEY_U) == "undo move");
    CHECK(config.getCommand(KEY_U, KeyMod::NONE) == "undo move");
    CHECK(config.getCommand(KEY_S) == "zoom camera");
}

TEST_CASE("a modified binding does not fire unmodified, and vice versa") {
    ConfigFixture f;
    const auto path = f.write("c.json", R"({
        "controls": [
            {"key": 30, "command": "zoom camera"},
            {"key": 30, "ctrl": true, "command": "save"}
        ]
    })");
    Configuration config(path);
    REQUIRE(config.valid);

    // The whole point: S and Ctrl+S are different bindings on the same key.
    CHECK(config.getCommand(KEY_S) == "zoom camera");
    CHECK(config.getCommand(KEY_S, KeyMod::CTRL) == "save");

    // Modifiers match exactly. Falling back to the unmodified binding would make
    // every accelerator also fire its plain twin — Ctrl+S would zoom the camera.
    CHECK(config.getCommand(KEY_S, KeyMod::SHIFT).empty());
    CHECK(config.getCommand(KEY_S, KeyMod::CTRL | KeyMod::SHIFT).empty());
}

TEST_CASE("shift and alt are distinct from ctrl and from each other") {
    ConfigFixture f;
    const auto path = f.write("c.json", R"({
        "controls": [
            {"key": 26, "ctrl": true, "command": "load"},
            {"key": 26, "shift": true, "command": "archive"},
            {"key": 26, "alt": true, "command": "report_bug"},
            {"key": 26, "ctrl": true, "shift": true, "command": "clear"}
        ]
    })");
    Configuration config(path);
    REQUIRE(config.valid);

    CHECK(config.getCommand(KEY_O, KeyMod::CTRL) == "load");
    CHECK(config.getCommand(KEY_O, KeyMod::SHIFT) == "archive");
    CHECK(config.getCommand(KEY_O, KeyMod::ALT) == "report_bug");
    CHECK(config.getCommand(KEY_O, KeyMod::CTRL | KeyMod::SHIFT) == "clear");
    CHECK(config.getCommand(KEY_O).empty());
}

TEST_CASE("parseModifiers reads the three booleans and defaults to none") {
    CHECK(Configuration::parseModifiers(nlohmann::json::object()) == KeyMod::NONE);
    CHECK(Configuration::parseModifiers({{"ctrl", true}}) == KeyMod::CTRL);
    CHECK(Configuration::parseModifiers({{"shift", true}}) == KeyMod::SHIFT);
    CHECK(Configuration::parseModifiers({{"alt", true}}) == KeyMod::ALT);
    CHECK(Configuration::parseModifiers({{"ctrl", true}, {"alt", true}})
          == (KeyMod::CTRL | KeyMod::ALT));
    // Explicitly false is the same as absent.
    CHECK(Configuration::parseModifiers({{"ctrl", false}}) == KeyMod::NONE);
}

TEST_CASE("an unbound key yields no command") {
    ConfigFixture f;
    const auto path = f.write("c.json", R"({"controls": []})");
    Configuration config(path);
    REQUIRE(config.valid);
    CHECK(config.getCommand(KEY_S).empty());
    CHECK(config.getCommand(KEY_S, KeyMod::CTRL).empty());
}

TEST_CASE("entries missing a key or a command are skipped, not fatal") {
    ConfigFixture f;
    const auto path = f.write("c.json", R"({
        "controls": [
            {"command": "no key here"},
            {"key": 30},
            {"key": 32, "command": "undo move"}
        ]
    })");
    Configuration config(path);
    REQUIRE(config.valid);
    CHECK(config.getCommand(KEY_U) == "undo move");
    CHECK(config.getCommand(KEY_S).empty());
}

TEST_CASE("the last binding for a chord wins") {
    ConfigFixture f;
    const auto path = f.write("c.json", R"({
        "controls": [
            {"key": 30, "command": "first"},
            {"key": 30, "command": "second"}
        ]
    })");
    Configuration config(path);
    CHECK(config.getCommand(KEY_S) == "second");
}

TEST_CASE("$include merges the base config and the includer overrides it") {
    // How every language config is built: config/cs.json includes base.json and
    // replaces `gui` and a few labels.
    ConfigFixture f;
    f.write("base.json", R"({
        "gui": "./config/gui/en",
        "shaders": [{"name": "Red Carpet"}],
        "controls": [{"key": 32, "command": "undo move"}]
    })");
    const auto path = f.write("lang.json", R"({
        "$include": "base.json",
        "gui": "./config/gui/cs",
        "language_name": "Čeština"
    })");

    Configuration config(path);
    REQUIRE(config.valid);

    CHECK(config.data.value("gui", "") == "./config/gui/cs");           // overridden
    CHECK(config.data.value("language_name", "") == "Čeština");         // added
    REQUIRE(config.data.contains("shaders"));                           // inherited
    CHECK(config.data["shaders"][0].value("name", "") == "Red Carpet");
    CHECK(config.getCommand(KEY_U) == "undo move");                     // inherited
}

TEST_CASE("$include of a missing file fails the whole config") {
    // Rather than silently yielding a config with no bots, fonts or shaders.
    ConfigFixture f;
    const auto path = f.write("lang.json", R"({"$include": "absent.json"})");
    Configuration config(path);
    CHECK_FALSE(config.valid);
}

TEST_CASE("an includer may add bindings to the base ones") {
    ConfigFixture f;
    f.write("base.json", R"({"controls": [{"key": 32, "command": "undo move"}]})");
    const auto path = f.write("lang.json", R"({
        "$include": "base.json",
        "controls": [{"key": 30, "ctrl": true, "command": "save"}]
    })");

    Configuration config(path);
    REQUIRE(config.valid);

    // Bindings *accumulate* across an include chain, and the effective table is
    // not what `data["controls"]` shows. load() recurses first, which parses the
    // base's controls into the map; merge_patch then replaces the `controls`
    // array wholesale, and the includer's entries are added to the same map
    // without it being cleared.
    //
    // The useful half is that a language config can add a binding without
    // restating the whole table. The trap is that it cannot *remove* one: an
    // includer that declares `controls` to override a key ends up with both
    // bindings live, and only the JSON looks replaced.
    CHECK(config.getCommand(KEY_S, KeyMod::CTRL) == "save");   // the includer's
    CHECK(config.getCommand(KEY_U) == "undo move");            // and the base's

    REQUIRE(config.data.contains("controls"));
    CHECK(config.data["controls"].size() == 1);   // …though the JSON shows one
}
