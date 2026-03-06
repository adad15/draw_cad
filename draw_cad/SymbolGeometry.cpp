#include "SymbolGeometry.h"
#include "DxfSymbolCollector.h"

namespace {
	constexpr double kEps = 1e-9;
	constexpr double kPi = 3.14159265358979323846;
}

std::vector<Point2D> SymbolGeometry::discretizeArc(const Point2D& center, double radius, double start_rad, double end_rad) {
	std::vector<Point2D> points;
	if (radius <= kEps) { return points; }

	double sweep = end_rad - start_rad;
	while (sweep <= 0.0) {
		sweep += 2.0 * kPi;
	}
	if (sweep > 2.0 * kPi) {
		sweep = std::fmod(sweep, 2.0 * kPi);
	}
	if (sweep <= 1e-12) {
		sweep = 2.0 * kPi;
	}
	// 至少 8 段,默认为每10度一段
	const int segment = std::max(8, static_cast<int>(std::ceil(sweep / (kPi / 18.0))));
	points.reserve(static_cast<size_t>(segment + 1));
	for (int i = 0; i <= segment; i++) {
		// 归一化参数
		const double t = static_cast<double>(i) / static_cast<double>(segment);
		const double angle = start_rad + t * sweep;
		points.push_back(Point2D{ center.x + radius * std::cos(angle),center.y + radius * std::sin(angle) });
	}
	return points;
}

std::vector<Point2D> SymbolGeometry::discretizeCircle(const Point2D& center, double radius) {
	std::vector<Point2D> points;
	if (radius <= kEps) { return points; }

	constexpr int kSegments = 36; // 每10度一个点
	points.reserve(static_cast<size_t>(kSegments));
	for (int i = 0; i < kSegments; i++) {
		const double angle = (2.0 * kPi) * static_cast<double>(i) / static_cast<double>(kSegments);
		points.push_back(Point2D{ center.x + radius * std::cos(angle),center.y + radius * std::sin(angle) });
	}

	return points;
}
// DXF bulge 定义：bulge = tan(theta / 4)
// 其中 theta 为从 p0 到 p1 的带符号圆心角（弧度）：
//   theta > 0 => 逆时针(CCW)
//   theta < 0 => 顺时针(CW)
void SymbolGeometry::appendBulgedSegment(std::vector<Point2D>& out, const Point2D& p0, const Point2D& p1, double bulge) {
	if (out.empty()) { out.push_back(p0); }
	// 计算弦长
	const double dx = p1.x - p0.x;
	const double dy = p1.y - p0.y;
	const double chord = std::sqrt(dx * dx + dy * dy);
	if (chord <= kEps) { return; }

	if (std::fabs(bulge) <= 1e-12) {
		out.push_back(p1);
		return;
	}

	// 带符号圆心角（弧度）
	const double theta = 4.0 * std::atan(bulge);
	if (std::fabs(theta) <= 1e-12) {
		out.push_back(p1);
		return;
	}

	// 弦方向单位向量 u，左法线 n = (-uy, ux)
	const double ux = dx / chord;
	const double uy = dy / chord;
	const double nx = -uy;
	const double ny = ux;

	const Point2D mid{ (p0.x + p1.x) * 0.5,(p0.y + p1.y) * 0.5 };

	// 圆心到弦中点的有符号偏移距离 h（沿左法线）
	// 由 chord = 2*R*sin(theta/2) 推导可得：
	// h = chord / (2*tan(theta/2))
	const double h = chord / (2.0 * std::tan(theta * 0.5));

	// 圆心位置
	const Point2D center{
		mid.x + nx * h,
		mid.y + ny * h
	};

	// 半径
	const double radius = std::sqrt((p0.x - center.x) * (p0.x - center.x) + (p0.y - center.y) * (p0.y - center.y));

	// 起始角：center -> p0 的极角
	const double start = std::atan2(p0.y - center.y, p0.x - center.x);

	// 角度扫掠量与 theta 同号
	const double sweep = theta;

	// 至少 4 段，默认每 10° 一段
	const int segments = std::max(4, static_cast<int>(std::ceil(std::fabs(theta) / (kPi / 18.0))));
	
	for (int i = 1; i <= segments; i++) {
		if (i == segments) {
			out.push_back(p1); // 强制精确终点
		}
		else {
			const double t = static_cast<double>(i) / static_cast<double>(segments);
			const double angle = start + t * sweep;
			out.push_back(Point2D{ center.x + radius * std::cos(angle),center.y + radius * std::sin(angle) });
		}
	}
}

bool SymbolGeometry::nearlyEqual(const Point2D& a, const Point2D& b, double eps) {
	return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps;
}

void SymbolGeometry::normalizeByAnchor(TuXing& tuxing) {
	for (TuYuan& tuyuan : tuxing.tuyuan_vector) {
		if (auto* line = std::get_if<LineTuYuan>(&tuyuan.geometry)) {
			line->a.x -= tuxing.anchor_point.x;
			line->a.y -= tuxing.anchor_point.y;
			line->b.x -= tuxing.anchor_point.x;
			line->b.y -= tuxing.anchor_point.y;
			continue;
		}

		auto* polyline = std::get_if<PolylineTuYuan>(&tuyuan.geometry);
		if (polyline == nullptr) continue;
		for (Point2D& point : polyline->points) {
			point.x -= tuxing.anchor_point.x;
			point.y -= tuxing.anchor_point.y;
		}
	}

	tuxing.anchor_point = Point2D{};
}

BBox SymbolGeometry::computeBBox(const TuXing& tuxing) {
	// 初始化边界为无穷大/无穷小
	double min_x = std::numeric_limits<double>::infinity();
	double min_y = std::numeric_limits<double>::infinity();
	double max_x = -std::numeric_limits<double>::infinity();
	double max_y = -std::numeric_limits<double>::infinity();
	// 表示是否至少包含过一个点，避免符号里完全没有几何时返回垃圾值。
	bool valid = false;

	auto include = [&](const Point2D& point) {
		min_x = std::min(min_x, point.x);
		min_y = std::min(min_y, point.y);
		max_x = std::max(max_x, point.x);
		max_y = std::max(max_y, point.y);
		valid = true;
	};

	for (const TuYuan& tuyuan : tuxing.tuyuan_vector) {
		if (const auto* line = std::get_if<LineTuYuan>(&tuyuan.geometry)) {
			include(line->a);
			include(line->b);
			continue;
		}

		const auto* polyline = std::get_if<PolylineTuYuan>(&tuyuan.geometry);
		if (polyline == nullptr) continue;

		for (const Point2D& point : polyline->points) {
			include(point);
		}
	}

	if (!valid) return BBox{};

	return BBox{ min_x, min_y, max_x, max_y };
}
