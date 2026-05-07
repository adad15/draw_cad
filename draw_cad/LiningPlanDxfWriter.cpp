#include "LiningPlanDxfWriter.h"

#include "../libxfrw/libdxfrw.h"
#include "../nlohmann/json.hpp"
#include "mytool.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using nlohmann::json;

namespace {

constexpr const char* kChineseTextStyleName = "CN_TEXT";
constexpr const char* kChineseFontFileName = "simsun.ttc";
constexpr double kLiningBodyZoneCount = 4.0;
constexpr double kCrownLineHalfRangeM = 0.2;

struct SlabEntry {
    std::string slab_no;
    int slab_index = 0;
    bool has_numeric_slab = false;
    double length_m = 0.0;
    bool has_length = false;
};

struct DrawLine {
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
    std::string layer;
};

struct DrawPolyline {
    std::vector<std::pair<double, double>> points;
    bool closed = false;
    std::string layer;
};

enum class DrawTextAlign {
    Left,
    Center
};

struct DrawText {
    double x = 0.0;
    double y = 0.0;
    double height = 0.35;
    std::string text;
    std::string layer;
    DrawTextAlign align = DrawTextAlign::Center;
};

struct DrawingData {
    std::vector<DrawLine> lines;
    std::vector<DrawPolyline> polylines;
    std::vector<DrawText> texts;
};

struct DiseaseRecord {
    int source_row = 0;
    std::string slab_no;
    int slab_index = 0;
    bool has_numeric_slab = false;
    std::string check_item;
    std::string defect_location;
    std::string defect_desc;
    std::string distance_from_slab_end;
    std::string length;
    std::string width;
    std::string area;
};

struct SlabPlacement {
    double x1 = 0.0;
    double x2 = 0.0;
    double body_bottom_y = 0.0;
    double top_y = 0.0;
    double length_m = 0.0;
};

struct SymbolPoint {
    double x = 0.0;
    double y = 0.0;
};

struct SymbolBBox {
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
};

struct DrawBox {
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
};

struct LocationBand {
    double min_center_y = 0.0;
    double max_center_y = 0.0;
};

struct SymbolPrimitive {
    std::string type;
    SymbolPoint a;
    SymbolPoint b;
    std::vector<SymbolPoint> points;
    bool closed = false;
};

struct SymbolDefinition {
    std::string id;
    std::string kind;
    std::string scale_mode;
    double base_length_m = 1.0;
    SymbolBBox bbox;
    std::vector<SymbolPrimitive> primitives;
};

struct DiseaseLayout {
    double symbol_left_x = 0.0;
    double symbol_center_y = 0.0;
    double symbol_width = 0.0;
    double symbol_height = 0.0;
    double label_x = 0.0;
    double label_y = 0.0;
    std::vector<std::string> annotation_lines;
    std::string seepage_area_text;
    DrawBox symbol_box;
    DrawBox occupied_box;
};

std::string utf8Literal(const char8_t* text) {
    return std::string(
        reinterpret_cast<const char*>(text),
        std::char_traits<char8_t>::length(text));
}

void appendDxfUnicodeEscape(std::ostringstream& out, uint32_t codepoint) {
    if (codepoint <= 0xFFFF) {
        out << "\\U+"
            << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
            << codepoint
            << std::dec << std::nouppercase << std::setfill(' ');
        return;
    }

    codepoint -= 0x10000;
    const uint32_t high = 0xD800 + ((codepoint >> 10) & 0x3FF);
    const uint32_t low = 0xDC00 + (codepoint & 0x3FF);
    appendDxfUnicodeEscape(out, high);
    appendDxfUnicodeEscape(out, low);
}

bool readUtf8Codepoint(std::string_view text, size_t& index, uint32_t& codepoint) {
    const unsigned char first = static_cast<unsigned char>(text[index]);
    if (first < 0x80) {
        codepoint = first;
        ++index;
        return true;
    }

    size_t needed = 0;
    uint32_t value = 0;
    uint32_t min_value = 0;
    if ((first & 0xE0) == 0xC0) {
        needed = 2;
        value = first & 0x1F;
        min_value = 0x80;
    }
    else if ((first & 0xF0) == 0xE0) {
        needed = 3;
        value = first & 0x0F;
        min_value = 0x800;
    }
    else if ((first & 0xF8) == 0xF0) {
        needed = 4;
        value = first & 0x07;
        min_value = 0x10000;
    }
    else {
        codepoint = first;
        ++index;
        return false;
    }

    if (index + needed > text.size()) {
        codepoint = first;
        ++index;
        return false;
    }

    for (size_t i = 1; i < needed; ++i) {
        const unsigned char next = static_cast<unsigned char>(text[index + i]);
        if ((next & 0xC0) != 0x80) {
            codepoint = first;
            ++index;
            return false;
        }
        value = (value << 6) | (next & 0x3F);
    }

    if (value < min_value || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
        codepoint = first;
        ++index;
        return false;
    }

    codepoint = value;
    index += needed;
    return true;
}

std::string toDxfText(std::string_view text) {
    std::ostringstream out;
    for (size_t i = 0; i < text.size();) {
        uint32_t codepoint = 0;
        readUtf8Codepoint(text, i, codepoint);
        if (codepoint >= 0x20 && codepoint <= 0x7E && codepoint != '\\') {
            out << static_cast<char>(codepoint);
        }
        else {
            appendDxfUnicodeEscape(out, codepoint);
        }
    }
    return out.str();
}

std::string trimAsciiWhitespace(std::string text) {
    const auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    if (begin == text.end()) {
        return "";
    }

    const auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();

    return std::string(begin, end);
}

std::string normalizeText(std::string text) {
    return trimAsciiWhitespace(std::move(text));
}

bool parseIntStrict(const std::string& text, int& out_value) {
    const std::string normalized = normalizeText(text);
    if (normalized.empty()) {
        return false;
    }

    char* end = nullptr;
    const long value = std::strtol(normalized.c_str(), &end, 10);
    if (end == normalized.c_str()) {
        return false;
    }

    while (end != nullptr && *end != '\0') {
        if (std::isspace(static_cast<unsigned char>(*end)) == 0) {
            return false;
        }
        ++end;
    }

    out_value = static_cast<int>(value);
    return true;
}

bool parseLeadingDouble(const std::string& text, double& out_value) {
    const std::string normalized = normalizeText(text);
    if (normalized.empty()) {
        return false;
    }

    char* end = nullptr;
    const double value = std::strtod(normalized.c_str(), &end);
    if (end == normalized.c_str() || !std::isfinite(value)) {
        return false;
    }

    out_value = value;
    return true;
}

std::string slabKey(const std::string& slab_no) {
    int slab_index = 0;
    if (parseIntStrict(slab_no, slab_index)) {
        return "N:" + std::to_string(slab_index);
    }

    return "T:" + normalizeText(slab_no);
}

std::string slabKey(const SlabEntry& slab) {
    if (slab.has_numeric_slab) {
        return "N:" + std::to_string(slab.slab_index);
    }

    return "T:" + slab.slab_no;
}

bool containsText(const std::string& text, const char8_t* needle) {
    return text.find(utf8Literal(needle)) != std::string::npos;
}

bool isCrownOnlyLocation(const std::string& location) {
    return containsText(location, u8"\u62f1\u9876")
        && !containsText(location, u8"\u5de6\u62f1\u8170")
        && !containsText(location, u8"\u53f3\u62f1\u8170");
}

std::string firstNonEmpty(std::initializer_list<std::string> values) {
    for (std::string value : values) {
        value = normalizeText(std::move(value));
        if (!value.empty()) {
            return value;
        }
    }

    return "";
}

std::string slabDisplayText(const SlabEntry& slab) {
    if (!slab.has_numeric_slab || slab.slab_index < 0 || slab.slab_index > 999) {
        return slab.slab_no;
    }

    std::ostringstream oss;
    oss << std::setw(3) << std::setfill('0') << slab.slab_index;
    return oss.str();
}

bool areAdjacent(const SlabEntry& lhs, const SlabEntry& rhs) {
    return lhs.has_numeric_slab
        && rhs.has_numeric_slab
        && rhs.slab_index == lhs.slab_index + 1;
}

json loadJsonFile(const std::string& path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open json: " + path);
    }

