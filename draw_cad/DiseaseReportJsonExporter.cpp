#include "DiseaseReportJsonExporter.h"

#include "../OpenXLSX/include/OpenXLSX.hpp"
#include "../nlohmann/json.hpp"
#include "mytool.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string_view>
#include <utility>

using nlohmann::json;

namespace {

struct ReportRow {
    int source_row = 0;
    std::string slab_no;
    std::string project_name;
    std::string defect_location;
    std::string check_item;
    std::string defect_desc;
    std::string judgement;
    bool judgement_is_zero = false;
    std::string distance_from_slab_end;
    std::string length;
    std::string width;
    std::string area;
};

std::string utf8Literal(const char8_t* text) {
    return std::string(
        reinterpret_cast<const char*>(text),
        std::char_traits<char8_t>::length(text));
}

std::string trimAsciiWhitespace(std::string text) {
    auto not_space = [](unsigned char ch) {
        return std::isspace(ch) == 0;
    };

    const auto begin = std::find_if(
        text.begin(),
        text.end(),
        [&](char ch) { return not_space(static_cast<unsigned char>(ch)); });
    if (begin == text.end()) {
        return "";
    }

    const auto end = std::find_if(
        text.rbegin(),
        text.rend(),
        [&](char ch) { return not_space(static_cast<unsigned char>(ch)); }).base();

    return std::string(begin, end);
}

std::string stripUtf8Bom(std::string text) {
    constexpr unsigned char bom0 = 0xEF;
    constexpr unsigned char bom1 = 0xBB;
    constexpr unsigned char bom2 = 0xBF;
    if (text.size() >= 3
        && static_cast<unsigned char>(text[0]) == bom0
        && static_cast<unsigned char>(text[1]) == bom1
        && static_cast<unsigned char>(text[2]) == bom2) {
        text.erase(0, 3);
    }
    return text;
}

std::string normalizeText(std::string text) {
    return trimAsciiWhitespace(stripUtf8Bom(std::move(text)));
}

std::string formatExcelDate(double serial) {
    if (serial <= 0.0) {
        return mytool::formatNumber(serial);
    }

    try {
        const OpenXLSX::XLDateTime date_time(serial);
        const std::tm tm = date_time.tm();
        const bool has_time = (tm.tm_hour != 0 || tm.tm_min != 0 || tm.tm_sec != 0);
        char buffer[32] = {};
        const char* format = has_time ? "%Y-%m-%d %H:%M:%S" : "%Y-%m-%d";
        if (std::strftime(buffer, sizeof(buffer), format, &tm) != 0) {
            return buffer;
        }
    }
    catch (...) {
    }

    return mytool::formatNumber(serial);
}

std::string cellToString(const OpenXLSX::XLCellValue& value, bool prefer_excel_date = false) {
    std::string text;
    switch (value.type()) {
    case OpenXLSX::XLValueType::Empty:
        text.clear();
        break;
    case OpenXLSX::XLValueType::Boolean:
        text = value.get<bool>() ? "true" : "false";
        break;
    case OpenXLSX::XLValueType::Integer:
        text = prefer_excel_date
            ? formatExcelDate(static_cast<double>(value.get<int64_t>()))
            : std::to_string(value.get<int64_t>());
        break;
    case OpenXLSX::XLValueType::Float:
        text = prefer_excel_date
            ? formatExcelDate(value.get<double>())
            : mytool::formatNumber(value.get<double>());
        break;
    case OpenXLSX::XLValueType::Error:
    case OpenXLSX::XLValueType::String:
        text = value.get<std::string>();
        break;
    default:
        text.clear();
        break;
    }

    return normalizeText(std::move(text));
}

std::string cellText(OpenXLSX::XLWorksheet& worksheet, uint32_t row, uint16_t column) {
    return cellToString(static_cast<OpenXLSX::XLCellValue>(worksheet.cell(row, column).value()));
}

bool isZeroJudgement(std::string text) {
    text = normalizeText(std::move(text));
    if (text.empty()) {
        return false;
    }

    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str()) {
        return false;
    }

