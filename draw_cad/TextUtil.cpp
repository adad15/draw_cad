#include "TextUtil.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace text_util {

std::string utf8Literal(const char8_t* text) {
    return std::string(
        reinterpret_cast<const char*>(text),
        std::char_traits<char8_t>::length(text));
}

std::string stripUtf8Bom(std::string text) {
    constexpr unsigned char bom0 = 0xEF;
    constexpr unsigned char bom1 = 0xBB;
    constexpr unsigned char bom2 = 0xBF;
    if (text.size() >= 3
        && static_cast<unsigned char>(text[0]) == bom0
        && static_cast<unsigned char>(text[1]) == bom1
        && static_cast<unsigned char>(text[2]) == bom2) {
        text.erase(0, 3);
    }
    return text;
}

std::string trimAsciiWhitespace(std::string text) {
    auto not_space = [](unsigned char ch) {
        return std::isspace(ch) == 0;
    };

    const auto begin = std::find_if(
        text.begin(),
        text.end(),
        [&](char ch) { return not_space(static_cast<unsigned char>(ch)); });
    if (begin == text.end()) {
        return "";
    }

    const auto end = std::find_if(
        text.rbegin(),
        text.rend(),
        [&](char ch) { return not_space(static_cast<unsigned char>(ch)); }).base();

    return std::string(begin, end);
}

std::string normalizeText(std::string text, bool trim_text) {
    text = stripUtf8Bom(std::move(text));
    if (trim_text) {
        text = trimAsciiWhitespace(std::move(text));
    }
    return text;
}

std::string joinStrings(const std::vector<std::string>& items, std::string_view separator) {
    std::ostringstream oss;
    for (size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            oss << separator;
        }
        oss << items[index];
    }
    return oss.str();
}

bool parseDoubleStrict(const std::string& text, double& out_value) {
    const std::string normalized = normalizeText(text);
    if (normalized.empty()) {
        return false;
    }

    char* end = nullptr;
    const double value = std::strtod(normalized.c_str(), &end);
    if (end == normalized.c_str()) {
        return false;
    }

    while (end != nullptr && *end != '\0') {
        if (std::isspace(static_cast<unsigned char>(*end)) == 0) {
            return false;
        }
        ++end;
    }

    out_value = value;
    return true;
}

} // namespace text_util
