#pragma once
#include <QAbstractListModel>
#include <QJsonArray>
class ActivityModel final:public QAbstractListModel {
    Q_OBJECT
public:
    enum Role{TextRole=Qt::UserRole+1,KindRole};
    explicit ActivityModel(QObject* parent=nullptr):QAbstractListModel(parent){}
    int rowCount(const QModelIndex& parent={})const override{return parent.isValid()?0:m_entries.size();}
    QVariant data(const QModelIndex& index,int role)const override;
    QHash<int,QByteArray> roleNames()const override{return {{TextRole,"entryText"},{KindRole,"entryKind"}};}
    void append(const QString& kind,const QString& text,bool stream=false);
    QJsonArray serialize()const{return m_entries;}
    void restore(const QJsonArray& entries);
private:QJsonArray m_entries;
};
