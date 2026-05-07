#include "AutoCadController.h"

#include <QDir>
#include <QFileInfo>

namespace {

QString quoteIfNeeded(const QString& value) {
    if (!value.contains(' ') && !value.contains('\t')) {
        return value;
    }
    QString escaped = value;
    escaped.replace('"', "\\\"");
    return '"' + escaped + '"';
}

QString cleanLine(QByteArray line) {
    if (line.endsWith('\r')) {
        line.chop(1);
    }
    return QString::fromUtf8(line);
}

} // namespace

AutoCadController::AutoCadController(QObject* parent)
    : QObject(parent),
      process_(new QProcess(this)) {
    process_->setProcessChannelMode(QProcess::SeparateChannels);

    connect(process_, &QProcess::readyReadStandardOutput, this, [this]() {
        appendProcessOutput(stdoutBuffer_, process_->readAllStandardOutput(), false);
    });

    connect(process_, &QProcess::readyReadStandardError, this, [this]() {
        appendProcessOutput(stderrBuffer_, process_->readAllStandardError(), true);
    });

    connect(process_,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        &AutoCadController::handleProcessFinished);

    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            emit progressTextChanged("error: 后端程序启动失败，请检查 draw_cad.exe 路径。");
        }
    });
}

bool AutoCadController::isRunning() const noexcept {
    return process_->state() != QProcess::NotRunning;
}

void AutoCadController::run(const WorkflowInput& input) {
    if (isRunning()) {
        emit progressTextChanged("warning: 当前已有任务正在运行。");
        return;
    }

    QString errorMessage;
    if (!validateInput(input, errorMessage)) {
        emit progressTextChanged("error: " + errorMessage);
        emit finished(false, errorMessage);
        return;
    }

    input_ = input;
    currentStep_ = 0;
    cancelling_ = false;
    stdoutBuffer_.clear();
    stderrBuffer_.clear();
    buildCommands(input_);

    emit progressTextChanged("任务开始。");
    emit progressTextChanged("工作目录: " + QDir::toNativeSeparators(input_.outputDir));
    emit progressUpdated(0, commands_.size(), "准备运行");
    startNextCommand();
}

void AutoCadController::cancel() {
    if (!isRunning()) {
        return;
    }

    cancelling_ = true;
    emit progressTextChanged("warning: 正在取消任务...");
    process_->kill();
}

bool AutoCadController::validateInput(const WorkflowInput& input, QString& errorMessage) const {
    if (input.exePath.trimmed().isEmpty()) {
        errorMessage = "后端程序路径为空。";
        return false;
    }
    if (!QFileInfo::exists(input.exePath) || !QFileInfo(input.exePath).isFile()) {
        errorMessage = "后端程序不存在: " + input.exePath;
        return false;
    }
    if (!QFileInfo::exists(input.diseaseWorkbook) || !QFileInfo(input.diseaseWorkbook).isFile()) {
        errorMessage = "批量病害总表不存在: " + input.diseaseWorkbook;
        return false;
    }
    if (!QFileInfo::exists(input.boardLengthSource) || !QFileInfo(input.boardLengthSource).isDir()) {
        errorMessage = "板长分布表目录不存在: " + input.boardLengthSource;
        return false;
    }
    if (input.outputDir.trimmed().isEmpty()) {
        errorMessage = "输出目录为空。";
        return false;
    }
    if (!QDir().mkpath(input.outputDir)) {
        errorMessage = "无法创建输出目录: " + input.outputDir;
        return false;
    }

    return true;
}

void AutoCadController::buildCommands(const WorkflowInput& input) {
    commands_.clear();
    commands_.push_back({
        "批量生成衬砌平面图 DXF",
        {"--batch-workflow", input.diseaseWorkbook, input.boardLengthSource, input.outputDir}
    });
}

void AutoCadController::startNextCommand() {
    if (currentStep_ >= commands_.size()) {
        emit progressUpdated(commands_.size(), commands_.size(), "完成");
        emit progressTextChanged("任务完成。");
        emit finished(true, "批量 DXF 已生成，输出目录: " + QDir::toNativeSeparators(input_.outputDir));
        return;
    }

    const Command& command = commands_[currentStep_];
    stdoutBuffer_.clear();
    stderrBuffer_.clear();

    emit progressUpdated(currentStep_, commands_.size(), command.label);
    emit progressTextChanged("");
    emit progressTextChanged(QString("[%1/%2] %3")
        .arg(currentStep_ + 1)
        .arg(commands_.size())
        .arg(command.label));
    emit progressTextChanged(commandLineForLog(input_.exePath, command.arguments));

    process_->setProgram(input_.exePath);
    process_->setArguments(command.arguments);
    process_->setWorkingDirectory(input_.outputDir);
    process_->start();
}

void AutoCadController::handleProcessFinished(int exitCode, QProcess::ExitStatus status) {
    flushProcessOutput(stdoutBuffer_, false);
    flushProcessOutput(stderrBuffer_, true);

    if (cancelling_) {
        cancelling_ = false;
        emit progressUpdated(currentStep_, commands_.size(), "已取消");
        emit finished(false, "任务已取消。");
        return;
    }

    if (status != QProcess::NormalExit || exitCode != 0) {
        const QString summary = QString("步骤失败: %1, exitCode=%2")
            .arg(commands_.value(currentStep_).label)
            .arg(exitCode);
        emit progressTextChanged("error: " + summary);
        emit finished(false, summary);
        return;
    }

    emit progressTextChanged(QString("步骤完成: %1").arg(commands_[currentStep_].label));
    ++currentStep_;
    startNextCommand();
}

void AutoCadController::appendProcessOutput(QByteArray& buffer, const QByteArray& data, bool standardError) {
    buffer.append(data);

    qsizetype index = buffer.indexOf('\n');
    while (index >= 0) {
        QByteArray line = buffer.left(index);
        buffer.remove(0, index + 1);
        const QString text = cleanLine(line);
        if (!text.isEmpty()) {
            emit progressTextChanged(standardError ? "stderr: " + text : text);
        }
        index = buffer.indexOf('\n');
    }
}

void AutoCadController::flushProcessOutput(QByteArray& buffer, bool standardError) {
    if (buffer.isEmpty()) {
        return;
    }
    const QString text = cleanLine(buffer);
    buffer.clear();
    if (!text.isEmpty()) {
        emit progressTextChanged(standardError ? "stderr: " + text : text);
    }
}

QString AutoCadController::commandLineForLog(const QString& program, const QStringList& arguments) const {
    QStringList parts;
    parts.push_back(quoteIfNeeded(QDir::toNativeSeparators(program)));
    for (const QString& argument : arguments) {
        parts.push_back(quoteIfNeeded(QDir::toNativeSeparators(argument)));
    }
    return "command: " + parts.join(' ');
}
