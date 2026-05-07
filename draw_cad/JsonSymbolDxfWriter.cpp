#include "JsonSymbolDxfWriter.h"

#include "../libxfrw/libdxfrw.h"
#include "SymbolGeometry.h"
#include "../nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using nlohmann::json;

namespace {

    // JSON -> DXF 转换过程中使用的精简符号结构。
    struct JsonSymbolData {
        std::string id;
        std::string scale_mode = "uniform";
        std::vector<TuYuan> primitives;
    };

    // 将 [x, y] 形式的 JSON 数组解析成 Point2D。
    bool parsePointArray(const json& value, Point2D& out_point, std::string& error_message) {
        if (!value.is_array() || value.size() < 2) {
            error_message = "point must be an array with at least two numbers";
            return false;
        }
        if (!value[0].is_number() || !value[1].is_number()) {
            error_message = "point array must start with two numbers";
            return false;
        }

        out_point.x = value[0].get<double>();
        out_point.y = value[1].get<double>();
        return true;
    }

    // style 字段是可选的，缺失或无效时回退到默认样式。
    TuYuanStyle parseStyleOrDefault(const json& primitive_object) {
        TuYuanStyle style;
        const auto style_it = primitive_object.find("style");
        if (style_it == primitive_object.end() || !style_it->is_object()) {
            return style;
        }

        const json& style_object = *style_it;
        const auto layer_it = style_object.find("layer");
        if (layer_it != style_object.end() && layer_it->is_string()) {
            style.layer = layer_it->get<std::string>();
        }

        const auto linetype_it = style_object.find("linetype");
        if (linetype_it != style_object.end() && linetype_it->is_string()) {
            style.linetype = linetype_it->get<std::string>();
        }

        const auto lineweight_it = style_object.find("lineweight_mm");
        if (lineweight_it != style_object.end() && lineweight_it->is_number()) {
            style.lineweight_mm = lineweight_it->get<double>();
        }

        return style;
    }

    // 将一个 JSON primitive 转换成项目内部的 TuYuan 结构。
    bool parsePrimitive(const json& primitive_object, TuYuan& out_primitive, std::string& error_message) {
        if (!primitive_object.is_object()) {
            error_message = "primitive must be an object";
            return false;
        }

        const auto type_it = primitive_object.find("type");
        if (type_it == primitive_object.end() || !type_it->is_string()) {
            error_message = "primitive.type is missing or invalid";
            return false;
        }

        const std::string type = type_it->get<std::string>();
        const TuYuanStyle style = parseStyleOrDefault(primitive_object);

        if (type == "LINE") {
            const auto a_it = primitive_object.find("a");
            const auto b_it = primitive_object.find("b");
            if (a_it == primitive_object.end() || b_it == primitive_object.end()) {
                error_message = "LINE is missing endpoint a or b";
                return false;
            }

            Point2D a;
            Point2D b;
            if (!parsePointArray(*a_it, a, error_message) || !parsePointArray(*b_it, b, error_message)) {
                error_message = "LINE endpoint format error: " + error_message;
                return false;
            }

            out_primitive = TuYuan::makeLine(a, b, style);
            return true;
        }

        if (type == "LWPOLYLINE") {
            const auto points_it = primitive_object.find("pts");
            if (points_it == primitive_object.end() || !points_it->is_array()) {
                error_message = "LWPOLYLINE is missing pts array";
                return false;
            }

            bool closed = false;
            const auto closed_it = primitive_object.find("closed");
            if (closed_it != primitive_object.end() && closed_it->is_boolean()) {
                closed = closed_it->get<bool>();
            }

            std::vector<Point2D> points;
            points.reserve(points_it->size());
            for (const json& point_item : *points_it) {
                Point2D point;
                if (!parsePointArray(point_item, point, error_message)) {
                    error_message = "LWPOLYLINE point format error: " + error_message;
                    return false;
                }
                points.push_back(point);
            }

            if (points.size() < 2) {
                error_message = "LWPOLYLINE needs at least two points";
                return false;
            }

            out_primitive = TuYuan::makePolyline(std::move(points), closed, style);
            return true;
        }

        error_message = "unsupported primitive.type: " + type;
        return false;
    }

