#include "projectcontroller.hpp"
#include "projectscaffold.hpp"
#include "provenance.hpp"
#include "joboutcome.hpp"
#include <QUuid>
#include <QtConcurrent>
#include <QFutureWatcher>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QUrl>
#include <QDesktopServices>
#include <QDateTime>
#include <QSettings>
#include <QSaveFile>
#include <QStorageInfo>
#include <csignal>
#include <unistd.h>

namespace {
QString toolkitRoot(){return QFileInfo(QStringLiteral(STUDIOHUB_PROJECT_ROOT)).canonicalFilePath();}

QString modulePath(const ProjectManifest& project)
{
    return project.module;
}



}

ProjectController::ProjectController(QObject* parent) : QObject(parent),
    m_projectPath(QDir(toolkitRoot()).absolutePath())
{
    m_streamTimer.setInterval(40);
    connect(&m_streamTimer,&QTimer::timeout,this,&ProjectController::flushStreams);m_streamTimer.start();
    m_job.setChildProcessModifier([] { ::setsid(); });
    m_codex.setChildProcessModifier([] { ::setsid(); });
    connect(&m_job, &QProcess::readyReadStandardOutput, this, [this] {
        appendOutput(QString::fromLocal8Bit(m_job.readAllStandardOutput()));
    });
    connect(&m_job, &QProcess::readyReadStandardError, this, [this] {
        appendOutput(QString::fromLocal8Bit(m_job.readAllStandardError()));
    });
    connect(&m_job, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus status) {
        m_jobTimeout.stop();
        const auto project=m_jobManifest;
        inspectAsync(project,[this,code,status](ProjectInspection snapshot) {
            m_completedInspection=std::move(snapshot);completeJob(code,status);
        });
    });
    m_handshakeTimeout.setSingleShot(true);
    connect(&m_handshakeTimeout,&QTimer::timeout,this,[this] {
        appendOutput("App Server handshake timed out; owned transport stopped. Resume explicitly.\n");
        m_codexState="Handshake timeout";m_activeTask="Idle";
        m_submitAfterConnect=false;m_codex.kill();emit codexStateChanged();emit taskChanged();
    });
    connect(&m_jobTimeout, &QTimer::timeout, this, [this] {
        appendOutput("\n[process timed out; terminating owned process]\n");
        m_jobTimedOut = true;
        if (m_job.processId() > 0) ::kill(-static_cast<pid_t>(m_job.processId()), SIGKILL);
        m_job.kill();
    });
    connect(&m_artifactWatcher,&QFileSystemWatcher::fileChanged,this,[this](const QString&){refreshProject();});
    connect(&m_job,&QProcess::started,this,[this] {
        bool configured=false;const int timeout=qEnvironmentVariableIntValue("STUDIOHUB_TEST_JOB_TIMEOUT_MS",&configured);
        m_jobTimeout.start(configured?std::clamp(timeout,50,120000):20*60*1000);
        setJobOutcome(m_activeTask,"RUNNING");
    });
    connect(&m_job,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error) {
        if(error!=QProcess::FailedToStart)return;
        setJobOutcome(m_activeTask,"FAIL");recordVerification(m_activeTask,"FAIL",-1);
        appendOutput("Process could not start: "+m_job.errorString()+"\n");m_activeTask="Idle";emit taskChanged();
    });
    connect(&m_codex,&QProcess::started,this,&ProjectController::initializeCodex);
    connect(&m_codex,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error!=QProcess::FailedToStart)return;
        m_handshakeTimeout.stop();m_codexState="Unavailable: "+m_codex.errorString();m_activeTask="Idle";
        emit codexStateChanged();emit taskChanged();
    });
    connect(&m_codex, &QProcess::readyReadStandardOutput,
            this, &ProjectController::consumeCodexOutput);
    connect(&m_codex, &QProcess::readyReadStandardError, this, [this] {
        const auto diagnostics = QString::fromLocal8Bit(m_codex.readAllStandardError());
        if (!diagnostics.trimmed().isEmpty())
            appendOutput("[Codex diagnostics] " + diagnostics);
    });
    connect(&m_codex, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus status) {
        m_handshakeTimeout.stop();
        if(m_codexState=="Handshake timeout") {emit codexStateChanged();return;}
        if (m_activeTask.startsWith("Codex")) {
            if (!m_threadId.isEmpty()) {
                appendOutput(QStringLiteral("\n[Codex transport disconnected without turn/completed; exit %1. Resume explicitly; no commands will be replayed.]\n").arg(code));
                m_codexState = QStringLiteral("Disconnected — turn outcome unknown");
                m_activeTask = QStringLiteral("Idle");
                m_turnId.clear(); m_interruptPending = false;
                persistSession(); emit codexStateChanged(); emit taskChanged();
            } else {
                appendOutput(QStringLiteral("\n[Codex app-server exited before a thread was created; exit %1]\n").arg(code));
                m_codexState = status == QProcess::NormalExit
                    ? QStringLiteral("Exited (%1)").arg(code)
                    : QStringLiteral("Process error (%1)").arg(code);
                emit codexStateChanged();
                m_activeTask = QStringLiteral("Idle");
                emit taskChanged();
            }
        } else if (m_codexState == QStringLiteral("Turn completed") ||
                   m_codexState == QStringLiteral("Turn interrupted") ||
                   m_codexState == QStringLiteral("Turn failed")) {
            m_codexState += QStringLiteral(" · App Server exited (%1)").arg(code);
            emit codexStateChanged();
        } else {
            m_codexState = status == QProcess::NormalExit
                ? QStringLiteral("Exited (%1)").arg(code)
                : QStringLiteral("Process error (%1)").arg(code);
            emit codexStateChanged();
        }
    });
    auto sourcePoll = new QTimer(this);
    sourcePoll->setInterval(15000);
    connect(sourcePoll, &QTimer::timeout, this, [this] {
        if (m_job.state() == QProcess::NotRunning && !m_activeTask.startsWith("Codex") &&
            !m_manifest.root.isEmpty() && !m_inspectionBusy) refreshProject();
    });
    sourcePoll->start();
    QSettings settings;
    m_projectFiles = settings.value("projects").toStringList();
    const QString reference = m_projectPath + "/studiohub-project.json";
    if (!m_projectFiles.contains(reference)) m_projectFiles.prepend(reference);
    const auto last = settings.value("selectedProject", reference).toString();
    if (!openProject(last)) openProject(reference);
}

