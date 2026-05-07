#pragma once

#include <fstream>
#include <string>

class WorkflowMessageLog final {
public:
    enum class Level {
        Warning,
        Error
    };

    WorkflowMessageLog() = default;
    explicit WorkflowMessageLog(const std::string& output_txt);

    bool open(const std::string& output_txt, std::string* error_message = nullptr);
    const std::string& path() const noexcept;

    void warning(const std::string& message);
    void error(const std::string& message);

private:
    void write(Level level, const std::string& message);

    std::string path_;
    std::ofstream out_;
};
