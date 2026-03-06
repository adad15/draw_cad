#pragma once
#include <string>

// 用来根据块名推断符号的“类型（kind）”和“缩放模式（scale_mode）”，供写 JSON 时用。
class SymbolClassifier final {
public:
	// 根据块名（内部会转成大写再匹配）推断“种类”字符串,这些字符串会写到 JSON 的 "kind" 里。
	static std::string BlockName_to_Kind(const std::string& block_name);
	// 根据上面得到的 kind 推断缩放方式：
	static std::string inferScaleMode(const std::string& kind);
};