    json root;
    in >> root;
    return root;
}

std::vector<SlabEntry> loadDiseaseSlabs(const std::string& path) {
    const json root = loadJsonFile(path);
    const auto records_it = root.find("records");
    if (records_it == root.end() || !records_it->is_array()) {
        throw std::runtime_error("disease report json must contain records array");
    }

    std::vector<SlabEntry> slabs;
    std::set<int> seen_numeric_slabs;
    std::set<std::string> seen_text_slabs;

    for (const json& record : *records_it) {
        const auto slab_it = record.find("slab_no");
        if (slab_it == record.end() || !slab_it->is_string()) {
            continue;
        }

        SlabEntry slab;
        slab.slab_no = normalizeText(slab_it->get<std::string>());
        if (slab.slab_no.empty()) {
            continue;
        }
        slab.has_numeric_slab = parseIntStrict(slab.slab_no, slab.slab_index);

        if (slab.has_numeric_slab) {
            if (!seen_numeric_slabs.insert(slab.slab_index).second) {
                continue;
            }
        }
        else if (!seen_text_slabs.insert(slab.slab_no).second) {
            continue;
        }

        slabs.push_back(std::move(slab));
    }

    std::stable_sort(slabs.begin(), slabs.end(), [](const SlabEntry& lhs, const SlabEntry& rhs) {
        if (lhs.has_numeric_slab != rhs.has_numeric_slab) {
            return lhs.has_numeric_slab;
        }
        if (lhs.has_numeric_slab && rhs.has_numeric_slab && lhs.slab_index != rhs.slab_index) {
            return lhs.slab_index < rhs.slab_index;
        }
        return lhs.slab_no < rhs.slab_no;
    });

    return slabs;
}

std::string getOptionalString(const json& object, const char* key) {
    const auto found = object.find(key);
    if (found == object.end() || !found->is_string()) {
        return "";
    }

    return normalizeText(found->get<std::string>());
}

std::vector<DiseaseRecord> loadDiseaseRecords(const std::string& path) {
    const json root = loadJsonFile(path);
    const auto records_it = root.find("records");
    if (records_it == root.end() || !records_it->is_array()) {
        throw std::runtime_error("disease report json must contain records array");
    }

    std::vector<DiseaseRecord> records;
    records.reserve(records_it->size());
    for (const json& record_json : *records_it) {
        if (!record_json.is_object()) {
            continue;
        }

        DiseaseRecord record;
        const auto source_row_it = record_json.find("source_row");
        if (source_row_it != record_json.end() && source_row_it->is_number_integer()) {
            record.source_row = source_row_it->get<int>();
        }

        record.slab_no = getOptionalString(record_json, "slab_no");
        if (record.slab_no.empty()) {
            continue;
        }
        record.has_numeric_slab = parseIntStrict(record.slab_no, record.slab_index);
        record.check_item = getOptionalString(record_json, "check_item");
        record.defect_location = getOptionalString(record_json, "defect_location");
        record.defect_desc = getOptionalString(record_json, "defect_desc");

        const auto parsed_it = record_json.find("parsed");
        if (parsed_it != record_json.end() && parsed_it->is_object()) {
            record.distance_from_slab_end = getOptionalString(*parsed_it, "distance_from_slab_end");
            record.length = getOptionalString(*parsed_it, "length");
            record.width = getOptionalString(*parsed_it, "width");
            record.area = getOptionalString(*parsed_it, "area");
        }

        record.distance_from_slab_end = firstNonEmpty({
            record.distance_from_slab_end,
            getOptionalString(record_json, "distance_from_slab_end")
        });
        record.length = firstNonEmpty({
            record.length,
            getOptionalString(record_json, "length")
        });
        record.width = firstNonEmpty({
            record.width,
            getOptionalString(record_json, "width")
        });
        record.area = firstNonEmpty({
            record.area,
            getOptionalString(record_json, "area")
        });

        records.push_back(std::move(record));
    }

    return records;
}

std::unordered_map<int, double> loadBoardLengths(const std::string& path) {
    const json root = loadJsonFile(path);
    const auto records_it = root.find("records");
    if (records_it == root.end() || !records_it->is_array()) {
        throw std::runtime_error("board lengths json must contain records array");
    }

    std::unordered_map<int, double> lengths;
    for (const json& record : *records_it) {
        const auto slab_it = record.find("slab_no");
        const auto length_it = record.find("length_m");
        if (slab_it == record.end() || length_it == record.end() || !slab_it->is_string() || !length_it->is_number()) {
            continue;
        }

        int slab_index = 0;
        if (!parseIntStrict(slab_it->get<std::string>(), slab_index)) {
            continue;
        }

        const double length_m = length_it->get<double>();
        if (std::isfinite(length_m) && length_m > 0.0) {
            lengths[slab_index] = length_m;
        }
    }

    return lengths;
}

bool parseSymbolPoint(const json& value, SymbolPoint& out_point) {
    if (!value.is_array() || value.size() < 2 || !value[0].is_number() || !value[1].is_number()) {
        return false;
    }

    out_point.x = value[0].get<double>();
    out_point.y = value[1].get<double>();
    return std::isfinite(out_point.x) && std::isfinite(out_point.y);
}

