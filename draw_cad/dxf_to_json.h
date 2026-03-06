#pragma once
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include "mytool.h"


struct MyConfig
{
	std::string input_dxf = R"(D:\vs2022 code\draw_cad\Drawing1.dxf)";
	std::string output_json = R"(D:\vs2022 code\draw_cad\symbols.json)";
	std::string unit = "m";
	std::string block_prefix = "SYM_";
};

struct ConvertResult {
	int code{};
	std::string message;

	bool ok() const noexcept {
		return code == 0;
	}

	static ConvertResult success() {
		return ConvertResult{ 0, "success" };
	}

	static ConvertResult failure(int error_code, std::string error_message) {
		return ConvertResult{ error_code, std::move(error_message) };
	}
};



// 用来表示 DXF 中的顶点、控制点等几何坐标。
struct Point2D {
	double x = 0.0;
	double y = 0.0;
};
// 记录某个实体（比如线、多段线、图层等）的范围，方便做缩放、平移、视图适配等操作。
struct BBox {
	double min_x = 0.0;
	double min_y = 0.0;
	double max_x = 0.0;
	double max_y = 0.0;
};
// 图元样式（线属性）
struct TuYuanStyle {
	std::string layer = "0";
	std::string linetype = "BYLAYER";
	double lineweight_mm = 0.25;
};
// 选中多条图元后，检查它们的样式是否一致
struct StyleUnify {
	TuYuanStyle tuyuanstyle{};
	bool initialized = false;
	bool mixed_layer = false;
	bool mixed_linetype = false;
	bool mixed_lineweight = false;

	bool hasMixed() const noexcept {
		return mixed_layer || mixed_linetype || mixed_lineweight;
	}
};
// 线段图元，包含起点和终点坐标
struct LineTuYuan {
	Point2D a{};
	Point2D b{};
};
// 多段线图元
struct PolylineTuYuan {
	std::vector<Point2D> points;
	bool closed = false; //表示这条多段线是否封闭
};

using TuYuanGeometry = std::variant<LineTuYuan, PolylineTuYuan>;

// 定义了一个完整的图元结构体，把几何形状和样式打包在一起，并提供了两个工厂函数方便创建：
struct TuYuan
{
	TuYuanGeometry geometry{};
	TuYuanStyle style{};

	static TuYuan makeLine(const Point2D& a, const Point2D& b, const TuYuanStyle& style);
	static TuYuan makePolyline(std::vector<Point2D> points, bool closed, const TuYuanStyle& style);
};
// 定义了符号草稿结构体，用来表示 DXF 里的一个块（Block）在导出/转换时的中间形态：
struct TuXing {
	std::string block_name;
	Point2D anchor_point{};  // 基准点
	std::vector<TuYuan> tuyuan_vector;  // 完整图形
	StyleUnify style_unify{};
};


class dxf_to_json final {
public:
	explicit dxf_to_json(MyConfig config);

	const std::vector<TuXing>& symbols() const noexcept { return m_symbols; }

	ConvertResult runExtractor();

private:
	MyConfig m_config;
	std::vector<TuXing> m_symbols;  // 一个dxf中的所有图形
};

