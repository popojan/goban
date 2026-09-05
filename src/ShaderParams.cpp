#include "ShaderParams.h"

#include <spdlog/spdlog.h>

std::vector<ShaderParam> resolveShaderParams(const nlohmann::json& declarations,
                                             const nlohmann::json& shaderEntry,
                                             const nlohmann::json& saved) {
    std::vector<ShaderParam> params;
    if (!declarations.is_object()) return params;

    for (auto it = declarations.begin(); it != declarations.end(); ++it) {
        const nlohmann::json& d = it.value();
        if (!d.is_object()) {
            spdlog::warn("shader_params[{}] is not an object; ignored.", it.key());
            continue;
        }

        // The type is explicit rather than inferred from `default`, because JSON
        // `0` and `0.0` are one keystroke apart and different types to
        // nlohmann — an inferred float written `"default": 0` would come back an
        // integer. ADR-0017 decision 1.
        const std::string type = d.value("type", std::string());
        if (type != "bool") {
            spdlog::warn("shader_params[{}]: type '{}' is not supported yet "
                         "(M1 is booleans only); ignored.", it.key(), type);
            continue;
        }

        ShaderParam p;
        p.name = it.key();
        p.label = d.value("label", p.name);
        p.group = d.value("group", std::string());
        p.requires_ = d.value("requires", std::string());

        // Silently, and on purpose: a shader with no bowls is not misconfigured,
        // it is four of the six we ship.
        if (!p.requires_.empty()) {
            if (!shaderEntry.is_object()) continue;
            const auto cap = shaderEntry.find(p.requires_);
            if (cap == shaderEntry.end()) continue;
            const bool present = cap->is_boolean() ? cap->get<bool>()
                               : cap->is_number()  ? cap->get<double>() != 0.0
                               : false;
            if (!present) continue;
        }

        if (!d.contains("default")) {
            spdlog::warn("shader_params[{}] has no default; assuming true.", p.name);
        }
        p.defaultValue = d.value("default", true);
        p.value = p.defaultValue;

        if (saved.is_object()) {
            const auto v = saved.find(p.name);
            if (v != saved.end()) {
                if (v->is_boolean()) {
                    p.value = v->get<bool>();
                } else {
                    spdlog::warn("Saved value for shader parameter '{}' is not a "
                                 "boolean; using the default.", p.name);
                }
            }
        }

        params.push_back(std::move(p));
    }
    return params;
}

const ShaderParam* findShaderParam(const std::vector<ShaderParam>& params,
                                   const std::string& name) {
    for (const auto& p : params) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

bool shaderParamValue(const std::vector<ShaderParam>& params,
                      const std::string& name, bool fallback) {
    const ShaderParam* p = findShaderParam(params, name);
    return p ? p->value : fallback;
}