SymbolBBox bboxFromPrimitives(const std::vector<SymbolPrimitive>& primitives) {
    SymbolBBox bbox;
    bool initialized = false;
    auto include_point = [&](const SymbolPoint& point) {
        if (!initialized) {
            bbox.min_x = bbox.max_x = point.x;
            bbox.min_y = bbox.max_y = point.y;
            initialized = true;
            return;
        }
        bbox.min_x = std::min(bbox.min_x, point.x);
        bbox.min_y = std::min(bbox.min_y, point.y);
        bbox.max_x = std::max(bbox.max_x, point.x);
        bbox.max_y = std::max(bbox.max_y, point.y);
    };

    for (const SymbolPrimitive& primitive : primitives) {
        if (primitive.type == "LINE") {
            include_point(primitive.a);
            include_point(primitive.b);
        }
        else {
            for (const SymbolPoint& point : primitive.points) {
                include_point(point);
            }
        }
    }

    return bbox;
}

std::unordered_map<std::string, SymbolDefinition> loadSymbols(const std::string& path) {
    const json root = loadJsonFile(path);
    const auto symbols_it = root.find("symbols");
    if (symbols_it == root.end() || !symbols_it->is_array()) {
        throw std::runtime_error("symbols json must contain symbols array");
    }

    std::unordered_map<std::string, SymbolDefinition> symbols;
    for (const json& symbol_json : *symbols_it) {
        if (!symbol_json.is_object()) {
            continue;
        }

        SymbolDefinition symbol;
        symbol.id = getOptionalString(symbol_json, "id");
        if (symbol.id.empty()) {
            continue;
        }
        symbol.kind = getOptionalString(symbol_json, "kind");

        const auto params_it = symbol_json.find("params");
        if (params_it != symbol_json.end() && params_it->is_object()) {
            symbol.scale_mode = getOptionalString(*params_it, "scale_mode");
            const auto base_it = params_it->find("base_length_m");
            if (base_it != params_it->end() && base_it->is_number()) {
                const double base_length = base_it->get<double>();
                if (std::isfinite(base_length) && base_length > 0.0) {
                    symbol.base_length_m = base_length;
                }
            }
        }

        const auto primitives_it = symbol_json.find("primitives");
        if (primitives_it == symbol_json.end() || !primitives_it->is_array()) {
            continue;
        }

        for (const json& primitive_json : *primitives_it) {
            if (!primitive_json.is_object()) {
                continue;
            }

            SymbolPrimitive primitive;
            primitive.type = getOptionalString(primitive_json, "type");
            if (primitive.type == "LINE") {
                const auto a_it = primitive_json.find("a");
                const auto b_it = primitive_json.find("b");
                if (a_it == primitive_json.end() || b_it == primitive_json.end()
                    || !parseSymbolPoint(*a_it, primitive.a)
                    || !parseSymbolPoint(*b_it, primitive.b)) {
                    continue;
                }
            }
            else if (primitive.type == "LWPOLYLINE") {
                const auto closed_it = primitive_json.find("closed");
                primitive.closed = (closed_it != primitive_json.end() && closed_it->is_boolean())
                    ? closed_it->get<bool>()
                    : false;

                const auto points_it = primitive_json.find("pts");
                if (points_it == primitive_json.end() || !points_it->is_array()) {
                    continue;
                }

                for (const json& point_json : *points_it) {
                    SymbolPoint point;
                    if (parseSymbolPoint(point_json, point)) {
                        primitive.points.push_back(point);
                    }
                }
                if (primitive.points.size() < 2) {
                    continue;
                }
            }
            else {
                continue;
            }

            symbol.primitives.push_back(std::move(primitive));
        }

        if (symbol.primitives.empty()) {
            continue;
        }

        const auto bbox_it = symbol_json.find("bbox");
        const auto bbox_min_it = (bbox_it != symbol_json.end() && bbox_it->is_object()) ? bbox_it->find("min") : json::const_iterator();
        const auto bbox_max_it = (bbox_it != symbol_json.end() && bbox_it->is_object()) ? bbox_it->find("max") : json::const_iterator();
        SymbolPoint bbox_min;
        SymbolPoint bbox_max;
        if (bbox_it != symbol_json.end()
            && bbox_it->is_object()
            && bbox_min_it != bbox_it->end()
            && bbox_max_it != bbox_it->end()
            && parseSymbolPoint(*bbox_min_it, bbox_min)
            && parseSymbolPoint(*bbox_max_it, bbox_max)) {
            symbol.bbox = { bbox_min.x, bbox_min.y, bbox_max.x, bbox_max.y };
        }
        else {
            symbol.bbox = bboxFromPrimitives(symbol.primitives);
        }

        symbols.emplace(symbol.id, std::move(symbol));
    }

    return symbols;
}

void addLine(DrawingData& drawing, double x1, double y1, double x2, double y2, std::string layer) {
    drawing.lines.push_back({ x1, y1, x2, y2, std::move(layer) });
}

void addText(
    DrawingData& drawing,
    double x,
    double y,
    double height,
    std::string text,
    std::string layer,
    DrawTextAlign align = DrawTextAlign::Center) {
    drawing.texts.push_back({ x, y, height, std::move(text), std::move(layer), align });
}

void addTextBlock(
    DrawingData& drawing,
    double x,
    double y,
    double height,
    double line_spacing,
    const std::vector<std::string>& lines,
    std::string layer,
    DrawTextAlign align) {
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].empty()) {
            continue;
        }
        addText(
            drawing,
            x,
            y - static_cast<double>(i) * line_spacing,
            height,
            lines[i],
            layer,
            align);
    }
}

void addBreakLine(
    DrawingData& drawing,
    double x,
    double y_bottom,
    double y_top,
    double line_gap,
    double offset_x,
    double half_height) {
    const double mid_y = (y_bottom + y_top) * 0.5;

    auto add_one = [&](double line_x) {
        DrawPolyline polyline;
        polyline.layer = "BREAK";
        polyline.points = {
            { line_x, y_top },
            { line_x, mid_y + half_height * 2.0 },
            { line_x + offset_x, mid_y + half_height },
            { line_x, mid_y },
            { line_x - offset_x, mid_y - half_height },
            { line_x, mid_y - half_height * 2.0 },
            { line_x, y_bottom }
        };
        drawing.polylines.push_back(std::move(polyline));
    };

    const double half_gap = std::max(0.0, line_gap) * 0.5;
    add_one(x - half_gap);
    add_one(x + half_gap);
}

double slabWidth(const SlabEntry& slab, const LiningPlanDxfWriter::Options& options) {
    const double length_m = slab.has_length ? slab.length_m : options.default_slab_length_m;
    return std::max(1.0, length_m * options.drawing_units_per_meter);
}

