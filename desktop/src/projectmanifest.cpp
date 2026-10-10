#include "projectmanifest.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

QString ProjectManifest::safePath(const QString& root, const QString& relative, QString& error)
{
    if (relative.isEmpty() || QDir::isAbsolutePath(relative) || relative.contains('\\') ||
        relative.contains(QChar::Null) || relative.split('/').contains("..")) {
        error = "Paths must be nonempty project-relative paths without traversal";
        return {};
    }
    const auto path = QDir::cleanPath(root + "/" + relative);
    QString ancestor = path;
    while (!QFileInfo::exists(ancestor) && ancestor != QFileInfo(ancestor).absolutePath()) {
        if (QFileInfo(ancestor).isSymLink()) { error="Dangling path symlink rejected";return {}; }
        ancestor = QFileInfo(ancestor).absolutePath();
    }
    const auto resolved = QFileInfo(ancestor).canonicalFilePath();
    if (resolved != root && !resolved.startsWith(root + '/')) {
        error = "Path or symlink escapes the selected project: " + relative;
        return {};
    }
    return path;
}

bool ProjectManifest::load(const QString& file, ProjectManifest& out, QString& error)
{
    QFile input(file);
    if (!input.open(QIODevice::ReadOnly) || input.size() > 65536) {
        error = "Cannot read project manifest (maximum 64 KiB): " + file; return false;
    }
    const auto doc = QJsonDocument::fromJson(input.readAll());
    if (!doc.isObject()) { error = "Manifest must be a JSON object"; return false; }
    const auto o = doc.object();
    ProjectManifest p;
    p.root = QFileInfo(file).canonicalPath();
    const QRegularExpression identifier("^[A-Za-z][A-Za-z0-9_-]{0,63}$");
    auto identifierField = [&](const QJsonObject& object, const QString& key, QString& value) {
        value = object.value(key).toString();
        if (!identifier.match(value).hasMatch()) { error = "Invalid identifier: " + key; return false; }
        return true;
    };
    if (o.value("schema_version").toInt() != 1 || o.value("format").toString() != "VST3" ||
        o.value("root").toString() != ".") { error = "Unsupported manifest version, format or root"; return false; }
    if (!identifierField(o, "id", p.id)) return false;
    p.name = o.value("name").toString(); p.description = o.value("description").toString();
    p.category = o.value("category").toString();
    if (p.name.trimmed().isEmpty() || p.name.size() > 128 || p.category.isEmpty() ||
        !o.value("gui").isBool()) { error = "Invalid name, category or GUI declaration"; return false; }
    p.gui = o.value("gui").toBool();
    if(!o.value("build").isObject() || !o.value("tests").isArray() || !o.value("measurements").isObject() || !o.value("dependency").isObject()) {error="Manifest field type mismatch";return false;}
    const auto b = o.value("build").toObject();
    if(!b.value("test_targets").isArray() || !b.value("definitions").isObject()) {error="Build field type mismatch";return false;}
    if (!identifierField(b, "target", p.target)) return false;
    p.configuration = b.value("configuration").toString();
    if (p.configuration != "Release" && p.configuration != "Debug" && p.configuration != "RelWithDebInfo") {
        error = "Unsupported CMake configuration"; return false;
    }
    auto path = [&](const QJsonObject& object, const QString& key, QString& value) {
        value = safePath(p.root, object.value(key).toString(), error); return !value.isEmpty();
    };
    if (!path(b, "source", p.source) || !path(b, "directory", p.build) ||
        !path(o, "artifact", p.bundle) || !path(o, "module", p.module) ||
        !path(o, "evidence", p.evidence)) return false;
    if (p.build == p.root || p.build == p.source || !p.module.startsWith(p.bundle + '/') ||
        !p.bundle.endsWith(".vst3") || !p.bundle.startsWith(p.build + '/')) {
        error = "Build and artifact locations are inconsistent"; return false;
    }
    p.targets << p.target;
    for (const auto& item : b.value("test_targets").toArray()) {
        const auto s = item.toString();
        if (!identifier.match(s).hasMatch()) { error = "Invalid test target"; return false; }
        p.targets << s;
    }
    for (const auto& item : o.value("tests").toArray()) {
        const auto s = item.toString();
        if (!identifier.match(s).hasMatch()) { error = "Invalid CTest test name"; return false; }
        p.tests << s;
    }
    if (p.tests.isEmpty()) { error = "At least one DSP test is required"; return false; }
    const auto defs = b.value("definitions").toObject();
    const QRegularExpression defineName("^[A-Z][A-Z0-9_]*$");
    const QRegularExpression defineValue("^[A-Za-z0-9_.-]{1,128}$");
    for (auto it = defs.begin(); it != defs.end(); ++it) {
        const auto value = it.value().toString();
        if (!defineName.match(it.key()).hasMatch() || !defineValue.match(value).hasMatch() ||
            it.key() == "CMAKE_PROJECT_INCLUDE" || it.key() == "CMAKE_TOOLCHAIN_FILE" ||
            it.key().startsWith("CMAKE_")) { error = "Unsafe CMake definition: " + it.key(); return false; }
        p.definitions << "-D" + it.key() + "=" + value;
    }
    // Development builds always suppress global plugin links and SDK example scans.
    p.definitions << "-DSMTG_CREATE_PLUGIN_LINK=OFF" << "-DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF"
                  << "-DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF";
    const auto m = o.value("measurements").toObject();
    p.capability = m.value("capability").toString();
    if (p.capability != "dynamics" && p.capability != "stereo-utility" && p.capability != "none") {
        error = "Unsupported measurement capability"; return false;
    }
    if (p.capability != "none" && (!path(m, "report", p.measurement) ||
        !path(m, "executable", p.measurementExecutable))) return false;
    if (p.capability != "none" && !p.measurementExecutable.startsWith(p.build + '/')) {
        error = "Measurement executable must be inside the build directory"; return false;
    }
    const auto d = o.value("dependency").toObject();
    p.dependencyIdentity = d.value("identity").toString();
    if (p.dependencyIdentity.isEmpty()) { error = "Missing dependency identity"; return false; }
    if(!d.value("fingerprint_files").isArray()) {error="Dependency fingerprint_files must be an array";return false;}
    for (const auto& item : d.value("fingerprint_files").toArray()) {
        const auto value = safePath(p.root, item.toString(), error);
        if (value.isEmpty() || !QFileInfo(value).isFile()) {error="Dependency fingerprint file is missing";return false;}
        p.dependencyFiles << value;
    }
    if (p.dependencyFiles.isEmpty()) { error = "Dependency fingerprint files are required"; return false; }
    out = p; return true;
}
