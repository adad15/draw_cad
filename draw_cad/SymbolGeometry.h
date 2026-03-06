#pragma once
#include "dxf_to_json.h"

// 专门处理“符号几何数据”的通用计算
class SymbolGeometry {
public:
	// 把圆弧按一定分段采样成一串点:：圆心坐标 center、半径 radius、起始角 start_rad、结束角 end_rad（弧度制）
	static std::vector<Point2D> discretizeArc(const Point2D& center, double radius, double start_rad, double end_rad);
	// 把圆离散成36个点的闭合折线
	static std::vector<Point2D> discretizeCircle(const Point2D& center, double radius);
	// 把 DXF 里的“隐式弧段 polyline”转换成普通点列 polyline。
	// DXF: bulge = tan(theta/4)，theta 为从 p0->p1 的带符号圆心角（弧度）。
	// bulge>0 表示逆时针(CCW)，bulge<0 表示顺时针(CW)。
	static void appendBulgedSegment(std::vector<Point2D>& out, const Point2D& p0, const Point2D& p1, double bulge);
	// 判断两个点是否近似相等，常用于浮点误差场景
	static bool nearlyEqual(const Point2D& a, const Point2D& b, double eps = 1e-9);
	// 根据基准点进行坐标归一化
	static void normalizeByAnchor(TuXing& tuxing);
	// 计算包围盒,所有图元点的最小外接矩形
	static BBox computeBBox(const TuXing& tuxing);
};