void appendPanel(
    DrawingData& drawing,
    const std::vector<SlabEntry>& slabs,
    size_t begin_index,
    size_t end_index,
    double origin_x,
    double origin_y,
    std::unordered_map<std::string, SlabPlacement>& slab_placements,
    const LiningPlanDxfWriter::Options& options) {
    const double label_w = options.label_column_width;
    const double bottom_h = options.bottom_label_height;
    const double zone_h = options.zone_height;
    const double body_bottom_y = origin_y + bottom_h;
    const double top_y = body_bottom_y + zone_h * kLiningBodyZoneCount;
    const double body_left_x = origin_x + label_w;

    std::vector<double> x_positions;
    x_positions.reserve(end_index - begin_index + 1);
    x_positions.push_back(body_left_x);
    double cursor_x = body_left_x;
    for (size_t i = begin_index; i < end_index; ++i) {
        cursor_x += slabWidth(slabs[i], options);
        x_positions.push_back(cursor_x);
    }
    const double right_x = cursor_x;

    addLine(drawing, origin_x, origin_y, right_x, origin_y, "FRAME");
    addLine(drawing, origin_x, top_y, right_x, top_y, "FRAME");
    addLine(drawing, origin_x, origin_y, origin_x, top_y, "FRAME");
    addLine(drawing, body_left_x, origin_y, body_left_x, top_y, "FRAME");
    addLine(drawing, right_x, origin_y, right_x, top_y, "FRAME");
    addLine(drawing, origin_x, body_bottom_y, right_x, body_bottom_y, "FRAME");

    for (int row = 1; row < static_cast<int>(kLiningBodyZoneCount); ++row) {
        const double y = body_bottom_y + zone_h * static_cast<double>(row);
        addLine(drawing, body_left_x, y, right_x, y, "FRAME");
    }

    for (size_t i = begin_index + 1; i < end_index; ++i) {
        const double x = x_positions[i - begin_index];
        if (areAdjacent(slabs[i - 1], slabs[i])) {
            addLine(drawing, x, origin_y, x, top_y, "FRAME");
        }
        else {
            addLine(drawing, x, origin_y, x, body_bottom_y, "FRAME");
            addBreakLine(
                drawing,
                x,
                body_bottom_y,
                top_y,
                options.break_line_gap,
                options.break_offset_x,
                options.break_half_height);
        }
    }

    const std::string labels[] = {
        utf8Literal(u8"\u5de6\u4fa7\u8fb9\u5899\u5e95\u7ebf"),
        utf8Literal(u8"\u5de6\u4fa7\u62f1\u8170\u7ebf"),
        utf8Literal(u8"\u62f1\u9876\u7ebf"),
        utf8Literal(u8"\u53f3\u4fa7\u62f1\u8170\u7ebf"),
        utf8Literal(u8"\u53f3\u4fa7\u8fb9\u5899\u5e95\u7ebf")
    };
    const double label_y_positions[] = {
        top_y - zone_h * 0.35,
        top_y - zone_h * 1.0,
        top_y - zone_h * 2.0,
        top_y - zone_h * 3.0,
        body_bottom_y + zone_h * 0.35
    };
    for (int row = 0; row < 5; ++row) {
        addText(drawing, origin_x + label_w * 0.5, label_y_positions[row], options.text_height, labels[row], "TEXT");
    }
    addText(
        drawing,
        origin_x + label_w * 0.5,
        origin_y + bottom_h * 0.5,
        options.text_height,
        utf8Literal(u8"\u886c\u780c\u677f\u5757\u53f7"),
        "TEXT");

    for (size_t i = begin_index; i < end_index; ++i) {
        const size_t local_index = i - begin_index;
        const double x1 = x_positions[local_index];
        const double x2 = x_positions[local_index + 1];
        slab_placements[slabKey(slabs[i])] = {
            x1,
            x2,
            body_bottom_y,
            top_y,
            slabs[i].has_length ? slabs[i].length_m : options.default_slab_length_m
        };
        addText(
            drawing,
            (x1 + x2) * 0.5,
            origin_y + bottom_h * 0.5,
            options.slab_no_text_height,
            slabDisplayText(slabs[i]),
            "TEXT");
    }
}

std::string symbolIdForText(const std::string& text) {
    if (containsText(text, u8"\u7eb5\u5411\u88c2\u7f1d")) {
        return "crack_long";
    }
    if (containsText(text, u8"\u73af\u5411\u88c2\u7f1d")
        || containsText(text, u8"\u6a2a\u5411\u88c2\u7f1d")) {
        return "crack_trans";
    }
    if (containsText(text, u8"\u659c\u5411\u88c2\u7f1d")) {
        return "crack_diag";
    }
    if (containsText(text, u8"\u9f9f\u88c2")
        || containsText(text, u8"\u7f51\u72b6\u88c2\u7f1d")) {
        return "crack_alligator";
    }
    if (containsText(text, u8"\u9732\u7b4b")) {
        return "rebar";
    }
    if (containsText(text, u8"\u7834\u635f")
        || containsText(text, u8"\u6d82\u6599\u8131\u843d")
        || containsText(text, u8"\u6d82\u5c42\u8131\u843d")) {
        return "damage";
    }
    if (containsText(text, u8"\u6e17\u6c34\u5370\u8ff9")) {
        return "seepage";
    }

    return "";
}

std::string symbolIdForDisease(const DiseaseRecord& disease) {
    const std::string symbol_from_check_item = symbolIdForText(disease.check_item);
    if (!symbol_from_check_item.empty()) {
        return symbol_from_check_item;
    }

    return symbolIdForText(disease.defect_desc);
}

bool isLongitudinalSymbol(const std::string& symbol_id) {
    return symbol_id == "crack_long" || symbol_id == "rebar";
}

bool isTransverseSymbol(const std::string& symbol_id) {
    return symbol_id == "crack_trans";
}

bool isDiagonalSymbol(const std::string& symbol_id) {
    return symbol_id == "crack_diag";
}

bool isLengthScaledSymbol(const std::string& symbol_id) {
    return isLongitudinalSymbol(symbol_id)
        || isTransverseSymbol(symbol_id);
}

bool isLengthAnnotatedSymbol(const std::string& symbol_id) {
    return isLongitudinalSymbol(symbol_id)
        || isTransverseSymbol(symbol_id)
        || isDiagonalSymbol(symbol_id);
}

double clampDouble(double value, double min_value, double max_value) {
    if (max_value < min_value) {
        return min_value;
    }

    return std::max(min_value, std::min(value, max_value));
}

double estimateTextWidth(std::string_view text, double height) {
    double width = 0.0;
    for (size_t i = 0; i < text.size();) {
        uint32_t codepoint = 0;
        readUtf8Codepoint(text, i, codepoint);
        width += (codepoint <= 0x7F) ? height * 0.58 : height;
    }
    return width;
}

