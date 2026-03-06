#include "dxf_to_json.h"
#include "DxfSymbolCollector.h"
#include "SymbolJsonWriter.h"
#include <iostream>

TuYuan TuYuan::makeLine(const Point2D& a, const Point2D& b, const TuYuanStyle& style) {
    TuYuan primitive;
    primitive.geometry = LineTuYuan{ a,b };
	primitive.style = style;
    return primitive;
}
TuYuan TuYuan::makePolyline(std::vector<Point2D> points, bool closed, const TuYuanStyle& style) {
	TuYuan primitive;
	primitive.geometry = PolylineTuYuan{ std::move(points), closed };
	primitive.style = style;
	return primitive;
}

dxf_to_json::dxf_to_json(MyConfig config) :m_config(std::move(config)) {}

ConvertResult dxf_to_json::runExtractor() {
	DxfSymbolCollector collector(m_config.block_prefix);
	dxfRW reader(m_config.input_dxf.c_str());
	if (!reader.read(&collector, false)) {
		return ConvertResult::failure(1, "读取dxf文件失败");
	}

	m_symbols = collector.takeSymbols();
	SymbolJsonWriter writer(m_config);
	ConvertResult write_result = writer.write(m_symbols);
	if (!write_result.ok()) {
		return write_result;
	}

	std::cout << "done, symbols(raw): " << m_symbols.size() << "\n";
	return ConvertResult::success();
	
}

