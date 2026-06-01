#include "DiseaseReportJsonExporter.h"

#include "InspectionTableReader.h"
#include "TextUtil.h"
#include "../nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
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

std::string normalizeText(std::string text) {
    return text_util::normalizeText(std::move(text));
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
    const std::string comma_cn = text_util::utf8Literal(u8"\uFF0C");
    const std::string semicolon_cn = text_util::utf8Literal(u8"\uFF1B");
    const std::string period_cn = text_util::utf8Literal(u8"\u3002");
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
        InspectionTableReader::Options reader_options;
        reader_options.sheet_name = options.sheet_name;
        reader_options.header_row = options.header_row;
        reader_options.skip_empty_rows = options.skip_empty_rows;
        reader_options.trim_text = true;
        reader_options.columns.stake_no.clear();
        reader_options.columns.inspect_date.clear();

        InspectionTableReader reader(reader_options);
        const InspectionTableReader::ReadResult read_result = reader.read(input_xlsx);
        if (!read_result.ok()) {
            return ExportResult::failure(
                40,
                "Failed to read disease report worksheet: "
                + text_util::joinStrings(read_result.errors, "; "));
        }

        std::vector<ReportRow> rows;
        size_t skipped_project_count = 0;
        size_t zero_judgement_count = 0;
        const std::string project_name_filter = options.project_name_filter.empty()
            ? text_util::utf8Literal(u8"\u886C\u780C")
            : normalizeText(options.project_name_filter);
        const std::string marker_distance = text_util::utf8Literal(u8"\u8DDD\u677F\u7AEF");
        const std::string marker_length = text_util::utf8Literal(u8"\u957F\u5EA6\u4E3A");
        const std::string marker_width = text_util::utf8Literal(u8"\u5BBD\u5EA6\u4E3A");
        const std::string marker_area = text_util::utf8Literal(u8"\u9762\u79EF\u4E3A");

        for (const InspectionRow& inspection_row : read_result.rows) {
            const std::string& slab_no = inspection_row.slab_no_raw;
            const std::string& project_name = inspection_row.project_name;
            const std::string& defect_location = inspection_row.defect_location_raw;
            const std::string& check_item = inspection_row.check_item_raw;
            const std::string& defect_desc = inspection_row.defect_desc_raw;
            const std::string& judgement = inspection_row.judgement_raw;

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
            row.source_row = inspection_row.source_row;
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
        warnings.insert(warnings.end(), read_result.warnings.begin(), read_result.warnings.end());
        if (rows.empty()) {
            warnings.emplace_back("No disease rows were exported.");
        }

        std::ofstream out(output_json.c_str(), std::ios::binary);
        if (!out) {
            return ExportResult::failure(38, "Failed to open output json: " + output_json);
        }

        const json root = makeOutputJson(
            input_xlsx,
            read_result.sheet_name,
            options.header_row,
            project_name_filter,
            skipped_project_count,
            zero_judgement_count,
            rows);
        out << root.dump(2) << "\n";
        if (!out.good()) {
            return ExportResult::failure(39, "Failed while writing output json: " + output_json);
        }

        return ExportResult::success(rows.size(), zero_judgement_count, std::move(warnings));
    }
    catch (const std::exception& ex) {
        return ExportResult::failure(40, std::string("Failed to export disease report json: ") + ex.what());
    }
}
