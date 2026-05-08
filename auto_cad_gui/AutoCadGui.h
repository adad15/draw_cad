#pragma once

#include "AutoCadController.h"

#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QLineEdit;
class QHBoxLayout;
class QLayout;
class QDialog;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTextEdit;
class QVBoxLayout;

class AutoCadGui final : public QWidget {
    Q_OBJECT

public:
    explicit AutoCadGui(QWidget* parent = nullptr);

private:
    enum class BrowseMode {
        ExistingFile,
        ExistingDirectory,
        SaveFile
    };

    void setupUi();
    QWidget* createCadGenerationPage();
    QWidget* createTenderGenerationPage();
    QWidget* createAiSettingsPage();
    QWidget* createCard(const QString& title, QLayout* contentLayout);
    QWidget* createModuleCard(const QString& title, const QString& hint);
    QWidget* createInfoRow(const QString& label, const QString& value, const QString& chip);
    QWidget* createOptionRow(const QString& title, const QString& description);
    QWidget* createFlowStep(int number, const QString& title, const QString& description, bool active = false);
    QWidget* createBoardLengthDataSourceCard();
    QWidget* createOutputDirectoryCard();
    QWidget* createGenerationControlCard();
    QWidget* createGenerationCheckRow(QLabel** iconLabel, QLabel** textLabel);
    QLineEdit* createSettingsInput(const QString& placeholder, bool password = false);
    QLabel* createBodyText(const QString& text, const QString& objectName = QString());
    QHBoxLayout* createPathRow(
        const QString& labelText,
        QLineEdit** lineEdit,
        BrowseMode mode,
        const QString& dialogTitle,
        const QString& filter = QString());
    void applyStyles();
    void setDefaults();
    WorkflowInput collectInput() const;
    void appendLog(const QString& text);
    void setRunning(bool running);
    void updateStepState(int current, int total);
    void setStepLabelState(QLabel* label, const QString& state);
    void ensureLogDialog();
    void setLogDialogRunning();
    void setLogDialogFinished(bool ok, const QString& summary);
    QWidget* createTaskCard(
        const QString& title,
        const QString& meta,
        const QString& state,
        int progress = -1);
    void chooseProjectFiles();
    void refreshUploadStatus();
    void addSelectedFileRow(const QString& label, QLineEdit* lineEdit);
    void clearProjectFile(QLineEdit* lineEdit);
    void chooseBoardLengthSource();
    void resetBoardLengthSource();
    void refreshBoardLengthSourceStatus();
    void updateGenerationPanel();
    void syncGenerationControlHeight();
    void setGenerationInfoMode(bool active);
    void setGenerationOutputSummary(const QString& text, const QString& state);
    void appendGenerationIssueMessage(const QString& text, const QString& state);
    void appendGenerationCompletionMessage(bool ok, const QString& summary);
    void setGenerationStatus(const QString& text, const QString& state);
    void setGenerationCheck(QLabel* iconLabel, QLabel* textLabel, bool ok, const QString& okText, const QString& failText);
    void chooseTenderFile();
    void refreshTenderFileStatus();
    void setActiveModule(int index);

    AutoCadController* controller_ = nullptr;
    QLabel* pageTitleLabel_ = nullptr;
    QLabel* pageTitleIconLabel_ = nullptr;
    QLabel* pageSubtitleLabel_ = nullptr;
    QStackedWidget* contentStack_ = nullptr;
    QPushButton* cadNavButton_ = nullptr;
    QPushButton* tenderNavButton_ = nullptr;
    QPushButton* fileLibraryNavButton_ = nullptr;
    QPushButton* aiSettingsNavButton_ = nullptr;
    QLineEdit* exeEdit_ = nullptr;
    QLineEdit* diseaseWorkbookEdit_ = nullptr;
    QLineEdit* boardLengthSourceEdit_ = nullptr;
    QLineEdit* tenderFileEdit_ = nullptr;
    QLineEdit* outputDirEdit_ = nullptr;
    QStackedWidget* uploadStack_ = nullptr;
    QVBoxLayout* selectedFilesLayout_ = nullptr;
    QLabel* boardLengthSourceNameLabel_ = nullptr;
    QLabel* boardLengthSourceMetaLabel_ = nullptr;
    QLabel* boardLengthSourceStatusLabel_ = nullptr;
    QStackedWidget* tenderUploadStack_ = nullptr;
    QLabel* tenderFileNameLabel_ = nullptr;
    QLabel* tenderFileMetaLabel_ = nullptr;
    QLabel* generationTitleLabel_ = nullptr;
    QLabel* generationStatusBadge_ = nullptr;
    QLabel* checkWorkbookIconLabel_ = nullptr;
    QLabel* checkWorkbookTextLabel_ = nullptr;
    QLabel* checkBoardSourceIconLabel_ = nullptr;
    QLabel* checkBoardSourceTextLabel_ = nullptr;
    QLabel* checkOutputIconLabel_ = nullptr;
    QLabel* checkOutputTextLabel_ = nullptr;
    QLabel* checkBackendIconLabel_ = nullptr;
    QLabel* checkBackendTextLabel_ = nullptr;
    QPushButton* startButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QPushButton* openOutputButton_ = nullptr;
    QTextEdit* logEdit_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QTextEdit* outputSummaryLabel_ = nullptr;
    QDialog* logDialog_ = nullptr;
    QLabel* logDialogStatusLabel_ = nullptr;
    QProgressBar* logDialogProgressBar_ = nullptr;
    QPushButton* logDialogCloseButton_ = nullptr;
    QVector<QLabel*> stepLabels_;
    QVector<QWidget*> generationPreflightWidgets_;
    QStringList generationIssueMessages_;
    bool generationInfoMode_ = false;
};
