#include "ExcelUtil.h"

#include "TextUtil.h"
#include "mytool.h"

#include <ctime>
#include <utility>

namespace excel_util {

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

std::string cellValueToString(
    const OpenXLSX::XLCellValue& value,
    bool prefer_excel_date,
    bool trim_text) {
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

    return text_util::normalizeText(std::move(text), trim_text);
}

std::string cellText(
    OpenXLSX::XLWorksheet& worksheet,
    uint32_t row,
    uint16_t column,
    bool prefer_excel_date,
    bool trim_text) {
    const auto value = static_cast<OpenXLSX::XLCellValue>(worksheet.cell(row, column).value());
    return cellValueToString(value, prefer_excel_date, trim_text);
}

} // namespace excel_util
