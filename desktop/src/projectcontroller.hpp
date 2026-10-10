#pragma once

#include "projectmanifest.hpp"
#include "projectinspection.hpp"
#include "activitymodel.hpp"
#include "toolchainprofile.hpp"
#include "codexframes.hpp"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QHash>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <functional>

class ProjectController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString specification READ specification NOTIFY projectChanged)
    Q_PROPERTY(QString recoveryState READ recoveryState NOTIFY projectChanged)
    Q_PROPERTY(QString workflowState READ workflowState NOTIFY projectChanged)
    Q_PROPERTY(QVariantList evidenceEntries READ evidenceEntries NOTIFY projectChanged)
    Q_PROPERTY(QString toolchainConfiguration READ toolchainConfiguration NOTIFY projectChanged)
    Q_PROPERTY(QAbstractItemModel* conversationModel READ conversationModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel* operationModel READ operationModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel* buildLogModel READ buildLogModel CONSTANT)
    Q_PROPERTY(bool inspectionBusy READ inspectionBusy NOTIFY projectChanged)
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectChanged)
    Q_PROPERTY(QStringList projects READ projects NOTIFY projectChanged)
    Q_PROPERTY(QString description READ description NOTIFY projectChanged)
    Q_PROPERTY(QString artifactPath READ artifactPath NOTIFY projectChanged)
    Q_PROPERTY(QString revision READ revision NOTIFY projectChanged)
    Q_PROPERTY(QString artifactHash READ artifactHash NOTIFY projectChanged)
    Q_PROPERTY(QString latestSuccessfulEvidence READ latestSuccessfulEvidence NOTIFY projectChanged)
    Q_PROPERTY(QString latestEvidence READ latestEvidence NOTIFY projectChanged)
    Q_PROPERTY(int selectedProjectIndex READ selectedProjectIndex NOTIFY projectChanged)
    Q_PROPERTY(QString dspState READ dspState NOTIFY projectChanged)
    Q_PROPERTY(QString sdkState READ sdkState NOTIFY projectChanged)
    Q_PROPERTY(QString metadataState READ metadataState NOTIFY projectChanged)
    Q_PROPERTY(QString measurementState READ measurementState NOTIFY projectChanged)
    Q_PROPERTY(QString history READ history NOTIFY projectChanged)
    Q_PROPERTY(QString buildState READ buildState NOTIFY projectChanged)
    Q_PROPERTY(QString validationState READ validationState NOTIFY projectChanged)
    Q_PROPERTY(QString guiLifecycleState READ guiLifecycleState NOTIFY projectChanged)
    Q_PROPERTY(QStringList models READ models NOTIFY modelsChanged)
    Q_PROPERTY(QStringList reasoningEfforts READ reasoningEfforts NOTIFY modelsChanged)
    Q_PROPERTY(QString selectedModel READ selectedModel NOTIFY modelsChanged)
    Q_PROPERTY(QString selectedEffort READ selectedEffort NOTIFY modelsChanged)
    Q_PROPERTY(QString effectiveModel READ effectiveModel NOTIFY codexStateChanged)
    Q_PROPERTY(QString authenticationState READ authenticationState NOTIFY codexStateChanged)
    Q_PROPERTY(QString codexState READ codexState NOTIFY codexStateChanged)
    Q_PROPERTY(QString codexPermissionState READ codexPermissionState NOTIFY codexStateChanged)
    Q_PROPERTY(QString output READ output NOTIFY outputChanged)
    Q_PROPERTY(QString prompt READ prompt WRITE setPrompt NOTIFY promptChanged)
    Q_PROPERTY(QString activeTask READ activeTask NOTIFY taskChanged)
    Q_PROPERTY(QString approvalText READ approvalText NOTIFY approvalChanged)
    Q_PROPERTY(bool approvalPending READ approvalPending NOTIFY approvalChanged)

public:
    explicit ProjectController(QObject* parent = nullptr);
    QAbstractItemModel* conversationModel(){return &m_conversation;}
    QAbstractItemModel* operationModel(){return &m_operations;}
    QAbstractItemModel* buildLogModel(){return &m_buildLog;}
    QString specification() const {return m_specification;}
    QString recoveryState() const {return m_recoveryState;}
    QString workflowState() const {return m_workflowState;}
    QVariantList evidenceEntries() const {return m_evidenceEntries;}
    Q_INVOKABLE bool saveSpecification(const QString& text);
    Q_INVOKABLE void inspectEvidence(int index);
    Q_INVOKABLE void verifyProject();
    Q_INVOKABLE void retryInterruptedJob();
    void advanceWorkflow();
    QString toolchainConfiguration() const;
    Q_INVOKABLE bool saveToolchainConfiguration(const QString& json);
    void flushStreams();
    bool inspectionBusy() const {return m_inspectionBusy;}
    void initializeCodex();
    QStringList projects() const;
    QString description() const;
    QString artifactPath() const;
    Q_INVOKABLE bool createProject(const QString& id, const QString& name, const QString& specification, const QString& capability="none");
    Q_INVOKABLE bool openProject(const QString& manifestPath);
    Q_INVOKABLE void selectProject(int index);
    QString projectName() const;
    QString projectPath() const;
    QString revision() const;
    QString artifactHash() const;
    QString latestEvidence() const;
    QString latestSuccessfulEvidence() const {return m_latestSuccessfulEvidence;}
    int selectedProjectIndex() const;
    QString dspState() const;
    QString sdkState() const;
    QString metadataState() const;
    QString measurementState() const;
    QString history() const;
    Q_INVOKABLE void runSdkValidator();
    Q_INVOKABLE void runMetadataValidator();
    Q_INVOKABLE void runMeasurements();
    Q_INVOKABLE void openArtifact();
    Q_INVOKABLE void prepareRepair();
    QString buildState() const;
    QString validationState() const;
    QString guiLifecycleState() const;
    QStringList models() const;
    QStringList reasoningEfforts() const;
    QString selectedModel() const;
    QString selectedEffort() const;
    QString effectiveModel() const;
    QString authenticationState() const;
    Q_INVOKABLE void connectCodex();
    Q_INVOKABLE void selectModel(int index);
    Q_INVOKABLE void selectEffort(int index);
    Q_INVOKABLE void shutdownOwnedProcesses();
    QString codexState() const;
    QString codexPermissionState() const;
    QString output() const;
    QString prompt() const;
    void setPrompt(const QString& value);
    QString activeTask() const;
    QString approvalText() const;
    bool approvalPending() const;

    Q_INVOKABLE void refreshProject();
    Q_INVOKABLE void buildVst3();
    Q_INVOKABLE void runDspTests();
    Q_INVOKABLE void runPluginval(bool gui = true);
    Q_INVOKABLE void runReadOnlyCodexTurn();
    Q_INVOKABLE void submitCodexPrompt();
    Q_INVOKABLE void stopTask();
    Q_INVOKABLE void answerApproval(bool accept);
    Q_INVOKABLE void loadMeasurementReport();
    Q_INVOKABLE void openEvidenceFolder();