DrawBox makeBox(double min_x, double min_y, double max_x, double max_y) {
    return {
        std::min(min_x, max_x),
        std::min(min_y, max_y),
        std::max(min_x, max_x),
        std::max(min_y, max_y)
    };
}

DrawBox expandBox(const DrawBox& box, double padding) {
    return {
        box.min_x - padding,
        box.min_y - padding,
        box.max_x + padding,
        box.max_y + padding
    };
}

DrawBox unionBox(const DrawBox& lhs, const DrawBox& rhs) {
    return {
        std::min(lhs.min_x, rhs.min_x),
        std::min(lhs.min_y, rhs.min_y),
        std::max(lhs.max_x, rhs.max_x),
        std::max(lhs.max_y, rhs.max_y)
    };
}

double intersectionArea(const DrawBox& lhs, const DrawBox& rhs) {
    const double width = std::min(lhs.max_x, rhs.max_x) - std::max(lhs.min_x, rhs.min_x);
    const double height = std::min(lhs.max_y, rhs.max_y) - std::max(lhs.min_y, rhs.min_y);
    if (width <= 0.0 || height <= 0.0) {
        return 0.0;
    }
    return width * height;
}

double overlapScore(const DrawBox& box, const std::vector<DrawBox>& placed_boxes) {
    double score = 0.0;
    for (const DrawBox& placed : placed_boxes) {
        score += intersectionArea(box, placed);
    }
    return score;
}

double outsideBodyPenalty(const DrawBox& box, const SlabPlacement& placement) {
    double penalty = 0.0;
    penalty += std::max(0.0, placement.x1 - box.min_x);
    penalty += std::max(0.0, box.max_x - placement.x2);
    penalty += std::max(0.0, placement.body_bottom_y - box.min_y);
    penalty += std::max(0.0, box.max_y - placement.top_y);
    return penalty * 10.0;
}

double outsideLocationBandPenalty(const DrawBox& box, const LocationBand& location_band) {
    double penalty = 0.0;
    penalty += std::max(0.0, location_band.min_center_y - box.min_y);
    penalty += std::max(0.0, box.max_y - location_band.max_center_y);
    return penalty * 100.0;
}

DrawBox symbolBox(double left_x, double center_y, double width, double height) {
    return makeBox(
        left_x,
        center_y - height * 0.5,
        left_x + width,
        center_y + height * 0.5);
}

LocationBand insetLocationBand(
    double min_y,
    double max_y,
    const LiningPlanDxfWriter::Options& options) {
    if (max_y < min_y) {
        std::swap(min_y, max_y);
    }

    const double height = max_y - min_y;
    const double inset = std::min(
        std::max(0.0, options.disease_horizontal_line_clearance),
        height * 0.45);
    return { min_y + inset, max_y - inset };
}

DrawBox annotationBox(
    double label_x,
    double label_y,
    const std::vector<std::string>& lines,
    const LiningPlanDxfWriter::Options& options,
    DrawTextAlign align) {
    if (lines.empty()) {
        return makeBox(label_x, label_y, label_x, label_y);
    }

    double max_width = 0.0;
    for (const std::string& line : lines) {
        max_width = std::max(max_width, estimateTextWidth(line, options.annotation_text_height));
    }

    const double text_half_h = options.annotation_text_height * 0.6;
    const double min_y = label_y - static_cast<double>(lines.size() - 1) * options.annotation_line_spacing - text_half_h;
    const double max_y = label_y + text_half_h;
    if (align == DrawTextAlign::Center) {
        return makeBox(label_x - max_width * 0.5, min_y, label_x + max_width * 0.5, max_y);
    }

    return makeBox(label_x, min_y, label_x + max_width, max_y);
}

LocationBand diseaseLocationBand(
    const std::string& location,
    const SlabPlacement& placement,
    const LiningPlanDxfWriter::Options& options);

double diseaseLocationY(
    const std::string& location,
    const SlabPlacement& placement,
    const LiningPlanDxfWriter::Options& options) {
    const LocationBand band = diseaseLocationBand(location, placement, options);
    if (isCrownOnlyLocation(location)) {
        return band.max_center_y;
    }
    return (band.min_center_y + band.max_center_y) * 0.5;
}

LocationBand diseaseLocationBand(
    const std::string& location,
    const SlabPlacement& placement,
    const LiningPlanDxfWriter::Options& options) {
    const double zone_h = options.zone_height;
    if (containsText(location, u8"\u5de6\u4fa7\u8fb9\u5899")
        || containsText(location, u8"\u5de6\u8fb9\u5899")
        || containsText(location, u8"\u5de6\u4fa7\u5899")) {
        return insetLocationBand(placement.top_y - zone_h, placement.top_y, options);
    }
    if (containsText(location, u8"\u5de6\u62f1\u8170")) {
        return insetLocationBand(placement.top_y - zone_h * 2.0, placement.top_y - zone_h, options);
    }
    if (containsText(location, u8"\u53f3\u62f1\u8170")) {
        return insetLocationBand(placement.top_y - zone_h * 3.0, placement.top_y - zone_h * 2.0, options);
    }
    if (isCrownOnlyLocation(location)) {
        const double crown_y = placement.top_y - zone_h * 2.0;
        const double half_range = kCrownLineHalfRangeM * options.drawing_units_per_meter;
        return { crown_y - half_range, crown_y + half_range };
    }
    if (containsText(location, u8"\u53f3\u4fa7\u8fb9\u5899")
        || containsText(location, u8"\u53f3\u8fb9\u5899")
        || containsText(location, u8"\u53f3\u4fa7\u5899")) {
        return insetLocationBand(placement.body_bottom_y, placement.top_y - zone_h * 3.0, options);
    }
    if (containsText(location, u8"\u4fa7\u5899")) {
        return insetLocationBand(placement.body_bottom_y, placement.top_y - zone_h * 3.0, options);
    }

    const double crown_y = placement.top_y - zone_h * 2.0;
    const double half_range = kCrownLineHalfRangeM * options.drawing_units_per_meter;
    return { crown_y - half_range, crown_y + half_range };
}

std::string areaTextForDisplay(std::string area) {
    area = normalizeText(std::move(area));
    const std::string superscript_two = utf8Literal(u8"\u00b2");

    for (const std::string& suffix : { std::string("m2"), std::string("M2") }) {
        const size_t pos = area.find(suffix);
        if (pos != std::string::npos) {
            area.replace(pos, suffix.size(), "m" + superscript_two);
            break;
        }
    }

    return area;
}