QStringList ProjectController::projects() const
{
    QStringList names;
    for (const auto& file : m_projectFiles) {
        ProjectManifest p; QString error;
        names << (ProjectManifest::load(file, p, error) ? p.name : "Unavailable: " + file);
    }
    return names;
}
QString ProjectController::description() const { return m_manifest.description; }
QString ProjectController::artifactPath() const { return m_manifest.bundle; }
QString ProjectController::projectName() const { return m_manifest.name; }
void ProjectController::selectProject(int index)
{
    if (index >= 0 && index < m_projectFiles.size()) openProject(m_projectFiles.at(index));
}
bool ProjectController::createProject(const QString& id, const QString& name, const QString& specification, const QString& capability)
{
    if (m_inspectionBusy || m_job.state() != QProcess::NotRunning || m_codex.state() != QProcess::NotRunning) {
        appendOutput("Stop the current session before creating a project.\n"); return false;
    }
    const QString toolchain = QFileInfo(toolkitRoot()).canonicalFilePath();
    QString error;
    if (ProjectManifest::safePath(toolchain, "projects", error).isEmpty()) { appendOutput("Project directory rejected: " + error + "\n"); return false; }
    const auto file = ProjectScaffold::create(toolchain + "/projects", toolchain + "/desktop/templates/vst3",
        toolchain, id, name, specification, error, capability);
    if (file.isEmpty()) { appendOutput("Project creation failed: " + error + "\n"); return false; }
    if (!openProject(file)) return false;
    setPrompt("Implement this plugin specification in the selected project. Use the existing skeleton, pinned SDK and VSTGUI. Add meaningful DSP tests and stereo measurements; build and repair failures. Do not modify the shared toolchain, normal REAPER, or protected audio software. Do not spawn agents. Specification:\n" + specification);
    appendOutput("Created project and Git workspace (no automatic commit): " + m_projectPath + "\n");
    return true;
}
bool ProjectController::openProject(const QString& path)
{
    if (m_inspectionBusy || m_job.state() != QProcess::NotRunning || m_codex.state() != QProcess::NotRunning) {
        appendOutput("Stop the owned task/App Server before changing projects.\n"); return false;
    }
    const auto file = QFileInfo(path).canonicalFilePath();
    ProjectManifest project; QString error;
    if (!ProjectManifest::load(file, project, error)) {
        appendOutput("Project rejected: " + error + "\n"); return false;
    }
    const auto reference = QFileInfo(toolkitRoot()).canonicalFilePath();
    if (project.root == reference && project.build != reference + "/build/desktop-vst3") {
        appendOutput("Reference builds must use build/desktop-vst3; protected artifacts cannot be selected for output.\n"); return false;
    }
    if (!m_artifactWatcher.files().isEmpty()) m_artifactWatcher.removePaths(m_artifactWatcher.files());
    m_shuttingDown=false;
    ++m_projectGeneration;
    m_manifest = project; m_projectPath = project.root;
    m_buildState = m_validationState = m_guiLifecycleState = "NOT RUN";
    m_dspState = m_sdkState = m_metadataState = m_measurementState = "NOT RUN";
    m_repairAttempts = 0; m_lastFailureFingerprint.clear(); m_history.clear();
    m_verifyQueue.clear();m_workflowState.clear();
    m_latestEvidence = "No current verification"; m_jobArtifactHash.clear();
    m_jobInspection={};m_completedInspection={};m_jobSourceIdentity.clear();m_jobBundleHash.clear();m_jobDependencyHash.clear();m_jobManifestHash.clear();
    m_threadId.clear(); m_turnId.clear(); m_prompt.clear(); emit promptChanged();
    QSettings threadSettings;
    m_toolchain=ToolchainProfile::load(toolkitRoot(),QJsonDocument::fromJson(threadSettings.value("toolchainOverrides").toByteArray()).object());
    m_threadId.clear(); m_turnId.clear(); m_interruptPending = false;
    m_agentMessageBuffer.clear(); m_agentMessageActive = false;
    m_codexPermissionState = "Thread policy not reported";
    m_codexState = "Disconnected";
    m_statusOverrides=QJsonDocument::fromJson(threadSettings.value(sessionKey()+"/jobStatuses").toByteArray()).object();
    m_savedThreadId = threadSettings.value(sessionKey() + "/thread").toString();
    m_selectedModel = "gpt-6-luna";
    m_selectedEffort.clear(); // Launch defaults follow the user preference and server catalog.
    m_pendingConversation.clear();m_pendingOperations.clear();m_pendingBuild.clear();
    m_conversation.restore(QJsonDocument::fromJson(threadSettings.value(sessionKey()+"/conversation").toByteArray()).array());
    m_operations.restore({});m_buildLog.restore({});
    m_output = threadSettings.value(sessionKey() + "/transcript").toString();
    if(m_conversation.rowCount()==0&&!m_output.isEmpty())m_conversation.append("Imported session transcript",m_output);
    emit outputChanged();
    m_effectiveModel.clear(); emit modelsChanged(); emit codexStateChanged();
    if (!m_projectFiles.contains(file)) m_projectFiles.append(file);
    QSettings settings; settings.setValue("projects", m_projectFiles);
    settings.setValue("selectedProject", file);
    refreshProject(); return true;
}
QString ProjectController::projectPath() const { return m_projectPath; }
QString ProjectController::revision() const { return m_revision; }
QString ProjectController::artifactHash() const { return m_artifactHash; }
QString ProjectController::latestEvidence() const { return m_latestEvidence; }
int ProjectController::selectedProjectIndex() const { return m_projectFiles.indexOf(m_projectPath + "/studiohub-project.json"); }
QString ProjectController::dspState() const { return m_dspState; }
QString ProjectController::sdkState() const { return m_sdkState; }
QString ProjectController::metadataState() const { return m_metadataState; }
QString ProjectController::measurementState() const { return m_measurementState; }
QString ProjectController::history() const { return m_history; }
QString ProjectController::buildState() const { return m_buildState; }
QString ProjectController::validationState() const { return m_validationState; }
QString ProjectController::guiLifecycleState() const { return m_guiLifecycleState; }
QString ProjectController::codexState() const { return m_codexState; }
QString ProjectController::codexPermissionState() const { return m_codexPermissionState; }
QString ProjectController::output() const { return m_output; }
QString ProjectController::prompt() const { return m_prompt; }
QString ProjectController::activeTask() const { return m_activeTask; }
QString ProjectController::approvalText() const { return m_approvalText; }
bool ProjectController::approvalPending() const { return m_approvalPending; }

void ProjectController::setPrompt(const QString& value)
{
    if (m_prompt == value) return;
    m_prompt = value;
    emit promptChanged();
}

void ProjectController::appendOutput(const QString& text)
{
    if (text.isEmpty()) return;
    if (!m_jobLogPath.isEmpty() && m_job.state() != QProcess::NotRunning) {
        QFile log(m_jobLogPath);
        if (log.open(QIODevice::Append)) {
            constexpr qint64 limit = 8 * 1024 * 1024;
            if (log.size() < limit) {
                const auto bytes = text.toUtf8();
                const auto captured=bytes.left(limit-log.size());
                if(log.write(captured)!=captured.size()||!log.flush())m_jobLogFailed=true;
                if (log.size() >= limit && log.write("\n[Log capture capped at 8 MiB; process status remains authoritative.]\n")<0)m_jobLogFailed=true;
            }
        } else m_jobLogFailed=true;
    }
    m_output += text;
    constexpr qsizetype maxChars = 120000;
    if (m_output.size() > maxChars)
        m_output.remove(0, m_output.size() - maxChars);
    m_pendingBuild += text;
}

void ProjectController::refreshProject() {
    if(m_inspectionBusy || m_job.state()!=QProcess::NotRunning)return;
    inspectAsync(m_manifest,[this](const ProjectInspection& snapshot){applyInspection(snapshot);});
}
void ProjectController::applyInspection(const ProjectInspection& snapshot) {
    const bool artifactChanged=!m_bundleHash.isEmpty() && snapshot.bundle!=m_bundleHash;
    m_history.clear();m_evidenceEntries.clear();m_specification=snapshot.specification;
    QSettings recovery;const auto unfinished=QJsonDocument::fromJson(recovery.value(sessionKey()+"/unfinishedJob").toByteArray()).object();
    m_recoveryState=unfinished.isEmpty()?QString{}:"Interrupted "+unfinished.value("task").toString()+" — explicit retry required";
    const auto unresolved=recovery.value(sessionKey()+"/unresolvedTurn").toString();
    if(!unresolved.isEmpty())m_recoveryState+="\nCodex turn "+unresolved+" outcome unresolved — reconnect explicitly; no task will be replayed";
    m_sourceIdentity=snapshot.source;m_bundleHash=snapshot.bundle;
    m_revision=snapshot.buildStamp.value("inspection_branch").toString()+" · "+snapshot.revision;
    m_validationState="NOT RUN";m_latestEvidence="No current verification";m_latestSuccessfulEvidence="No current successful verification";
    m_buildState = m_dspState = m_sdkState = m_metadataState = m_measurementState = m_guiLifecycleState = "NOT RUN";
    m_artifactHash = snapshot.module;
    if (m_artifactHash.isEmpty()) m_artifactHash = QStringLiteral("Not built");
    if (QFileInfo::exists(modulePath(m_manifest)))
        m_artifactWatcher.addPath(modulePath(m_manifest));
    if (artifactChanged) {
        m_buildState=m_dspState=m_sdkState=m_metadataState=m_measurementState=m_guiLifecycleState="STALE — artifact changed";
    }
    if (snapshot.dirty) {
        m_revision += " +dirty";
        m_buildState = QStringLiteral("DIRTY — rebuild required");
        m_validationState = QStringLiteral("STALE — source changed");
    }
    {
        const auto lines = snapshot.history.split('\n');
        for (const auto& line : lines) {
            const auto document = QJsonDocument::fromJson(line);
            if (!document.isObject()) continue;
            const auto entry = document.object();
            auto displayEntry=entry.toVariantMap();displayEntry["label"]=entry.value("finished_at_utc").toString()+" · "+entry.value("task").toString()+" · "+entry.value("status").toString();
            m_evidenceEntries.prepend(displayEntry);
            m_history += entry.value("finished_at_utc").toString() + " · " + entry.value("task").toString() + " · " + entry.value("status").toString() + " · " + entry.value("artifact_sha256").toString().left(12) + "\n";
            const auto toolPath=entry.value("tool_path").toString();
            if(!toolPath.isEmpty() && snapshot.toolHashes.value(toolPath).toString()!=entry.value("tool_sha256").toString())continue;
            if(!entry.value("adapter_sha256").toString().isEmpty() && entry.value("adapter_sha256").toString()!=snapshot.toolHashes.value(toolkitRoot()+"/desktop/run-isolated-validator.py").toString())continue;
            if (entry.value("project_id").toString() != m_manifest.id ||
                entry.value("bundle_sha256").toString() != snapshot.bundle ||
                entry.value("source_fingerprint").toString() != m_sourceIdentity ||
                entry.value("dependency_fingerprint").toString() != snapshot.dependency ||
                m_sourceIdentity.isEmpty() || entry.value("artifact_sha256").toString() != m_artifactHash)
                continue;
            const auto task = entry.value("task").toString();
            if(task!="Building VST3"&&task!="Configuring isolated VST3 build"&&entry.value("build_products").toObject()!=snapshot.products)continue;
            const auto status = entry.value("status").toString();
            const auto hash = m_artifactHash.left(12);
            m_latestEvidence = QStringLiteral("%1 · %2 · %3")
                .arg(task, status, entry.value("finished_at_utc").toString());
            if(status=="PASS")m_latestSuccessfulEvidence=m_latestEvidence;
            if (task == "Building VST3") m_buildState = status;
            if (task == "Running DSP tests") m_dspState = status;
            if (task == "Steinberg validator") m_sdkState = status;
            if (task == "Plugin metadata validator") m_metadataState = status;
            if (task == "Running measurements") m_measurementState = status;
            if (task == "pluginval strictness 5 full GUI")
                m_guiLifecycleState = status == "PASS"
                    ? QStringLiteral("PASS · %1").arg(hash)
                    : QStringLiteral("%1 · %2").arg(status, hash);
            if (task.contains("pluginval"))
                m_validationState = status == "PASS"
                    ? QStringLiteral("PASS — pluginval · %1").arg(hash)
                    : QStringLiteral("%1 — pluginval · %2").arg(status, hash);
            if (task == "Running DSP tests")
                m_validationState = status == "PASS"
                    ? QStringLiteral("PASS — DSP · %1").arg(hash)
                    : QStringLiteral("FAIL — DSP · %1").arg(hash);
            if (status == "STALE") {
                m_latestEvidence = QStringLiteral("Last run %1 · different artifact %2")
                    .arg(entry.value("finished_at_utc").toString(),
                         entry.value("artifact_sha256").toString().left(12));
            }
        }
    }
    if(!snapshot.buildStamp.isEmpty() && snapshot.buildStamp.value("products").toObject()!=snapshot.products) {
        m_buildState="STALE — build products changed";
        m_dspState=m_sdkState=m_metadataState=m_measurementState=m_guiLifecycleState="STALE";
    }
    const bool inputsChanged=!snapshot.buildStamp.isEmpty() &&
        (snapshot.buildStamp.value("products").toObject()!=snapshot.products ||
         snapshot.buildStamp.value("source_fingerprint").toString()!=snapshot.source ||
         snapshot.buildStamp.value("dependency_fingerprint").toString()!=snapshot.dependency);
    m_workflowState=Job::workflowAfterInspection(m_workflowState,inputsChanged);
    for(auto it=m_statusOverrides.begin();it!=m_statusOverrides.end();++it) {
        const auto saved=it.value().toObject();
        if(!saved.value("bundle").toString().isEmpty()&&saved.value("bundle").toString()!=snapshot.bundle)continue;
        if(!saved.value("source").toString().isEmpty()&&saved.value("source").toString()!=snapshot.source)continue;
        const auto status=saved.value("status").toString();if(status=="RUNNING"||status=="PASS")continue;
        const auto name=it.key();
        if(name=="Running DSP tests")m_dspState=status;
        if(name=="Building VST3"||name=="Configuring isolated VST3 build")m_buildState=status;
        if(name=="pluginval strictness 5 non-GUI")m_validationState=status;
        if(name=="Plugin metadata validator")m_metadataState=status;
        if(name=="Steinberg validator")m_sdkState=status;
        if(name=="Running measurements")m_measurementState=status;
        if(name=="pluginval strictness 5 full GUI")m_guiLifecycleState=status;
    }
    emit projectChanged();
}