    while (end != nullptr && *end != '\0') {
        if (std::isspace(static_cast<unsigned char>(*end)) == 0) {
            return false;
        }
        ++end;
    }

    return std::fabs(value) < 1e-12;
}

std::string extractMarkedValue(const std::string& text, const std::string& marker) {
    const size_t marker_pos = text.find(marker);
    if (marker_pos == std::string::npos) {
        return "";
    }

    const size_t value_start = marker_pos + marker.size();
    size_t value_end = text.size();
    const std::string comma_cn = utf8Literal(u8"\uFF0C");
    const std::string semicolon_cn = utf8Literal(u8"\uFF1B");
    const std::string period_cn = utf8Literal(u8"\u3002");
    const std::string delimiters[] = { comma_cn, ",", semicolon_cn, ";", period_cn, "\r", "\n" };

    for (const std::string& delimiter : delimiters) {
        const size_t delimiter_pos = text.find(delimiter, value_start);
        if (delimiter_pos != std::string::npos) {
            value_end = std::min(value_end, delimiter_pos);
        }
    }

    return normalizeText(text.substr(value_start, value_end - value_start));
}

bool isEmptyReportRow(
    const std::string& slab_no,
    const std::string& defect_location,
    const std::string& check_item,
    const std::string& defect_desc,
    const std::string& judgement) {
    return slab_no.empty()
        && defect_location.empty()
        && check_item.empty()
        && defect_desc.empty()
        && judgement.empty();
}

std::string joinStrings(const std::vector<std::string>& items, std::string_view separator) {
    std::ostringstream oss;
    for (size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            oss << separator;
        }
        oss << items[index];
    }
    return oss.str();
}

json makeOutputJson(
    const std::string& input_xlsx,
    const std::string& sheet_name,
    uint32_t header_row,
    const std::string& project_name_filter,
    size_t skipped_project_count,
    size_t zero_judgement_count,
    const std::vector<ReportRow>& rows) {
    json root;
    root["source"] = {
        { "file", input_xlsx },
        { "sheet", sheet_name },
        { "header_row", header_row }
    };
    root["summary"] = {
        { "exported_count", rows.size() },
        { "project_name_filter", project_name_filter },
        { "skipped_project_count", skipped_project_count },
        { "zero_judgement_count", zero_judgement_count }
    };

    root["records"] = json::array();
    for (const ReportRow& row : rows) {
        root["records"].push_back({
            { "source_row", row.source_row },
            { "slab_no", row.slab_no },
            { "project_name", row.project_name },
            { "defect_location", row.defect_location },
            { "check_item", row.check_item },
            { "defect_desc", row.defect_desc },
            { "judgement", row.judgement },
            { "judgement_is_zero", row.judgement_is_zero },
            { "parsed", {
                { "distance_from_slab_end", row.distance_from_slab_end },
                { "length", row.length },
                { "width", row.width },
                { "area", row.area }
            } }
        });
    }

    return root;
}

} // namespace