std::string diseaseNoteText(const DiseaseRecord& disease, const std::string& symbol_id) {
    const std::string& desc = disease.defect_desc;
    if (containsText(desc, u8"\u4fee\u8865\u5904") && containsText(desc, u8"\u8d77\u76ae")) {
        return utf8Literal(u8"\u4fee\u8865\u5904\u8d77\u76ae");
    }
    if (containsText(desc, u8"\u5c01\u7f1d\u6599") && containsText(desc, u8"\u8d77\u76ae")) {
        return utf8Literal(u8"\u5c01\u7f1d\u6599\u8d77\u76ae");
    }
    if (containsText(desc, u8"\u5df2\u4fee\u8865")) {
        return utf8Literal(u8"\u5df2\u4fee\u8865");
    }
    if (containsText(desc, u8"\u7834\u635f")) {
        return utf8Literal(u8"\u7834\u635f");
    }
    if (containsText(desc, u8"\u6d82\u6599\u8131\u843d")
        || containsText(desc, u8"\u6d82\u5c42\u8131\u843d")) {
        return utf8Literal(u8"\u6d82\u6599\u8131\u843d");
    }
    if (symbol_id != "seepage" && containsText(desc, u8"\u6e17\u6c34\u5370\u8ff9")) {
        return utf8Literal(u8"\u4f34\u968f\u6e17\u6c34\u5370\u8ff9");
    }

    return "";
}

std::vector<std::string> annotationLinesForDisease(const DiseaseRecord& disease, const std::string& symbol_id) {
    std::vector<std::string> lines;
    if (!disease.length.empty() && isLengthAnnotatedSymbol(symbol_id)) {
        lines.push_back("L=" + disease.length);
    }
    else if (!disease.area.empty()) {
        lines.push_back("A=" + areaTextForDisplay(disease.area));
    }
    else if (!disease.width.empty()) {
        lines.push_back("W=" + disease.width);
    }

    const std::string note = diseaseNoteText(disease, symbol_id);
    if (!note.empty()) {
        lines.push_back(note);
    }
    else if (lines.empty() && !disease.check_item.empty()) {
        lines.push_back(disease.check_item);
    }

    return lines;
}

SymbolPoint transformSymbolPoint(
    const SymbolPoint& point,
    const SymbolDefinition& symbol,
    double left_x,
    double center_y,
    double scale_x,
    double scale_y) {
    const double source_mid_y = (symbol.bbox.min_y + symbol.bbox.max_y) * 0.5;
    return {
        left_x + (point.x - symbol.bbox.min_x) * scale_x,
        center_y + (point.y - source_mid_y) * scale_y
    };
}

void addSymbolGeometry(
    DrawingData& drawing,
    const SymbolDefinition& symbol,
    double left_x,
    double center_y,
    double scale_x,
    double scale_y) {
    for (const SymbolPrimitive& primitive : symbol.primitives) {
        if (primitive.type == "LINE") {
            const SymbolPoint a = transformSymbolPoint(primitive.a, symbol, left_x, center_y, scale_x, scale_y);
            const SymbolPoint b = transformSymbolPoint(primitive.b, symbol, left_x, center_y, scale_x, scale_y);
            addLine(drawing, a.x, a.y, b.x, b.y, "DISEASE");
            continue;
        }

        DrawPolyline polyline;
        polyline.layer = "DISEASE";
        polyline.closed = primitive.closed;
        polyline.points.reserve(primitive.points.size());
        for (const SymbolPoint& point : primitive.points) {
            const SymbolPoint transformed = transformSymbolPoint(point, symbol, left_x, center_y, scale_x, scale_y);
            polyline.points.push_back({ transformed.x, transformed.y });
        }
        drawing.polylines.push_back(std::move(polyline));
    }
}

DiseaseLayout makeDiseaseLayout(
    const SlabPlacement& placement,
    const std::string& symbol_id,
    const LocationBand& location_band,
    double symbol_left_x,
    double symbol_center_y,
    double symbol_width,
    double symbol_height,
    const DiseaseRecord& disease,
    const LiningPlanDxfWriter::Options& options) {
    DiseaseLayout layout;
    layout.symbol_left_x = clampDouble(symbol_left_x, placement.x1, placement.x2 - symbol_width);
    const double location_mid_y = (location_band.min_center_y + location_band.max_center_y) * 0.5;
    double min_symbol_center_y = location_band.min_center_y + symbol_height * 0.5;
    double max_symbol_center_y = location_band.max_center_y - symbol_height * 0.5;
    if (max_symbol_center_y < min_symbol_center_y) {
        min_symbol_center_y = location_mid_y;
        max_symbol_center_y = location_mid_y;
    }
    layout.symbol_center_y = clampDouble(
        symbol_center_y,
        min_symbol_center_y,
        max_symbol_center_y);
    layout.symbol_width = symbol_width;
    layout.symbol_height = symbol_height;
    layout.symbol_box = symbolBox(layout.symbol_left_x, layout.symbol_center_y, symbol_width, symbol_height);
    layout.occupied_box = layout.symbol_box;

    if (symbol_id == "seepage") {
        layout.seepage_area_text = areaTextForDisplay(disease.area);
        if (!layout.seepage_area_text.empty()) {
            const DrawBox text_box = annotationBox(
                layout.symbol_left_x + symbol_width * 0.5,
                layout.symbol_center_y,
                { layout.seepage_area_text },
                options,
                DrawTextAlign::Center);
            layout.occupied_box = unionBox(layout.occupied_box, text_box);
        }
        return layout;
    }

    layout.annotation_lines = annotationLinesForDisease(disease, symbol_id);
    if (layout.annotation_lines.empty()) {
        return layout;
    }

    const double max_label_width = [&]() {
        double width = 0.0;
        for (const std::string& line : layout.annotation_lines) {
            width = std::max(width, estimateTextWidth(line, options.annotation_text_height));
        }
        return width;
    }();

    layout.label_x = clampDouble(
        layout.symbol_left_x + symbol_width + options.annotation_gap,
        placement.x1,
        placement.x2 - max_label_width);
    layout.label_y = layout.symbol_center_y + std::max(symbol_height * 0.25, options.annotation_text_height);
    const DrawBox label_box = annotationBox(
        layout.label_x,
        layout.label_y,
        layout.annotation_lines,
        options,
        DrawTextAlign::Left);
    layout.occupied_box = unionBox(layout.occupied_box, label_box);
    return layout;
}

