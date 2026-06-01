#include "BoardLengthJsonExporter.h"
#include "DiseaseReportJsonExporter.h"
#include "ExcelUtil.h"
#include "LiningPlanDxfWriter.h"
#include "TextUtil.h"
#include "WorkflowMessageLog.h"
#include "dxf_to_json.h"
#include "../OpenXLSX/include/OpenXLSX.hpp"

#include <algorithm>
#include <cctype>
#include <clocale>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

std::string workspaceRoot() {
    return R"(D:\vs2022 code\auto_cad\)";
}

std::string defaultBatchDiseaseReportPath() {
    return workspaceRoot() + text_util::utf8Literal(u8"2025\u5E74\u9E64\u5927\u96A7\u9053\u5916\u89C2\u6574\u7406--\u672C\u6EAA\u539F\u59CB\u7248\u672C.xlsx");
}

std::string defaultBoardLengthDirectory() {
    return workspaceRoot() + text_util::utf8Literal(u8"\u677F\u957F\u5206\u5E03\u8868");
}

std::string defaultSymbolsDxfPath() {
    return workspaceRoot() + "Drawing1.dxf";
}

std::string defaultSymbolsJsonPath() {
    return workspaceRoot() + "symbols.json";
}

std::string defaultBatchOutputDir() {
    return workspaceRoot() + "batch_output";
}

std::string defaultWarningErrorLogPath() {
    return workspaceRoot() + "warnings_errors.txt";
}

void configureConsoleEncoding() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    std::setlocale(LC_ALL, ".UTF-8");
}

bool isDiseaseWorksheet(OpenXLSX::XLWorksheet& worksheet) {
    return excel_util::cellText(worksheet, 1, 2) == text_util::utf8Literal(u8"\u886C\u780C\u677F\u5757\u53F7")
        && excel_util::cellText(worksheet, 1, 3) == text_util::utf8Literal(u8"\u9879\u76EE\u540D\u79F0")
        && excel_util::cellText(worksheet, 1, 4) == text_util::utf8Literal(u8"\u75C5\u5BB3\u4F4D\u7F6E")
        && excel_util::cellText(worksheet, 1, 5) == text_util::utf8Literal(u8"\u68C0\u67E5\u5185\u5BB9")
        && excel_util::cellText(worksheet, 1, 6) == text_util::utf8Literal(u8"\u75C5\u5BB3\u63CF\u8FF0")
        && excel_util::cellText(worksheet, 1, 7) == text_util::utf8Literal(u8"\u5224\u5B9A\u7ED3\u8BBA");
}

std::vector<std::string> diseaseWorksheetNames(const std::string& input_xlsx) {
    OpenXLSX::XLDocument document;
    document.open(input_xlsx);

    std::vector<std::string> result;
    auto workbook = document.workbook();
    const auto worksheet_names = workbook.worksheetNames();
    for (const std::string& sheet_name : worksheet_names) {
        auto worksheet = workbook.worksheet(sheet_name);
        if (isDiseaseWorksheet(worksheet)) {
            result.push_back(sheet_name);
        }
    }

    document.close();
    return result;
}

std::string sanitizeFileStem(std::string text) {
    text = text_util::trimAsciiWhitespace(std::move(text));
    for (char& ch : text) {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (value < 0x20 || ch == '<' || ch == '>' || ch == ':' || ch == '"'
            || ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*') {
            ch = '_';
        }
    }

    while (!text.empty() && (text.back() == ' ' || text.back() == '.')) {
        text.pop_back();
    }
    if (text.empty()) {
        return "sheet";
    }
    return text;
}

std::string uniqueFileStem(const std::string& sheet_name, std::set<std::string>& used_stems) {
    const std::string base = sanitizeFileStem(sheet_name);
    std::string stem = base;
    size_t suffix = 2;
    while (!used_stems.insert(stem).second) {
        stem = base + "_" + std::to_string(suffix);
        ++suffix;
    }
    return stem;
}

