#include "provenance.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <cmath>

namespace Provenance {
QString fileHash(const QString& path) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return {};
    return QString::fromLatin1(hash.result().toHex());
}
QString treeHash(const QString& root) {
    if (!QFileInfo(root).isDir()) return {};
    QStringList paths;
    QDirIterator it(root, QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const auto path=it.next();
        const auto relative=QDir(root).relativeFilePath(path);
        if (relative.split('/').contains(".git")) continue;
        if (it.fileInfo().isSymLink()) return {}; // No untracked external resource inputs.
        if(it.fileInfo().isFile())paths << relative;
    }
    paths.sort(); if (paths.isEmpty()) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const auto& relative:paths) {
        const auto digest=fileHash(root+"/"+relative); if(digest.isEmpty()) return {};
        hash.addData(relative.toUtf8()); hash.addData(QByteArray(1,'\0')); hash.addData(digest.toUtf8());
    }
    return QString::fromLatin1(hash.result().toHex());
}
QString dependencyHash(const ProjectManifest& project,const QString& toolkit) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const auto sdk=treeHash(toolkit+"/third_party/vst3sdk"); if(sdk.isEmpty()) return {};
    hash.addData(sdk.toUtf8());
    for(const auto& path:project.dependencyFiles) {
        const auto digest=fileHash(path); if(digest.isEmpty()) return {};
        hash.addData(path.toUtf8());hash.addData(digest.toUtf8());
    }
    return QString::fromLatin1(hash.result().toHex());
}
QJsonObject buildProducts(const ProjectManifest& project) {
    QJsonObject products;
    QDirIterator it(project.build,QDir::Files,QDirIterator::Subdirectories);
    while(it.hasNext()) {
        const auto path=it.next();
        if(it.fileInfo().isExecutable() && project.targets.contains(it.fileInfo().fileName()) && it.fileInfo().fileName()!=project.target)
            products[QDir(project.build).relativeFilePath(path)]=fileHash(path);
    }
    products["module"]=fileHash(project.module);
    products["bundle"]=treeHash(project.bundle);
    products["cmake_cache"]=fileHash(project.build+"/CMakeCache.txt");
    QFile cache(project.build+"/CMakeCache.txt");
    if(cache.open(QIODevice::ReadOnly))for(const auto& line:cache.readAll().split('\n'))
        if(line.startsWith("CMAKE_CXX_COMPILER:FILEPATH=")) {
            const auto compiler=QString::fromUtf8(line.mid(line.indexOf('=')+1));
            products["compiler_path"]=compiler;products["compiler_sha256"]=fileHash(compiler);
        }
    return products;
}
bool writeJson(const QString& path,const QJsonObject& value,QString& error) {
    QSaveFile file(path);const auto bytes=QJsonDocument(value).toJson();
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()) {error=file.errorString();return false;}
    return true;
}
bool validateMeasurement(const QJsonObject& report,const QString& capability,QString& error) {
    if(capability=="dynamics") {
        if(!report.value("transfer").isArray() || report.value("transfer").toArray().isEmpty() || (!report.value("sample_rate_hz").isDouble() || !std::isfinite(report.value("sample_rate_hz").toDouble()) || report.value("sample_rate_hz").toDouble()<=0)) {
            error="Dynamics report requires transfer results and sample rate";return false;
        }
    } else if(capability=="stereo-utility") {
        const auto metrics=report.value("metrics").toArray();
        if(metrics.isEmpty()) {error="Stereo report requires measured metrics";return false;}
        for(const auto& item:metrics) {
            const auto m=item.toObject();
            for(const auto& key:{"sample_rate_hz","gain_error_db","channel_integrity_max_ratio_error","smoothing_samples","balance_max_absolute_error"})
                if(!m.value(key).isDouble()||!std::isfinite(m.value(key).toDouble())) {error=QString("Missing or invalid metric: ")+key;return false;}
            if(!m.value("bypass_bit_transparent").isBool()) {error="Missing bypass measurement";return false;}
        }
    } else {error="Unsupported measurement capability";return false;}
    return true;
}
QJsonObject measurementAcceptance(const QJsonObject& report,const QString& capability) {
    QString schemaError;bool pass=validateMeasurement(report,capability,schemaError);QJsonObject limits;
    if(capability=="stereo-utility") {
        limits={{"gain_error_db",1e-9},{"channel_integrity_max_ratio_error",1e-12},{"balance_max_absolute_error",1e-12},
                {"bypass_bit_transparent",true},{"smoothing_contract","10 ms ramp; first settled output sample at round(rate*0.01)+1"}};
        for(const auto& value:report.value("metrics").toArray()) {
            const auto row=value.toObject();
            pass &= std::abs(row.value("gain_error_db").toDouble())<=1e-9 &&
                    std::abs(row.value("channel_integrity_max_ratio_error").toDouble())<=1e-12 &&
                    std::abs(row.value("balance_max_absolute_error").toDouble())<=1e-12 && row.value("bypass_bit_transparent").toBool() &&
                    row.value("smoothing_samples").toInt()==int(std::llround(row.value("sample_rate_hz").toDouble()*.01))+1;
        }
    } else if(capability=="dynamics") {
        // Existing laboratory transfer gate: sine/peak-detector reference tolerance, not a commercial sound target.
        limits={{"transfer_error_db",0.1},{"reference","existing check_transfer.py sinusoidal detector/settling tolerance"}};
        for(const auto& value:report.value("transfer").toArray()) {
            const auto row=value.toObject();
            for(const auto& key:{"hard_knee_error_db","soft_knee_error_db"})
                pass &= row.value(key).isDouble()&&std::isfinite(row.value(key).toDouble())&&std::abs(row.value(key).toDouble())<=.1;
        }
    } else pass=false;
    return {{"status",pass?"PASS":"FAIL"},{"limits",limits},{"scope","tested source-DSP conditions only; human listening separate"}};
}

}

bool Provenance::compilerChanged(const QJsonObject& stamp,const QJsonObject& products) {
 const auto previous=stamp.value("products").toObject();
 return !previous.isEmpty() && (previous.value("compiler_path")!=products.value("compiler_path") || previous.value("compiler_sha256")!=products.value("compiler_sha256"));
}
