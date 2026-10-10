#pragma once
#include "projectmanifest.hpp"
#include <QByteArray>
#include <QJsonObject>
struct ProjectInspection {
    ProjectManifest project;
    QString error, source, dependency, module, bundle, manifestHash, revision, configuration, specification;
    bool dirty=false;
    QJsonObject products, buildStamp, toolHashes, toolVersions;
    QByteArray history;
    static ProjectInspection inspect(const ProjectManifest& project,const QString& toolkit);
};
