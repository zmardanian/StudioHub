#pragma once
#include <QJsonObject>
#include <QList>
#include <QByteArray>
#include <QString>
class CodexFrames {
 QByteArray m_buffer;
public:
 void reset(){m_buffer.clear();}
 QList<QJsonObject> feed(const QByteArray& bytes,QString& error);
};
