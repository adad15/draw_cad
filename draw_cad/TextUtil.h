#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace text_util {

std::string utf8Literal(const char8_t* text);
std::string stripUtf8Bom(std::string text);
std::string trimAsciiWhitespace(std::string text);
std::string normalizeText(std::string text, bool trim_text = true);
std::string joinStrings(const std::vector<std::string>& items, std::string_view separator);
bool parseDoubleStrict(const std::string& text, double& out_value);

} // namespace text_util