std::string joinPath(const std::string& directory, const std::string& file_name) {
    if (directory.empty()) {
        return file_name;
    }
    const char last = directory.back();
    if (last == '\\' || last == '/') {
        return directory + file_name;
    }
    return directory + "\\" + file_name;
}

std::string removeAll(std::string text, const std::string& token) {
    if (token.empty()) {
        return text;
    }

    size_t pos = 0;
    while ((pos = text.find(token, pos)) != std::string::npos) {
        text.erase(pos, token.size());
    }
    return text;
}

std::string normalizeBoardLengthMatchKey(std::string text) {
    text = text_util::trimAsciiWhitespace(std::move(text));
    text = removeAll(std::move(text), text_util::utf8Literal(u8"\u96A7\u9053"));
    text.erase(
        std::remove_if(
            text.begin(),
            text.end(),
            [](char ch) { return std::isspace(static_cast<unsigned char>(ch)) != 0; }),
        text.end());
    return text;
}

std::string toLowerAscii(std::string text) {
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

bool ensureDirectory(const std::string& directory, std::string& error_message) {
    try {
        if (directory.empty()) {
            error_message = "output directory must not be empty.";
            return false;
        }
        std::filesystem::create_directories(std::filesystem::path(directory));
        return true;
    }
    catch (const std::exception& ex) {
        error_message = ex.what();
        return false;
    }
}

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    const int required_size = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (required_size <= 0) {
        return {};
    }

    std::wstring result(static_cast<size_t>(required_size), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
        result.data(),
        required_size);
    return result;
}

std::string wideToUtf8(const wchar_t* text) {
    if (text == nullptr) {
        return {};
    }

    const int required_size = WideCharToMultiByte(
        CP_UTF8,
        0,
        text,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required_size <= 0) {
        return {};
    }

    std::string result(static_cast<size_t>(required_size - 1), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        text,
        -1,
        result.data(),
        required_size,
        nullptr,
        nullptr);
    return result;
}
#endif

std::filesystem::path pathFromUtf8(const std::string& text) {
#ifdef _WIN32
    return std::filesystem::path(utf8ToWide(text));
#else
    return std::filesystem::path(text);
#endif
}

std::string pathToUtf8(const std::filesystem::path& path) {
#ifdef _WIN32
    return wideToUtf8(path.wstring().c_str());
#else
    return path.string();
#endif
}

bool isExcelWorkbookPath(const std::filesystem::path& path) {
    const std::string extension = toLowerAscii(pathToUtf8(path.extension()));
    return extension == ".xlsx" || extension == ".xlsm";
}

std::string resolveBoardLengthWorkbook(
    const std::string& board_lengths_source,
    const std::string& disease_sheet_name,
    std::string& error_message) {
    const std::filesystem::path source_path = pathFromUtf8(board_lengths_source);
    std::error_code ec;
    if (std::filesystem::is_regular_file(source_path, ec)) {
        return board_lengths_source;
    }

    if (!std::filesystem::is_directory(source_path, ec)) {
        error_message = "Board length source is neither a workbook nor a directory: " + board_lengths_source;
        return {};
    }

    const std::string expected_key = normalizeBoardLengthMatchKey(disease_sheet_name);
    std::vector<std::string> matches;
    for (const auto& entry : std::filesystem::directory_iterator(source_path, ec)) {
        if (ec) {
            error_message = "Failed while reading board length directory: " + ec.message();
            return {};
        }
        if (!entry.is_regular_file(ec) || !isExcelWorkbookPath(entry.path())) {
            continue;
        }

        const std::string file_stem = pathToUtf8(entry.path().stem());
        if (normalizeBoardLengthMatchKey(file_stem) == expected_key) {
            matches.push_back(pathToUtf8(entry.path()));
        }
    }

    if (matches.empty()) {
        error_message =
            "No board length workbook matched disease sheet \"" + disease_sheet_name
            + "\" in directory: " + board_lengths_source;
        return {};
    }
    if (matches.size() > 1) {
        error_message = "Multiple board length workbooks matched disease sheet \"" + disease_sheet_name + "\":";
        for (const std::string& match : matches) {
            error_message += "\n  " + match;
        }
        return {};
    }

    return matches.front();
}

