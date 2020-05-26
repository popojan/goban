#include "gtpclient.h"
#include <string>

void replaceAll(std::string& out, const std::string& what, const std::string& by) {
    size_t index(0);
    while (true) {
        spdlog::trace("replace [{}] by [{}] in [{}]", what, by, out);
        index = out.find(what, index);
        if (index == std::string::npos) break;
        out.replace(index, what.size(), by);
        index += what.size();
    }
}