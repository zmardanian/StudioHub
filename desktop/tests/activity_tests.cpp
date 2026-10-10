#include "activitymodel.hpp"
#include <QCoreApplication>
#include <cstdio>
int main(int argc,char**argv){QCoreApplication app(argc,argv);ActivityModel model;int changed=0;
 QObject::connect(&model,&QAbstractItemModel::dataChanged,[&]{++changed;});
 model.append("user","task");model.append("assistant","hello",true);model.append("assistant"," world",true);
 if(model.rowCount()!=2||changed!=1||model.data(model.index(1),ActivityModel::TextRole).toString()!="hello world")return 1;
 ActivityModel recovered;recovered.restore(model.serialize());if(recovered.rowCount()!=2)return 2;
 for(int i=0;i<600;++i)model.append("log",QString::number(i));if(model.rowCount()!=500)return 3;
 puts("Structured streaming, bounded entries and recovery passed");}
