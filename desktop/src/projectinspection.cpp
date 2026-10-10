#include "projectinspection.hpp"
#include "provenance.hpp"
#include <QCryptographicHash>
#include <QFile>
#include <QProcess>
#include <QJsonDocument>
#include <QDir>
#include <QStandardPaths>
#include "toolchainprofile.hpp"
ProjectInspection ProjectInspection::inspect(const ProjectManifest& project,const QString& toolkit) {
    ProjectInspection result;result.project=project;
    result.manifestHash=Provenance::fileHash(project.root+"/studiohub-project.json");
    result.dependency=Provenance::dependencyHash(project,toolkit);
    result.module=Provenance::fileHash(project.module);result.bundle=Provenance::treeHash(project.bundle);
    auto git=[&](const QStringList& args) {
        QProcess process;process.setWorkingDirectory(project.root);process.start("git",args);
        if(!process.waitForFinished(5000)||process.exitCode()!=0)return QByteArray{};
        return process.readAllStandardOutput();
    };
    result.revision=QString::fromUtf8(git({"rev-parse","HEAD"})).trimmed();
    const auto branch=QString::fromUtf8(git({"rev-parse","--abbrev-ref","HEAD"})).trimmed();
    result.dirty=!git({"status","--porcelain","--untracked-files=all"}).trimmed().isEmpty();
    QCryptographicHash source(QCryptographicHash::Sha256),configuration(QCryptographicHash::Sha256);
    configuration.addData((project.source+project.configuration+project.definitions.join('\n')+result.dependency).toUtf8());
    const auto names=git({"ls-files","-z","--cached","--others","--exclude-standard"});
    bool valid=!names.isEmpty()&&!result.dependency.isEmpty();
    for(const auto& name:names.split('\0')) {
        if(name.isEmpty()||name.startsWith("engineering/")||name.startsWith("desktop/")||name.startsWith("build/")||name.startsWith("build-")||name.endsWith(".wav"))continue;
        QString error;const auto path=ProjectManifest::safePath(project.root,QString::fromUtf8(name),error);
        const auto hash=path.isEmpty()?QString{}:Provenance::fileHash(path);
        if(hash.isEmpty()) {valid=false;break;}
        if(name.endsWith("CMakeLists.txt")||name.endsWith(".cmake")){configuration.addData(name);configuration.addData(hash.toUtf8());}
        source.addData(name);source.addData(QByteArray(1,'\0'));source.addData(hash.toUtf8());
    }
    source.addData(result.dependency.toUtf8());
    result.configuration=QString::fromLatin1(configuration.result().toHex());
    if(valid)result.source=QString::fromLatin1(source.result().toHex());
    if(result.source.isEmpty())result.error="Source/dependency fingerprint unavailable";
    result.products=Provenance::buildProducts(project);
    const auto defaults=ToolchainProfile::load(toolkit);
    for(const auto& tool:QStringList{defaults.pluginval, QStandardPaths::findExecutable("cmake"),QStandardPaths::findExecutable("ctest"),
            project.build+"/bin/"+project.configuration+"/validator",project.build+"/bin/"+project.configuration+"/moduleinfotool",project.measurementExecutable,toolkit+"/desktop/run-isolated-validator.py"})
        if(!tool.isEmpty())result.toolHashes[tool]=Provenance::fileHash(tool);
    QFile spec(project.root+"/SPEC.md");if(spec.open(QIODevice::ReadOnly))result.specification=QString::fromUtf8(spec.read(65536));
    QFile stamp(project.build+"/studiohub-build.json");if(stamp.open(QIODevice::ReadOnly))result.buildStamp=QJsonDocument::fromJson(stamp.readAll()).object();
    QFile history(project.evidence+"/validation-history.jsonl");if(history.open(QIODevice::ReadOnly))result.history=history.readAll();
    result.buildStamp["inspection_branch"]=branch;
    return result;
}
