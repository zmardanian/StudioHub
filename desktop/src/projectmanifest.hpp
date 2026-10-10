#pragma once
#include <QJsonObject>
#include <QStringList>

struct ProjectManifest {
    QString id, name, root, description, category, target, configuration;
    QString source, build, bundle, module, evidence, measurement, measurementExecutable, capability;
    QStringList targets, tests, definitions, dependencyFiles;
    QString dependencyIdentity;
    bool gui = false;
    static bool load(const QString& file, ProjectManifest& result, QString& error);
    static QString safePath(const QString& root, const QString& relative, QString& error);
};
