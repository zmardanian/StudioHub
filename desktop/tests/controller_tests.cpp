#include "projectcontroller.hpp"
#include "projectscaffold.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QThread>
#include <cstdio>
#include <functional>
static bool wait(const std::function<bool()>& condition) {
 QElapsedTimer timer; timer.start();
 while(!condition() && timer.elapsed()<12000) { QCoreApplication::processEvents(); QThread::msleep(5); }
 return condition();
}
int main(int argc,char** argv) {
 QCoreApplication app(argc,argv);int heartbeats=0;QTimer heartbeat;heartbeat.setInterval(5);
 QObject::connect(&heartbeat,&QTimer::timeout,[&]{++heartbeats;});heartbeat.start(); QCoreApplication::setApplicationName("StudioHubFixture");
 QTemporaryDir state; QSettings::setDefaultFormat(QSettings::IniFormat);
 QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,state.path());
 qputenv("STUDIOHUB_CODEX_EXECUTABLE",app.arguments().at(1).toUtf8());
 ProjectController c;if(!wait([&]{return !c.inspectionBusy();}))return 34; c.connectCodex();
 if(!wait([&]{return c.models().contains("gpt-6-luna") && c.activeTask()=="Idle";})) return 1;
 if(c.selectedEffort()!="low") return 2;
 c.setPrompt("fixture only"); c.submitCodexPrompt();
 if(!wait([&]{return c.codexState()=="Turn still running";})) return 3;
 if(c.activeTask()=="Idle") return 4;
 if(!wait([&]{const auto model=c.conversationModel();for(int i=0;i<model->rowCount();++i)if(model->data(model->index(i,0),ActivityModel::TextRole).toString().contains("final payload retained"))return true;return false;}))return 70;
 if(!wait([&]{return c.output().contains("automatic approval verified");}) || c.approvalPending()) return 16;
 if(!wait([&]{return c.output().contains("unsupported request rejected");}) || c.approvalPending()) return 25;
 if(!wait([&]{return c.output().contains("legacy patch approval verified");}))return 45;
 c.stopTask(); if(!wait([&]{return c.codexState()=="Turn interrupted";})) return 5;
 c.shutdownOwnedProcesses();
 if(!wait([&]{return c.activeTask()=="Idle";})) return 6;
 c.connectCodex(); if(!wait([&]{return c.output().contains("thread/resume observed");})) return 7;
 c.shutdownOwnedProcesses();
 qputenv("STUDIOHUB_FIXTURE_EXIT", "1");
 c.connectCodex();
 if(!wait([&]{return c.activeTask()=="Idle";})) return 17;
 c.submitCodexPrompt();
 if(!wait([&]{return c.codexState()=="Disconnected — turn outcome unknown" && c.activeTask()=="Idle";})) return 18;
 c.refreshProject();if(!wait([&]{return !c.inspectionBusy();})||!c.recoveryState().contains("outcome unresolved"))return 52;
 qunsetenv("STUDIOHUB_FIXTURE_EXIT");
 c.connectCodex();
 if(!wait([&]{return c.codexState()=="Connected · thread resumed";})) return 19;
 c.shutdownOwnedProcesses();
 qputenv("STUDIOHUB_FIXTURE_HANG","1");qputenv("STUDIOHUB_TEST_HANDSHAKE_MS","100");
 c.connectCodex();if(!wait([&]{return c.codexState()=="Handshake timeout" && c.activeTask()=="Idle";}))return 26;
 c.shutdownOwnedProcesses();qunsetenv("STUDIOHUB_FIXTURE_HANG");qunsetenv("STUDIOHUB_TEST_HANDSHAKE_MS");
 QTemporaryDir projects; QString error;
 const auto root=QStringLiteral(STUDIOHUB_PROJECT_ROOT);
 const auto file=ProjectScaffold::create(projects.path(),root+"/desktop/templates/vst3",root,
  "fixture-stereo","Fixture Stereo","Stereo gain and balance.",error,"stereo-utility");
 if(file.isEmpty()) {fprintf(stderr,"%s\n",qPrintable(error));return 8;}
 if(!ProjectScaffold::create(projects.path(),root+"/desktop/templates/vst3",root,
  "fixture-stereo","Fixture Stereo","duplicate",error).isEmpty()) return 9;
 if(!wait([&]{return !c.inspectionBusy();}))return 30;
 if(!c.openProject(file)) return 10;
 if(!wait([&]{return !c.inspectionBusy();}))return 31;
 c.loadMeasurementReport(); if(!c.output().contains("Measurement report is missing")) return 11;
 const auto projectPath=QFileInfo(file).absolutePath();
 QFile broken(projectPath+"/CMakeLists.txt"); if(!broken.open(QIODevice::WriteOnly)) return 12;
 broken.write("cmake_minimum_required(VERSION 3.28)\nmessage(FATAL_ERROR \"controlled fixture failure\")\n"); broken.close();
 const auto beforeBuild=heartbeats;
 c.buildVst3(); if(!wait([&]{return c.activeTask()=="Idle";})) return 13;
 if(heartbeats-beforeBuild<3)return 33;
 if(!c.buildState().contains("FAIL")) return 14;
 qputenv("STUDIOHUB_TEST_JOB_TIMEOUT_MS","100");
 if(!broken.open(QIODevice::WriteOnly))return 27;
 broken.write("cmake_minimum_required(VERSION 3.28)\nexecute_process(COMMAND ${CMAKE_COMMAND} -E sleep 2)\n");broken.close();
 c.buildVst3();if(!wait([&]{return c.activeTask()=="Idle";}))return 28;
 if(c.buildState()!="TIMEOUT")return 29;
 qunsetenv("STUDIOHUB_TEST_JOB_TIMEOUT_MS");
 c.buildVst3();if(!wait([&]{return c.buildState()=="RUNNING";}))return 50;
 c.stopTask();if(!wait([&]{return c.activeTask()=="Idle"&&!c.inspectionBusy();})||c.buildState()!="CANCELED")return 51;
 if(!broken.open(QIODevice::WriteOnly))return 58;
 broken.write("cmake_minimum_required(VERSION 3.28)\nexecute_process(COMMAND ${CMAKE_COMMAND} -E sleep 1)\nmessage(STATUS \"log failure probe\")\n");broken.close();
 c.buildVst3();if(!wait([&]{return c.buildState()=="RUNNING";}))return 59;
 const auto logs=QDir(projectPath+"/evidence/jobs").entryInfoList({"*.log"},QDir::Files,QDir::Time);
 const auto logPath=logs.first().absoluteFilePath();
 if(!QFile::rename(logPath,logPath+".retained")||!QDir().mkdir(logPath))return 60;
 if(!wait([&]{return c.activeTask()=="Idle"&&!c.inspectionBusy();})||c.buildState()!="EVIDENCE ERROR")return 61;
 if(!QDir().rmdir(logPath)||!QFile::rename(logPath+".retained",logPath))return 62;
 QFile history(projectPath+"/evidence/jobs/validation-history.jsonl");
 if(!history.open(QIODevice::ReadOnly) || !history.readAll().contains("FAIL")) return 15;
 c.shutdownOwnedProcesses();
 // Purpose-built fake build fixture: successful analyzer must not relabel an old report.
 const auto cmakeFixture=QString(R"(cmake_minimum_required(VERSION 3.28)
project(ControlledFixture LANGUAGES CXX)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/VST3/Release/StudioHubFixtureStereo.vst3/Contents/x86_64-linux")
file(WRITE "${CMAKE_BINARY_DIR}/VST3/Release/StudioHubFixtureStereo.vst3/Contents/x86_64-linux/StudioHubFixtureStereo.so" "fixture only")
file(WRITE "${CMAKE_BINARY_DIR}/StudioHubFixtureStereoTests" "#!/bin/sh\nexit 0\n")
file(WRITE "${CMAKE_BINARY_DIR}/StudioHubFixtureStereoMeasurements" "#!/bin/sh\nexit 0\n")
file(CHMOD "${CMAKE_BINARY_DIR}/StudioHubFixtureStereoTests" "${CMAKE_BINARY_DIR}/StudioHubFixtureStereoMeasurements" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
add_custom_target(StudioHubFixtureStereo)
add_custom_target(StudioHubFixtureStereoTests)
add_custom_target(StudioHubFixtureStereoMeasurements)
enable_testing()
add_test(NAME StudioHubFixtureStereoTests COMMAND "${CMAKE_BINARY_DIR}/StudioHubFixtureStereoTests")
)");
 auto profile=QJsonDocument::fromJson(c.toolchainConfiguration().toUtf8()).object();profile["jobs"]=1;
 if(!c.saveToolchainConfiguration(QString::fromUtf8(QJsonDocument(profile).toJson()))||!wait([&]{return !c.inspectionBusy();}))return 73;
 if(!broken.open(QIODevice::WriteOnly))return 35;broken.write(cmakeFixture.toUtf8());broken.close();
 c.buildVst3();if(!wait([&]{return c.activeTask()=="Idle"&&!c.inspectionBusy();})||c.buildState()!="PASS"||!c.output().contains("-j1"))return 36;
 QFile buildStamp(projectPath+"/build/vst3/studiohub-build.json");
 if(!buildStamp.open(QIODevice::ReadOnly))return 53;
 auto stamp=QJsonDocument::fromJson(buildStamp.readAll()).object();buildStamp.close();
 auto products=stamp.value("products").toObject();products["compiler_sha256"]="fixture changed compiler";stamp["products"]=products;
 if(!buildStamp.open(QIODevice::WriteOnly|QIODevice::Truncate))return 54;
 buildStamp.write(QJsonDocument(stamp).toJson());buildStamp.close();
 c.buildVst3();if(!wait([&]{return c.activeTask()=="Idle"&&!c.inspectionBusy();})||c.buildState()!="PASS"||!c.output().contains("--clean-first"))return 55;
 QSettings recovery;const auto session="sessions/"+QString::fromLatin1(QCryptographicHash::hash(c.projectPath().toUtf8(),QCryptographicHash::Sha256).toHex());
 recovery.setValue(session+"/unfinishedJob",QJsonDocument(QJsonObject{{"task","Running DSP tests"}}).toJson(QJsonDocument::Compact));recovery.sync();
 c.refreshProject();if(!wait([&]{return !c.inspectionBusy();})||!c.recoveryState().contains("explicit retry")||c.activeTask()!="Idle")return 56;
 recovery.remove(session+"/unfinishedJob");
 c.runDspTests();if(!wait([&]{return c.activeTask()=="Idle"&&!c.inspectionBusy();})||c.dspState()!="PASS")return 63;
 QFile tester(projectPath+"/build/vst3/StudioHubFixtureStereoTests");
 if(!tester.open(QIODevice::ReadOnly))return 64;const auto originalTester=tester.readAll();tester.close();
 if(!tester.open(QIODevice::WriteOnly|QIODevice::Append))return 65;tester.write("# changed executable\n");tester.close();
 c.refreshProject();if(!wait([&]{return !c.inspectionBusy();})||c.dspState()=="PASS")return 66;
 if(!tester.open(QIODevice::WriteOnly|QIODevice::Truncate))return 67;tester.write(originalTester);tester.close();
 QFile oldReport(projectPath+"/evidence/measurements.json");if(!oldReport.open(QIODevice::WriteOnly))return 71;oldReport.write("{\"old_report\":true}");oldReport.close();
 c.runMeasurements();if(!wait([&]{return c.activeTask()=="Idle"&&!c.inspectionBusy();})||c.measurementState()!="FAIL")return 37;
 QFile measurementHistory(projectPath+"/evidence/jobs/validation-history.jsonl");
 if(!measurementHistory.open(QIODevice::ReadOnly))return 68;
 const auto entries=measurementHistory.readAll().trimmed().split('\n');const auto measurementJob=QJsonDocument::fromJson(entries.last()).object();
 if(measurementJob.value("tool_version").toString().isEmpty())return 74;
 if(!measurementJob.value("measurement_report_path").toString().contains("/"+measurementJob.value("job_id").toString()+"/measurements.json"))return 69;
 if(!oldReport.open(QIODevice::ReadOnly))return 72;if(oldReport.readAll()!="{\"old_report\":true}")return 38;oldReport.close();
 if(!broken.open(QIODevice::Append))return 39;broken.write("\n# source changed after successful build\n");broken.close();
 c.runDspTests();if(!wait([&]{return c.activeTask()=="Idle"&&!c.inspectionBusy();})||c.dspState()!="BLOCKED")return 40;
 if(!c.saveSpecification("Updated fixture specification"))return 41;
 if(!wait([&]{return !c.inspectionBusy();})||c.specification()!="Updated fixture specification")return 42;
 c.verifyProject();if(!wait([&]{return c.activeTask()=="Idle"&&!c.inspectionBusy();}))return 43;
 if(c.workflowState().contains("all requested gates passed"))return 44;
 const auto historyPath=projectPath+"/evidence/jobs/validation-history.jsonl";
 if(!QFile::rename(historyPath,historyPath+".retained")||!QDir().mkdir(historyPath))return 46;
 c.runDspTests();if(!wait([&]{return c.activeTask()=="Idle"&&!c.inspectionBusy();})||c.dspState()!="EVIDENCE ERROR")return 47;
 if(!QDir().rmdir(historyPath)||!QFile::rename(historyPath+".retained",historyPath))return 48;
 c.refreshProject();if(!wait([&]{return !c.inspectionBusy();})||c.dspState()!="EVIDENCE ERROR")return 49;
 // A failed evidence write must remain visible across refresh instead of restoring an older PASS. // fake module cannot pass metadata validation.
 ProjectManifest manifest;
 if(!ProjectManifest::load(file,manifest,error)) return 20;
 QDir().mkpath(QFileInfo(manifest.module).absolutePath());
 QFile artifact(manifest.module);
 if(!artifact.open(QIODevice::WriteOnly)) return 21;
 artifact.write("controlled artifact one"); artifact.close(); c.refreshProject();
 if(!wait([&]{return !c.inspectionBusy();}))return 32;
 if(!artifact.open(QIODevice::WriteOnly|QIODevice::Truncate)) return 22;
 artifact.write("controlled artifact two"); artifact.close();
 if(!wait([&]{return c.guiLifecycleState().contains("STALE");})) return 23;
 if(!c.openProject(file)||!wait([&]{return !c.inspectionBusy();})||!c.workflowState().isEmpty())return 57;
 qputenv("STUDIOHUB_CODEX_EXECUTABLE", "/nonexistent/studiohub-fixture-codex");
 c.connectCodex();
 if(!wait([&]{return c.codexState().startsWith("Unavailable:") && c.activeTask()=="Idle";})) return 24;
 puts("Incremental protocol, model/effort, terminal state, interrupt, resume, scaffold and failure tests passed");
}
