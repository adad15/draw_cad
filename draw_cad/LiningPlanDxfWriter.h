#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

class LiningPlanDxfWriter final {
public:
    struct Options {
        size_t slabs_per_panel = 5;
        double drawing_units_per_meter = 1.0;
        double default_slab_length_m = 8.0;
        double label_column_width = 10.0;
        double zone_height = 2.0;
        double bottom_label_height = 0.75;
        double panel_gap_y = 2.0;
        double text_height = 0.35;
        double slab_no_text_height = 0.4;
        std::string symbols_json;
        double defect_symbol_scale = 0.22;
        double annotation_text_height = 0.32;
        double annotation_line_spacing = 0.42;
        double annotation_gap = 0.30;
        double disease_collision_padding = 0.12;
        double disease_collision_step_x = 0.45;
        double disease_collision_step_y = 0.45;
        size_t disease_collision_max_offset_steps = 4;
        double disease_horizontal_line_clearance = 0.20;
        double break_line_gap = 0.18;
        double break_offset_x = 0.35;
        double break_half_height = 0.55;
    };

    struct WriteResult {
        int code = 0;
        std::string message;
        size_t slab_count = 0;
        size_t panel_count = 0;
        std::vector<std::string> warnings;

        bool ok() const noexcept {
            return code == 0;
        }

        static WriteResult success(size_t slabs, size_t panels, std::vector<std::string> warnings = {}) {
            WriteResult result;
            result.message = "success";
            result.slab_count = slabs;
            result.panel_count = panels;
            result.warnings = std::move(warnings);
            return result;
        }

        static WriteResult failure(int error_code, std::string error_message) {
            WriteResult result;
            result.code = error_code;
            result.message = std::move(error_message);
            return result;
        }
    };

    static WriteResult run(
        const std::string& disease_report_json,
        const std::string& board_lengths_json,
        const std::string& output_dxf,
        const Options& options = {});
};
