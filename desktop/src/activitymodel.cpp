#include "activitymodel.hpp"
#include <QJsonObject>
QVariant ActivityModel::data(const QModelIndex& index,int role)const {
    if(!index.isValid()||index.row()<0||index.row()>=m_entries.size())return {};
    return m_entries.at(index.row()).toObject().value(role==TextRole?"text":"kind").toString();
}
void ActivityModel::append(const QString& kind,const QString& text,bool stream) {
    if(text.isEmpty())return;
    if(stream&&!m_entries.isEmpty()&&m_entries.last().toObject().value("kind").toString()==kind) {
        auto entry=m_entries.last().toObject();entry["text"]=(entry.value("text").toString()+text).right(32000);
        const auto row=m_entries.size()-1;m_entries.replace(row,entry);emit dataChanged(index(row),index(row));return;
    }
    if(m_entries.size()>=500){beginRemoveRows({},0,0);m_entries.removeFirst();endRemoveRows();}
    beginInsertRows({},m_entries.size(),m_entries.size());m_entries.append(QJsonObject{{"kind",kind},{"text",text.right(32000)}});endInsertRows();
}
void ActivityModel::restore(const QJsonArray& entries) {
    beginResetModel();m_entries={};
    for(const auto& value:entries){const auto e=value.toObject();if(e.value("text").isString()&&e.value("kind").isString())m_entries.append(QJsonObject{{"kind",e.value("kind")},{"text",e.value("text").toString().right(32000)}});}
    while(m_entries.size()>500)m_entries.removeFirst();endResetModel();
}
