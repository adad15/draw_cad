#pragma once

#include "InspectionRow.h"

#include <cstdint>
#include <string>
#include <vector>

namespace inspection_table_reader_detail {
inline std::string utf8Literal(const char8_t* text) {
    return std::string(
        reinterpret_cast<const char*>(text),
        std::char_traits<char8_t>::length(text));
}
}

// Reads required business columns from an xlsx worksheet into InspectionRow.
class InspectionTableReader final {
public:
    struct ColumnMapping {
        std::string stake_no = inspection_table_reader_detail::utf8Literal(u8"\u6869\u53f7");
        std::string slab_no = inspection_table_reader_detail::utf8Literal(u8"\u886c\u780c\u677f\u5757\u53f7");
        std::string project_name = inspection_table_reader_detail::utf8Literal(u8"\u9879\u76ee\u540d\u79f0");
        std::string defect_location = inspection_table_reader_detail::utf8Literal(u8"\u75c5\u5bb3\u4f4d\u7f6e");
        std::string check_item = inspection_table_reader_detail::utf8Literal(u8"\u68c0\u67e5\u5185\u5bb9");
        std::string defect_desc = inspection_table_reader_detail::utf8Literal(u8"\u75c5\u5bb3\u63cf\u8ff0");
        std::string judgement = inspection_table_reader_detail::utf8Literal(u8"\u5224\u5b9a\u7ed3\u8bba");
        std::string inspect_date = inspection_table_reader_detail::utf8Literal(u8"\u68c0\u6d4b\u65e5\u671f");
    };

    struct Options {
        std::string sheet_name;
        uint32_t header_row = 1;
        bool skip_empty_rows = true;
        bool trim_text = true;
        ColumnMapping columns;
    };

    struct ReadResult {
        std::vector<InspectionRow> rows;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;

        bool ok() const {
            return errors.empty();
        }
    };

    explicit InspectionTableReader(Options options = {});

    ReadResult read(const std::string& file_path) const;

private:
    Options options_;
};
