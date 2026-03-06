#include "DxfSymbolCollector.h"
#include "SymbolGeometry.h"

namespace {
	constexpr double kStyleEps = 1e-9;

	double lineweightToMm(int lineweight) {
		if (lineweight <= 0) {
			return 0.25;
		}
		return static_cast<double>(lineweight) / 100.0;
	}
}

DxfSymbolCollector::DxfSymbolCollector(std::string block_prefix) :m_prefix(std::move(block_prefix)) {}

std::vector<TuXing> DxfSymbolCollector::takeSymbols()
{
	resetCurrent();
	return std::move(m_symbols);
}

void DxfSymbolCollector::addBlock(const DRW_Block& data) {
	if (!mytool::startsWith(data.name, m_prefix)) {
		resetCurrent();
		return;
	}

	TuXing symbol;
	symbol.block_name = data.name;
	symbol.anchor_point = Point2D{ data.basePoint.x, data.basePoint.y };
	m_symbols.push_back(std::move(symbol));
	// 从现在开始，后续图元都归这个新块。
	current_index = m_symbols.size() - 1;
}

void DxfSymbolCollector::endBlock() {
	resetCurrent();
}

void DxfSymbolCollector::addLine(const DRW_Line& data) {
	if (!hasCurrent()) return;

	const TuYuanStyle style = GetTuYuanStyle(data);
	addTuYuan(TuYuan::makeLine(Point2D{ data.basePoint.x,data.basePoint.y },
		Point2D{ data.secPoint.x,data.secPoint.y },
		style));
}

void DxfSymbolCollector::addArc(const DRW_Arc& data) {
	if (!hasCurrent()) return;

	const std::vector<Point2D> points = SymbolGeometry::discretizeArc(
		Point2D{ data.basePoint.x,data.basePoint.y }, data.radious, data.staangle, data.endangle);
	if (points.size() < 2) return;

	const TuYuanStyle style = GetTuYuanStyle(data);
	addTuYuan(TuYuan::makePolyline(points, false, style));
}

void DxfSymbolCollector::addCircle(const DRW_Circle& data) {
	if (!hasCurrent()) return;
	
	const std::vector<Point2D> points = SymbolGeometry::discretizeCircle(
		Point2D{ data.basePoint.x,data.basePoint.y }, data.radious);
	if (points.size() < 3) {
		return;
	}

	const TuYuanStyle style = GetTuYuanStyle(data);
	addTuYuan(TuYuan::makePolyline(points, true, style));
}

void DxfSymbolCollector::addLWPolyline(const DRW_LWPolyline& data) {
	if (!hasCurrent() || data.vertlist.size() < 2) return;

	std::vector<Point2D> points;
	const size_t count = data.vertlist.size();
	for (size_t i = 0; i + 1 < count; ++i) {
		const DRW_Vertex2D* v0 = data.vertlist[i];
		const DRW_Vertex2D* v1 = data.vertlist[i + 1];
		if (v0 == nullptr || v1 == nullptr) {
			continue;
		}
		SymbolGeometry::appendBulgedSegment(points, Point2D{ v0->x, v0->y }, Point2D{ v1->x, v1->y }, v0->bulge);
	}
	// 判断多段线是否闭合，并处理首尾连接段
	const bool closed = (data.flags & 0x01) != 0;
	if (closed) {
		const DRW_Vertex2D* last = data.vertlist[count - 1];
		const DRW_Vertex2D* first = data.vertlist[0];
		if (last != nullptr && first != nullptr) {
			SymbolGeometry::appendBulgedSegment(points, Point2D{ last->x, last->y }, Point2D{ first->x, first->y }, last->bulge);
		}
		// 首点重复加了一遍，去掉最后加的首点
		if (points.size() >= 2 && SymbolGeometry::nearlyEqual(points.front(), points.back())) {
			points.pop_back();
		}

		if (points.size() < 2) return;

		const TuYuanStyle style = GetTuYuanStyle(data);
		addTuYuan(TuYuan::makePolyline(std::move(points), closed, style));
	}
}

TuXing& DxfSymbolCollector::current()
{
	return m_symbols[current_index.value()];
}

void DxfSymbolCollector::resetCurrent() noexcept {
	current_index.reset();
}

void DxfSymbolCollector::addTuYuan(TuYuan tuyuan) {
	// 更新样式汇总，如果当前图元的样式和之前的图元不一致，就把对应的 mixed_* 标记为 true
	StyleUnify& summary = current().style_unify;
	if (!summary.initialized) {
		summary.tuyuanstyle = tuyuan.style;
		summary.initialized = true;
	}

	if (summary.tuyuanstyle.layer != tuyuan.style.layer) { summary.mixed_layer = true; }
	if (summary.tuyuanstyle.linetype != tuyuan.style.linetype) { summary.mixed_linetype = true; }
	if (std::fabs(summary.tuyuanstyle.lineweight_mm - tuyuan.style.lineweight_mm) > kStyleEps) { summary.mixed_lineweight = true; }
	// 把当前图元塞进对应的图形中
	current().tuyuan_vector.push_back(std::move(tuyuan));
}

TuYuanStyle DxfSymbolCollector::GetTuYuanStyle(const DRW_Entity& entity) const {
	TuYuanStyle style;
	if (!style.layer.empty()) {
		style.layer = entity.layer;
	}
	if (!style.linetype.empty()) {
		style.linetype = entity.lineType;
	}
	style.lineweight_mm = lineweightToMm(static_cast<int>(entity.lWeight));
	return style;
}