void ProjectController::startJob(const QString& name, const QString& program,
                                 const QStringList& args, const QString& workingDirectory)
{
    if (m_job.state() != QProcess::NotRunning || m_activeTask.startsWith("Codex")) return;
    m_jobArtifactHash=m_jobInspection.module;
    if (Provenance::fileHash(m_projectPath + "/studiohub-project.json") != m_jobManifestHash) {
        setJobOutcome(name, "BLOCKED"); appendOutput("Manifest changed during job preparation; retry explicitly.\n"); return;
    }
    m_jobCanceled = m_jobTimedOut = m_jobLogFailed = false;
    QDir().mkpath(m_manifest.evidence);
    if (QDir(m_manifest.evidence).entryList({"*.log"}, QDir::Files).size() >= 500) {
        setJobOutcome(name,"BLOCKED");
        appendOutput("Job blocked: evidence directory has 500 logs. Archive reviewed evidence before continuing. No files deleted.\n");
        return;
    }
    m_jobId = name=="Running measurements" ? QFileInfo(QFileInfo(m_jobMeasurementPath).absolutePath()).fileName() : QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_jobLogPath = m_manifest.evidence + "/" + m_jobId + ".log";
    m_activeTask = name;
    QSettings recovery;recovery.setValue(sessionKey()+"/unfinishedJob",QJsonDocument(QJsonObject{{"task",name},{"project_id",m_manifest.id},{"program",program},{"arguments",QJsonArray::fromStringList(args)}}).toJson(QJsonDocument::Compact));recovery.sync();
    m_jobStartedAt = QDateTime::currentDateTimeUtc();
    emit taskChanged();
    appendOutput(QStringLiteral("\n$ %1 %2\n").arg(program, args.join(' ')));
    QFile initialLog(m_jobLogPath);
    const auto header=("$ "+program+" "+args.join(' ')+"\n").toUtf8();
    if(!initialLog.open(QIODevice::WriteOnly)||initialLog.write(header)!=header.size()||!initialLog.flush()) {
        m_activeTask="Idle";setJobOutcome(name,"EVIDENCE ERROR");appendOutput("Job not launched: initial evidence log could not be saved.\n");emit taskChanged();return;
    }
    auto environment = QProcessEnvironment::systemEnvironment();
    if (name.contains("pluginval") || name.contains("validator")) {
        const auto isolated = m_manifest.evidence + "/host-environment";
        QDir().mkpath(isolated + "/home"); QDir().mkpath(isolated + "/config");
        QDir().mkpath(isolated + "/data"); QDir().mkpath(isolated + "/cache"); QDir().mkpath(isolated + "/empty-plugins");
        environment.insert("HOME", isolated + "/home");
        environment.insert("XDG_CONFIG_HOME", isolated + "/config");
        environment.insert("XDG_DATA_HOME", isolated + "/data");
        environment.insert("XDG_CACHE_HOME", isolated + "/cache");
        for (const auto& key : {"VST_PATH", "VST3_PATH", "LV2_PATH", "CLAP_PATH", "DSSI_PATH", "LADSPA_PATH"})
            environment.insert(key, isolated + "/empty-plugins");
        environment.insert("GDK_BACKEND", "x11");
    }
    m_job.setProcessEnvironment(environment);
    m_job.setProgram(program);
    m_job.setArguments(args);
    m_job.setWorkingDirectory(workingDirectory);
    m_job.setProcessChannelMode(QProcess::SeparateChannels);
    m_job.start();

}

void ProjectController::buildVst3()
{
    if (!prepareJob("Configuring isolated VST3 build")) return;
    const auto build = m_manifest.build;
    if(m_jobInspection.buildStamp.value("configuration_fingerprint").toString()==m_jobInspection.configuration && !m_jobInspection.products.value("cmake_cache").toString().isEmpty()) {
        QStringList args{"--build",build,"--target"};args<<m_manifest.targets<<("-j"+QString::number(m_toolchain.jobs));
        if(Provenance::compilerChanged(m_jobInspection.buildStamp,m_jobInspection.products)){args<<"--clean-first";appendOutput("Compiler identity changed; rebuilding candidate outputs with --clean-first.\n");}
        startJob("Building VST3",m_toolchain.cmake,args,m_projectPath);return;
    }
    QStringList configure{"-S", m_manifest.source, "-B", build, "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=" + m_manifest.configuration};
    configure << m_manifest.definitions << ("-DSTUDIOHUB_SDK_ROOT="+m_toolchain.sdk) << ("-DSTUDIOHUB_TOOLKIT_ROOT="+toolkitRoot());
    startJob("Configuring isolated VST3 build", m_toolchain.cmake, configure, m_projectPath);
}

void ProjectController::runDspTests()
{
    if (!prepareJob("Running DSP tests")) return;
    m_jobArtifactHash = m_jobInspection.module;
    startJob("Running DSP tests", m_toolchain.ctest,
        {"--test-dir", m_manifest.build, "--output-on-failure", "--no-tests=error",
         "-R", "^(" + m_manifest.tests.join("|") + ")$"}, m_projectPath);
}

void ProjectController::runPluginval(bool gui)
{
    if (!prepareJob(gui ? "pluginval strictness 5 full GUI" : "pluginval strictness 5 non-GUI")) return;
    if (m_toolchain.pluginval.isEmpty()) {
        appendOutput("pluginval is not configured. Set it in desktop/toolchain-profile.json or a project toolchain override.");
        setJobOutcome(gui ? "pluginval strictness 5 full GUI" : "pluginval strictness 5 non-GUI", "BLOCKED");
        emit projectChanged();
        return;
    }
    const auto bundle = m_manifest.bundle;
    if (!QFileInfo::exists(bundle)) {
        appendOutput("The isolated Desktop VST3 artifact has not been built yet.\n");
        setJobOutcome(gui ? "pluginval strictness 5 full GUI" : "pluginval strictness 5 non-GUI", "BLOCKED");
        emit projectChanged();
        return;
    }
    m_jobArtifactHash = m_jobInspection.module;
    const auto adapter = toolkitRoot() + "/desktop/run-isolated-validator.py";
    QStringList args{adapter, "--kind", gui ? "gui" : "non-gui", "--tool", m_toolchain.pluginval,
                     "--bundle", bundle, "--evidence", m_manifest.evidence + "/pluginval"};
    startJob(gui ? "pluginval strictness 5 full GUI" : "pluginval strictness 5 non-GUI", "python3", args, m_projectPath);
}

