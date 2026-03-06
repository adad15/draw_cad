//#include "dxf_to_json.h"
//#include "mytool.h"
//
//#include <algorithm>
//#include <cctype>
//#include <cerrno>
//#include <cstdlib>
//#include <fstream>
//#include <limits>
//#include <set>
//#include <sstream>
//#include <string>
//#include <utility>
//#include <vector>
//
//class JsonScalePerSymbolTest final {
//public:
//    static ConvertResult run(
//        const std::string& input_json = R"(D:\vs2022 code\draw_cad\symbols.json)",
//        const std::string& output_dxf = R"(D:\vs2022 code\draw_cad\scaled_symbols_from_json_test.dxf)");
//};
//
//namespace {
//
//    struct JsonValue {
//        enum class Type {
//            Null,
//            Bool,
//            Number,
//            String,
//            Array,
//            Object
//        };
//
//        Type type = Type::Null;
//        bool bool_value = false;
//        double number_value = 0.0;
//        std::string string_value;
//        std::vector<JsonValue> array_value;
//        std::vector<std::pair<std::string, JsonValue>> object_value;
//
//        const JsonValue* find(const std::string& key) const {
//            if (type != Type::Object) {
//                return nullptr;
//            }
//            for (const auto& kv : object_value) {
//                if (kv.first == key) {
//                    return &kv.second;
//                }
//            }
//            return nullptr;
//        }
//    };
//
//    class JsonParser final {
//    public:
//        explicit JsonParser(const std::string& text) : text_(text) {}
//
//        bool parse(JsonValue& out, std::string& error) {
//            pos_ = 0;
//            if (!parseValue(out, error)) {
//                return false;
//            }
//            skipWhitespace();
//            if (pos_ != text_.size()) {
//                error = "invalid trailing JSON content at position: " + std::to_string(pos_);
//                return false;
//            }
//            return true;
//        }
//
//    private:
//        const std::string& text_;
//        size_t pos_ = 0;
//
//        void skipWhitespace() {
//            while (pos_ < text_.size()) {
//                const char ch = text_[pos_];
//                if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
//                    ++pos_;
//                    continue;
//                }
//                break;
//            }
//        }
//
//        bool consume(char expected) {
//            if (pos_ < text_.size() && text_[pos_] == expected) {
//                ++pos_;
//                return true;
//            }
//            return false;
//        }
//
//        bool parseValue(JsonValue& out, std::string& error) {
//            skipWhitespace();
//            if (pos_ >= text_.size()) {
//                error = "unexpected end of JSON";
//                return false;
//            }
//
//            const char ch = text_[pos_];
//            if (ch == '{') {
//                return parseObject(out, error);
//            }
//            if (ch == '[') {
//                return parseArray(out, error);
//            }
//            if (ch == '"') {
//                out.type = JsonValue::Type::String;
//                return parseString(out.string_value, error);
//            }
//            if (ch == 't') {
//                return parseTrue(out, error);
//            }
//            if (ch == 'f') {
//                return parseFalse(out, error);
//            }
//            if (ch == 'n') {
//                return parseNull(out, error);
//            }
//            return parseNumber(out, error);
//        }
//
//        bool parseObject(JsonValue& out, std::string& error) {
//            if (!consume('{')) {
//                error = "failed to parse object: expected '{'";
//                return false;
//            }
//
//            out = JsonValue{};
//            out.type = JsonValue::Type::Object;
//
//            skipWhitespace();
//            if (consume('}')) {
//                return true;
//            }
//
//            while (true) {
//                skipWhitespace();
//                if (pos_ >= text_.size() || text_[pos_] != '"') {
//                    error = "object key must be string at position: " + std::to_string(pos_);
//                    return false;
//                }
//
//                std::string key;
//                if (!parseString(key, error)) {
//                    return false;
//                }
//
//                skipWhitespace();
//                if (!consume(':')) {
//                    error = "missing ':' after object key at position: " + std::to_string(pos_);
//                    return false;
//                }
//
//                JsonValue value;
//                if (!parseValue(value, error)) {
//                    return false;
//                }
//                out.object_value.emplace_back(std::move(key), std::move(value));
//
//                skipWhitespace();
//                if (consume('}')) {
//                    break;
//                }
//                if (!consume(',')) {
//                    error = "missing ',' or '}' after object value at position: " + std::to_string(pos_);
//                    return false;
//                }
//            }
//
//            return true;
//        }
//
//        bool parseArray(JsonValue& out, std::string& error) {
//            if (!consume('[')) {
//                error = "failed to parse array: expected '['";
//                return false;
//            }
//
//            out = JsonValue{};
//            out.type = JsonValue::Type::Array;
//
//            skipWhitespace();
//            if (consume(']')) {
//                return true;
//            }
//
//            while (true) {
//                JsonValue item;
//                if (!parseValue(item, error)) {
//                    return false;
//                }
//                out.array_value.push_back(std::move(item));
//
//                skipWhitespace();
//                if (consume(']')) {
//                    break;
//                }
//                if (!consume(',')) {
//                    error = "missing ',' or ']' after array item at position: " + std::to_string(pos_);
//                    return false;
//                }
//            }
//
//            return true;
//        }
//
//        bool parseString(std::string& out, std::string& error) {
//            if (!consume('"')) {
//                error = "failed to parse string: expected '\"'";
//                return false;
//            }
//
//            out.clear();
//            while (pos_ < text_.size()) {
//                char ch = text_[pos_++];
//                if (ch == '"') {
//                    return true;
//                }
//                if (ch == '\\') {
//                    if (pos_ >= text_.size()) {
//                        error = "incomplete string escape";
//                        return false;
//                    }
//                    const char esc = text_[pos_++];
//                    switch (esc) {
//                    case '"': out.push_back('"'); break;
//                    case '\\': out.push_back('\\'); break;
//                    case '/': out.push_back('/'); break;
//                    case 'b': out.push_back('\b'); break;
//                    case 'f': out.push_back('\f'); break;
//                    case 'n': out.push_back('\n'); break;
//                    case 'r': out.push_back('\r'); break;
//                    case 't': out.push_back('\t'); break;
//                    case 'u': {
//                        if (pos_ + 4 > text_.size()) {
//                            error = "incomplete unicode escape";
//                            return false;
//                        }
//                        unsigned code = 0;
//                        for (int i = 0; i < 4; ++i) {
//                            const char hex = text_[pos_++];
//                            code <<= 4;
//                            if (hex >= '0' && hex <= '9') {
//                                code += static_cast<unsigned>(hex - '0');
//                            }
//                            else if (hex >= 'a' && hex <= 'f') {
//                                code += static_cast<unsigned>(hex - 'a' + 10);
//                            }
//                            else if (hex >= 'A' && hex <= 'F') {
//                                code += static_cast<unsigned>(hex - 'A' + 10);
//                            }
//                            else {
//                                error = "invalid unicode escape";
//                                return false;
//                            }
//                        }
//                        out.push_back(code <= 0x7F ? static_cast<char>(code) : '?');
//                        break;
//                    }
//                    default:
//                        error = "unknown escape sequence";
//                        return false;
//                    }
//                    continue;
//                }
//                if (static_cast<unsigned char>(ch) < 0x20U) {
//                    error = "string contains control character";
//                    return false;
//                }
//                out.push_back(ch);
//            }
//
//            error = "unterminated string";
//            return false;
//        }
//
//        bool parseTrue(JsonValue& out, std::string& error) {
//            if (text_.compare(pos_, 4, "true") != 0) {
//                error = "invalid token while parsing true at position: " + std::to_string(pos_);
//                return false;
//            }
//            pos_ += 4;
//            out = JsonValue{};
//            out.type = JsonValue::Type::Bool;
//            out.bool_value = true;
//            return true;
//        }
//
//        bool parseFalse(JsonValue& out, std::string& error) {
//            if (text_.compare(pos_, 5, "false") != 0) {
//                error = "invalid token while parsing false at position: " + std::to_string(pos_);
//                return false;
//            }
//            pos_ += 5;
//            out = JsonValue{};
//            out.type = JsonValue::Type::Bool;
//            out.bool_value = false;
//            return true;
//        }
//
//        bool parseNull(JsonValue& out, std::string& error) {
//            if (text_.compare(pos_, 4, "null") != 0) {
//                error = "invalid token while parsing null at position: " + std::to_string(pos_);
//                return false;
//            }
//            pos_ += 4;
//            out = JsonValue{};
//            out.type = JsonValue::Type::Null;
//            return true;
//        }
//
//        bool parseNumber(JsonValue& out, std::string& error) {
//            const size_t start = pos_;
//
//            if (pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+')) {
//                ++pos_;
//            }
//            bool has_digit = false;
//            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
//                has_digit = true;
//                ++pos_;
//            }
//            if (pos_ < text_.size() && text_[pos_] == '.') {
//                ++pos_;
//                while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
//                    has_digit = true;
//                    ++pos_;
//                }
//            }
//            if (!has_digit) {
//                error = "invalid number format at position: " + std::to_string(start);
//                return false;
//            }
//            if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
//                ++pos_;
//                if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
//                    ++pos_;
//                }
//                bool exp_digit = false;
//                while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
//                    exp_digit = true;
//                    ++pos_;
//                }
//                if (!exp_digit) {
//                    error = "invalid exponent format at position: " + std::to_string(start);
//                    return false;
//                }
//            }
//
//            const std::string num_text = text_.substr(start, pos_ - start);
//            errno = 0;
//            char* end_ptr = nullptr;
//            const double value = std::strtod(num_text.c_str(), &end_ptr);
//            if (end_ptr == nullptr || *end_ptr != '\0' || errno == ERANGE) {
//                error = "number conversion failed at position: " + std::to_string(start);
//                return false;
//            }
//
//            out = JsonValue{};
//            out.type = JsonValue::Type::Number;
//            out.number_value = value;
//            return true;
//        }
//    };
//
//    struct Primitive {
//        enum class Type {
//            Line,
//            Polyline
//        };
//
//        Type type = Type::Line;
//        Point2D a{};
//        Point2D b{};
//        std::vector<Point2D> pts;
//        bool closed = false;
//        std::string layer = "0";
//    };
//
//    struct SymbolData {
//        std::string id;
//        std::vector<Primitive> primitives;
//    };
//
//    struct DxfLine {
//        Point2D a{};
//        Point2D b{};
//        std::string layer = "0";
//    };
//
//    bool readAllText(const std::string& path, std::string& content, std::string& error) {
//        std::ifstream in(path.c_str(), std::ios::binary);
//        if (!in) {
//            error = "cannot open json file: " + path;
//            return false;
//        }
//
//        std::ostringstream oss;
//        oss << in.rdbuf();
//        if (!in.good() && !in.eof()) {
//            error = "failed to read json file: " + path;
//            return false;
//        }
//
//        content = oss.str();
//        return true;
//    }
//
//    std::string sanitizeLayerName(std::string layer) {
//        if (layer.empty()) {
//            return "0";
//        }
//        for (char& ch : layer) {
//            const unsigned char c = static_cast<unsigned char>(ch);
//            const bool invalid =
//                ch == '<' || ch == '>' || ch == '/' || ch == '\\' ||
//                ch == '"' || ch == ':' || ch == ';' || ch == '?' ||
//                ch == '*' || ch == '|' || ch == ',' || ch == '=';
//            if (invalid || std::isspace(c) != 0) {
//                ch = '_';
//            }
//        }
//        return layer;
//    }
//
//    bool toPoint(const JsonValue& value, Point2D& out) {
//        if (value.type != JsonValue::Type::Array || value.array_value.size() < 2) {
//            return false;
//        }
//        const JsonValue& x = value.array_value[0];
//        const JsonValue& y = value.array_value[1];
//        if (x.type != JsonValue::Type::Number || y.type != JsonValue::Type::Number) {
//            return false;
//        }
//        out.x = x.number_value;
//        out.y = y.number_value;
//        return true;
//    }
//
//    std::string getStringOrDefault(const JsonValue* value, const std::string& fallback) {
//        if (value == nullptr || value->type != JsonValue::Type::String) {
//            return fallback;
//        }
//        return value->string_value;
//    }
//
//    bool getBoolOrDefault(const JsonValue* value, bool fallback) {
//        if (value == nullptr || value->type != JsonValue::Type::Bool) {
//            return fallback;
//        }
//        return value->bool_value;
//    }
//
//    std::vector<SymbolData> parseSymbols(const JsonValue& root, std::string& error) {
//        std::vector<SymbolData> out;
//
//        const JsonValue* symbols = root.find("symbols");
//        if (symbols == nullptr || symbols->type != JsonValue::Type::Array) {
//            error = "json does not contain a valid symbols array";
//            return out;
//        }
//
//        out.reserve(symbols->array_value.size());
//        for (size_t i = 0; i < symbols->array_value.size(); ++i) {
//            const JsonValue& symbol = symbols->array_value[i];
//            if (symbol.type != JsonValue::Type::Object) {
//                continue;
//            }
//
//            SymbolData item;
//            item.id = sanitizeLayerName(getStringOrDefault(symbol.find("id"), "symbol_" + std::to_string(i + 1)));
//
//            const JsonValue* symbol_style = symbol.find("style");
//            std::string symbol_layer = "0";
//            if (symbol_style != nullptr && symbol_style->type == JsonValue::Type::Object) {
//                symbol_layer = getStringOrDefault(symbol_style->find("layer"), symbol_layer);
//            }
//
//            const JsonValue* primitives = symbol.find("primitives");
//            if (primitives == nullptr || primitives->type != JsonValue::Type::Array) {
//                continue;
//            }
//
//            item.primitives.reserve(primitives->array_value.size());
//            for (const JsonValue& primitive : primitives->array_value) {
//                if (primitive.type != JsonValue::Type::Object) {
//                    continue;
//                }
//
//                const std::string type = mytool::toUpperAscii(getStringOrDefault(primitive.find("type"), ""));
//                std::string layer = symbol_layer;
//
//                const JsonValue* prim_style = primitive.find("style");
//                if (prim_style != nullptr && prim_style->type == JsonValue::Type::Object) {
//                    layer = getStringOrDefault(prim_style->find("layer"), layer);
//                }
//                layer = sanitizeLayerName(layer);
//
//                if (type == "LINE") {
//                    const JsonValue* a = primitive.find("a");
//                    const JsonValue* b = primitive.find("b");
//                    if (a == nullptr || b == nullptr) {
//                        continue;
//                    }
//
//                    Primitive p;
//                    p.type = Primitive::Type::Line;
//                    p.layer = layer;
//                    if (!toPoint(*a, p.a) || !toPoint(*b, p.b)) {
//                        continue;
//                    }
//                    item.primitives.push_back(std::move(p));
//                    continue;
//                }
//
//                if (type == "LWPOLYLINE" || type == "POLYLINE") {
//                    const JsonValue* pts = primitive.find("pts");
//                    if (pts == nullptr || pts->type != JsonValue::Type::Array || pts->array_value.size() < 2) {
//                        continue;
//                    }
//
//                    Primitive p;
//                    p.type = Primitive::Type::Polyline;
//                    p.layer = layer;
//                    p.closed = getBoolOrDefault(primitive.find("closed"), false);
//                    p.pts.reserve(pts->array_value.size());
//
//                    for (const JsonValue& one : pts->array_value) {
//                        Point2D point;
//                        if (toPoint(one, point)) {
//                            p.pts.push_back(point);
//                        }
//                    }
//
//                    if (p.pts.size() >= 2) {
//                        item.primitives.push_back(std::move(p));
//                    }
//                }
//            }
//
//            if (!item.primitives.empty()) {
//                out.push_back(std::move(item));
//            }
//        }
//
//        if (out.empty()) {
//            error = "symbols contains no available primitives";
//        }
//        return out;
//    }
//
//    BBox computeBBox(const SymbolData& symbol) {
//        double min_x = std::numeric_limits<double>::infinity();
//        double min_y = std::numeric_limits<double>::infinity();
//        double max_x = -std::numeric_limits<double>::infinity();
//        double max_y = -std::numeric_limits<double>::infinity();
//        bool valid = false;
//
//        auto include = [&](const Point2D& p) {
//            min_x = std::min(min_x, p.x);
//            min_y = std::min(min_y, p.y);
//            max_x = std::max(max_x, p.x);
//            max_y = std::max(max_y, p.y);
//            valid = true;
//        };
//
//        for (const Primitive& primitive : symbol.primitives) {
//            if (primitive.type == Primitive::Type::Line) {
//                include(primitive.a);
//                include(primitive.b);
//                continue;
//            }
//            for (const Point2D& p : primitive.pts) {
//                include(p);
//            }
//        }
//
//        if (!valid) {
//            return BBox{};
//        }
//        return BBox{ min_x, min_y, max_x, max_y };
//    }
//
//    Point2D scalePoint(const Point2D& src, const BBox& bbox, double scale, double offset_x, double offset_y) {
//        return Point2D{
//            (src.x - bbox.min_x) * scale + offset_x,
//            (src.y - bbox.min_y) * scale + offset_y
//        };
//    }
//
//    void appendScaledSymbol(
//        const SymbolData& symbol,
//        const BBox& bbox,
//        double scale,
//        const std::string& scale_tag,
//        double offset_x,
//        double offset_y,
//        std::vector<DxfLine>& out_lines) {
//
//        for (const Primitive& primitive : symbol.primitives) {
//            const std::string layer = sanitizeLayerName(symbol.id + "_" + scale_tag + "_" + primitive.layer);
//
//            if (primitive.type == Primitive::Type::Line) {
//                const Point2D a = scalePoint(primitive.a, bbox, scale, offset_x, offset_y);
//                const Point2D b = scalePoint(primitive.b, bbox, scale, offset_x, offset_y);
//                out_lines.push_back(DxfLine{ a, b, layer });
//                continue;
//            }
//
//            if (primitive.pts.size() < 2) {
//                continue;
//            }
//
//            for (size_t i = 1; i < primitive.pts.size(); ++i) {
//                const Point2D a = scalePoint(primitive.pts[i - 1], bbox, scale, offset_x, offset_y);
//                const Point2D b = scalePoint(primitive.pts[i], bbox, scale, offset_x, offset_y);
//                out_lines.push_back(DxfLine{ a, b, layer });
//            }
//
//            if (primitive.closed && primitive.pts.size() >= 3) {
//                const Point2D a = scalePoint(primitive.pts.back(), bbox, scale, offset_x, offset_y);
//                const Point2D b = scalePoint(primitive.pts.front(), bbox, scale, offset_x, offset_y);
//                out_lines.push_back(DxfLine{ a, b, layer });
//            }
//        }
//    }
//
//    bool writeLineEntity(std::ostream& out, const DxfLine& line) {
//        out << "0\nLINE\n";
//        out << "8\n" << line.layer << "\n";
//        out << "10\n" << mytool::formatNumber(line.a.x) << "\n";
//        out << "20\n" << mytool::formatNumber(line.a.y) << "\n";
//        out << "30\n0\n";
//        out << "11\n" << mytool::formatNumber(line.b.x) << "\n";
//        out << "21\n" << mytool::formatNumber(line.b.y) << "\n";
//        out << "31\n0\n";
//        return out.good();
//    }
//
//    ConvertResult writeDxf(const std::string& output_dxf, const std::vector<DxfLine>& lines) {
//        std::ofstream out(output_dxf.c_str(), std::ios::binary);
//        if (!out) {
//            return ConvertResult::failure(6, "cannot create output dxf: " + output_dxf);
//        }
//
//        std::set<std::string> layers;
//        layers.insert("0");
//        for (const DxfLine& line : lines) {
//            layers.insert(line.layer.empty() ? "0" : line.layer);
//        }
//
//        // Use a conservative R12-like layout for broader CAD compatibility.
//        out << "0\nSECTION\n2\nHEADER\n";
//        out << "9\n$ACADVER\n1\nAC1009\n";
//        out << "9\n$INSBASE\n10\n0\n20\n0\n30\n0\n";
//        out << "0\nENDSEC\n";
//
//        out << "0\nSECTION\n2\nTABLES\n";
//        out << "0\nTABLE\n2\nLTYPE\n70\n1\n";
//        out << "0\nLTYPE\n2\nCONTINUOUS\n70\n0\n3\nSolid line\n72\n65\n73\n0\n40\n0\n";
//        out << "0\nENDTAB\n";
//
//        out << "0\nTABLE\n2\nLAYER\n70\n" << layers.size() << "\n";
//        for (const std::string& layer : layers) {
//            out << "0\nLAYER\n";
//            out << "2\n" << layer << "\n";
//            out << "70\n0\n";
//            out << "62\n7\n";
//            out << "6\nCONTINUOUS\n";
//        }
//        out << "0\nENDTAB\n";
//        out << "0\nENDSEC\n";
//
//        out << "0\nSECTION\n2\nBLOCKS\n";
//        out << "0\nENDSEC\n";
//
//        out << "0\nSECTION\n2\nENTITIES\n";
//        for (const DxfLine& line : lines) {
//            if (!writeLineEntity(out, line)) {
//                return ConvertResult::failure(7, "failed to write line entity: " + output_dxf);
//            }
//        }
//        out << "0\nENDSEC\n0\nEOF\n";
//
//        if (!out.good()) {
//            return ConvertResult::failure(8, "failed to finish dxf writing: " + output_dxf);
//        }
//        return ConvertResult::success();
//    }
//
//} // namespace
//
//ConvertResult JsonScalePerSymbolTest::run(const std::string& input_json, const std::string& output_dxf) {
//    std::string json_text;
//    std::string io_error;
//    if (!readAllText(input_json, json_text, io_error)) {
//        return ConvertResult::failure(1, io_error);
//    }
//
//    JsonValue root;
//    JsonParser parser(json_text);
//    std::string parse_error;
//    if (!parser.parse(root, parse_error)) {
//        return ConvertResult::failure(2, "json parse failed: " + parse_error);
//    }
//    if (root.type != JsonValue::Type::Object) {
//        return ConvertResult::failure(3, "json root must be an object");
//    }
//
//    std::string symbol_error;
//    const std::vector<SymbolData> symbols = parseSymbols(root, symbol_error);
//    if (symbols.empty()) {
//        return ConvertResult::failure(4, symbol_error);
//    }
//
//    // "scale up/down by 1x" here means 2.0x / 1.0x / 0.5x.
//    constexpr double kScale2x = 2.0;
//    constexpr double kScale1x = 1.0;
//    constexpr double kScaleHalf = 0.5;
//    const double scales[] = { kScale2x, kScale1x, kScaleHalf };
//    const std::string scale_tags[] = { "UP_1X", "ORIGINAL", "DOWN_1X" };
//
//    std::vector<DxfLine> lines;
//    double cursor_y = 0.0;
//    constexpr double col_gap = 20.0;
//    constexpr double row_gap = 20.0;
//
//    for (const SymbolData& symbol : symbols) {
//        const BBox bbox = computeBBox(symbol);
//        const double width = std::max(1.0, bbox.max_x - bbox.min_x);
//        const double height = std::max(1.0, bbox.max_y - bbox.min_y);
//
//        double cursor_x = 0.0;
//        for (size_t i = 0; i < 3; ++i) {
//            appendScaledSymbol(symbol, bbox, scales[i], scale_tags[i], cursor_x, cursor_y, lines);
//            cursor_x += width * scales[i] + col_gap;
//        }
//
//        cursor_y += height * kScale2x + row_gap;
//    }
//
//    if (lines.empty()) {
//        return ConvertResult::failure(5, "no line geometry generated");
//    }
//
//    return writeDxf(output_dxf, lines);
//}
//
//#ifdef JSON_SCALE_PER_SYMBOL_TEST_STANDALONE
//#include <iostream>
//
//int main(int argc, char* argv[]) {
//    const std::string input = (argc > 1 && argv[1] != nullptr)
//        ? argv[1]
//        : R"(D:\vs2022 code\draw_cad\symbols.json)";
//
//    const std::string output = (argc > 2 && argv[2] != nullptr)
//        ? argv[2]
//        : R"(D:\vs2022 code\draw_cad\scaled_symbols_from_json_test.dxf)";
//
//    const ConvertResult result = JsonScalePerSymbolTest::run(input, output);
//    if (!result.ok()) {
//        std::cerr << "[ERROR] code=" << result.code << ", message=" << result.message << "\n";
//        return result.code;
//    }
//
//    std::cout << "[OK] " << result.message << "\n";
//    return 0;
//}
//#endif
