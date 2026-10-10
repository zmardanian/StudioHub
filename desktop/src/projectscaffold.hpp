#pragma once
#include <QString>
struct ProjectScaffold {
 static QString create(const QString& parent, const QString& templates, const QString& toolchain,
                       const QString& id, const QString& name, const QString& specification, QString& error, const QString& capability="none");
};