std::vector<std::string> commandLineArguments(int argc, char* argv[]) {
#ifdef _WIN32
    int wide_argc = 0;
    LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    if (wide_argv != nullptr) {
        std::vector<std::string> args;
        args.reserve(static_cast<size_t>(wide_argc));
        for (int i = 0; i < wide_argc; ++i) {
            args.push_back(wideToUtf8(wide_argv[i]));
        }
        LocalFree(wide_argv);
        return args;
    }
#endif

    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i] == nullptr ? "" : argv[i]);
    }
    return args;
}

void printUsage() {
    std::cerr
        << "Usage:\n"
        << "  draw_cad.exe\n"
        << "  draw_cad.exe --batch-workflow [disease_xlsx] [board_length_dir] [output_dir]\n"
        << "  draw_cad.exe --symbols [input_dxf] [output_json]\n"
        << "  draw_cad.exe --board-lengths <input_xlsx> <output_json>\n"
        << "  draw_cad.exe --disease-report <input_xlsx> <output_json>\n"
        << "  draw_cad.exe --lining-plan <disease_json> <board_lengths_json> <output_dxf> [symbols_json]\n";
}

int runSymbolsJsonExport(
    const std::string& input_dxf,
    const std::string& output_json,
    WorkflowMessageLog& message_log) {
    MyConfig config;
    config.input_dxf = input_dxf;
    config.output_json = output_json;

    dxf_to_json converter(config);
    const ConvertResult result = converter.runExtractor();
    if (!result.ok()) {
        message_log.error(
            "dxf_to_json failed(" + std::to_string(result.code) + "): " + result.message);
        return (result.code == 0) ? 1 : result.code;
    }

    std::cout << "Symbols JSON generated: " << output_json << "\n";
    std::cout << "  input dxf: " << input_dxf << "\n";
    std::cout << "  symbols(raw): " << converter.symbols().size() << "\n";
    return 0;
}

int runDiseaseReportExport(
    const std::string& input_xlsx,
    const std::string& output_json,
    WorkflowMessageLog& message_log) {
    const DiseaseReportJsonExporter::ExportResult result =
        DiseaseReportJsonExporter::run(input_xlsx, output_json);
    if (!result.ok()) {
        message_log.error(
            "DiseaseReportJsonExporter failed(" + std::to_string(result.code) + "): " + result.message);
        return (result.code == 0) ? 1 : result.code;
    }

    std::cout << "Disease report JSON generated: " << output_json << "\n";
    std::cout << "  exported rows: " << result.exported_count << "\n";
    std::cout << "  zero judgement rows: " << result.zero_judgement_count << "\n";
    for (const std::string& warning : result.warnings) {
        message_log.warning(warning);
    }
    return 0;
}

int runDiseaseReportExportForSheet(
    const std::string& input_xlsx,
    const std::string& sheet_name,
    const std::string& output_json,
    DiseaseReportJsonExporter::ExportResult& result,
    WorkflowMessageLog& message_log) {
    DiseaseReportJsonExporter::Options options;
    options.sheet_name = sheet_name;
    result = DiseaseReportJsonExporter::run(input_xlsx, output_json, options);
    if (!result.ok()) {
        message_log.error(
            "DiseaseReportJsonExporter failed(" + std::to_string(result.code) + "): " + result.message);
        return (result.code == 0) ? 1 : result.code;
    }

    std::cout << "Disease report JSON generated: " << output_json << "\n";
    std::cout << "  sheet: " << sheet_name << "\n";
    std::cout << "  exported lining rows: " << result.exported_count << "\n";
    std::cout << "  zero judgement rows: " << result.zero_judgement_count << "\n";
    for (const std::string& warning : result.warnings) {
        message_log.warning("sheet " + sheet_name + ": " + warning);
    }
    return 0;
}