void ProjectController::runSdkValidator()
{
    if (!prepareJob("Steinberg validator")) return;
    m_jobArtifactHash = m_jobInspection.module;
    if (m_jobArtifactHash.isEmpty()) { m_sdkState = "BLOCKED — artifact missing"; emit projectChanged(); return; }
    startJob("Steinberg validator", "python3", {toolkitRoot() + "/desktop/run-isolated-validator.py",
        "--kind", "steinberg", "--tool", m_manifest.build + "/bin/" + m_manifest.configuration + "/validator",
        "--bundle", m_manifest.bundle, "--evidence", m_manifest.evidence + "/steinberg"}, m_projectPath);
}
void ProjectController::runMetadataValidator()
{
    if (!prepareJob("Plugin metadata validator")) return;
    m_jobArtifactHash = m_jobInspection.module;
    if (m_jobArtifactHash.isEmpty()) { m_metadataState = "BLOCKED — artifact missing"; emit projectChanged(); return; }
    startJob("Plugin metadata validator", "python3", {toolkitRoot() + "/desktop/run-isolated-validator.py",
        "--kind", "metadata", "--tool", m_manifest.build + "/bin/" + m_manifest.configuration + "/moduleinfotool",
        "--bundle", m_manifest.bundle, "--evidence", m_manifest.evidence + "/metadata"}, m_projectPath);
}
void ProjectController::runMeasurements()
{
    if (!prepareJob("Running measurements")) return;
    m_jobArtifactHash = m_jobInspection.module;
    if (m_manifest.capability == "none" || m_jobArtifactHash.isEmpty()) {
        m_measurementState = "BLOCKED — unsupported capability or missing artifact"; emit projectChanged(); return;
    }
    QDir().mkpath(QFileInfo(m_manifest.measurement).absolutePath());
    const auto directory=m_manifest.evidence + "/measurements/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir().mkpath(directory);
    m_jobMeasurementPath=directory+"/measurements.json";
    QStringList args;
    if (m_manifest.capability == "dynamics") args << directory << m_revision << m_jobArtifactHash;
    else args << m_jobMeasurementPath;
    startJob("Running measurements", m_manifest.measurementExecutable, args, m_projectPath);
}
void ProjectController::openArtifact()
{
    if (!QFileInfo::exists(m_manifest.bundle)) { appendOutput("Artifact is missing.\n"); return; }
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_manifest.bundle).absolutePath()));
}
void ProjectController::prepareRepair()
{
    if (m_lastFailureLog.isEmpty() || m_repairAttempts >= 3) {
        appendOutput("Repair blocked: no failed job, or three repair prompts already prepared. Review evidence manually.\n"); return;
    }
    QSettings settings;
    const auto key = sessionKey() + "/lastRepairFingerprint";
    if (settings.value(key).toString() == m_lastFailureFingerprint) {
        appendOutput("Identical failed attempt already submitted for repair; produce new diagnostic evidence first.\n"); return;
    }
    QFile log(m_lastFailureLog);
    if (!log.open(QIODevice::ReadOnly)) { appendOutput("Failure evidence is missing.\n"); return; }
    const auto data = log.readAll().right(16000);
    ++m_repairAttempts; settings.setValue(key, m_lastFailureFingerprint);
    setPrompt("Diagnose and minimally repair the latest failed engineering job in this project. Preserve tests and failure evidence; do not modify shared toolchain or protected audio software. Retest the exact affected target. Failure log: " + m_lastFailureLog + "\n" + QString::fromUtf8(data));
    appendOutput("Repair prompt prepared; submit explicitly after review.\n");
}

void ProjectController::recordVerification(const QString& task, const QString& status, int exitCode)
{
    m_evidenceSaved=false;
    if(m_jobLogFailed){setJobOutcome(task,"EVIDENCE ERROR");appendOutput("Execution "+status+"; job log write failed. Evidence incomplete.\n");m_latestEvidence="EVIDENCE ERROR";return;}
    if (m_jobArtifactHash.isEmpty()) m_jobArtifactHash = m_jobInspection.module;
    const auto finishedAt = QDateTime::currentDateTimeUtc();
    auto toolPath = m_job.program();
    if (!QDir::isAbsolutePath(toolPath)) toolPath = QStandardPaths::findExecutable(toolPath);
    QString adapterHash;
    const auto args=m_job.arguments();
    if(args.contains("--tool")) {
        adapterHash=m_jobInspection.toolHashes.value(args.value(0)).toString();
        toolPath=args.value(args.indexOf("--tool")+1);
    }
    const QString toolHash=m_jobInspection.toolHashes.value(toolPath).toString();
    const QJsonObject entry{
        {"task", task}, {"status", status}, {"exit_code", exitCode},
        {"started_at_utc", m_jobStartedAt.toString(Qt::ISODateWithMs)},
        {"finished_at_utc", finishedAt.toString(Qt::ISODateWithMs)},
        {"project_id", m_manifest.id}, {"project_path", m_projectPath}, {"source_revision", m_jobRevision}, {"source_fingerprint", m_jobSourceIdentity}, {"log_path", m_jobLogPath},
        {"configuration", m_manifest.configuration}, {"artifact_path", m_manifest.bundle},
        {"dependency_identity", m_manifest.dependencyIdentity}, {"dependency_fingerprint", m_jobDependencyHash},
        {"measurement_report_path",task=="Running measurements"?m_jobMeasurementPath:QString{}},
        {"build_products",m_jobInspection.products}, {"source_dirty", m_jobDirty}, {"job_id",m_jobId}, {"bundle_sha256",m_jobBundleHash},
        {"tool", QFileInfo(toolPath).fileName()}, {"tool_path",toolPath}, {"adapter_sha256",adapterHash}, {"tool_sha256", toolHash},
        {"tool_version",m_jobInspection.toolVersions.value(toolPath).toString(toolHash.isEmpty()?"Unavailable":"content SHA-256 "+toolHash)},
        {"program", m_job.program()}, {"arguments", QJsonArray::fromStringList(m_job.arguments())},
        {"artifact_sha256", m_jobArtifactHash}
    };
    const QString historyPath = m_manifest.evidence + "/validation-history.jsonl";
    QDir().mkpath(QFileInfo(historyPath).absolutePath());
    QFile history(historyPath);
    if (history.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        const auto bytes=QJsonDocument(entry).toJson(QJsonDocument::Compact)+'\n';
        if(history.write(bytes)!=bytes.size() || !history.flush()) {
            setJobOutcome(task,"EVIDENCE ERROR");appendOutput("Execution "+status+"; evidence write/flush failed.\n");m_latestEvidence="EVIDENCE ERROR";return;
        }
    }
    if (history.error() != QFileDevice::NoError || !history.isOpen()) {
        setJobOutcome(task, "EVIDENCE ERROR");
        appendOutput("Execution outcome " + status + "; evidence could not be saved: " + history.errorString() + "\n");
        m_latestEvidence="EVIDENCE ERROR"; return;
    }
    m_latestEvidence = QStringLiteral("%1 · %2 · %3")
        .arg(task, status, finishedAt.toString(Qt::ISODateWithMs));
    m_history += m_latestEvidence + " · " + m_jobArtifactHash.left(12) + "\n";
    auto displayEntry=entry.toVariantMap();displayEntry["label"]=task+" · "+status+" · "+finishedAt.toString(Qt::ISODateWithMs);
    m_evidenceEntries.prepend(displayEntry);m_evidenceSaved=true;
    if(status=="PASS")m_latestSuccessfulEvidence=m_latestEvidence;
    QSettings recovery;recovery.remove(sessionKey()+"/unfinishedJob");m_recoveryState.clear();
}

void ProjectController::startCodex()
{
    if (m_codex.state() != QProcess::NotRunning) return;
    m_shuttingDown=false;
    m_codexFrames.reset();
    m_threadId.clear(); m_pendingRequests.clear(); m_turnId.clear();
    m_codexState = QStringLiteral("Starting app-server");
    emit codexStateChanged();
    m_activeTask = QStringLiteral("Codex initialization");
    emit taskChanged();
    m_codex.setProgram(qEnvironmentVariable("STUDIOHUB_CODEX_EXECUTABLE", "codex"));
    m_codex.setArguments({QStringLiteral("app-server"), QStringLiteral("--listen"), QStringLiteral("stdio://")});
    m_codex.setWorkingDirectory(m_projectPath);
    m_codex.setProcessChannelMode(QProcess::SeparateChannels);
    m_codex.start();
    bool deadlineConfigured=false;const int deadline=qEnvironmentVariableIntValue("STUDIOHUB_TEST_HANDSHAKE_MS",&deadlineConfigured);
    m_handshakeTimeout.start(deadlineConfigured?std::clamp(deadline,50,120000):30000);
}
void ProjectController::initializeCodex() {
    bool timeoutConfigured=false;const int deadline=qEnvironmentVariableIntValue("STUDIOHUB_TEST_HANDSHAKE_MS",&timeoutConfigured);
    m_handshakeTimeout.start(timeoutConfigured?std::clamp(deadline,50,120000):30000);
    const int id = m_nextRequestId++;
    m_pendingRequests.insert(id, 1);
    QJsonObject clientInfo{{"name", "studiohub_desktop"}, {"title", "StudioHub Desktop"},
                           {"version", QStringLiteral(STUDIOHUB_DESKTOP_VERSION)}};
    const auto request = QJsonDocument(QJsonObject{
        {"jsonrpc", "2.0"}, {"id", id}, {"method", "initialize"},
        {"params", QJsonObject{{"clientInfo", clientInfo}, {"capabilities", QJsonObject{}}}}
    }).toJson(QJsonDocument::Compact) + '\n';
    m_codex.write(request);
}

void ProjectController::sendRpc(const QString& method, const QJsonObject& params, int purpose)
{
    const int id = m_nextRequestId++;
    m_pendingRequests.insert(id, purpose);
    const auto line = QJsonDocument(QJsonObject{{"jsonrpc", "2.0"}, {"id", id},
        {"method", method}, {"params", params}}).toJson(QJsonDocument::Compact) + '\n';
    m_codex.write(line);
}

void ProjectController::runReadOnlyCodexTurn()
{
    setPrompt(QStringLiteral("Inspect the selected plugin project and summarize its current build targets and latest validation evidence. Do not modify files."));
    submitCodexPrompt();
}

