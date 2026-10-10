#include "provenance.hpp"
#include "joboutcome.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <cstdio>
int main(int argc,char**argv) {
 QCoreApplication app(argc,argv);QTemporaryDir temp;
 QDir().mkpath(temp.path()+"/bundle/Contents/Resources");
 auto write=[](const QString& p,const QByteArray& b){QFile f(p);return f.open(QIODevice::WriteOnly)&&f.write(b)==b.size();};
 if(!write(temp.path()+"/bundle/module.so","module")||!write(temp.path()+"/bundle/Contents/Resources/editor.uidesc","before"))return 1;
 const auto hash=Provenance::treeHash(temp.path()+"/bundle");
 write(temp.path()+"/bundle/Contents/Resources/editor.uidesc","after");
 if(hash.isEmpty()||hash==Provenance::treeHash(temp.path()+"/bundle"))return 2;
 QString error;
 if(Provenance::validateMeasurement({{"old",true}},"stereo-utility",error))return 3;
 if(Provenance::writeJson(temp.path()+"/missing/report.json",{{"valid",true}},error))return 4;
 if(!Provenance::writeJson(temp.path()+"/report.json",{{"valid",true}},error))return 5;
 if(Job::classify(true,0,false,false,false,true)!=Job::Outcome::Pass)return 6;
 if(Job::classify(false,125,false,false,false,true)!=Job::Outcome::Blocked)return 7;
 if(Job::classify(false,0,false,false,false,false)!=Job::Outcome::Stale)return 8;
 if(Job::classify(false,125,true,true,true,false)!=Job::Outcome::Timeout)return 9;
 if(Job::classify(false,0,true,true,false,true)!=Job::Outcome::Canceled)return 10;
 QJsonObject metric{{"sample_rate_hz",48000},{"gain_error_db",0.},{"channel_integrity_max_ratio_error",0.},{"balance_max_absolute_error",0.},{"bypass_bit_transparent",true},{"smoothing_samples",481}};
 QJsonObject report{{"metrics",QJsonArray{metric}}};
 if(Provenance::measurementAcceptance(report,"stereo-utility").value("status")!="PASS")return 11;
 metric["gain_error_db"]=1.;report["metrics"]=QJsonArray{metric};
 if(Provenance::measurementAcceptance(report,"stereo-utility").value("status")!="FAIL")return 12;
 QJsonObject compiler{{"compiler_path","/fixture/compiler"},{"compiler_sha256","one"}};
 QJsonObject stamp{{"products",compiler}};
 if(Provenance::compilerChanged(stamp,compiler))return 13;
 compiler["compiler_sha256"]="two";
 if(!Provenance::compilerChanged(stamp,compiler))return 14;
 if(!QFile::link(temp.path()+"/bundle/Contents",temp.path()+"/bundle/linked-directory"))return 15;
 if(!Provenance::treeHash(temp.path()+"/bundle").isEmpty())return 16;
 if(!Job::workflowAfterInspection("Verification completed — all requested gates passed",true).startsWith("Verification stale"))return 17;
 if(Job::workflowAfterInspection("Verify project: Build",true)!="Verify project: Build")return 18;
 if(Provenance::measurementAcceptance({},"stereo-utility").value("status")!="FAIL")return 19;
 if(Provenance::measurementAcceptance({},"dynamics").value("status")!="FAIL")return 20;
 puts("Bundle resource mutation, measurement schema and evidence write failure passed");
}
