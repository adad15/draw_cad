#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

class DiseaseReportJsonExporter final {
public:
    struct Options {
        std::string sheet_name;
        uint32_t header_row = 1;
        bool skip_empty_rows = true;
        std::string project_name_filter;
    };

    struct ExportResult {
        int code = 0;
        std::string message;
        size_t exported_count = 0;
        size_t zero_judgement_count = 0;
        std::vector<std::string> warnings;

        bool ok() const noexcept {
            return code == 0;
        }

        static ExportResult success(size_t exported, size_t zero_judgement, std::vector<std::string> warnings = {}) {
            ExportResult result;
            result.message = "success";
            result.exported_count = exported;
            result.zero_judgement_count = zero_judgement;
            result.warnings = std::move(warnings);
            return result;
        }

        static ExportResult failure(int error_code, std::string error_message) {
            ExportResult result;
            result.code = error_code;
            result.message = std::move(error_message);
            return result;
        }
    };

    static ExportResult run(
        const std::string& input_xlsx,
        const std::string& output_json,
        const Options& options = {});
};