QString ProjectController::sessionKey() const
{
    return "sessions/" + QString::fromLatin1(QCryptographicHash::hash(m_projectPath.toUtf8(), QCryptographicHash::Sha256).toHex());
}
void ProjectController::persistSession()
{
    QSettings s;
    if(!m_turnId.isEmpty())s.setValue(sessionKey()+"/unresolvedTurn",m_turnId);
    s.setValue(sessionKey() + "/thread", m_savedThreadId);
    s.setValue(sessionKey() + "/model", m_selectedModel);
    s.setValue(sessionKey() + "/effort", m_selectedEffort);
    flushStreams();
    s.setValue(sessionKey() + "/transcript", m_output);
    s.setValue(sessionKey()+"/conversation",QJsonDocument(m_conversation.serialize()).toJson(QJsonDocument::Compact));
    s.sync();
}
QStringList ProjectController::models() const
{
    QStringList values; for (const auto& item : m_models) values << item.toObject().value("model").toString();
    return values;
}
QStringList ProjectController::reasoningEfforts() const
{
    QStringList values;
    for (const auto& item : m_models) {
        const auto o = item.toObject();
        if (o.value("model").toString() == m_selectedModel)
            for (const auto& e : o.value("supportedReasoningEfforts").toArray())
                values << e.toObject().value("reasoningEffort").toString();
    }
    return values;
}
QString ProjectController::selectedModel() const { return m_selectedModel; }
QString ProjectController::selectedEffort() const { return m_selectedEffort; }
QString ProjectController::effectiveModel() const { return m_effectiveModel; }
QString ProjectController::authenticationState() const { return m_authenticationState; }
void ProjectController::selectModel(int index)
{
    if (m_activeTask.startsWith("Codex") || index < 0 || index >= m_models.size()) return;
    const auto o = m_models.at(index).toObject(); m_selectedModel = o.value("model").toString();
    m_selectedEffort = o.value("defaultReasoningEffort").toString();
    persistSession(); emit modelsChanged();
}
void ProjectController::selectEffort(int index)
{
    const auto efforts = reasoningEfforts();
    if (m_activeTask.startsWith("Codex") || index < 0 || index >= efforts.size()) return;
    m_selectedEffort = efforts.at(index); persistSession(); emit modelsChanged();
}
void ProjectController::connectCodex()
{
    if (m_job.state() != QProcess::NotRunning || m_activeTask.startsWith("Codex")) return;
    m_submitAfterConnect = false; startCodex();
}
void ProjectController::submitCodexPrompt()
{
    if (m_prompt.trimmed().isEmpty()) return;
    if (m_inspectionBusy || m_job.state() != QProcess::NotRunning || m_activeTask.startsWith("Codex")) {
        appendOutput("A task is active; wait or interrupt it before submitting.\n"); return;
    }
    appendOutput("\n[User]\n" + m_prompt + "\n");
    m_submitAfterConnect = true;
    if (m_codex.state() == QProcess::NotRunning) startCodex();
    else if (!m_threadId.isEmpty()) beginTurn();
    else beginThread();
}
void ProjectController::beginThread()
{
    if (m_selectedModel.isEmpty() || !models().contains(m_selectedModel) ||
        !reasoningEfforts().contains(m_selectedEffort)) {
        appendOutput("Selected model/effort unavailable in the server catalog; select explicitly.\n");
        m_activeTask = "Idle"; m_codexState = "Model selection required";
        emit taskChanged(); emit codexStateChanged(); return;
    }
    QJsonObject params{{"cwd", m_projectPath}, {"model", m_selectedModel},
        {"config", QJsonObject{{"model_reasoning_effort", m_selectedEffort}}}};
    m_activeTask = "Codex thread initialization"; m_handshakeTimeout.start(30000); emit taskChanged();
    if (!m_savedThreadId.isEmpty()) {
        params["threadId"] = m_savedThreadId; sendRpc("thread/resume", params, 2);
    } else sendRpc("thread/start", params, 2);
}
void ProjectController::beginTurn()
{
    if (!models().contains(m_selectedModel) || !reasoningEfforts().contains(m_selectedEffort)) {
        appendOutput("Cannot start turn with an unsupported model or effort.\n"); return;
    }
    m_submitAfterConnect = false; m_interruptPending = false;
    m_codexState = "Turn starting"; emit codexStateChanged();
    m_activeTask = "Codex turn starting"; m_codexState = "Turn starting"; emit codexStateChanged(); emit taskChanged();
    flushStreams();m_conversation.append("user",m_prompt);
    const QJsonArray input{QJsonObject{{"type", "text"}, {"text", m_prompt}}};
    sendRpc("turn/start", QJsonObject{{"threadId", m_threadId}, {"input", input},
        {"cwd", m_projectPath}, {"model", m_selectedModel}, {"effort", m_selectedEffort}}, 3);
    persistSession();
}

void ProjectController::consumeCodexOutput() {
    QString error;const auto messages=m_codexFrames.feed(m_codex.readAllStandardOutput(),error);
    if(!error.isEmpty()) {
        appendOutput(error+"; owned transport disconnected without claiming success.\n");
        m_codexState="Protocol error";m_codex.kill();emit codexStateChanged();return;
    }
    for(const auto& message:messages)handleCodexMessage(message);
}

