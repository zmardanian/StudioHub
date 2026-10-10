#include "projectmanifest.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <cstdio>
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ProjectManifest p; QString error;
    if (!ProjectManifest::load(app.arguments().at(1), p, error)) { fprintf(stderr,"%s\n",qPrintable(error)); return 1; }
    QTemporaryDir d; QDir().mkdir(d.path()+"/build");
    const QStringList unsafe{"../outside", "/tmp/outside", "build/../../outside", ""};
    for (const auto& path : unsafe) if (!ProjectManifest::safePath(d.path(), path, error).isEmpty()) return 2;
    QFile::link("/tmp", d.path()+"/escape");
    if (!ProjectManifest::safePath(d.path(), "escape/artifact", error).isEmpty()) return 3;
    QFile source(app.arguments().at(1)); if (!source.open(QIODevice::ReadOnly)) return 6;
    auto o = QJsonDocument::fromJson(source.readAll()).object();
    o["id"] = "second-project"; o["name"] = "Independent plugin";
    o["dependency"] = QJsonObject{{"identity","fixture"},{"fingerprint_files",QJsonArray{"CMakeLists.txt"}}};
    QFile dependency(d.path()+"/CMakeLists.txt");dependency.open(QIODevice::WriteOnly);dependency.write("fixture");dependency.close();
    QFile f(d.path()+"/studiohub-project.json"); if (!f.open(QIODevice::WriteOnly)) return 7; f.write(QJsonDocument(o).toJson()); f.close();
    if (!ProjectManifest::load(f.fileName(), p, error) || p.name != "Independent plugin") return 4;
    auto valid=o;
    o["tests"]=QString("not an array");f.open(QIODevice::WriteOnly);f.write(QJsonDocument(o).toJson());f.close();
    if(ProjectManifest::load(f.fileName(),p,error))return 8;
    o=valid;dependency.remove();f.open(QIODevice::WriteOnly);f.write(QJsonDocument(o).toJson());f.close();
    if(ProjectManifest::load(f.fileName(),p,error))return 9;
    dependency.open(QIODevice::WriteOnly);dependency.write("fixture");dependency.close();
    o["artifact"] = "../escape.vst3"; if (!f.open(QIODevice::WriteOnly)) return 7; f.write(QJsonDocument(o).toJson()); f.close();
    if (ProjectManifest::load(f.fileName(), p, error)) return 5;
    puts("Manifest independence, traversal and symlink controls passed");
}
