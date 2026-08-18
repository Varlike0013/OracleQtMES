#ifndef CONDITIONMANAGER_H
#define CONDITIONMANAGER_H

#include <QMap>
#include <QString>
#include <QStringList>

class ConditionManager
{
public:
    ConditionManager();
    ~ConditionManager();

    // 条件管理（增删改查）
    void addCondition(const QString &key, const QString &value);
    bool removeCondition(const QString &key, const QString &value);
    void clearConditions();
    QStringList getConditionValues(const QString &key) const;
    QMap<QString, QStringList> getAllConditions() const;
    bool conditionExists(const QString &key, const QString &value) const;

private:
    QMap<QString, QStringList> m_conditions;
};

#endif // CONDITIONMANAGER_H