void ProjectController::handleCodexMessage(const QJsonObject& message)
{
    if (message.contains("id") && message.contains("method")) {
        const auto method = message.value("method").toString();
        m_serverRequestId = message.value("id");
        m_serverRequestMethod = method;
        m_approvalText = method + "\n" + QString::fromUtf8(
            QJsonDocument(message.value("params").toObject()).toJson(QJsonDocument::Indented));
        m_approvalPending = true;
        emit approvalChanged();

        answerApproval(true);
        return;
    }
    if (message.contains("method")) {
        const auto method = message.value("method").toString();
        const auto params = message.value("params").toObject();
        if (method == "item/agentMessage/delta") {
            const auto delta = params.value("delta").toString();
            if (!m_agentMessageActive) {
                appendOutput("\n[Codex assistant]\n");
                m_agentMessageActive = true;
                m_agentMessageBuffer.clear();
            }
            m_agentMessageBuffer += delta;
            m_pendingConversation += delta;
            appendOutput(delta);
        } else if (method == "turn/completed") {
            const auto turn=params.value("turn").toObject();
            if(!m_activeTask.startsWith("Codex") || (!params.value("threadId").toString().isEmpty()&&params.value("threadId").toString()!=m_threadId) ||
               (!m_turnId.isEmpty()&&turn.value("id").toString()!=m_turnId)) {appendOutput("Ignored terminal event for an inactive/different turn.\n");return;}
            const auto status = params.value("turn").toObject().value("status").toString();
            finishCodexTurn(status);
        } else if (method == "item/completed") {
            const auto item = params.value("item").toObject();
            if (item.value("type").toString() == "agentMessage") {
                const auto completeText = item.value("text").toString();
                if (!completeText.isEmpty() && completeText != m_agentMessageBuffer) {
                    if (completeText.startsWith(m_agentMessageBuffer)) {
                        const auto remainder=completeText.mid(m_agentMessageBuffer.size());
                        m_pendingConversation+=remainder;appendOutput(remainder);
                    } else {
                        flushStreams();m_conversation.append("assistant",completeText);
                        appendOutput(completeText + "\n");
                    }
                }
                m_agentMessageBuffer.clear();
                m_agentMessageActive = false;
            } else {
                appendOutput("\n[Codex] item/completed\n");
            }
        } else if (method == "item/commandExecution/outputDelta") {
            m_pendingOperations += params.value("delta").toString();
            appendOutput(params.value("delta").toString());
        } else if (method == "item/started") {
            const auto item = params.value("item").toObject();
            if (item.value("type").toString() == "commandExecution") {
                flushStreams();m_operations.append("command",item.value("command").toString()+"\ncwd: "+item.value("cwd").toString());
                appendOutput(QStringLiteral("\n[Codex command · cwd %1]\n%2\n")
                    .arg(item.value("cwd").toString(), item.value("command").toString()));
            } else {
                appendOutput("\n[Codex] item/started · " + item.value("type").toString() + "\n");
            }
        } else if (method == "turn/started") {
            m_turnId = params.value("turn").toObject().value("id").toString();
            persistSession();
            appendOutput("\n[Codex] turn/started\n");
        } else if (method.contains("approval")) {
            m_approvalText = method + "\n" + QString::fromUtf8(
                QJsonDocument(params).toJson(QJsonDocument::Indented));
            m_approvalPending = true;
            m_serverRequestId = message.value("id");
            m_serverRequestMethod = method;
            emit approvalChanged();
            answerApproval(true);
        }
        return;
    }
    if (message.contains("id") && (message.contains("result") || message.contains("error"))) {
        const int id = message.value("id").toInt(-1);
        const int purpose = m_pendingRequests.take(id);
        if(purpose==2 || purpose==3 || (purpose==4 && message.value("result").toObject().value("nextCursor").toString().isEmpty()))m_handshakeTimeout.stop();
        if (message.contains("error")) {
            appendOutput("\n[Codex RPC error] " + QString::fromUtf8(
                QJsonDocument(message.value("error").toObject()).toJson(QJsonDocument::Compact)) + "\n");
            m_codexState = QStringLiteral("Protocol error");
            emit codexStateChanged();
            if (purpose == 1 || purpose == 2 || purpose == 3 || purpose == 4) {
                m_activeTask = QStringLiteral("Idle");
                emit taskChanged();
                m_codex.terminate();
            }
            return;
        }
        const auto result = message.value("result").toObject();
        if (purpose == 1) {
            const auto notification = QJsonDocument(QJsonObject{
                {"jsonrpc", "2.0"}, {"method", "initialized"}, {"params", QJsonObject{}}
            }).toJson(QJsonDocument::Compact) + '\n';
            m_codex.write(notification);
            m_codexState = QStringLiteral("Connected");
            emit codexStateChanged();
            sendRpc("account/read", QJsonObject{{"refreshToken", false}}, 5);
            m_models = QJsonArray{};
            sendRpc("model/list", QJsonObject{{"limit", 100}}, 4);
        } else if (purpose == 2) {
            const auto sandbox = result.value("sandbox").toObject();
            const auto sandboxType = sandbox.value("type").toString();
            const auto approval = result.value("approvalPolicy");
            QString approvalText;
            if (approval.isString()) {
                approvalText = approval.toString();
            } else if (approval.isObject()) {
                approvalText = QStringLiteral("granular");
            }
            const auto reviewer = result.value("approvalsReviewer").toString();
            QStringList policyParts;
            policyParts << (sandboxType.isEmpty()
                ? QStringLiteral("sandbox unreported")
                : QStringLiteral("sandbox %1").arg(sandboxType));
            policyParts << (approvalText.isEmpty()
                ? QStringLiteral("approval policy unreported")
                : QStringLiteral("approval %1").arg(approvalText));
            if (!reviewer.isEmpty())
                policyParts << QStringLiteral("reviewer %1").arg(reviewer);
            m_codexPermissionState = policyParts.join(QStringLiteral(" · "));
            emit codexStateChanged();
            const auto returnedModel = result.value("model").toString();
            const auto returnedEffort = result.value("reasoningEffort").toString();
            const auto returnedCwd = QFileInfo(result.value("cwd").toString()).canonicalFilePath();
            m_effectiveModel = returnedModel + " · " + (returnedEffort.isEmpty() ? "effort unreported" : returnedEffort);
            emit codexStateChanged();
            if (returnedModel != m_selectedModel || returnedCwd != m_projectPath ||
                (!returnedEffort.isEmpty() && returnedEffort != m_selectedEffort)) {
                appendOutput("[Server thread configuration does not match selection; turn blocked]\n");
                m_codexState = "Configuration mismatch"; m_activeTask = "Idle";
                emit taskChanged(); emit codexStateChanged(); return;
            }
            m_threadId = result.value("thread").toObject().value("id").toString();
            if (m_threadId.isEmpty()) {
                appendOutput("Codex thread/start returned no thread id.\n");
                m_codex.kill();
                return;
            }
            m_savedThreadId = m_threadId; persistSession();
            appendOutput("[Codex thread " + m_threadId + " · " + m_effectiveModel + " · cwd " + m_projectPath + "]\n");
            if (m_submitAfterConnect) beginTurn();
            else { m_activeTask = "Idle"; m_codexState = "Connected · thread resumed"; emit taskChanged(); emit codexStateChanged(); }

        } else if (purpose == 4) {
            for (const auto& model : result.value("data").toArray()) m_models.append(model);
            const auto cursor = result.value("nextCursor").toString();
            if (!cursor.isEmpty()) { sendRpc("model/list", QJsonObject{{"cursor", cursor}, {"limit", 100}}, 4); return; }
            if (m_selectedModel.isEmpty()) m_selectedModel = "gpt-6-luna";
            if (m_selectedEffort.isEmpty()) {
                for (const auto& item : m_models)
                    if (item.toObject().value("model").toString() == m_selectedModel)
                        m_selectedEffort = item.toObject().value("defaultReasoningEffort").toString();
            }
            emit modelsChanged(); persistSession();
            appendOutput("[Model catalog loaded; access remains unverified until a completed turn]\n");
            if (m_submitAfterConnect || !m_savedThreadId.isEmpty()) beginThread();
            else { m_activeTask = "Idle"; emit taskChanged(); }
        } else if (purpose == 5) {
            const auto account = result.value("account").toObject();
            m_authenticationState = account.isEmpty() ? "Not authenticated" : account.value("type").toString();
            emit codexStateChanged();
        } else if (purpose == 3) {
            m_turnId = result.value("turn").toObject().value("id").toString();
            persistSession();
            m_effectiveModel = m_selectedModel + " · " + m_selectedEffort + " · requested (access pending)";
            emit codexStateChanged();
            m_codexState = "Turn running";
            emit codexStateChanged();
            m_activeTask = QStringLiteral("Codex turn running");
            emit taskChanged();
        }
    }
}

void ProjectController::finishCodexTurn(const QString& status)
{
    if (status == QStringLiteral("inProgress")) {
        m_codexState = QStringLiteral("Turn still running");
        m_activeTask = QStringLiteral("Codex turn running");
        emit codexStateChanged();
        emit taskChanged();
        appendOutput("\n[Codex reported inProgress; turn remains active]\n");
        return;
    }
    if (status != QStringLiteral("completed") &&
        status != QStringLiteral("interrupted") &&
        status != QStringLiteral("failed")) {
        m_codexState = QStringLiteral("Protocol error: unknown turn status");
        m_activeTask = QStringLiteral("Codex status unknown");
        emit codexStateChanged();
        emit taskChanged();
        appendOutput(QStringLiteral("\n[Codex sent unknown terminal status: %1; keeping turn state unresolved]\n")
                         .arg(status.isEmpty() ? QStringLiteral("<missing>") : status));
        return;
    }
    m_codexState = QStringLiteral("Turn %1").arg(status);
    m_activeTask = QStringLiteral("Idle");
    emit codexStateChanged();
    emit taskChanged();
    appendOutput(QStringLiteral("\n[Codex terminal status: %1]\n").arg(status));
    m_agentMessageBuffer.clear();
    m_agentMessageActive = false;
    if (status == "completed") m_effectiveModel = m_selectedModel + " · " + m_selectedEffort + " · completed turn verified";
    QSettings recovery;recovery.remove(sessionKey()+"/unresolvedTurn");
    m_turnId.clear(); m_interruptPending = false; persistSession(); emit codexStateChanged();
}

void ProjectController::answerApproval(bool accept)
{
    if (!m_approvalPending) return;
    QJsonValue decision = accept ? QJsonValue(QStringLiteral("accept"))
                                 : QJsonValue(QStringLiteral("decline"));
    if (m_serverRequestMethod == "execCommandApproval" || m_serverRequestMethod == "applyPatchApproval") {
        decision = accept ? QJsonValue(QStringLiteral("approved"))
                          : QJsonValue(QJsonObject{{"denied", QJsonObject{{
                                "rejection", "Declined in StudioHub Desktop"}}}});
    }
    if (m_serverRequestMethod != "item/commandExecution/requestApproval" &&
        m_serverRequestMethod != "item/fileChange/requestApproval" &&
        m_serverRequestMethod != "execCommandApproval" &&
        m_serverRequestMethod != "applyPatchApproval") {
        m_codexState = "BLOCKED — unsupported server request: " + m_serverRequestMethod;
        emit codexStateChanged();
        appendOutput("\n[Unsupported server request rejected; automatic approval does not cover this method]\n");
        const auto reply=QJsonDocument(QJsonObject{{"jsonrpc","2.0"},{"id",m_serverRequestId},
            {"error",QJsonObject{{"code",-32601},{"message","Unsupported StudioHub server request"}}}}).toJson(QJsonDocument::Compact)+'\n';
        m_codex.write(reply);m_approvalPending=false;emit approvalChanged();return;
    }
    const auto response = QJsonDocument(QJsonObject{{"jsonrpc", "2.0"},
        {"id", m_serverRequestId}, {"result", QJsonObject{{"decision", decision}}}})
        .toJson(QJsonDocument::Compact) + '\n';
    m_codex.write(response);
    appendOutput(QStringLiteral("\n[approval %1]\n").arg(accept ? "accept" : "decline"));
    m_approvalPending = false;
    m_approvalText.clear();
    emit approvalChanged();
}

void ProjectController::stopTask()
{
    if(m_inspectionBusy && m_activeTask.startsWith("Preparing"))m_jobCanceled=true;
    if (m_job.state() != QProcess::NotRunning) {
        m_jobCanceled = true;
        appendOutput("\n[cancelling owned process]\n");
        const auto group = static_cast<pid_t>(m_job.processId());
        if (group > 0) ::kill(-group, SIGTERM);
        m_job.terminate();
        QTimer::singleShot(1500,this,[this,group]{if(m_job.state()!=QProcess::NotRunning && m_job.processId()==group){if(group>0)::kill(-group,SIGKILL);m_job.kill();}});
    }
    if (m_codex.state() != QProcess::NotRunning) {
        if (!m_threadId.isEmpty() && !m_turnId.isEmpty() && !m_interruptPending) {
            m_interruptPending = true;
            sendRpc("turn/interrupt", QJsonObject{{"threadId", m_threadId}, {"turnId", m_turnId}}, 6);
            m_codexState = "Interrupt requested; awaiting terminal status"; emit codexStateChanged();
        } else if (!m_activeTask.startsWith("Codex")) {
            persistSession(); m_codex.terminate();
        } else appendOutput("[Awaiting turn identity; disconnect with application shutdown if needed]\n");
    }
}

