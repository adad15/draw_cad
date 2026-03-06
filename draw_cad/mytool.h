#pragma once
#include <string>
#include <string_view>

namespace mytool
{
	// 判断字符串是否以某个前缀开头  "ABC_DEF" <-> "ABC"
	bool startsWith(std::string_view value, std::string_view prefix);
	std::string toUpperAscii(std::string value);
	// 把 CAD 的块名标准化为稳定、可用的“机器 ID”，
	std::string toIdFromBlockName(const std::string& block_name, std::string_view prefix);
	std::string formatNumber(double value);
}

