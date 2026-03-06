#include "SymbolJsonWriter.h"
#include "SymbolGeometry.h"
#include "SymbolClassifier.h"
#include "mytool.h"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace {
	/*
     * 块存在，但里面只有你没处理的实体类型
	    比如块里只有 TEXT、HATCH，而你的 DxfSymbolCollector 只实现了 addLine/addArc/addCircle/addLWPolyline，那么：
		进入 addBlock 时创建了 SymbolDraft
		但整个块里从未往 primitives 里塞东西 → 最后这个 SymbolDraft.primitives 就是空的。
      块被识别为符号，但几何全部被过滤掉
		比如某些实体数据不合法、顶点列表异常、vertlist.size()<2 等，你的 addLine/addLWPolyline 直接 return，那这个块也会变成“空”。
	*/
	std::vector<TuXing> removeEmptySymbols(const std::vector<TuXing>& input) {
		std::vector<TuXing> out;
		out.reserve(input.size());
		for (const TuXing& symbol : input) {
			if (!symbol.tuyuan_vector.empty()) {
				out.push_back(symbol);
			}
		}
		return out;
	}
	// 把任意 std::string 转成一个可直接写入 JSON 的字符串字面量
	/*
	*   std::string s = "A\"B\\C\n\t\x01"; =》 "\"A\\\"B\\\\C\\n\\t\\u0001\""
	*   
	* 
	*/
	std::string jsonEscape(const std::string& value) {
		std::string out;
		out.reserve(value.size() + 8);
		out.push_back('"');
		for (unsigned char c : value) {
			switch (c) {
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b"; break;
			case '\f': out += "\\f"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				// •	如果是控制字符
				if (c < 0x20) {
					std::ostringstream oss;
					oss << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
						<< static_cast<int>(c);
					out += oss.str();
				}
				else {
					out.push_back(static_cast<char>(c));
				}
				break;
			}
		}
		out.push_back('"');
		return out;
	}

	std::string pointToJson(const Point2D& p) {
		return "[" + mytool::formatNumber(p.x) + ", " + mytool::formatNumber(p.y) + "]";
	}
	// 把一个 TuYuanStyle 样式对象，按 JSON 对象格式写到输出流里。
	void writeStyleObject(std::ostream& out, const TuYuanStyle& style) {
		out << "{ \"layer\": " << jsonEscape(style.layer)
			<< ", \"linetype\": " << jsonEscape(style.linetype)
			<< ", \"lineweight_mm\": " << mytool::formatNumber(style.lineweight_mm) << " }";
	}
}

SymbolJsonWriter::SymbolJsonWriter(MyConfig config) :m_config(std::move(config)) {}

ConvertResult SymbolJsonWriter::write(const std::vector<TuXing>& tuxing) const {
	const std::vector<TuXing> filtered = removeEmptySymbols(tuxing);
	std::ofstream out(m_config.output_json.c_str(), std::ios::binary);
	if (!out) {
		return ConvertResult::failure(2, "Failed to open output file: " + m_config.output_json);
	}

	out << "{\n";
	out << "  \"meta\": {\n";
	out << "    \"unit\": " << jsonEscape(m_config.unit) << ",\n";
	out << "    \"version\": 1\n";
	out << "  },\n";
	out << "  \"symbols\": [\n";

	for (size_t i = 0; i < filtered.size(); i++) {
		TuXing tuxing = filtered[i];
		SymbolGeometry::normalizeByAnchor(tuxing);
		const BBox bbox = SymbolGeometry::computeBBox(tuxing);

		const std::string id = mytool::toIdFromBlockName(tuxing.block_name, m_config.block_prefix);
		const std::string kind = SymbolClassifier::BlockName_to_Kind(tuxing.block_name);
		const std::string scale_mode = SymbolClassifier::inferScaleMode(kind);
		// 符号的基准长度
		double base_length_m = 1.0;
		if (scale_mode == "stretch_x") {
			base_length_m = std::max(bbox.max_x - bbox.min_x, 1e-6);
		}

		const TuYuanStyle style = tuxing.style_unify.initialized ? tuxing.style_unify.tuyuanstyle : TuYuanStyle{};
		out << "    {\n";
		out << "      \"id\": " << jsonEscape(id) << ",\n";
		out << "      \"block\": " << jsonEscape(tuxing.block_name) << ",\n";
		out << "      \"kind\": " << jsonEscape(kind) << ",\n";
		out << "      \"anchor\": [0, 0],\n";
		out << "      \"bbox\": {\n";
		out << "        \"min\": [" << mytool::formatNumber(bbox.min_x) << ", " << mytool::formatNumber(bbox.min_y) << "],\n";
		out << "        \"max\": [" << mytool::formatNumber(bbox.max_x) << ", " << mytool::formatNumber(bbox.max_y) << "]\n";
		out << "      },\n";
		out << "      \"style\": {\n";
		out << "        \"layer\": " << jsonEscape(style.layer) << ",\n";
		out << "        \"linetype\": " << jsonEscape(style.linetype) << ",\n";
		out << "        \"lineweight_mm\": " << mytool::formatNumber(style.lineweight_mm) << ",\n";
		out << "        \"mixed\": " << (tuxing.style_unify.hasMixed() ? "true" : "false") << "\n";
		out << "      },\n";
		out << "      \"params\": {\n";
		out << "        \"scale_mode\": " << jsonEscape(scale_mode) << ",\n";
		out << "        \"base_length_m\": " << mytool::formatNumber(base_length_m) << "\n";
		out << "      },\n";
		out << "      \"primitives\": [\n";

		for (size_t j = 0; j < tuxing.tuyuan_vector.size(); ++j) {
			const TuYuan& tuyuan = tuxing.tuyuan_vector[j];
			out << "        ";
			if (const auto* line = std::get_if<LineTuYuan>(&tuyuan.geometry)) {
				out << "{ \"type\": \"LINE\", \"a\": " << pointToJson(line->a)
					<< ", \"b\": " << pointToJson(line->b) << ", \"style\": ";
				writeStyleObject(out, tuyuan.style);
				out << " }";
			}
			else if (const auto* polyline = std::get_if<PolylineTuYuan>(&tuyuan.geometry)) {
				out << "{ \"type\": \"LWPOLYLINE\", \"closed\": " << (polyline->closed ? "true" : "false") << ", \"pts\": [";
				for (size_t k = 0; k < polyline->points.size(); ++k) {
					out << pointToJson(polyline->points[k]);
					if (k + 1 < polyline->points.size()) {
						out << ", ";
					}
				}
				out << "], \"style\": ";
				writeStyleObject(out, tuyuan.style);
				out << " }";
			}
			// 在循环写 primitives 时，每个图元对象后面通常要加 ,，但最后一个元素不能加
			if (j + 1 < tuxing.tuyuan_vector.size()) {
				out << ",";
			}
			out << "\n";
		}

		out << "      ]\n";
		out << "    }";
		if (i + 1 < filtered.size()) {
			out << ",";
		}
		out << "\n";
	}
	out << "  ]\n";
	out << "}\n";

	if (!out.good()) {
		return ConvertResult::failure(3, "Failed while writing output file: " + m_config.output_json);
	}

	return ConvertResult::success();
}
