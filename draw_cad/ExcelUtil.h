#pragma once

#include "../OpenXLSX/include/OpenXLSX.hpp"

#include <cstdint>
#include <string>

namespace excel_util {

std::string formatExcelDate(double serial);
std::string cellValueToString(
    const OpenXLSX::XLCellValue& value,
    bool prefer_excel_date = false,
    bool trim_text = true);
std::string cellText(
    OpenXLSX::XLWorksheet& worksheet,
    uint32_t row,
    uint16_t column,
    bool prefer_excel_date = false,
    bool trim_text = true);

} // namespace excel_util