    bool parseSymbol(const json& symbol_object, JsonSymbolData& out_symbol, std::string& error_message) {
        if (!symbol_object.is_object()) {
            error_message = "symbol must be an object";
            return false;
        }

        const auto id_it = symbol_object.find("id");
        if (id_it == symbol_object.end() || !id_it->is_string()) {
            error_message = "symbol.id is missing or invalid";
            return false;
        }

        out_symbol.id = id_it->get<std::string>();
        if (out_symbol.id.empty()) {
            error_message = "symbol.id must not be empty";
            return false;
        }

        const auto params_it = symbol_object.find("params");
        if (params_it != symbol_object.end() && params_it->is_object()) {
            const auto scale_mode_it = params_it->find("scale_mode");
            if (scale_mode_it != params_it->end() && scale_mode_it->is_string()) {
                out_symbol.scale_mode = scale_mode_it->get<std::string>();
            }
        }

        const auto primitives_it = symbol_object.find("primitives");
        if (primitives_it == symbol_object.end() || !primitives_it->is_array()) {
            error_message = "symbol.primitives is missing or invalid";
            return false;
        }

        out_symbol.primitives.clear();
        out_symbol.primitives.reserve(primitives_it->size());
        for (const json& primitive_item : *primitives_it) {
            TuYuan primitive;
            if (!parsePrimitive(primitive_item, primitive, error_message)) {
                return false;
            }
            out_symbol.primitives.push_back(std::move(primitive));
        }

        return true;
    }

    // 使用 nlohmann/json 解析完整的 symbols.json。
    ConvertResult loadSymbolsFromJson(const std::string& input_json, std::vector<JsonSymbolData>& out_symbols) {
        std::ifstream in(input_json.c_str(), std::ios::binary);
        if (!in) {
            return ConvertResult::failure(11, "failed to open input json: " + input_json);
        }

        json root;
        try {
            in >> root;
        }
        catch (const json::exception& ex) {
            return ConvertResult::failure(12, "failed to parse input json: " + std::string(ex.what()));
        }

        if (!root.is_object()) {
            return ConvertResult::failure(13, "root json value must be an object");
        }

        const auto symbols_it = root.find("symbols");
        if (symbols_it == root.end() || !symbols_it->is_array()) {
            return ConvertResult::failure(14, "json does not contain a valid symbols array");
        }

        out_symbols.clear();
        out_symbols.reserve(symbols_it->size());
        for (size_t i = 0; i < symbols_it->size(); ++i) {
            JsonSymbolData symbol;
            std::string symbol_error;
            if (!parseSymbol((*symbols_it)[i], symbol, symbol_error)) {
                return ConvertResult::failure(15, "failed to parse symbols[" + std::to_string(i) + "]: " + symbol_error);
            }
            out_symbols.push_back(std::move(symbol));
        }

        return ConvertResult::success();
    }

    // stretch_x 只缩放 X 方向，Y 坐标保持不变。
    void applyStretchX(TuYuan& primitive, double scale_x) {
        if (auto* line = std::get_if<LineTuYuan>(&primitive.geometry)) {
            line->a.x *= scale_x;
            line->b.x *= scale_x;
            return;
        }

        auto* polyline = std::get_if<PolylineTuYuan>(&primitive.geometry);
        if (polyline == nullptr) {
            return;
        }

        for (Point2D& point : polyline->points) {
            point.x *= scale_x;
        }
    }

    void translatePrimitive(TuYuan& primitive, double dx, double dy) {
        if (auto* line = std::get_if<LineTuYuan>(&primitive.geometry)) {
            line->a.x += dx;
            line->a.y += dy;
            line->b.x += dx;
            line->b.y += dy;
            return;
        }

        auto* polyline = std::get_if<PolylineTuYuan>(&primitive.geometry);
        if (polyline == nullptr) {
            return;
        }

        for (Point2D& point : polyline->points) {
            point.x += dx;
            point.y += dy;
        }
    }

