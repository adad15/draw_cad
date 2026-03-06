#pragma once
#include "dxf_to_json.h"

// 把内存里的 SymbolDraft 列表，按自定义的 schema 序列化成 symbols.json 文件
class SymbolJsonWriter final {
public:
	explicit SymbolJsonWriter(MyConfig config);
	ConvertResult write(const std::vector<TuXing>& tuxing) const;

private:
	MyConfig m_config;
};