void ProjectController::shutdownOwnedProcesses()
{
    m_shuttingDown = true; persistSession();
    if (m_job.state() != QProcess::NotRunning) {
        m_jobCanceled = true;
        const auto group = static_cast<pid_t>(m_job.processId());
        if (group > 0) ::kill(-group, SIGTERM);
        m_job.terminate();
        if (!m_job.waitForFinished(1500)) { if (group > 0) ::kill(-group, SIGKILL); m_job.kill(); m_job.waitForFinished(1000); }
    }
    if (m_codex.state() != QProcess::NotRunning) {
        if (!m_threadId.isEmpty() && !m_turnId.isEmpty())
            sendRpc("turn/interrupt", QJsonObject{{"threadId", m_threadId}, {"turnId", m_turnId}}, 6);
        const auto group = static_cast<pid_t>(m_codex.processId());
        m_codex.terminate();
        if (!m_codex.waitForFinished(1500)) { if (group > 0) ::kill(-group, SIGKILL); m_codex.kill(); m_codex.waitForFinished(1000); }
        if (group > 0) ::kill(-group, SIGTERM);
    }
}

void ProjectController::loadMeasurementReport()
{
    const auto path = m_manifest.measurement;
    QFile report(path);
    if (!report.open(QIODevice::ReadOnly)) {
        appendOutput("Measurement report is missing: " + path + "\n");
        return;
    }
    appendOutput("\n[measurement report: " + path + "]\n" + QString::fromUtf8(report.readAll()) + "\n");
}

