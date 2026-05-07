#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVector>

struct WorkflowInput {
    QString exePath;
    QString diseaseWorkbook;
    QString boardLengthSource;
    QString outputDir;
};

class AutoCadController final : public QObject {
    Q_OBJECT

public:
    explicit AutoCadController(QObject* parent = nullptr);

    bool isRunning() const noexcept;

public slots:
    void run(const WorkflowInput& input);
    void cancel();

signals:
    void progressTextChanged(const QString& text);
    void progressUpdated(int current, int total, const QString& message);
    void finished(bool ok, const QString& summary);

private:
    struct Command {
        QString label;
        QStringList arguments;
    };

    bool validateInput(const WorkflowInput& input, QString& errorMessage) const;
    void buildCommands(const WorkflowInput& input);
    void startNextCommand();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void appendProcessOutput(QByteArray& buffer, const QByteArray& data, bool standardError);
    void flushProcessOutput(QByteArray& buffer, bool standardError);
    QString commandLineForLog(const QString& program, const QStringList& arguments) const;

    QProcess* process_ = nullptr;
    WorkflowInput input_;
    QVector<Command> commands_;
    int currentStep_ = 0;
    bool cancelling_ = false;
    QByteArray stdoutBuffer_;
    QByteArray stderrBuffer_;
};
