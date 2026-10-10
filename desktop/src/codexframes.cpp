#include "codexframes.hpp"
#include <QJsonDocument>
#include <QJsonParseError>
QList<QJsonObject> CodexFrames::feed(const QByteArray& bytes,QString& error) {
 QList<QJsonObject> messages;error.clear();m_buffer+=bytes;
 if(m_buffer.size()>8*1024*1024){error="Codex frame exceeds 8 MiB";reset();return {};}
 for(;;){const auto end=m_buffer.indexOf('\n');if(end<0)break;
  const auto line=m_buffer.left(end).trimmed();m_buffer.remove(0,end+1);if(line.isEmpty())continue;
  QJsonParseError parse;const auto doc=QJsonDocument::fromJson(line,&parse);
  if(parse.error!=QJsonParseError::NoError||!doc.isObject()){error="Invalid Codex protocol frame";reset();return {};}
  messages<<doc.object();
 }
 return messages;
}