DiseaseLayout chooseNonOverlappingLayout(
    const SlabPlacement& placement,
    const std::string& symbol_id,
    const LocationBand& location_band,
    double base_symbol_left_x,
    double base_symbol_center_y,
    double symbol_width,
    double symbol_height,
    const DiseaseRecord& disease,
    const std::vector<DrawBox>& placed_boxes,
    const LiningPlanDxfWriter::Options& options) {
    DiseaseLayout best_layout;
    double best_score = 1e100;
    bool has_best = false;

    const size_t max_steps = options.disease_collision_max_offset_steps;
    for (size_t step = 0; step <= max_steps; ++step) {
        const std::vector<double> dy_values = (step == 0)
            ? std::vector<double>{ 0.0 }
            : std::vector<double>{
                static_cast<double>(step) * options.disease_collision_step_y,
                -static_cast<double>(step) * options.disease_collision_step_y
            };
        const std::vector<double> dx_values = (step == 0)
            ? std::vector<double>{ 0.0 }
            : std::vector<double>{
                0.0,
                static_cast<double>(step) * options.disease_collision_step_x,
                -static_cast<double>(step) * options.disease_collision_step_x
            };

        for (const double dy : dy_values) {
            for (const double dx : dx_values) {
                DiseaseLayout candidate = makeDiseaseLayout(
                    placement,
                    symbol_id,
                    location_band,
                    base_symbol_left_x + dx,
                    base_symbol_center_y + dy,
                    symbol_width,
                    symbol_height,
                    disease,
                    options);
                candidate.occupied_box = expandBox(candidate.occupied_box, options.disease_collision_padding);
                const double score = overlapScore(candidate.occupied_box, placed_boxes)
                    + outsideBodyPenalty(candidate.occupied_box, placement)
                    + outsideLocationBandPenalty(candidate.symbol_box, location_band)
                    + (std::fabs(dx) + std::fabs(dy)) * 1e-4;
                if (!has_best || score < best_score) {
                    best_layout = candidate;
                    best_score = score;
                    has_best = true;
                }
                if (score <= 1e-9) {
                    return candidate;
                }
            }
        }
    }

    return best_layout;
}

void appendDiseases(
    DrawingData& drawing,
    const std::vector<DiseaseRecord>& diseases,
    const std::unordered_map<std::string, SlabPlacement>& slab_placements,
    const std::unordered_map<std::string, SymbolDefinition>& symbols,
    std::vector<std::string>& warnings,
    const LiningPlanDxfWriter::Options& options) {
    if (symbols.empty()) {
        return;
    }

    std::vector<DrawBox> placed_boxes;
    for (const DiseaseRecord& disease : diseases) {
        const auto placement_it = slab_placements.find(slabKey(disease.slab_no));
        if (placement_it == slab_placements.end()) {
            continue;
        }

        const std::string symbol_id = symbolIdForDisease(disease);
        if (symbol_id.empty()) {
            warnings.emplace_back(
                "No symbol mapping found for disease source_row "
                + std::to_string(disease.source_row)
                + ", check_item: " + disease.check_item);
            continue;
        }

        const auto symbol_it = symbols.find(symbol_id);
        if (symbol_it == symbols.end()) {
            warnings.emplace_back(
                "Symbol id not found in symbols json: " + symbol_id
                + " for disease source_row " + std::to_string(disease.source_row));
            continue;
        }

        const SymbolDefinition& symbol = symbol_it->second;
        const SlabPlacement& placement = placement_it->second;
        const double slab_width = placement.x2 - placement.x1;
        const double slab_length_m = std::max(placement.length_m, 1e-6);

        double distance_m = 0.0;
        parseLeadingDouble(disease.distance_from_slab_end, distance_m);
        distance_m = clampDouble(distance_m, 0.0, slab_length_m);
        const double disease_x = placement.x1 + slab_width * (distance_m / slab_length_m);
        const LocationBand location_band = diseaseLocationBand(disease.defect_location, placement, options);
        const double disease_y = diseaseLocationY(disease.defect_location, placement, options);

        const double bbox_width = std::max(symbol.bbox.max_x - symbol.bbox.min_x, 1e-6);
        const double bbox_height = std::max(symbol.bbox.max_y - symbol.bbox.min_y, 1e-6);
        double scale_x = options.defect_symbol_scale;
        double scale_y = options.defect_symbol_scale;

        double length_m = 0.0;
        const bool has_length = parseLeadingDouble(disease.length, length_m) && length_m > 0.0;
        if (has_length && isLongitudinalSymbol(symbol_id)) {
            scale_x = (length_m * options.drawing_units_per_meter) / bbox_width;
            scale_y = 1.0;
        }
        else if (has_length && isTransverseSymbol(symbol_id)) {
            scale_x = 1.0;
            scale_y = (length_m * options.drawing_units_per_meter) / bbox_height;
        }

        const double symbol_width = bbox_width * scale_x;
        const double symbol_height = bbox_height * scale_y;
        double base_left_x = disease_x;
        if (isTransverseSymbol(symbol_id) || isDiagonalSymbol(symbol_id)) {
            base_left_x = disease_x - symbol_width * 0.5;
        }

        const DiseaseLayout layout = chooseNonOverlappingLayout(
            placement,
            symbol_id,
            location_band,
            base_left_x,
            disease_y,
            symbol_width,
            symbol_height,
            disease,
            placed_boxes,
            options);

        addSymbolGeometry(drawing, symbol, layout.symbol_left_x, layout.symbol_center_y, scale_x, scale_y);
        if (symbol_id == "seepage") {
            if (!layout.seepage_area_text.empty()) {
                addText(
                    drawing,
                    layout.symbol_left_x + symbol_width * 0.5,
                    layout.symbol_center_y,
                    options.annotation_text_height,
                    layout.seepage_area_text,
                    "TEXT");
            }
            placed_boxes.push_back(layout.occupied_box);
            continue;
        }

        if (!layout.annotation_lines.empty()) {
            addTextBlock(
                drawing,
                layout.label_x,
                layout.label_y,
                options.annotation_text_height,
                options.annotation_line_spacing,
                layout.annotation_lines,
                "TEXT",
                DrawTextAlign::Left);
        }
        placed_boxes.push_back(layout.occupied_box);
    }
}

DrawingData buildDrawing(
    const std::vector<SlabEntry>& slabs,
    const std::vector<DiseaseRecord>& diseases,
    const std::unordered_map<std::string, SymbolDefinition>& symbols,
    std::vector<std::string>& warnings,
    const LiningPlanDxfWriter::Options& options) {
    DrawingData drawing;
    const double panel_height = options.bottom_label_height + options.zone_height * kLiningBodyZoneCount;
    std::unordered_map<std::string, SlabPlacement> slab_placements;

    for (size_t begin = 0, panel_index = 0; begin < slabs.size(); begin += options.slabs_per_panel, ++panel_index) {
        const size_t end = std::min(begin + options.slabs_per_panel, slabs.size());
        const double origin_y = -static_cast<double>(panel_index) * (panel_height + options.panel_gap_y);
        appendPanel(drawing, slabs, begin, end, 0.0, origin_y, slab_placements, options);
    }

    appendDiseases(drawing, diseases, slab_placements, symbols, warnings, options);

    return drawing;
}

