#ifndef GOBAN_SHADERPARAMS_H
#define GOBAN_SHADERPARAMS_H

/// \file
/// Tunable shader parameters: what a shader offers, and what the user set it to.
///
/// See `docs/adr/0017-tunable-shader-parameters-are-declared-in-configuration.md`.
/// The short version: a parameter is **declared in the configuration**, in a
/// `shader_params` object keyed by the uniform's own name, and the GLSL declares
/// nothing but the uniform. The alternative — metadata in a GLSL comment, which
/// `backlog/shader-parameter-editor.md` originally chose — loses to the same
/// argument ADR-0011 made about the move-quality palette, plus three others:
/// a comment cannot be localised, it is a second parsing mechanism where
/// `Configuration` already does layered defaults, and it splits one concept
/// across two file formats keyed differently.
///
/// Everything here is pure over `nlohmann::json`, with no GL context, no config
/// singleton and no thread, for the reason `resolveQualityPalette()` is: it has
/// to be testable in `goban_core`, where its callers cannot follow.

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

/// One tunable parameter, resolved for one shader.
///
/// M1 is booleans only — scene features on and off. `type` is carried anyway so
/// that a declaration of a type this build does not implement is *reported*
/// rather than silently dropped, which is the difference between a typo you find
/// now and one you find when the slider does not appear.
struct ShaderParam {
    /// The uniform name. Also the key in `shader_params`, deliberately: two
    /// identities for one thing is how two keys drift apart.
    std::string name;
    /// User-visible, and localisable — a language file can `merge_patch` this
    /// one key, which is the whole reason the block is an object and not an
    /// array. See ADR-0017 decision 1.
    std::string label;
    /// Free-form grouping for the generated UI ("Scene", "Lighting"). A flat key
    /// rather than nesting, so regrouping overwrites a string instead of moving
    /// an object between parents.
    std::string group;
    /// A capability key on the *shader entry* that must be present and truthy
    /// for this parameter to be offered at all. Empty means always offered.
    ///
    /// This is what keeps the capability check generic: `showBowls` requires
    /// `bowls`, so a shader whose entry does not declare it never offers the
    /// toggle, and no C++ anywhere knows the word "bowls".
    std::string requires_;

    bool defaultValue = false;
    /// The value in force: the default, with the user's saved choice laid over.
    bool value = false;
};

/// The parameters a given shader offers, in declaration order.
///
/// \param declarations the global `shader_params` object from `config/base.json`
/// \param shaderEntry  that shader's own entry, consulted only for `requires_`
/// \param saved        the user's values for this shader (an object of
///                     name -> bool), or null — absent is the ordinary case
///
/// A parameter whose `requires_` is unmet is **omitted silently**: four of the
/// six shipped shaders have no bowls, and that is not a misconfiguration. A
/// parameter of an unsupported type is omitted *loudly*.
std::vector<ShaderParam> resolveShaderParams(const nlohmann::json& declarations,
                                             const nlohmann::json& shaderEntry,
                                             const nlohmann::json& saved);

/// Look a parameter up by name. Returns nullptr when it is not offered — which
/// callers must treat as "the feature is on", not as "off": a shader with no
/// `showLids` declaration draws its lids unconditionally, exactly as it did
/// before this mechanism existed.
const ShaderParam* findShaderParam(const std::vector<ShaderParam>& params,
                                   const std::string& name);

/// The value in force, or \p fallback when the parameter is not offered.
bool shaderParamValue(const std::vector<ShaderParam>& params,
                      const std::string& name, bool fallback);

#endif //GOBAN_SHADERPARAMS_H
