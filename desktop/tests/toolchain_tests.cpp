#include "toolchainprofile.hpp"
#include <QCoreApplication>
#include <cstdio>
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto p=ToolchainProfile::load(app.arguments().at(1));
 if(!p.error.isEmpty()){fprintf(stderr,"%s\n",qPrintable(p.error));return 1;}
 if(ToolchainProfile::load(app.arguments().at(1),{{"jobs",0}}).error.isEmpty())return 2;
 if(ToolchainProfile::load(app.arguments().at(1),{{"pluginval","/nonexistent/fixture"}}).error.isEmpty())return 3;
 if(ToolchainProfile::load(app.arguments().at(1),{{"pluginval","/do-not-access/yabridge/fixture"}}).error.isEmpty())return 4;
 if(ToolchainProfile::load(app.arguments().at(1),{{"jobs",1.5}}).error.isEmpty())return 5;
 if(ToolchainProfile::load(app.arguments().at(1),{{"cmake","/do-not-access/yabridge/fixture"}}).error.isEmpty())return 6;
 auto pathTool=ToolchainProfile::load(app.arguments().at(1),{{"pluginval","sh"}});
 if(!pathTool.error.isEmpty() || pathTool.pluginval.isEmpty())return 7;
 puts("Toolchain identity, missing tool, unsafe path and parallelism validation passed");}