class LiningPlanDxfInterface final : public DRW_Interface {
public:
    explicit LiningPlanDxfInterface(dxfRW& writer, const DrawingData& drawing)
        : writer_(writer), drawing_(drawing) {
        layers_.insert("FRAME");
        layers_.insert("BREAK");
        layers_.insert("DISEASE");
        layers_.insert("TEXT");
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

    void writeHeader(DRW_Header& data) override {
        data.addStr("$DWGCODEPAGE", "UTF-8", 3);
    }
    void writeBlocks() override {}
    void writeBlockRecords() override {}
    void writeLTypes() override {}
    void writeVports() override {}
    void writeDimstyles() override {}
    void writeAppId() override {}

    void writeLayers() override {
        for (const std::string& layer_name : layers_) {
            DRW_Layer layer;
            layer.name = layer_name;
            writer_.writeLayer(&layer);
        }
    }

    void writeTextstyles() override {
        DRW_Textstyle text_style;
        text_style.name = kChineseTextStyleName;
        text_style.font = kChineseFontFileName;
        writer_.writeTextstyle(&text_style);
    }

    void writeEntities() override {
        for (const DrawLine& item : drawing_.lines) {
            DRW_Line line;
            line.layer = item.layer;
            line.basePoint.x = item.x1;
            line.basePoint.y = item.y1;
            line.basePoint.z = 0.0;
            line.secPoint.x = item.x2;
            line.secPoint.y = item.y2;
            line.secPoint.z = 0.0;
            writer_.writeLine(&line);
        }

        for (const DrawPolyline& item : drawing_.polylines) {
            DRW_LWPolyline polyline;
            polyline.layer = item.layer;
            polyline.flags = item.closed ? 0x01 : 0x00;
            for (const auto& point : item.points) {
                DRW_Vertex2D vertex;
                vertex.x = point.first;
                vertex.y = point.second;
                polyline.addVertex(vertex);
            }
            writer_.writeLWPolyline(&polyline);
        }

        for (const DrawText& item : drawing_.texts) {
            DRW_Text text;
            text.layer = item.layer;
            text.style = kChineseTextStyleName;
            text.text = toDxfText(item.text);
            text.height = item.height;
            text.basePoint.x = item.x;
            text.basePoint.y = item.y;
            text.basePoint.z = 0.0;
            text.secPoint = text.basePoint;
            text.alignH = (item.align == DrawTextAlign::Left) ? DRW_Text::HLeft : DRW_Text::HCenter;
            text.alignV = DRW_Text::VMiddle;
            writer_.writeText(&text);
        }
    }

private:
    dxfRW& writer_;
    const DrawingData& drawing_;
    std::set<std::string> layers_;
};

} // namespace

LiningPlanDxfWriter::WriteResult LiningPlanDxfWriter::run(
    const std::string& disease_report_json,
    const std::string& board_lengths_json,
    const std::string& output_dxf,
    const Options& options) {
    if (disease_report_json.empty()) {
        return WriteResult::failure(71, "disease report json path must not be empty.");
    }
    if (board_lengths_json.empty()) {
        return WriteResult::failure(72, "board lengths json path must not be empty.");
    }
    if (output_dxf.empty()) {
        return WriteResult::failure(73, "output dxf path must not be empty.");
    }
    if (options.slabs_per_panel == 0) {
        return WriteResult::failure(74, "slabs_per_panel must be greater than 0.");
    }
    if (!std::isfinite(options.drawing_units_per_meter) || options.drawing_units_per_meter <= 0.0) {
        return WriteResult::failure(75, "drawing_units_per_meter must be greater than 0.");
    }
    if (!std::isfinite(options.default_slab_length_m) || options.default_slab_length_m <= 0.0) {
        return WriteResult::failure(76, "default_slab_length_m must be greater than 0.");
    }
    if (!std::isfinite(options.defect_symbol_scale) || options.defect_symbol_scale <= 0.0) {
        return WriteResult::failure(80, "defect_symbol_scale must be greater than 0.");
    }
    if (!std::isfinite(options.annotation_text_height) || options.annotation_text_height <= 0.0) {
        return WriteResult::failure(81, "annotation_text_height must be greater than 0.");
    }
    if (!std::isfinite(options.disease_collision_padding) || options.disease_collision_padding < 0.0) {
        return WriteResult::failure(83, "disease_collision_padding must be >= 0.");
    }
    if (!std::isfinite(options.disease_collision_step_x) || options.disease_collision_step_x <= 0.0) {
        return WriteResult::failure(84, "disease_collision_step_x must be greater than 0.");
    }
    if (!std::isfinite(options.disease_collision_step_y) || options.disease_collision_step_y <= 0.0) {
        return WriteResult::failure(85, "disease_collision_step_y must be greater than 0.");
    }
    if (!std::isfinite(options.disease_horizontal_line_clearance) || options.disease_horizontal_line_clearance < 0.0) {
        return WriteResult::failure(86, "disease_horizontal_line_clearance must be >= 0.");
    }

    try {
        std::vector<SlabEntry> slabs = loadDiseaseSlabs(disease_report_json);
        if (slabs.empty()) {
            return WriteResult::failure(77, "no slab_no values were found in disease report json.");
        }

        const std::vector<DiseaseRecord> diseases = loadDiseaseRecords(disease_report_json);
        std::unordered_map<std::string, SymbolDefinition> symbols;
        if (!options.symbols_json.empty()) {
            symbols = loadSymbols(options.symbols_json);
            if (symbols.empty()) {
                return WriteResult::failure(82, "no symbols were found in symbols json: " + options.symbols_json);
            }
        }

        const std::unordered_map<int, double> length_by_slab = loadBoardLengths(board_lengths_json);
        std::vector<std::string> warnings;
        for (SlabEntry& slab : slabs) {
            if (slab.has_numeric_slab) {
                const auto found = length_by_slab.find(slab.slab_index);
                if (found != length_by_slab.end()) {
                    slab.length_m = found->second;
                    slab.has_length = true;
                    continue;
                }
            }

            slab.length_m = options.default_slab_length_m;
            slab.has_length = false;
            warnings.emplace_back(
                "No board length found for slab_no " + slab.slab_no
                + "; default length " + mytool::formatNumber(options.default_slab_length_m) + "m was used.");
        }

        const DrawingData drawing = buildDrawing(slabs, diseases, symbols, warnings, options);
        dxfRW writer(output_dxf.c_str());
        LiningPlanDxfInterface iface(writer, drawing);
        if (!writer.write(&iface, DRW::AC1021, false)) {
            return WriteResult::failure(78, "failed to write output dxf: " + output_dxf);
        }

        const size_t panel_count = (slabs.size() + options.slabs_per_panel - 1) / options.slabs_per_panel;
        return WriteResult::success(slabs.size(), panel_count, std::move(warnings));
    }
    catch (const std::exception& ex) {
        return WriteResult::failure(79, std::string("failed to write lining plan dxf: ") + ex.what());
    }
}