signals:
    void projectChanged();
    void modelsChanged();
    void codexStateChanged();
    void outputChanged();
    void promptChanged();
    void taskChanged();
    void approvalChanged();

private:
    void appendOutput(const QString& text);
    void startJob(const QString& name, const QString& program,
                  const QStringList& args, const QString& workingDirectory);
    void inspectAsync(const ProjectManifest& project, std::function<void(ProjectInspection)> done);
    void applyInspection(const ProjectInspection& inspection);
    void completeJob(int code, QProcess::ExitStatus status);
    bool prepareJob(const QString& name);
    void setJobOutcome(const QString& name, const QString& outcome);
    bool writeBuildStamp();
    bool hasCurrentBuild(QString& error) const;
    void startCodex();
    void beginThread();
    void beginTurn();
    void persistSession();
    QString sessionKey() const;
    void sendRpc(const QString& method, const QJsonObject& params, int purpose);
    void consumeCodexOutput();
    void handleCodexMessage(const QJsonObject& message);
    void finishCodexTurn(const QString& status);
    void recordVerification(const QString& task, const QString& status, int exitCode);

    QString m_specification,m_recoveryState,m_workflowState="Idle";
    QStringList m_verifyQueue;
    QVariantList m_evidenceEntries;
    bool m_evidenceSaved=false;
    ProjectManifest m_manifest;
    ProjectManifest m_jobManifest;
    ProjectInspection m_jobInspection, m_completedInspection;
    ToolchainProfile m_toolchain;
    bool m_inspectionBusy=false, m_prepared=false;
    quint64 m_projectGeneration=0;
    QString m_bundleHash;
    QString m_jobDependencyHash, m_jobBundleHash, m_jobManifestHash, m_jobId;
    QString m_jobMeasurementPath;
    bool m_jobDirty = false, m_jobLogFailed=false;
    QStringList m_projectFiles;
    QString m_projectPath;
    QString m_revision;
    QString m_sourceIdentity;
    QString m_jobSourceIdentity;
    QString m_jobRevision;
    QString m_jobLogPath;
    bool m_jobCanceled = false;
    bool m_jobTimedOut = false;
    QString m_artifactHash = "Not built";
    QString m_jobArtifactHash;
    QString m_latestSuccessfulEvidence="No current successful verification";
    QJsonObject m_statusOverrides;
    QString m_latestEvidence = "No recorded Desktop run";
    QDateTime m_jobStartedAt;
    QString m_dspState = "NOT RUN", m_sdkState = "NOT RUN", m_metadataState = "NOT RUN", m_measurementState = "NOT RUN";
    QString m_history;
    int m_repairAttempts = 0;
    QString m_lastFailureFingerprint;
    QString m_lastFailureLog;
    QString m_buildState = "NOT RUN";
    QString m_validationState = "NOT RUN";
    QString m_guiLifecycleState = "NOT RUN";
    QString m_codexState = "Disconnected";
    QString m_codexPermissionState = "Thread policy not reported";
    QString m_output;
    QString m_prompt;
    QString m_activeTask;
    QString m_approvalText;
    QString m_serverRequestMethod;
    QString m_agentMessageBuffer;
    bool m_approvalPending = false;
    bool m_agentMessageActive = false;
    QJsonArray m_models;
    QString m_selectedModel, m_selectedEffort, m_effectiveModel;
    QString m_authenticationState = "Not checked";
    QString m_threadId, m_savedThreadId, m_turnId;
    bool m_submitAfterConnect = false;
    bool m_interruptPending = false;
    bool m_shuttingDown = false;
    int m_nextRequestId = 1;
    QHash<int, int> m_pendingRequests;
    QJsonValue m_serverRequestId;
    CodexFrames m_codexFrames;
    QProcess m_job;
    QProcess m_codex;
    ActivityModel m_conversation, m_operations, m_buildLog;
    QString m_pendingConversation, m_pendingOperations, m_pendingBuild;
    QTimer m_streamTimer;
    QTimer m_jobTimeout;
    QTimer m_handshakeTimeout;
    QFileSystemWatcher m_artifactWatcher;
};
