#include "BoardLengthJsonExporter.h"

#include "../OpenXLSX/include/OpenXLSX.hpp"
#include "../nlohmann/json.hpp"
#include "mytool.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>

using nlohmann::json;

namespace {

struct BoardLengthRow {
    int source_row = 0;
    std::string column_group;
    std::string slab_no;
    std::string length_raw;
    double length_m = 0.0;
    bool has_numeric_length = false;
    double slab_sort_value = 0.0;
    bool has_numeric_slab = false;
};

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

bool parseDoubleStrict(const std::string& text, double& out_value) {
    const std::string normalized = normalizeText(text);
    if (normalized.empty()) {
        return false;
    }

    char* end = nullptr;
    const double value = std::strtod(normalized.c_str(), &end);
    if (end == normalized.c_str()) {
        return false;
    }

    while (end != nullptr && *end != '\0') {
        if (std::isspace(static_cast<unsigned char>(*end)) == 0) {
            return false;
        }
        ++end;
    }

    out_value = value;
    return true;
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

bool isEmptyGroup(const std::string& slab_no, const std::string& length_raw) {
    return slab_no.empty() && length_raw.empty();
}

void appendBoardLengthRow(
    std::vector<BoardLengthRow>& rows,
    std::vector<std::string>& warnings,
    uint32_t row_index,
    std::string column_group,
    std::string slab_no,
    std::string length_raw) {
    if (slab_no.empty()) {
        warnings.emplace_back(
            "Row " + std::to_string(row_index) + " group " + column_group
            + " has length but no slab number; skipped.");
        return;
    }

    double length_m = 0.0;
    if (length_raw.empty()) {
        warnings.emplace_back(
            "Row " + std::to_string(row_index) + " group " + column_group
            + " has slab number but no length; skipped.");
        return;
    }
    if (!parseDoubleStrict(length_raw, length_m)) {
        warnings.emplace_back(
            "Row " + std::to_string(row_index) + " group " + column_group
            + " has a non-numeric length and was skipped: " + length_raw);
        return;
    }

    BoardLengthRow row;
    row.source_row = static_cast<int>(row_index);
    row.column_group = std::move(column_group);
    row.slab_no = std::move(slab_no);
    row.length_raw = std::move(length_raw);
    row.length_m = length_m;
    row.has_numeric_length = true;
    row.has_numeric_slab = parseDoubleStrict(row.slab_no, row.slab_sort_value);

    rows.push_back(std::move(row));
}

json makeOutputJson(
    const std::string& input_xlsx,
    const std::string& sheet_name,
    uint32_t header_row,
    const std::vector<BoardLengthRow>& rows) {
    json root;
    root["source"] = {
        { "file", input_xlsx },
        { "sheet", sheet_name },
        { "header_row", header_row },
        { "length_unit", "m" },
        { "columns", {
            { "A", "slab_no" },
            { "B", "length_m" },
            { "D", "slab_no" },
            { "E", "length_m" }
        } }
    };
    root["summary"] = {
        { "exported_count", rows.size() }
    };

    root["records"] = json::array();
    for (const BoardLengthRow& row : rows) {
        json item = {
            { "source_row", row.source_row },
            { "column_group", row.column_group },
            { "slab_no", row.slab_no },
            { "length", mytool::formatNumber(row.length_m) + "m" },
            { "length_raw", row.length_raw }
        };
        item["length_m"] = row.length_m;
        root["records"].push_back(std::move(item));
    }

    return root;
}

} // namespace

BoardLengthJsonExporter::ExportResult BoardLengthJsonExporter::run(
    const std::string& input_xlsx,
    const std::string& output_json,
    const Options& options) {
    if (input_xlsx.empty()) {
        return ExportResult::failure(51, "xlsx path must not be empty.");
    }
    if (output_json.empty()) {
        return ExportResult::failure(52, "output json path must not be empty.");
    }
    if (options.header_row == 0) {
        return ExportResult::failure(53, "header_row must be 1-based.");
    }

    try {
        OpenXLSX::XLDocument document;
        document.open(input_xlsx);

        auto workbook = document.workbook();
        const auto worksheet_names = workbook.worksheetNames();
        if (worksheet_names.empty()) {
            document.close();
            return ExportResult::failure(54, "No readable worksheet was found in the workbook.");
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
            return ExportResult::failure(55, message);
        }

        auto worksheet = workbook.worksheet(target_sheet_name);
        const uint32_t row_count = worksheet.rowCount();
        const uint16_t column_count = worksheet.columnCount();
        if (row_count <= options.header_row) {
            document.close();
            return ExportResult::failure(
                56,
                "No data rows were found. header_row=" + std::to_string(options.header_row)
                + ", row_count=" + std::to_string(row_count) + ".");
        }
        if (column_count < 5) {
            document.close();
            return ExportResult::failure(57, "The worksheet must contain at least columns A-E.");
        }

        std::vector<BoardLengthRow> rows;
        std::vector<std::string> warnings;
        std::set<std::string> seen_slab_numbers;
        std::set<std::string> duplicate_slab_numbers;

        for (uint32_t row_index = options.header_row + 1; row_index <= row_count; ++row_index) {
            const std::string left_slab_no = cellText(worksheet, row_index, 1);
            const std::string left_length = cellText(worksheet, row_index, 2);
            const std::string right_slab_no = cellText(worksheet, row_index, 4);
            const std::string right_length = cellText(worksheet, row_index, 5);

            if (!(options.skip_empty_rows && isEmptyGroup(left_slab_no, left_length))) {
                appendBoardLengthRow(rows, warnings, row_index, "A:B", left_slab_no, left_length);
            }
            if (!(options.skip_empty_rows && isEmptyGroup(right_slab_no, right_length))) {
                appendBoardLengthRow(rows, warnings, row_index, "D:E", right_slab_no, right_length);
            }
        }

        for (const BoardLengthRow& row : rows) {
            if (!seen_slab_numbers.insert(row.slab_no).second) {
                duplicate_slab_numbers.insert(row.slab_no);
            }
        }
        for (const std::string& slab_no : duplicate_slab_numbers) {
            warnings.emplace_back("Duplicate slab number found: " + slab_no);
        }

        std::stable_sort(rows.begin(), rows.end(), [](const BoardLengthRow& lhs, const BoardLengthRow& rhs) {
            if (lhs.has_numeric_slab != rhs.has_numeric_slab) {
                return lhs.has_numeric_slab;
            }
            if (lhs.has_numeric_slab && rhs.has_numeric_slab && lhs.slab_sort_value != rhs.slab_sort_value) {
                return lhs.slab_sort_value < rhs.slab_sort_value;
            }
            return lhs.slab_no < rhs.slab_no;
        });

        if (rows.empty()) {
            warnings.emplace_back("No board length rows were exported.");
        }

        std::ofstream out(output_json.c_str(), std::ios::binary);
        if (!out) {
            document.close();
            return ExportResult::failure(58, "Failed to open output json: " + output_json);
        }

        const json root = makeOutputJson(input_xlsx, target_sheet_name, options.header_row, rows);
        out << root.dump(2) << "\n";
        if (!out.good()) {
            document.close();
            return ExportResult::failure(59, "Failed while writing output json: " + output_json);
        }

        document.close();
        return ExportResult::success(rows.size(), std::move(warnings));
    }
    catch (const std::exception& ex) {
        return ExportResult::failure(60, std::string("Failed to export board length json: ") + ex.what());
    }
}