    BBox computePrimitivesBBox(const std::vector<TuYuan>& primitives) {
        TuXing symbol;
        symbol.tuyuan_vector = primitives;
        return SymbolGeometry::computeBBox(symbol);
    }

    std::string normalizeLineType(const std::string& source) {
        if (source.empty()) {
            return "BYLAYER";
        }

        std::string upper = source;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
            });

        if (upper == "BYLAYER" || upper == "BYBLOCK" || upper == "CONTINUOUS") {
            return source;
        }

        return "BYLAYER";
    }

    DRW_LW_Conv::lineWidth toLineWeight(double lineweight_mm) {
        if (!std::isfinite(lineweight_mm)) {
            return DRW_LW_Conv::widthDefault;
        }

        const int dxf_int = static_cast<int>(std::lround(lineweight_mm * 100.0));
        return DRW_LW_Conv::dxfInt2lineWidth(dxf_int);
    }

    void applyStyleToEntity(DRW_Entity& entity, const TuYuanStyle& style) {
        entity.layer = style.layer.empty() ? "0" : style.layer;
        entity.lineType = normalizeLineType(style.linetype);
        entity.lWeight = toLineWeight(style.lineweight_mm);
    }

    // 将内部图元列表适配到 libdxfrw 的回调式写接口。
    class DxfPrimitiveWriter final : public DRW_Interface {
    public:
        DxfPrimitiveWriter(dxfRW& writer, const std::vector<TuYuan>& primitives)
            : m_writer(writer), m_primitives(primitives) {
            collectLayers();
        }

        void addHeader(const DRW_Header* /*data*/) override {}
        void addLType(const DRW_LType& /*data*/) override {}
        void addLayer(const DRW_Layer& /*data*/) override {}
        void addDimStyle(const DRW_Dimstyle& /*data*/) override {}
        void addVport(const DRW_Vport& /*data*/) override {}
        void addTextStyle(const DRW_Textstyle& /*data*/) override {}
        void addAppId(const DRW_AppId& /*data*/) override {}
        void addBlock(const DRW_Block& /*data*/) override {}
        void setBlock(const int /*handle*/) override {}
        void endBlock() override {}
        void addPoint(const DRW_Point& /*data*/) override {}
        void addLine(const DRW_Line& /*data*/) override {}
        void addRay(const DRW_Ray& /*data*/) override {}
        void addXline(const DRW_Xline& /*data*/) override {}
        void addArc(const DRW_Arc& /*data*/) override {}
        void addCircle(const DRW_Circle& /*data*/) override {}
        void addEllipse(const DRW_Ellipse& /*data*/) override {}
        void addLWPolyline(const DRW_LWPolyline& /*data*/) override {}
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
        void writeLTypes() override {}
        void writeTextstyles() override {}
        void writeVports() override {}
        void writeDimstyles() override {}
        void writeAppId() override {}

        void writeEntities() override {
            for (const TuYuan& primitive : m_primitives) {
                if (const auto* line = std::get_if<LineTuYuan>(&primitive.geometry)) {
                    DRW_Line line_entity;
                    applyStyleToEntity(line_entity, primitive.style);
                    line_entity.basePoint.x = line->a.x;
                    line_entity.basePoint.y = line->a.y;
                    line_entity.basePoint.z = 0.0;
                    line_entity.secPoint.x = line->b.x;
                    line_entity.secPoint.y = line->b.y;
                    line_entity.secPoint.z = 0.0;
                    m_writer.writeLine(&line_entity);
                    continue;
                }

                const auto* polyline = std::get_if<PolylineTuYuan>(&primitive.geometry);
                if (polyline == nullptr || polyline->points.size() < 2) {
                    continue;
                }

                DRW_LWPolyline lwpolyline;
                applyStyleToEntity(lwpolyline, primitive.style);
                lwpolyline.flags = polyline->closed ? 0x01 : 0x00;
                for (const Point2D& point : polyline->points) {
                    DRW_Vertex2D vertex;
                    vertex.x = point.x;
                    vertex.y = point.y;
                    lwpolyline.addVertex(vertex);
                }
                m_writer.writeLWPolyline(&lwpolyline);
            }
        }

        void writeLayers() override {
            for (const std::string& layer_name : m_layers) {
                DRW_Layer layer;
                layer.name = layer_name;
                m_writer.writeLayer(&layer);
            }
        }

    private:
        void collectLayers() {
            for (const TuYuan& primitive : m_primitives) {
                if (primitive.style.layer.empty()) {
                    m_layers.insert("0");
                }
                else {
                    m_layers.insert(primitive.style.layer);
                }
            }
        }

    private:
        dxfRW& m_writer;
        const std::vector<TuYuan>& m_primitives;
        std::set<std::string> m_layers;
    };

} // namespace

