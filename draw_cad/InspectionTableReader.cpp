#include "InspectionTableReader.h"

#include "mytool.h"
#include "../OpenXLSX/include/OpenXLSX.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {

struct ResolvedColumns {
    uint16_t stake_no = 0;
    uint16_t slab_no = 0;
    uint16_t project_name = 0;
    uint16_t defect_location = 0;
    uint16_t check_item = 0;
    uint16_t defect_desc = 0;
    uint16_t judgement = 0;
    uint16_t inspect_date = 0;
};

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

std::string normalizeText(std::string text, bool trim_text) {
    text = stripUtf8Bom(std::move(text));
    if (trim_text) {
        text = trimAsciiWhitespace(std::move(text));
    }
    return text;
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

std::string cellToString(const OpenXLSX::XLCellValue& value, bool prefer_excel_date, bool trim_text) {
    std::string text;
    switch (value.type()) {
    case OpenXLSX::XLValueType::Empty:
        text.clear();
        break;
    case OpenXLSX::XLValueType::Boolean:
        text = value.get<bool>() ? "true" : "false";
        break;
    case OpenXLSX::XLValueType::Integer:
        if (prefer_excel_date) {
            text = formatExcelDate(static_cast<double>(value.get<int64_t>()));
        }
        else {
            text = std::to_string(value.get<int64_t>());
        }
        break;
    case OpenXLSX::XLValueType::Float:
        if (prefer_excel_date) {
            text = formatExcelDate(value.get<double>());
        }
        else {
            text = mytool::formatNumber(value.get<double>());
        }
        break;
    case OpenXLSX::XLValueType::Error:
    case OpenXLSX::XLValueType::String:
        text = value.get<std::string>();
        break;
    default:
        text.clear();
        break;
    }

    return normalizeText(std::move(text), trim_text);
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

bool isEmptyRow(const InspectionRow& row) {
    return row.stake_no.empty()
        && row.slab_no_raw.empty()
        && row.project_name.empty()
        && row.defect_location_raw.empty()
        && row.check_item_raw.empty()
        && row.defect_desc_raw.empty()
        && row.judgement_raw.empty()
        && row.inspect_date_raw.empty();
}

bool resolveColumn(
    const std::unordered_map<std::string, uint16_t>& header_to_column,
    const std::string& expected_header,
    uint16_t& output_column) {
    const std::string normalized_header = normalizeText(expected_header, true);
    const auto found = header_to_column.find(normalized_header);
    if (found == header_to_column.end()) {
        return false;
    }

    output_column = found->second;
    return true;
}

} // namespace

InspectionTableReader::InspectionTableReader(Options options)
    : options_(std::move(options)) {
}

InspectionTableReader::ReadResult InspectionTableReader::read(const std::string& file_path) const {
    ReadResult result;

    if (file_path.empty()) {
        result.errors.emplace_back("xlsx path must not be empty.");
        return result;
    }

    if (options_.header_row == 0) {
        result.errors.emplace_back("header_row must be 1-based.");
        return result;
    }

    try {
        OpenXLSX::XLDocument document;
        document.open(file_path);

        auto workbook = document.workbook();
        const auto worksheet_names = workbook.worksheetNames();
        if (worksheet_names.empty()) {
            result.errors.emplace_back("No readable worksheet was found in the workbook.");
            document.close();
            return result;
        }

        std::string target_sheet_name = options_.sheet_name;
        if (target_sheet_name.empty()) {
            target_sheet_name = worksheet_names.front();
        }
        else if (!workbook.worksheetExists(target_sheet_name)) {
            result.errors.emplace_back(
                "Worksheet \"" + target_sheet_name + "\" was not found. Available worksheets: " + joinStrings(worksheet_names, ", "));
            document.close();
            return result;
        }

        auto worksheet = workbook.worksheet(target_sheet_name);
        const uint32_t row_count = worksheet.rowCount();
        const uint16_t column_count = worksheet.columnCount();
        if (row_count < options_.header_row) {
            result.errors.emplace_back(
                "header_row is out of range. header_row=" + std::to_string(options_.header_row)
                + ", row_count=" + std::to_string(row_count) + ".");
            document.close();
            return result;
        }
        if (column_count == 0) {
            result.errors.emplace_back("The worksheet has no readable columns.");
            document.close();
            return result;
        }

        std::unordered_map<std::string, uint16_t> header_to_column;
        std::vector<std::string> duplicate_headers;
        for (uint16_t column = 1; column <= column_count; ++column) {
            const auto header_value = static_cast<OpenXLSX::XLCellValue>(worksheet.cell(options_.header_row, column).value());
            const std::string header = normalizeText(cellToString(header_value, false, false), true);
            if (header.empty()) {
                continue;
            }

            const auto [_, inserted] = header_to_column.emplace(header, column);
            if (!inserted) {
                duplicate_headers.push_back(header);
            }
        }

        if (!duplicate_headers.empty()) {
            result.warnings.emplace_back(
                "Duplicate headers were found. The first matching column will be used: " + joinStrings(duplicate_headers, ", "));
        }

        ResolvedColumns columns;
        std::vector<std::string> missing_headers;
        if (!resolveColumn(header_to_column, options_.columns.stake_no, columns.stake_no)) {
            missing_headers.push_back(options_.columns.stake_no);
        }
        if (!resolveColumn(header_to_column, options_.columns.slab_no, columns.slab_no)) {
            missing_headers.push_back(options_.columns.slab_no);
        }
        if (!resolveColumn(header_to_column, options_.columns.project_name, columns.project_name)) {
            missing_headers.push_back(options_.columns.project_name);
        }
        if (!resolveColumn(header_to_column, options_.columns.defect_location, columns.defect_location)) {
            missing_headers.push_back(options_.columns.defect_location);
        }
        if (!resolveColumn(header_to_column, options_.columns.check_item, columns.check_item)) {
            missing_headers.push_back(options_.columns.check_item);
        }
        if (!resolveColumn(header_to_column, options_.columns.defect_desc, columns.defect_desc)) {
            missing_headers.push_back(options_.columns.defect_desc);
        }
        if (!resolveColumn(header_to_column, options_.columns.judgement, columns.judgement)) {
            missing_headers.push_back(options_.columns.judgement);
        }
        if (!resolveColumn(header_to_column, options_.columns.inspect_date, columns.inspect_date)) {
            missing_headers.push_back(options_.columns.inspect_date);
        }

        if (!missing_headers.empty()) {
            result.errors.emplace_back(
                "Missing required headers: " + joinStrings(missing_headers, ", "));
            document.close();
            return result;
        }

        for (uint32_t row = options_.header_row + 1; row <= row_count; ++row) {
            InspectionRow inspection_row;
            inspection_row.source_row = static_cast<int>(row);

            inspection_row.stake_no = cellToString(
                static_cast<OpenXLSX::XLCellValue>(worksheet.cell(row, columns.stake_no).value()),
                false,
                options_.trim_text);
            inspection_row.slab_no_raw = cellToString(
                static_cast<OpenXLSX::XLCellValue>(worksheet.cell(row, columns.slab_no).value()),
                false,
                options_.trim_text);
            inspection_row.project_name = cellToString(
                static_cast<OpenXLSX::XLCellValue>(worksheet.cell(row, columns.project_name).value()),
                false,
                options_.trim_text);
            inspection_row.defect_location_raw = cellToString(
                static_cast<OpenXLSX::XLCellValue>(worksheet.cell(row, columns.defect_location).value()),
                false,
                options_.trim_text);
            inspection_row.check_item_raw = cellToString(
                static_cast<OpenXLSX::XLCellValue>(worksheet.cell(row, columns.check_item).value()),
                false,
                options_.trim_text);
            inspection_row.defect_desc_raw = cellToString(
                static_cast<OpenXLSX::XLCellValue>(worksheet.cell(row, columns.defect_desc).value()),
                false,
                options_.trim_text);
            inspection_row.judgement_raw = cellToString(
                static_cast<OpenXLSX::XLCellValue>(worksheet.cell(row, columns.judgement).value()),
                false,
                options_.trim_text);
            inspection_row.inspect_date_raw = cellToString(
                static_cast<OpenXLSX::XLCellValue>(worksheet.cell(row, columns.inspect_date).value()),
                true,
                options_.trim_text);

            if (options_.skip_empty_rows && isEmptyRow(inspection_row)) {
                continue;
            }

            result.rows.push_back(std::move(inspection_row));
        }

        if (result.rows.empty()) {
            result.warnings.emplace_back("No non-empty data rows were read.");
        }

        document.close();
    }
    catch (const std::exception& ex) {
        result.errors.emplace_back(std::string("Failed to read xlsx: ") + ex.what());
    }

    return result;
}
