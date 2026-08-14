/** \file
 *  \brief The application config (`config/*.json`) and the keybinding table.
 *
 * Read-only as far as the application is concerned — the user edits it by hand.
 * Distinct from UserSettings, which is what the application writes.
 */
#ifndef GOBAN_CONFIGURATION_H
#define GOBAN_CONFIGURATION_H

#include <RmlUi/Core/Input.h>
#include <unordered_map>
#include <string>
#include <nlohmann/json.hpp>

/// Modifier flags for a keybinding. A bitmask rather than a set of bools so a
/// binding is one hashable value.
namespace KeyMod {
    enum : unsigned {
        NONE  = 0,
        CTRL  = 1u << 0,
        SHIFT = 1u << 1,
        ALT   = 1u << 2,
    };
}

/// A key plus the modifiers held with it.
///
/// Bindings were a bare KeyIdentifier, which made Ctrl+S, Ctrl+O and Ctrl+Z
/// impossible to express — and every unmodified letter was already spent on
/// camera and shader controls, so there was nowhere to put them. A chord with no
/// modifiers is exactly the old behaviour, so existing `controls` entries and
/// anyone's hand-edited keybindings keep working unchanged.
struct KeyChord {
    Rml::Input::KeyIdentifier key = Rml::Input::KI_UNKNOWN;
    unsigned mods = KeyMod::NONE;

    bool operator==(const KeyChord& other) const {
        return key == other.key && mods == other.mods;
    }
};

struct KeyChordHash {
    size_t operator()(const KeyChord& chord) const {
        return (static_cast<size_t>(chord.key) << 3) ^ chord.mods;
    }
};

struct Configuration {

    explicit Configuration(const std::string& fileName) {
        valid = load(fileName);
    }
    bool load(const std::string& fileName);

    nlohmann::json data;
    bool valid = false;

    /// The command bound to a chord, or empty. Modifiers must match exactly:
    /// Ctrl+S does not fall back to the binding for S, or every accelerator
    /// would also fire its unmodified twin.
    std::string getCommand(Rml::Input::KeyIdentifier key, unsigned mods = KeyMod::NONE) const;

    /// Parses the `ctrl`/`shift`/`alt` booleans of one `controls` entry.
    /// Exposed for tests.
    static unsigned parseModifiers(const nlohmann::json& entry);

private:

    void addKey(KeyChord chord, const std::string& cmd);

    std::unordered_map<KeyChord, std::string, KeyChordHash> keyToCommand;
};

#endif
