#pragma once
#include "../libxfrw/libdxfrw.h"
#include "dxf_to_json.h"

class DxfSymbolCollector final :public DRW_Interface {
public:
	explicit DxfSymbolCollector(std::string block_prefix);

	const std::vector<TuXing>& symbols() const noexcept { return m_symbols; }

	std::vector<TuXing> takeSymbols();

	void addHeader(const DRW_Header* /*data*/) override {}
	void addLType(const DRW_LType& /*data*/) override {}
	void addLayer(const DRW_Layer& /*data*/) override {}
	void addDimStyle(const DRW_Dimstyle& /*data*/) override {}
	void addVport(const DRW_Vport& /*data*/) override {}
	void addTextStyle(const DRW_Textstyle& /*data*/) override {}
	void addAppId(const DRW_AppId& /*data*/) override {}
	// 用来接收一个图形块,是自己关心的块就把它转换成内部的TuXing并加入列表里，同时更新当前正在处理的块索引。
	void addBlock(const DRW_Block& data) override;
	void setBlock(int /*handle*/) override {}
	void endBlock() override;

	void addPoint(const DRW_Point& /*data*/) override {}
	// 把 DXF 的 DRW_Line 转成内部统一的 TuYuan（线段图元），并追加到当前正在收集的 SymbolDraft 里
	void addLine(const DRW_Line& data) override;
	void addRay(const DRW_Ray& /*data*/) override {}
	void addXline(const DRW_Xline& /*data*/) override {}
	// 当 libdxfrw 读到一个 ARC 实体时，把这段圆弧转换成你项目内部的折线图元，并加入当前符号。
	void addArc(const DRW_Arc& data) override;
	void addCircle(const DRW_Circle& data) override;
	void addEllipse(const DRW_Ellipse& /*data*/) override {}
	void addLWPolyline(const DRW_LWPolyline& data) override;
	void addPolyline(const DRW_Polyline& /*data*/) override {}
	void addSpline(const DRW_Spline* /*data*/) override {}
	void addKnot(const DRW_Entity& /*data*/) override {}
	void addInsert(const DRW_Insert& /*data*/) override {}
	void addTrace(const DRW_Trace& /*data*/) override {}
	void add3dFace(const DRW_3Dface& /*data*/) override {}
	void addSolid(const DRW_Solid& /*data*/) override {}
	void addMText(const DRW_MText& /*data*/) override {}
	void addText(const DRW_Text& /*data*/) override {}
	void addDimAlign(const DRW_DimAligned* /*data*/) override {}
	void addDimLinear(const DRW_DimLinear* /*data*/) override {}
	void addDimRadial(const DRW_DimRadial* /*data*/) override {}
	void addDimDiametric(const DRW_DimDiametric* /*data*/) override {}
	void addDimAngular(const DRW_DimAngular* /*data*/) override {}
	void addDimAngular3P(const DRW_DimAngular3p* /*data*/) override {}
	void addDimOrdinate(const DRW_DimOrdinate* /*data*/) override {}
	void addLeader(const DRW_Leader* /*data*/) override {}
	void addHatch(const DRW_Hatch* /*data*/) override {}
	void addViewport(const DRW_Viewport& /*data*/) override {}
	void addImage(const DRW_Image* /*data*/) override {}
	void linkImage(const DRW_ImageDef* /*data*/) override {}
	void addComment(const char* /*comment*/) override {}

	void writeHeader(DRW_Header& /*data*/) override {}
	void writeBlocks() override {}
	void writeBlockRecords() override {}
	void writeEntities() override {}
	void writeLTypes() override {}
	void writeLayers() override {}
	void writeTextstyles() override {}
	void writeVports() override {}
	void writeDimstyles() override {}
	void writeAppId() override {}

private:
	bool hasCurrent() const noexcept { return current_index.has_value(); }
	// 读取索引，根据索引找到图元对应的图形
	TuXing& current();
	// 把curremt_index恢复到初始值
	void resetCurrent() noexcept;
	// 通过这个函数将正确的图元添加到图形中
	void addTuYuan(TuYuan tuyuan);
	// 提取图元样式
	TuYuanStyle GetTuYuanStyle(const DRW_Entity& entity) const;

private:
	std::string m_prefix;
	std::vector<TuXing> m_symbols;  //图形块列表，记录了所有符合条件的块
	std::optional<size_t> current_index;  // 索引，方便标记图元属于哪些块
};