int runBoardLengthExport(
    const std::string& input_xlsx,
    const std::string& output_json,
    WorkflowMessageLog& message_log) {
    const BoardLengthJsonExporter::ExportResult result =
        BoardLengthJsonExporter::run(input_xlsx, output_json);
    if (!result.ok()) {
        message_log.error(
            "BoardLengthJsonExporter failed(" + std::to_string(result.code) + "): " + result.message);
        return (result.code == 0) ? 1 : result.code;
    }

    std::cout << "Board length JSON generated: " << output_json << "\n";
    std::cout << "  exported rows: " << result.exported_count << "\n";
    for (const std::string& warning : result.warnings) {
        message_log.warning(warning);
    }
    return 0;
}

int runLiningPlanDxfWrite(
    const std::string& disease_json,
    const std::string& board_lengths_json,
    const std::string& output_dxf,
    const std::string& symbols_json,
    WorkflowMessageLog& message_log) {
    LiningPlanDxfWriter::Options options;
    options.symbols_json = symbols_json;
    const LiningPlanDxfWriter::WriteResult result =
        LiningPlanDxfWriter::run(disease_json, board_lengths_json, output_dxf, options);
    if (!result.ok()) {
        message_log.error(
            "LiningPlanDxfWriter failed(" + std::to_string(result.code) + "): " + result.message);
        return (result.code == 0) ? 1 : result.code;
    }

    std::cout << "Lining plan DXF generated: " << output_dxf << "\n";
    std::cout << "  slabs: " << result.slab_count << "\n";
    std::cout << "  panels: " << result.panel_count << "\n";
    for (const std::string& warning : result.warnings) {
        message_log.warning(warning);
    }
    return 0;
}

