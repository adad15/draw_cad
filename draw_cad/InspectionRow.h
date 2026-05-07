#pragma once

#include <string>

// Raw fields pulled from the inspection workbook for downstream parsing.
struct InspectionRow {
    std::string stake_no;
    std::string slab_no_raw;
    std::string project_name;
    std::string defect_location_raw;
    std::string check_item_raw;
    std::string defect_desc_raw;
    std::string judgement_raw;
    std::string inspect_date_raw;
    int source_row = 0;
};
