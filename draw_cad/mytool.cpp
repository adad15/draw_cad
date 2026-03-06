#include "mytool.h"
#include <cctype>`
#include <cmath>
#include <sstream>
#include <iomanip>

bool mytool::startsWith(std::string_view value, std::string_view prefix)
{
    if (value.size() < prefix.size()) return false;
    return value.compare(0, prefix.size(), prefix) == 0;
}

std::string mytool::toUpperAscii(std::string value) {
	for (char& ch : value) {
		const unsigned char c = static_cast<unsigned char>(ch);
		ch = static_cast<char>(std::toupper(c));
	}
	return value;
}
// block_name="ABC_Motor-01(Left)", prefix="ABC_" → motor_01_left
// block_name="@@@", prefix="ABC_" → symbol
std::string mytool::toIdFromBlockName(const std::string& block_name, std::string_view prefix) {
	std::string work = block_name;
	if (startsWith(work, prefix)) {
		work = work.substr(prefix.size());
	}

	std::string out;
	out.reserve(work.size());
	bool last_is_underscore = false;
	for (char ch : work) {
		const unsigned char c = static_cast<unsigned char>(ch);
		if (std::isalnum(c) != 0) {
			out.push_back(static_cast<char>(std::tolower(c)));
			last_is_underscore = false;
		}
		else if (!last_is_underscore) {
			out.push_back('_');
			last_is_underscore = true;
		}
	}

	while (!out.empty() && out.front() == '_') {
		out.erase(out.begin());
	}
	while (!out.empty() && out.back() == '_') {
		out.pop_back();
	}
	if (out.empty()) {
		out = "symbol";
	}
	return out;
}

std::string mytool::formatNumber(double value) {
	if (std::fabs(value) < 1e-12) {
		value = 0.0;
	}

	std::ostringstream oss;
	oss << std::fixed << std::setprecision(6) << value;
	std::string text = oss.str();
	while (!text.empty() && text.back() == '0') {
		text.pop_back();
	}
	if (!text.empty() && text.back() == '.') {
		text.pop_back();
	}
	if (text.empty() || text == "-0") {
		text = "0";
	}
	return text;
}