int runBatchWorkflow(
    const std::string& disease_xlsx,
    const std::string& board_lengths_source,
    const std::string& output_dir,
    WorkflowMessageLog& message_log) {
    std::string directory_error;
    if (!ensureDirectory(output_dir, directory_error)) {
        message_log.error(
            "Failed to create output directory: " + output_dir + "\n  reason: " + directory_error);
        return 90;
    }

    const std::string warning_error_log = joinPath(output_dir, "warnings_errors.txt");
    std::string log_error;
    if (!message_log.open(warning_error_log, &log_error)) {
        message_log.warning(log_error);
    }

    std::cout << "Batch workflow started.\n";
    std::cout << "  disease workbook: " << disease_xlsx << "\n";
    std::cout << "  board length source: " << board_lengths_source << "\n";
    std::cout << "  output dir: " << output_dir << "\n";
    std::cout << "  warning/error log: " << message_log.path() << "\n";

    const std::string symbols_json = joinPath(output_dir, "symbols.json");

    const int symbols_result = runSymbolsJsonExport(defaultSymbolsDxfPath(), symbols_json, message_log);
    if (symbols_result != 0) {
        return symbols_result;
    }

    std::vector<std::string> sheet_names;
    try {
        sheet_names = diseaseWorksheetNames(disease_xlsx);
    }
    catch (const std::exception& ex) {
        message_log.error(std::string("Failed to read worksheet names: ") + ex.what());
        return 91;
    }

    if (sheet_names.empty()) {
        message_log.error("No disease report worksheets were found in: " + disease_xlsx);
        return 92;
    }

    std::set<std::string> used_stems;
    size_t generated_count = 0;
    size_t skipped_count = 0;
    for (const std::string& sheet_name : sheet_names) {
        const std::string stem = uniqueFileStem(sheet_name, used_stems);
        const std::string disease_json = joinPath(output_dir, stem + "_disease_report.json");
        const std::string board_lengths_json = joinPath(output_dir, stem + "_board_lengths.json");
        const std::string output_dxf = joinPath(output_dir, stem + ".dxf");

        std::cout << "\nProcessing sheet: " << sheet_name << "\n";
        DiseaseReportJsonExporter::ExportResult disease_result;
        const int disease_code = runDiseaseReportExportForSheet(
            disease_xlsx,
            sheet_name,
            disease_json,
            disease_result,
            message_log);
        if (disease_code != 0) {
            return disease_code;
        }
        if (disease_result.exported_count == 0) {
            ++skipped_count;
            std::cout << "  skipped DXF generation: no lining disease rows.\n";
            continue;
        }

        std::string board_length_error;
        const std::string board_lengths_xlsx =
            resolveBoardLengthWorkbook(board_lengths_source, sheet_name, board_length_error);
        if (board_lengths_xlsx.empty()) {
            message_log.error(
                "Failed to resolve board length workbook for sheet "
                + sheet_name + "\n  reason: " + board_length_error);
            return 93;
        }

        std::cout << "  board length workbook: " << board_lengths_xlsx << "\n";
        const int board_result = runBoardLengthExport(board_lengths_xlsx, board_lengths_json, message_log);
        if (board_result != 0) {
            return board_result;
        }

        const int lining_result = runLiningPlanDxfWrite(
            disease_json,
            board_lengths_json,
            output_dxf,
            symbols_json,
            message_log);
        if (lining_result != 0) {
            return lining_result;
        }
        ++generated_count;
    }

    std::cout << "\nBatch workflow completed.\n";
    std::cout << "  disease worksheets: " << sheet_names.size() << "\n";
    std::cout << "  generated dxf files: " << generated_count << "\n";
    std::cout << "  skipped sheets: " << skipped_count << "\n";
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    configureConsoleEncoding();

    const std::vector<std::string> args = commandLineArguments(argc, argv);
    WorkflowMessageLog message_log;
    std::string log_error;
    if (!message_log.open(defaultWarningErrorLogPath(), &log_error)) {
        std::cerr << "warning: " << log_error << "\n";
    }

    if (args.size() >= 2) {
        const std::string mode = args[1];
        if (mode == "--symbols") {
            const std::string input_dxf = (args.size() >= 3) ? args[2] : defaultSymbolsDxfPath();
            const std::string output_json = (args.size() >= 4) ? args[3] : defaultSymbolsJsonPath();
            return runSymbolsJsonExport(input_dxf, output_json, message_log);
        }

        if (mode == "--board-lengths") {
            if (args.size() < 4) {
                message_log.error("--board-lengths requires <input_xlsx> and <output_json>.");
                printUsage();
                return 2;
            }
            return runBoardLengthExport(args[2], args[3], message_log);
        }

        if (mode == "--disease-report") {
            if (args.size() < 4) {
                message_log.error("--disease-report requires <input_xlsx> and <output_json>.");
                printUsage();
                return 2;
            }
            return runDiseaseReportExport(args[2], args[3], message_log);
        }

        if (mode == "--lining-plan") {
            if (args.size() < 5) {
                message_log.error("--lining-plan requires <disease_json>, <board_lengths_json>, and <output_dxf>.");
                printUsage();
                return 2;
            }
            const std::string disease_json = args[2];
            const std::string board_lengths_json = args[3];
            const std::string output_dxf = args[4];
            const std::string symbols_json = (args.size() >= 6) ? args[5] : defaultSymbolsJsonPath();
            return runLiningPlanDxfWrite(disease_json, board_lengths_json, output_dxf, symbols_json, message_log);
        }

        if (mode == "--batch-workflow") {
            const std::string disease_xlsx = (args.size() >= 3) ? args[2] : defaultBatchDiseaseReportPath();
            const std::string board_lengths_source = (args.size() >= 4) ? args[3] : defaultBoardLengthDirectory();
            const std::string output_dir = (args.size() >= 5) ? args[4] : defaultBatchOutputDir();
            return runBatchWorkflow(disease_xlsx, board_lengths_source, output_dir, message_log);
        }

        message_log.error("Unknown mode: " + mode);
        printUsage();
        return 2;
    }

    return runBatchWorkflow(
        defaultBatchDiseaseReportPath(),
        defaultBoardLengthDirectory(),
        defaultBatchOutputDir(),
        message_log);
}
