#include "toolchainprofile.hpp"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QThread>
#include <cmath>
ToolchainProfile ToolchainProfile::load(const QString& toolkit,const QJsonObject& overrides) {
 ToolchainProfile p;QFile file(toolkit+"/desktop/toolchain-profile.json");
 if(!file.open(QIODevice::ReadOnly)){p.error="Toolchain profile missing";return p;}
 auto config=QJsonDocument::fromJson(file.readAll()).object();
 for(auto it=overrides.begin();it!=overrides.end();++it)config[it.key()]=it.value();
 if(config.value("schema_version").toInt()!=1 || !config.value("jobs").isDouble()) {p.error="Invalid toolchain schema/jobs";return p;}
 p.jobs=config.value("jobs").toInt();
 int memoryJobs=2;QFile memory("/proc/meminfo");
 if(memory.open(QIODevice::ReadOnly))for(const auto& line:memory.readAll().split('\n'))if(line.startsWith("MemAvailable:")) {
     const auto fields=line.simplified().split(' ');const auto available=fields.value(1).toLongLong();
     memoryJobs=int(std::clamp<qint64>(available/(2LL*1024*1024),1,8));
 }
 if(config.value("jobs").toDouble()!=p.jobs || p.jobs<1 || p.jobs>std::min({8,memoryJobs,std::max(1,QThread::idealThreadCount())})) {p.error="Build jobs exceed conservative CPU limit";return p;}
 auto path=[&](const QString& key,bool executable){
  const auto text=config.value(key).toString();
  // Reject protected names before filesystem resolution, never probe these paths.
  for(const auto& forbidden:{"uadx","ua connect","pace","wine","yabridge","uad plugins"})
   if(text.contains(forbidden,Qt::CaseInsensitive)){p.error="Protected tool path rejected";return QString{};}
  // Bare executable names resolve through PATH. Explicit relative paths remain
  // rooted at the selected toolkit and absolute paths remain explicit.
  if(executable && !QDir::isAbsolutePath(text) && !text.contains('/')) {
   const auto found=QStandardPaths::findExecutable(text);
   if(found.isEmpty()){p.error="Toolchain executable unavailable: "+key;return QString{};}
   return QFileInfo(found).canonicalFilePath();
  }
  const auto candidate=QDir::isAbsolutePath(text)?text:QDir(toolkit).absoluteFilePath(text);
  const auto canonical=QFileInfo(candidate).canonicalFilePath();
  if(canonical.isEmpty() || (executable&&!QFileInfo(canonical).isExecutable())) {p.error="Toolchain path unavailable: "+key;return QString{};}
  return canonical;
 };
 p.sdk=path("sdk",false);
 const auto pluginval=config.value("pluginval").toString();
 if(!pluginval.isEmpty())p.pluginval=path("pluginval",true);
 auto command=[&](const QString& key){
  const auto text=config.value(key).toString();
  for(const auto& forbidden:{"uadx","ua connect","pace","wine","yabridge","uad plugins"})
   if(text.contains(forbidden,Qt::CaseInsensitive)){p.error="Protected tool path rejected";return QString{};}
  return QStandardPaths::findExecutable(text);
 };
 p.cmake=command("cmake");p.ctest=command("ctest");
 if(p.sdk!=QFileInfo(toolkit+"/third_party/vst3sdk").canonicalFilePath())p.error="SDK must match the pinned toolkit dependency";
 if(p.cmake.isEmpty()||p.ctest.isEmpty())p.error="CMake/CTest unavailable";
 return p;
}
