#include "WorkflowMessageLog.h"

#include <filesystem>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

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
#endif

std::filesystem::path pathFromUtf8(const std::string& text) {
#ifdef _WIN32
    return std::filesystem::path(utf8ToWide(text));
#else
    return std::filesystem::path(text);
#endif
}

const char* levelName(WorkflowMessageLog::Level level) {
    return (level == WorkflowMessageLog::Level::Warning) ? "WARNING" : "ERROR";
}

const char* consolePrefix(WorkflowMessageLog::Level level) {
    return (level == WorkflowMessageLog::Level::Warning) ? "warning: " : "error: ";
}

} // namespace

WorkflowMessageLog::WorkflowMessageLog(const std::string& output_txt) {
    open(output_txt);
}

bool WorkflowMessageLog::open(const std::string& output_txt, std::string* error_message) {
    if (out_.is_open()) {
        out_.close();
    }

    path_ = output_txt;
    out_.open(pathFromUtf8(output_txt), std::ios::binary | std::ios::trunc);
    if (!out_) {
        if (error_message != nullptr) {
            *error_message = "Failed to open warning/error log: " + output_txt;
        }
        return false;
    }

    const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    out_.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    return out_.good();
}

const std::string& WorkflowMessageLog::path() const noexcept {
    return path_;
}

void WorkflowMessageLog::warning(const std::string& message) {
    write(Level::Warning, message);
}

void WorkflowMessageLog::error(const std::string& message) {
    write(Level::Error, message);
}

void WorkflowMessageLog::write(Level level, const std::string& message) {
    std::ostream& console = (level == Level::Error) ? std::cerr : std::cout;
    console << consolePrefix(level) << message << "\n";

    if (out_) {
        out_ << "[" << levelName(level) << "] " << message << "\n";
        out_.flush();
    }
}
