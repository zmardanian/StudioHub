#include "codexframes.hpp"
#include <QCoreApplication>
#include <cstdio>
int main(int argc,char**argv){QCoreApplication app(argc,argv);CodexFrames frames;QString error;
 if(!frames.feed("{\"id\":",error).isEmpty()||!error.isEmpty())return 1;
 auto messages=frames.feed("1}\n{\"id\":2}\n",error);if(messages.size()!=2||messages[0].value("id").toInt()!=1)return 2;
 frames.feed("arbitrary output\n",error);if(error.isEmpty())return 3;
 frames.feed(QByteArray(8*1024*1024+1,'x'),error);if(error.isEmpty())return 4;
 if(frames.feed("{\"id\":3}\n",error).size()!=1)return 5;
 puts("Incremental framing, multiple messages, invalid/oversized frames and reset passed");}
