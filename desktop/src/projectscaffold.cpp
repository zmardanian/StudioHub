#include "projectscaffold.hpp"
#include "projectmanifest.hpp"
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

QString ProjectScaffold::create(const QString& parent, const QString& templates, const QString& toolchain,
 const QString& id, const QString& name, const QString& spec, QString& error, const QString& capability)
{
 if(capability!="none"&&capability!="dynamics"&&capability!="stereo-utility"){error="Unsupported capability";return {};}
 if (!QRegularExpression("^[a-z][a-z0-9-]{0,47}$").match(id).hasMatch() ||
     !QRegularExpression("^[A-Za-z][A-Za-z0-9 _-]{0,79}$").match(name).hasMatch() ||
     spec.trimmed().isEmpty() || spec.size()>32768) { error="Invalid project ID, name or specification"; return {}; }
 QDir().mkpath(parent);
 const auto base = QFileInfo(parent).canonicalFilePath();
 const auto root = base + "/" + id;
 if (QFileInfo::exists(root)) { error="Project directory already exists; nothing overwritten"; return {}; }
 if (!QFileInfo::exists(templates + "/CMakeLists.txt") || !QFileInfo::exists(toolchain + "/third_party/vst3sdk/CMakeLists.txt")) {
  error="Pinned template/toolchain is unavailable"; return {};
 }
 if (!QDir().mkdir(root)) { error="Cannot create project directory"; return {}; }
 auto write=[&](const QString& path,const QByteArray& data) {
  QDir().mkpath(QFileInfo(path).absolutePath()); QSaveFile f(path);
  if(!f.open(QIODevice::WriteOnly) || f.write(data)!=data.size() || !f.commit()) {error="Cannot write " + path; return false;} return true;
 };
 QString target="StudioHub";
 for (const auto& word : id.split('-')) target += word.left(1).toUpper()+word.mid(1);
 auto uid=[] { auto b=QUuid::createUuid().toRfc4122().toHex(); QStringList words;
  for(int i=0;i<32;i+=8) words << "0x"+QString::fromLatin1(b.mid(i,8)); return words.join(", "); };
 const auto processor=uid(),controller=uid();
 QDirIterator files(templates,QDir::Files,QDirIterator::Subdirectories);
 while(files.hasNext()) {
  const auto file=files.next(); if(QFileInfo(file).isSymLink()) {error="Template symlink rejected"; return {};}
  QFile source(file); if(!source.open(QIODevice::ReadOnly)) {error="Cannot read template";return {};}
  QString text=QString::fromUtf8(source.readAll());
  text.replace("@TARGET@",target).replace("@DISPLAY@",name).replace("@PROCESSOR_UID@",processor)
      .replace("@CONTROLLER_UID@",controller).replace("@SDK@",toolchain+"/third_party/vst3sdk")
      .replace("@NORMALIZE@",toolchain+"/scripts/normalize_moduleinfo.py").replace("@ADAPTER@",toolchain+"/desktop/run-isolated-validator.py");
  if(!write(root+"/"+QDir(templates).relativeFilePath(file),text.toUtf8())) return {};
 }
 QJsonObject manifest{{"schema_version",1},{"id",id},{"name",name},{"root","."},{"format","VST3"},
  {"category",capability=="dynamics"?"Fx|Dynamics":"Fx|Tools"},{"description",spec.left(240)},{"gui",true},
  {"build",QJsonObject{{"source","."},{"directory","build/vst3"},{"target",target},{"configuration","Release"},
   {"test_targets",QJsonArray{target+"Tests",target+"Measurements"}},{"definitions",QJsonObject{}}}},
  {"artifact","build/vst3/VST3/Release/"+target+".vst3"},
  {"module","build/vst3/VST3/Release/"+target+".vst3/Contents/x86_64-linux/"+target+".so"},
  {"tests",QJsonArray{target+"Tests"}},{"evidence","evidence/jobs"},
  {"measurements",QJsonObject{{"capability",capability},{"report","evidence/measurements.json"},{"executable","build/vst3/"+target+"Measurements"}}},
  {"dependency",QJsonObject{{"identity","StudioHub pinned VST3 SDK / Cairo-corrected VSTGUI"},{"fingerprint_files",QJsonArray{"dependency-lock.json"}}}}};
 QFile cairo(toolchain+"/third_party/vst3sdk/vstgui4/vstgui/lib/platform/linux/cairographicscontext.cpp");
 if(!cairo.open(QIODevice::ReadOnly)) {error="Cannot fingerprint toolchain";return {};}
 QJsonObject lock{{"toolchain",toolchain},{"cairo_sha256",QString::fromLatin1(QCryptographicHash::hash(cairo.readAll(),QCryptographicHash::Sha256).toHex())}};
 if(!write(root+"/studiohub-project.json",QJsonDocument(manifest).toJson()) ||
    !write(root+"/dependency-lock.json",QJsonDocument(lock).toJson()) ||
    !write(root+"/SPEC.md",spec.toUtf8()) || !write(root+"/.gitignore","/build/\n/evidence/\n") ||
    !write(root+"/AGENTS.md","Implement SPEC.md using the pinned SDK/VSTGUI. Keep audio processing bounded and allocation-free. Never access UADX, UA Connect, PACE, Wine, yabridge or UAD plugins, or normal REAPER configuration. Never scan plugins. Build only in build/vst3 with global links disabled. Retain failures; never weaken assertions. Do not spawn agents. Do not modify the shared SDK/toolchain.\n")) return {};
 QProcess git; git.setWorkingDirectory(root); git.start("git",{"init","-b","studiohub-development"});
 if(!git.waitForFinished(5000) || git.exitCode()!=0) {error="Git initialization failed; files preserved at "+root;return {};}
 // Reuse repository-local author identity, never global credentials or hidden commits.
 for (const QString key : {QString("user.name"),QString("user.email")}) {
  QProcess identity; identity.setWorkingDirectory(toolchain);
  identity.start("git", {"config", "--get", key});
  if(identity.waitForFinished(2000) && identity.exitCode()==0) {
   const auto value=QString::fromUtf8(identity.readAllStandardOutput()).trimmed();
   if(!value.isEmpty()) {git.start("git",{"config","--local",key,value});git.waitForFinished(2000);}
  }
 }
 ProjectManifest checked; const auto file=root+"/studiohub-project.json";
 if(!ProjectManifest::load(file,checked,error)) return {};
 return file;
}