ConvertResult JsonSymbolDxfWriter::run(
    const std::string& input_json,
    const std::string& output_dxf,
    const std::vector<SymbolRequest>& requested_symbols,
    double symbol_gap) {

    if (requested_symbols.empty()) {
        return ConvertResult::failure(21, "requested_symbols is empty");
    }
    if (!std::isfinite(symbol_gap) || symbol_gap < 0.0) {
        return ConvertResult::failure(22, "symbol_gap must be >= 0");
    }

    std::vector<JsonSymbolData> all_symbols;
    ConvertResult load_result = loadSymbolsFromJson(input_json, all_symbols);
    if (!load_result.ok()) {
        return load_result;
    }

    std::map<std::string, const JsonSymbolData*> symbol_by_id;
    for (const JsonSymbolData& symbol : all_symbols) {
        if (symbol.id.empty()) {
            continue;
        }
        if (symbol_by_id.find(symbol.id) == symbol_by_id.end()) {
            symbol_by_id[symbol.id] = &symbol;
        }
    }

    std::vector<TuYuan> output_primitives;
    double cursor_x = 0.0;

    for (const SymbolRequest& request : requested_symbols) {
        const auto found = symbol_by_id.find(request.symbol_id);
        if (found == symbol_by_id.end()) {
            return ConvertResult::failure(23, "symbol id not found in json: " + request.symbol_id);
        }

        if (!std::isfinite(request.stretch_x_scale) || request.stretch_x_scale <= 0.0) {
            return ConvertResult::failure(24, "invalid stretch_x_scale for symbol: " + request.symbol_id);
        }

        const JsonSymbolData& symbol = *(found->second);
        if (symbol.primitives.empty()) {
            continue;
        }

        std::vector<TuYuan> placed_primitives = symbol.primitives;
        if (symbol.scale_mode == "stretch_x") {
            for (TuYuan& primitive : placed_primitives) {
                applyStretchX(primitive, request.stretch_x_scale);
            }
        }

        const BBox bbox = computePrimitivesBBox(placed_primitives);
        const double dx = cursor_x - bbox.min_x;

        for (TuYuan& primitive : placed_primitives) {
            translatePrimitive(primitive, dx, 0.0);
            output_primitives.push_back(std::move(primitive));
        }

        double width = bbox.max_x - bbox.min_x;
        if (!std::isfinite(width) || width < 1e-6) {
            width = 1.0;
        }
        cursor_x += width + symbol_gap;
    }

    if (output_primitives.empty()) {
        return ConvertResult::failure(25, "no primitives were selected for output");
    }

    dxfRW writer(output_dxf.c_str());
    DxfPrimitiveWriter iface(writer, output_primitives);
    if (!writer.write(&iface, DRW::AC1021, false)) {
        return ConvertResult::failure(26, "failed to write output dxf: " + output_dxf);
    }

    return ConvertResult::success();
}
