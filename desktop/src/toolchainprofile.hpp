#pragma once
#include <QJsonObject>
#include <QString>
struct ToolchainProfile {
 QString sdk,pluginval,cmake,ctest,error;
 int jobs=2;
 static ToolchainProfile load(const QString& toolkit,const QJsonObject& overrides={});
};