void ProjectController::openEvidenceFolder()
{
    const auto path = m_manifest.evidence;
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void ProjectController::setJobOutcome(const QString& name,const QString& outcome) {
    if(name.contains("build",Qt::CaseInsensitive)||name=="Building VST3")m_buildState=outcome;
    if(name=="Running DSP tests")m_dspState=outcome;
    if(name=="Steinberg validator")m_sdkState=outcome;
    if(name=="Plugin metadata validator")m_metadataState=outcome;
    if(name=="Running measurements")m_measurementState=outcome;
    if(name=="pluginval strictness 5 full GUI")m_guiLifecycleState=outcome;
    m_statusOverrides[name]=QJsonObject{{"status",outcome},{"source",m_jobSourceIdentity},{"bundle",m_jobBundleHash}};
    QSettings statusSettings;statusSettings.setValue(sessionKey()+"/jobStatuses",QJsonDocument(m_statusOverrides).toJson(QJsonDocument::Compact));
    m_validationState=name+" · "+outcome;
    if(outcome=="BLOCKED"||outcome=="EVIDENCE ERROR") {m_verifyQueue.clear();m_workflowState="Stopped: "+name+" · "+outcome;}
    emit projectChanged();
}
bool ProjectController::prepareJob(const QString& name) {
    if(m_prepared) {m_prepared=false;return true;}
    if(m_inspectionBusy||m_job.state()!=QProcess::NotRunning||m_activeTask.startsWith("Codex"))return false;
    QString error;ProjectManifest selected;
    if(!ProjectManifest::load(m_projectPath+"/studiohub-project.json",selected,error)) {setJobOutcome(name,"BLOCKED");appendOutput(error+"\n");return false;}
    const auto toolkit=QFileInfo(toolkitRoot()).canonicalFilePath();
    if(selected.root==toolkit && selected.build!=toolkit+"/build/desktop-vst3") {setJobOutcome(name,"BLOCKED");return false;}
    QSettings profileSettings;
    m_toolchain=ToolchainProfile::load(toolkit,QJsonDocument::fromJson(profileSettings.value("toolchainOverrides").toByteArray()).object());
    if(!m_toolchain.error.isEmpty()){setJobOutcome(name,"BLOCKED");appendOutput(m_toolchain.error+"\n");return false;}
    m_shuttingDown=false;m_jobCanceled=false;
    m_activeTask="Preparing "+name;emit taskChanged();
    const auto manifestDigest=Provenance::fileHash(m_projectPath+"/studiohub-project.json");
    inspectAsync(selected,[this,name,manifestDigest](ProjectInspection snapshot) {
        if(snapshot.manifestHash!=manifestDigest){m_activeTask="Idle";setJobOutcome(name,"BLOCKED");appendOutput("Manifest changed during snapshot inspection. Retry.\n");emit taskChanged();return;}
        if(m_jobCanceled){m_activeTask="Idle";setJobOutcome(name,"CANCELED");emit taskChanged();return;}
        if(!snapshot.error.isEmpty()) {m_activeTask="Idle";setJobOutcome(name,"BLOCKED");appendOutput(snapshot.error+"\n");emit taskChanged();return;}
        m_manifest=m_jobManifest=snapshot.project;m_jobInspection=snapshot;
        m_jobManifestHash=snapshot.manifestHash;m_jobDependencyHash=snapshot.dependency;
        m_jobBundleHash=snapshot.bundle;m_jobSourceIdentity=snapshot.source;
        m_jobRevision=snapshot.revision;m_jobDirty=snapshot.dirty;
        QString error;
        if(name!="Building VST3" && name!="Configuring isolated VST3 build" && !hasCurrentBuild(error)) {
            m_activeTask="Idle";setJobOutcome(name,"BLOCKED");appendOutput("Verification blocked: "+error+". Build first.\n");emit taskChanged();return;
        }
        m_activeTask="Idle";m_prepared=true;
        if(name=="Configuring isolated VST3 build")buildVst3();
        else if(name=="Building VST3") {m_prepared=false;QStringList args{"--build",m_manifest.build,"--target"};args<<m_manifest.targets<<("-j"+QString::number(m_toolchain.jobs));
        if(Provenance::compilerChanged(m_jobInspection.buildStamp,m_jobInspection.products)){args<<"--clean-first";appendOutput("Compiler identity changed; rebuilding candidate outputs with --clean-first.\n");}startJob(name,m_toolchain.cmake,args,m_projectPath);}
        else if(name=="Running DSP tests")runDspTests();
        else if(name=="Steinberg validator")runSdkValidator();
        else if(name=="Plugin metadata validator")runMetadataValidator();
        else if(name=="Running measurements")runMeasurements();
        else runPluginval(name=="pluginval strictness 5 full GUI");
    });
    return false;
}
bool ProjectController::hasCurrentBuild(QString& error) const {
    const auto stamp=m_jobInspection.buildStamp;
    if(stamp.isEmpty()) {error="No build provenance";return false;}
    if(stamp.value("source_fingerprint").toString()!=m_jobInspection.source || stamp.value("dependency_fingerprint").toString()!=m_jobDependencyHash ||
       stamp.value("configuration").toString()!=m_manifest.configuration || stamp.value("products").toObject()!=m_jobInspection.products) {
        error="Build inputs or products changed";return false;
    }
    return true;
}
bool ProjectController::writeBuildStamp() {
    const auto products=m_completedInspection.products;
    if(products.value("module").toString().isEmpty() || products.value("bundle").toString().isEmpty()) return false;
    for(const auto& target:m_jobManifest.targets) {
        if(target==m_jobManifest.target)continue;
        bool found=false;for(auto it=products.begin();it!=products.end();++it)if(QFileInfo(it.key()).fileName()==target && !it.value().toString().isEmpty())found=true;
        if(!found){appendOutput("Build target executable missing: "+target+"\n");return false;}
    }
    QString error;
    const bool saved=Provenance::writeJson(m_jobManifest.build+"/studiohub-build.json",{{"project_id",m_jobManifest.id},{"source_revision",m_jobRevision},
        {"source_fingerprint",m_jobSourceIdentity},{"dependency_fingerprint",m_jobDependencyHash},{"configuration",m_jobManifest.configuration},{"configuration_fingerprint",m_jobInspection.configuration},{"products",products}},error);
    if(!saved)appendOutput("Build succeeded; provenance could not be saved: "+error+"\n");
    return saved;
}

void ProjectController::completeJob(int code,QProcess::ExitStatus status) {
        m_jobTimeout.stop();
        bool ok = status == QProcess::NormalExit && code == 0 && !m_jobCanceled && !m_jobTimedOut;
        if (m_activeTask == "Building VST3" && m_completedInspection.module.isEmpty()) {
            ok = false; appendOutput("[build returned without the expected module]\n");
        }
        const QString finishedTask = m_activeTask;
        const bool isVerification = finishedTask != "Configuring isolated VST3 build" && finishedTask != "Building VST3";
        const bool sameArtifact = (!isVerification || (m_jobArtifactHash == m_completedInspection.module && m_jobBundleHash == m_completedInspection.bundle)) &&
                                  !m_jobSourceIdentity.isEmpty() && m_jobSourceIdentity == m_completedInspection.source;
        if (!sameArtifact) ok = false;
        if (ok && finishedTask == "Running measurements") {
            QFile report(m_jobMeasurementPath);
            QJsonObject data;
            if (report.open(QIODevice::ReadOnly)) data = QJsonDocument::fromJson(report.readAll()).object();
            QString reportError;
            if (!Provenance::validateMeasurement(data, m_jobManifest.capability, reportError)) { ok = false; appendOutput("[measurement process did not produce valid JSON]\n"); }
            else {
                data["project_id"] = m_manifest.id; data["artifact_sha256"] = m_jobArtifactHash;
                data["source_revision"] = m_jobRevision; data["source_fingerprint"] = m_jobSourceIdentity;
                data["dependency_fingerprint"] = m_jobDependencyHash;
                data["bundle_sha256"] = m_jobBundleHash; data["job_id"] = m_jobId;
                data["measurement_scope"] = "source DSP; not a VST3 host render";
                data["human_review_required"] = true;
                data["acceptance"]=Provenance::measurementAcceptance(data,m_jobManifest.capability);
                QString error;
                ok = Provenance::writeJson(m_jobMeasurementPath, data, error) && Provenance::writeJson(m_jobManifest.measurement, data, error);
                if (!ok) appendOutput("Measurement evidence could not be saved: " + error + "\n");
                else if(data.value("acceptance").toObject().value("status").toString()!="PASS") {
                    ok=false;appendOutput("Measured behavior failed declared acceptance limits; report retained.\n");
                }
            }
        }
        if (ok && finishedTask == "Building VST3") {
            m_jobBundleHash = m_completedInspection.bundle;
            ok = writeBuildStamp();
        }
        const auto outcome=Job::label(Job::classify(ok,code,status==QProcess::CrashExit,m_jobCanceled,m_jobTimedOut,sameArtifact));
        if (ok && finishedTask == "Building VST3") {
            m_artifactHash = m_completedInspection.module;
            if (!m_artifactHash.isEmpty()) {
                m_buildState = QStringLiteral("PASS · %1").arg(m_artifactHash.left(12));
                m_jobArtifactHash = m_artifactHash;
                if (!m_artifactWatcher.files().contains(modulePath(m_manifest)))
                    m_artifactWatcher.addPath(modulePath(m_manifest));
            }
        }
        setJobOutcome(finishedTask, outcome);
        recordVerification(finishedTask, outcome, code);
        if (!ok) {
            m_lastFailureLog = m_jobLogPath;
            m_lastFailureFingerprint = m_jobSourceIdentity + ":" + finishedTask + ":" + QString::number(code);
        }
        m_activeTask = QStringLiteral("Idle");
        emit taskChanged();
        emit projectChanged();
        appendOutput(ok ? "\n[process exited successfully]\n"
                        : QStringLiteral("\n[process %1: exit %2]\n")
                              .arg(status == QProcess::CrashExit ? "crashed" : "failed")
                              .arg(code));
        if (ok && m_evidenceSaved && finishedTask == "Configuring isolated VST3 build") prepareJob("Building VST3");
        else if(m_workflowState.startsWith("Verify project:")) {
            if(ok&&m_evidenceSaved)advanceWorkflow();
            else {m_verifyQueue.clear();m_workflowState="Stopped: "+finishedTask+" · "+outcome;emit projectChanged();}
        }

}

void ProjectController::inspectAsync(const ProjectManifest& project,std::function<void(ProjectInspection)> done) {
    m_inspectionBusy=true;emit projectChanged();
    auto watcher=new QFutureWatcher<ProjectInspection>(this);
    const auto generation=m_projectGeneration;
    connect(watcher,&QFutureWatcher<ProjectInspection>::finished,this,[this,watcher,generation,done=std::move(done)] {
        const auto snapshot=watcher->result();watcher->deleteLater();
        m_inspectionBusy=false;emit projectChanged();
        if(!m_shuttingDown && generation==m_projectGeneration)done(snapshot);
    });
    const auto toolkit=QFileInfo(toolkitRoot()).canonicalFilePath();
    const QStringList extraTools{m_toolchain.pluginval,m_toolchain.cmake,m_toolchain.ctest};
    watcher->setFuture(QtConcurrent::run([project,toolkit,extraTools]{
        auto snapshot=ProjectInspection::inspect(project,toolkit);
        for(const auto& tool:extraTools)if(!tool.isEmpty())snapshot.toolHashes[tool]=Provenance::fileHash(tool);
        for(auto it=snapshot.toolHashes.begin();it!=snapshot.toolHashes.end();++it) {
            const auto name=QFileInfo(it.key()).fileName();
            if(name=="cmake"||name=="ctest") {
                QProcess version;version.start(it.key(),{"--version"});
                if(version.waitForFinished(2000)&&version.exitCode()==0)snapshot.toolVersions[it.key()]=QString::fromUtf8(version.readAllStandardOutput()).section('\n',0,0);
            } else if(it.value().toString()=="d3d342ea92293b0699a5c983937211e3faf2b0f38145b31cb8d78424a25e10ba")
                snapshot.toolVersions[it.key()]="pluginval 1.0.4 (binary identity verified in retained Cairo experiment)";
            else if(name=="validator"||name=="moduleinfotool") {
                QFile header(toolkit+"/third_party/vst3sdk/pluginterfaces/vst/vsttypes.h");
                if(header.open(QIODevice::ReadOnly))for(const auto& line:header.readAll().split('\n'))
                    if(line.startsWith("#define kVstVersionString"))snapshot.toolVersions[it.key()]="Steinberg "+QString::fromUtf8(line).section('"',1,1)+" / "+name;
            }
        }
        return snapshot;
    }));
}

void ProjectController::flushStreams() {
    if(!m_pendingConversation.isEmpty()){m_conversation.append("assistant",m_pendingConversation,true);m_pendingConversation.clear();}
    if(!m_pendingOperations.isEmpty()){m_operations.append("output",m_pendingOperations,true);m_pendingOperations.clear();}
    if(!m_pendingBuild.isEmpty()){m_buildLog.append("log",m_pendingBuild);m_pendingBuild.clear();emit outputChanged();}
}

QString ProjectController::toolchainConfiguration() const {
    QFile defaults(toolkitRoot()+"/desktop/toolchain-profile.json");if(!defaults.open(QIODevice::ReadOnly))return "Toolchain defaults unavailable";
    auto config=QJsonDocument::fromJson(defaults.readAll()).object();QSettings settings;
    const auto overrides=QJsonDocument::fromJson(settings.value("toolchainOverrides").toByteArray()).object();
    for(auto it=overrides.begin();it!=overrides.end();++it)config[it.key()]=it.value();
    return QString::fromUtf8(QJsonDocument(config).toJson());
}
bool ProjectController::saveToolchainConfiguration(const QString& text) {
    if(m_inspectionBusy||m_job.state()!=QProcess::NotRunning)return false;
    QJsonParseError parse;const auto doc=QJsonDocument::fromJson(text.toUtf8(),&parse);
    if(parse.error!=QJsonParseError::NoError||!doc.isObject()){appendOutput("Invalid toolchain JSON\n");return false;}
    const auto profile=ToolchainProfile::load(toolkitRoot(),doc.object());
    if(!profile.error.isEmpty()){appendOutput(profile.error+"\n");return false;}
    QSettings settings;settings.setValue("toolchainOverrides",doc.toJson(QJsonDocument::Compact));settings.sync();m_toolchain=profile;refreshProject();emit projectChanged();return true;
}

bool ProjectController::saveSpecification(const QString& text) {
    if(m_inspectionBusy||m_job.state()!=QProcess::NotRunning||m_activeTask.startsWith("Codex")||text.size()>65536)return false;
    QString error;const auto path=ProjectManifest::safePath(m_projectPath,"SPEC.md",error);
    if(path.isEmpty())return false;
    QSaveFile file(path);const auto bytes=text.toUtf8();
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){appendOutput("Specification could not be saved\n");return false;}
    m_specification=text;refreshProject();return true;
}
void ProjectController::inspectEvidence(int index) {
    if(index<0||index>=m_evidenceEntries.size())return;
    const auto entry=QJsonObject::fromVariantMap(m_evidenceEntries.at(index).toMap());
    appendOutput("\n[Evidence record]\n"+QString::fromUtf8(QJsonDocument(entry).toJson()));
    const auto log=entry.value("log_path").toString();QString error;
    const auto relative=QDir(m_projectPath).relativeFilePath(log);
    const auto safe=ProjectManifest::safePath(m_projectPath,relative,error);
    if(safe.isEmpty()||!safe.startsWith(m_manifest.evidence+"/")){appendOutput("Evidence log path rejected\n");return;}
    QFile file(safe);if(!file.open(QIODevice::ReadOnly)){appendOutput("Evidence file is missing: "+safe+"\n");return;}
    appendOutput(QString::fromUtf8(file.read(8*1024*1024)));
}
void ProjectController::verifyProject() {
    if(m_inspectionBusy||m_job.state()!=QProcess::NotRunning||m_activeTask.startsWith("Codex"))return;
    m_verifyQueue={"Build","DSP","Metadata","Steinberg","Pluginval"};
    if(m_manifest.capability!="none")m_verifyQueue<<"Measurements";
    advanceWorkflow();
}
void ProjectController::advanceWorkflow() {
    if(m_verifyQueue.isEmpty()){m_workflowState="Verification completed — all requested gates passed";emit projectChanged();return;}
    const auto step=m_verifyQueue.takeFirst();m_workflowState="Verify project: "+step;emit projectChanged();
    if(step=="Build")buildVst3();else if(step=="DSP")runDspTests();else if(step=="Metadata")runMetadataValidator();
    else if(step=="Steinberg")runSdkValidator();else if(step=="Pluginval")runPluginval(m_manifest.gui);else runMeasurements();
}
void ProjectController::retryInterruptedJob() {
    if(m_inspectionBusy||m_job.state()!=QProcess::NotRunning||m_activeTask.startsWith("Codex"))return;
    QSettings settings;const auto unfinished=QJsonDocument::fromJson(settings.value(sessionKey()+"/unfinishedJob").toByteArray()).object();
    const auto task=unfinished.value("task").toString();
    if(task=="Building VST3"||task=="Configuring isolated VST3 build")buildVst3();else if(task=="Running DSP tests")runDspTests();
    else if(task=="Steinberg validator")runSdkValidator();else if(task=="Plugin metadata validator")runMetadataValidator();
    else if(task=="Running measurements")runMeasurements();else if(task.contains("pluginval"))runPluginval(task.contains("full GUI"));
}
