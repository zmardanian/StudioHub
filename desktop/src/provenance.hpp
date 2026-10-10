#pragma once
#include "projectmanifest.hpp"
#include <QJsonObject>

namespace Provenance {
QString fileHash(const QString& path);
QString treeHash(const QString& root);
QString dependencyHash(const ProjectManifest& project, const QString& toolkit);
QJsonObject buildProducts(const ProjectManifest& project);
bool compilerChanged(const QJsonObject& stamp, const QJsonObject& products);
bool writeJson(const QString& path, const QJsonObject& value, QString& error);
QJsonObject measurementAcceptance(const QJsonObject& report, const QString& capability);
bool validateMeasurement(const QJsonObject& report, const QString& capability, QString& error);
}
