//#include "SymbolGeometry.h"
//#include <cassert>
//#include <cmath>
//#include <iostream>
//#include <string>
//
//namespace {
//
//	constexpr double kPi = 3.14159265358979323846;
//	constexpr double kTestEps = 1e-6;
//
//	bool approxEqual(double a, double b, double eps = kTestEps) {
//		return std::fabs(a - b) < eps;
//	}
//
//	bool isOnCircle(const Point2D& p, const Point2D& center, double radius) {
//		const double dx = p.x - center.x;
//		const double dy = p.y - center.y;
//		return approxEqual(std::sqrt(dx * dx + dy * dy), radius);
//	}
//
//	int passed = 0;
//	int failed = 0;
//
//	void check(const std::string& name, bool ok) {
//		if (ok) {
//			std::cout << "  [PASS] " << name << "\n";
//			++passed;
//		}
//		else {
//			std::cout << "  [FAIL] " << name << "\n";
//			++failed;
//		}
//	}
//
//	// ==================== discretizeArc ====================
//
//	void test_discretizeArc_zeroRadius() {
//		auto pts = SymbolGeometry::discretizeArc({ 0, 0 }, 0.0, 0.0, kPi);
//		check("半径为0返回空", pts.empty());
//	}
//
//	void test_discretizeArc_negativeRadius() {
//		auto pts = SymbolGeometry::discretizeArc({ 0, 0 }, -5.0, 0.0, kPi);
//		check("负半径返回空", pts.empty());
//	}
//
//	void test_discretizeArc_quarterEndpoints() {
//		// 单位圆上 0 → π/2 弧段，起点应在 (1,0)，终点应在 (0,1)
//		auto pts = SymbolGeometry::discretizeArc({ 0, 0 }, 1.0, 0.0, kPi / 2.0);
//		bool ok = pts.size() >= 9; // 至少 8 段 → 9 个点
//		ok = ok && approxEqual(pts.front().x, 1.0) && approxEqual(pts.front().y, 0.0);
//		ok = ok && approxEqual(pts.back().x, 0.0) && approxEqual(pts.back().y, 1.0);
//		check("90度弧起点终点正确", ok);
//	}
//
//	void test_discretizeArc_allPointsOnCircle() {
//		const Point2D c{ 2.0, 3.0 };
//		const double r = 5.0;
//		auto pts = SymbolGeometry::discretizeArc(c, r, 0.0, kPi);
//		bool ok = !pts.empty();
//		for (const auto& p : pts) {
//			if (!isOnCircle(p, c, r)) { ok = false; break; }
//		}
//		check("180度弧所有点在圆上", ok);
//	}
//
//	void test_discretizeArc_fullCircleCloses() {
//		auto pts = SymbolGeometry::discretizeArc({ 0, 0 }, 3.0, 0.0, 2.0 * kPi);
//		bool ok = pts.size() >= 9;
//		if (ok) {
//			ok = approxEqual(pts.front().x, pts.back().x) && approxEqual(pts.front().y, pts.back().y);
//		}
//		check("完整圆弧首尾重合", ok);
//	}
//
//	void test_discretizeArc_crossZero() {
//		// 350° → 10°，跨越 0 度
//		const double start = 350.0 * kPi / 180.0;
//		const double end = 10.0 * kPi / 180.0;
//		auto pts = SymbolGeometry::discretizeArc({ 0, 0 }, 1.0, start, end);
//		bool ok = !pts.empty();
//		for (const auto& p : pts) {
//			if (!isOnCircle(p, { 0, 0 }, 1.0)) { ok = false; break; }
//		}
//		check("跨越0度弧段所有点在圆上", ok);
//	}
//
//	void test_discretizeArc_withOffset() {
//		// 圆心不在原点
//		const Point2D c{ -10.0, 20.0 };
//		auto pts = SymbolGeometry::discretizeArc(c, 7.0, kPi / 4.0, kPi);
//		bool ok = !pts.empty();
//		for (const auto& p : pts) {
//			if (!isOnCircle(p, c, 7.0)) { ok = false; break; }
//		}
//		check("圆心偏移后所有点仍在圆上", ok);
//	}
//
//	// ==================== discretizeCircle ====================
//
//	void test_discretizeCircle_count36() {
//		auto pts = SymbolGeometry::discretizeCircle({ 0, 0 }, 5.0);
//		check("返回36个点", pts.size() == 36);
//	}
//
//	void test_discretizeCircle_zeroRadius() {
//		auto pts = SymbolGeometry::discretizeCircle({ 0, 0 }, 0.0);
//		check("半径为0返回空", pts.empty());
//	}
//
//	void test_discretizeCircle_allOnCircle() {
//		const Point2D c{ 1.0, -1.0 };
//		const double r = 4.0;
//		auto pts = SymbolGeometry::discretizeCircle(c, r);
//		bool ok = !pts.empty();
//		for (const auto& p : pts) {
//			if (!isOnCircle(p, c, r)) { ok = false; break; }
//		}
//		check("所有点在圆上", ok);
//	}
//
//	void test_discretizeCircle_firstPointAtZeroAngle() {
//		// 第一个点应在 0 度方向，即 (center.x + r, center.y)
//		const Point2D c{ 3.0, 4.0 };
//		const double r = 2.0;
//		auto pts = SymbolGeometry::discretizeCircle(c, r);
//		bool ok = !pts.empty();
//		ok = ok && approxEqual(pts.front().x, c.x + r) && approxEqual(pts.front().y, c.y);
//		check("第一个点在0度方向", ok);
//	}
//
//	// ==================== appendBulgedSegment ====================
//
//	void test_appendBulged_zeroBulge() {
//		// bulge=0 退化为直线段
//		std::vector<Point2D> out;
//		SymbolGeometry::appendBulgedSegment(out, { 0, 0 }, { 10, 0 }, 0.0);
//		bool ok = (out.size() == 2);
//		ok = ok && approxEqual(out[0].x, 0.0) && approxEqual(out[0].y, 0.0);
//		ok = ok && approxEqual(out[1].x, 10.0) && approxEqual(out[1].y, 0.0);
//		check("bulge=0退化直线段", ok);
//	}
//
//	void test_appendBulged_semicircleEndpoint() {
//		// bulge=1 → 半圆（θ=π），终点应精确到达 p1
//		std::vector<Point2D> out;
//		Point2D p0{ 0, 0 }, p1{ 2, 0 };
//		SymbolGeometry::appendBulgedSegment(out, p0, p1, 1.0);
//		bool ok = out.size() >= 3;
//		if (ok) {
//			ok = approxEqual(out.back().x, p1.x) && approxEqual(out.back().y, p1.y);
//		}
//		check("bulge=1半圆终点正确", ok);
//	}
//
//	void test_appendBulged_semicircleMaxY() {
//		// 按 DXF bulge 定义：bulge=1 -> theta=+pi（CCW 半圆）
//		// 对 p0=(0,0), p1=(2,0) 该半圆走下方，中间点 y 应 < 0
//		std::vector<Point2D> out;
//		SymbolGeometry::appendBulgedSegment(out, { 0, 0 }, { 2, 0 }, 1.0);
//		bool ok = out.size() >= 3;
//		if (ok) {
//			size_t mid = out.size() / 2;
//			ok = out[mid].y < 0.0;
//		}
//		check("bulge=1逆时针半圆中间点y<0", ok);
//	}
//
//	void test_appendBulged_negativeBulgeFlipsDirection() {
//		// 正负 bulge 中间点 y 方向应相反
//		std::vector<Point2D> out_pos, out_neg;
//		SymbolGeometry::appendBulgedSegment(out_pos, { 0, 0 }, { 2, 0 }, 1.0);
//		SymbolGeometry::appendBulgedSegment(out_neg, { 0, 0 }, { 2, 0 }, -1.0);
//		bool ok = (out_pos.size() == out_neg.size()) && out_pos.size() >= 3;
//		if (ok) {
//			size_t mid = out_pos.size() / 2;
//			ok = (out_pos[mid].y < 0.0) && (out_neg[mid].y > 0.0);
//		}
//		check("正负bulge方向相反", ok);
//	}
//
//	void test_appendBulged_samePointsNoCrash() {
//		// 两点重合不崩溃
//		std::vector<Point2D> out;
//		SymbolGeometry::appendBulgedSegment(out, { 5, 5 }, { 5, 5 }, 0.5);
//		check("两点重合不崩溃", out.size() <= 1);
//	}
//
//	void test_appendBulged_noExtraPushWhenNotEmpty() {
//		// 容器非空时不应重复添加 p0
//		std::vector<Point2D> out;
//		out.push_back({ 0, 0 });
//		SymbolGeometry::appendBulgedSegment(out, { 0, 0 }, { 1, 0 }, 0.0);
//		check("非空容器不重复添加p0", out.size() == 2);
//	}
//
//	// ==================== nearlyEqual ====================
//
//	void test_nearlyEqual_identical() {
//		check("相同点判定相等", SymbolGeometry::nearlyEqual({ 1, 2 }, { 1, 2 }));
//	}
//
//	void test_nearlyEqual_withinEps() {
//		check("误差1e-10以内判定相等",
//			SymbolGeometry::nearlyEqual({ 1.0, 2.0 }, { 1.0 + 1e-10, 2.0 - 1e-10 }));
//	}
//
//	void test_nearlyEqual_outsideEps() {
//		check("超出默认容差判定不等",
//			!SymbolGeometry::nearlyEqual({ 0, 0 }, { 0, 1e-5 }));
//	}
//
//	void test_nearlyEqual_customEps() {
//		check("自定义容差0.1",
//			SymbolGeometry::nearlyEqual({ 0, 0 }, { 0.05, 0.0 }, 0.1));
//	}
//
//	void test_nearlyEqual_onlyXDiffers() {
//		check("仅x超差判定不等",
//			!SymbolGeometry::nearlyEqual({ 100.0, 0.0 }, { 100.001, 0.0 }));
//	}
//
//	// ==================== normalizeByAnchor ====================
//
//	void test_normalizeByAnchor_line() {
//		TuXing tuxing;
//		tuxing.anchor_point = { 10.0, 20.0 };
//		tuxing.tuyuan_vector.push_back(
//			TuYuan::makeLine({ 15.0, 25.0 }, { 20.0, 30.0 }, {}));
//		SymbolGeometry::normalizeByAnchor(tuxing);
//
//		const auto& line = std::get<LineTuYuan>(tuxing.tuyuan_vector[0].geometry);
//		bool ok = approxEqual(line.a.x, 5.0) && approxEqual(line.a.y, 5.0);
//		ok = ok && approxEqual(line.b.x, 10.0) && approxEqual(line.b.y, 10.0);
//		ok = ok && approxEqual(tuxing.anchor_point.x, 0.0) && approxEqual(tuxing.anchor_point.y, 0.0);
//		check("线段坐标归一化正确", ok);
//	}
//
//	void test_normalizeByAnchor_polyline() {
//		TuXing tuxing;
//		tuxing.anchor_point = { 5.0, 5.0 };
//		tuxing.tuyuan_vector.push_back(
//			TuYuan::makePolyline({ {10, 15}, {20, 25}, {30, 35} }, false, {}));
//		SymbolGeometry::normalizeByAnchor(tuxing);
//
//		const auto& poly = std::get<PolylineTuYuan>(tuxing.tuyuan_vector[0].geometry);
//		bool ok = (poly.points.size() == 3);
//		ok = ok && approxEqual(poly.points[0].x, 5.0) && approxEqual(poly.points[0].y, 10.0);
//		ok = ok && approxEqual(poly.points[1].x, 15.0) && approxEqual(poly.points[1].y, 20.0);
//		ok = ok && approxEqual(poly.points[2].x, 25.0) && approxEqual(poly.points[2].y, 30.0);
//		check("多段线坐标归一化正确", ok);
//	}
//
//	void test_normalizeByAnchor_zeroAnchor() {
//		// 基准点为原点时坐标不变
//		TuXing tuxing;
//		tuxing.anchor_point = { 0.0, 0.0 };
//		tuxing.tuyuan_vector.push_back(
//			TuYuan::makeLine({ 3.0, 4.0 }, { 5.0, 6.0 }, {}));
//		SymbolGeometry::normalizeByAnchor(tuxing);
//
//		const auto& line = std::get<LineTuYuan>(tuxing.tuyuan_vector[0].geometry);
//		bool ok = approxEqual(line.a.x, 3.0) && approxEqual(line.a.y, 4.0);
//		check("基准点为原点时坐标不变", ok);
//	}
//
//	// ==================== computeBBox ====================
//
//	void test_computeBBox_singleLine() {
//		TuXing tuxing;
//		tuxing.tuyuan_vector.push_back(
//			TuYuan::makeLine({ 1.0, 2.0 }, { 5.0, 8.0 }, {}));
//		BBox box = SymbolGeometry::computeBBox(tuxing);
//		bool ok = approxEqual(box.min_x, 1.0) && approxEqual(box.min_y, 2.0);
//		ok = ok && approxEqual(box.max_x, 5.0) && approxEqual(box.max_y, 8.0);
//		check("单条线段包围盒正确", ok);
//	}
//
//	void test_computeBBox_multipleElements() {
//		TuXing tuxing;
//		tuxing.tuyuan_vector.push_back(
//			TuYuan::makeLine({ -3.0, -4.0 }, { 1.0, 2.0 }, {}));
//		tuxing.tuyuan_vector.push_back(
//			TuYuan::makePolyline({ {0, 0}, {10, 10}, {-1, 5} }, false, {}));
//		BBox box = SymbolGeometry::computeBBox(tuxing);
//		bool ok = approxEqual(box.min_x, -3.0) && approxEqual(box.min_y, -4.0);
//		ok = ok && approxEqual(box.max_x, 10.0) && approxEqual(box.max_y, 10.0);
//		check("多图元合并包围盒正确", ok);
//	}
//
//	void test_computeBBox_empty() {
//		TuXing tuxing;
//		BBox box = SymbolGeometry::computeBBox(tuxing);
//		bool ok = approxEqual(box.min_x, 0.0) && approxEqual(box.min_y, 0.0);
//		ok = ok && approxEqual(box.max_x, 0.0) && approxEqual(box.max_y, 0.0);
//		check("空图形返回默认BBox", ok);
//	}
//
//	void test_computeBBox_negativeCoords() {
//		TuXing tuxing;
//		tuxing.tuyuan_vector.push_back(
//			TuYuan::makeLine({ -100.0, -200.0 }, { -50.0, -80.0 }, {}));
//		BBox box = SymbolGeometry::computeBBox(tuxing);
//		bool ok = approxEqual(box.min_x, -100.0) && approxEqual(box.min_y, -200.0);
//		ok = ok && approxEqual(box.max_x, -50.0) && approxEqual(box.max_y, -80.0);
//		check("全负坐标包围盒正确", ok);
//	}
//
//} // anonymous namespace
//
//// 测试入口
//int main() {
//	std::cout << "============ SymbolGeometry 单元测试 ============\n\n";
//
//	std::cout << "[discretizeArc]\n";
//	test_discretizeArc_zeroRadius();
//	test_discretizeArc_negativeRadius();
//	test_discretizeArc_quarterEndpoints();
//	test_discretizeArc_allPointsOnCircle();
//	test_discretizeArc_fullCircleCloses();
//	test_discretizeArc_crossZero();
//	test_discretizeArc_withOffset();
//
//	std::cout << "\n[discretizeCircle]\n";
//	test_discretizeCircle_count36();
//	test_discretizeCircle_zeroRadius();
//	test_discretizeCircle_allOnCircle();
//	test_discretizeCircle_firstPointAtZeroAngle();
//
//	std::cout << "\n[appendBulgedSegment]\n";
//	test_appendBulged_zeroBulge();
//	test_appendBulged_semicircleEndpoint();
//	test_appendBulged_semicircleMaxY();
//	test_appendBulged_negativeBulgeFlipsDirection();
//	test_appendBulged_samePointsNoCrash();
//	test_appendBulged_noExtraPushWhenNotEmpty();
//
//	std::cout << "\n[nearlyEqual]\n";
//	test_nearlyEqual_identical();
//	test_nearlyEqual_withinEps();
//	test_nearlyEqual_outsideEps();
//	test_nearlyEqual_customEps();
//	test_nearlyEqual_onlyXDiffers();
//
//	std::cout << "\n[normalizeByAnchor]\n";
//	test_normalizeByAnchor_line();
//	test_normalizeByAnchor_polyline();
//	test_normalizeByAnchor_zeroAnchor();
//
//	std::cout << "\n[computeBBox]\n";
//	test_computeBBox_singleLine();
//	test_computeBBox_multipleElements();
//	test_computeBBox_empty();
//	test_computeBBox_negativeCoords();
//
//	std::cout << "\n============ 结果: " << passed << " 通过, " << failed << " 失败 ============\n";
//	return (failed == 0) ? 0 : 1;
//}