DiseaseReportJsonExporter::ExportResult DiseaseReportJsonExporter::run(
    const std::string& input_xlsx,
    const std::string& output_json,
    const Options& options) {
    if (input_xlsx.empty()) {
        return ExportResult::failure(31, "xlsx path must not be empty.");
    }
    if (output_json.empty()) {
        return ExportResult::failure(32, "output json path must not be empty.");
    }
    if (options.header_row == 0) {
        return ExportResult::failure(33, "header_row must be 1-based.");
    }

    try {
        OpenXLSX::XLDocument document;
        document.open(input_xlsx);

        auto workbook = document.workbook();
        const auto worksheet_names = workbook.worksheetNames();
        if (worksheet_names.empty()) {
            document.close();
            return ExportResult::failure(34, "No readable worksheet was found in the workbook.");
        }

        std::string target_sheet_name = options.sheet_name;
        if (target_sheet_name.empty()) {
            target_sheet_name = worksheet_names.front();
        }
        else if (!workbook.worksheetExists(target_sheet_name)) {
            const std::string message =
                "Worksheet \"" + target_sheet_name + "\" was not found. Available worksheets: "
                + joinStrings(worksheet_names, ", ");
            document.close();
            return ExportResult::failure(35, message);
        }

        auto worksheet = workbook.worksheet(target_sheet_name);
        const uint32_t row_count = worksheet.rowCount();
        const uint16_t column_count = worksheet.columnCount();
        if (row_count <= options.header_row) {
            document.close();
            return ExportResult::failure(
                36,
                "No data rows were found. header_row=" + std::to_string(options.header_row)
                + ", row_count=" + std::to_string(row_count) + ".");
        }
        if (column_count < 7) {
            document.close();
            return ExportResult::failure(37, "The worksheet must contain at least columns A-G.");
        }

        std::vector<ReportRow> rows;
        size_t skipped_project_count = 0;
        size_t zero_judgement_count = 0;
        const std::string project_name_filter = options.project_name_filter.empty()
            ? utf8Literal(u8"\u886C\u780C")
            : normalizeText(options.project_name_filter);
        const std::string marker_distance = utf8Literal(u8"\u8DDD\u677F\u7AEF");
        const std::string marker_length = utf8Literal(u8"\u957F\u5EA6\u4E3A");
        const std::string marker_width = utf8Literal(u8"\u5BBD\u5EA6\u4E3A");
        const std::string marker_area = utf8Literal(u8"\u9762\u79EF\u4E3A");

        for (uint32_t row_index = options.header_row + 1; row_index <= row_count; ++row_index) {
            const std::string slab_no = cellText(worksheet, row_index, 2);
            const std::string project_name = cellText(worksheet, row_index, 3);
            const std::string defect_location = cellText(worksheet, row_index, 4);
            const std::string check_item = cellText(worksheet, row_index, 5);
            const std::string defect_desc = cellText(worksheet, row_index, 6);
            const std::string judgement = cellText(worksheet, row_index, 7);

            if (options.skip_empty_rows && isEmptyReportRow(slab_no, defect_location, check_item, defect_desc, judgement)) {
                continue;
            }

            if (!project_name_filter.empty() && project_name != project_name_filter) {
                ++skipped_project_count;
                continue;
            }

            const bool judgement_is_zero = isZeroJudgement(judgement);
            if (judgement_is_zero) {
                ++zero_judgement_count;
            }

            ReportRow row;
            row.source_row = static_cast<int>(row_index);
            row.slab_no = slab_no;
            row.project_name = project_name;
            row.defect_location = defect_location;
            row.check_item = check_item;
            row.defect_desc = defect_desc;
            row.judgement = judgement;
            row.judgement_is_zero = judgement_is_zero;
            row.distance_from_slab_end = extractMarkedValue(defect_desc, marker_distance);
            row.length = extractMarkedValue(defect_desc, marker_length);
            row.width = extractMarkedValue(defect_desc, marker_width);
            row.area = extractMarkedValue(defect_desc, marker_area);
            rows.push_back(std::move(row));
        }

        std::vector<std::string> warnings;
        if (rows.empty()) {
            warnings.emplace_back("No disease rows were exported.");
        }

        std::ofstream out(output_json.c_str(), std::ios::binary);
        if (!out) {
            document.close();
            return ExportResult::failure(38, "Failed to open output json: " + output_json);
        }

        const json root = makeOutputJson(
            input_xlsx,
            target_sheet_name,
            options.header_row,
            project_name_filter,
            skipped_project_count,
            zero_judgement_count,
            rows);
        out << root.dump(2) << "\n";
        if (!out.good()) {
            document.close();
            return ExportResult::failure(39, "Failed while writing output json: " + output_json);
        }

        document.close();
        return ExportResult::success(rows.size(), zero_judgement_count, std::move(warnings));
    }
    catch (const std::exception& ex) {
        return ExportResult::failure(40, std::string("Failed to export disease report json: ") + ex.what());
    }
}
