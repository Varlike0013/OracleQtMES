#include "conditionmanager.h"

ConditionManager::ConditionManager() {}

ConditionManager::~ConditionManager() {}

void ConditionManager::addCondition(const QString &key, const QString &value)
{
    if (key.isEmpty() || value.isEmpty()) return;
    if (m_conditions.contains(key)) {
        if (!m_conditions[key].contains(value)) {
            m_conditions[key].append(value);
        }
    } else {
        m_conditions[key] = QStringList() << value;
    }
}

bool ConditionManager::removeCondition(const QString &key, const QString &value)
{
    if (!m_conditions.contains(key)) return false;
    bool removed = m_conditions[key].removeOne(value);
    if (m_conditions[key].isEmpty()) {
        m_conditions.remove(key);
    }
    return removed;
}

void ConditionManager::clearConditions()
{
    m_conditions.clear();
}

QStringList ConditionManager::getConditionValues(const QString &key) const
{
    return m_conditions.value(key, QStringList());
}

QMap<QString, QStringList> ConditionManager::getAllConditions() const
{
    return m_conditions;
}

bool ConditionManager::conditionExists(const QString &key, const QString &value) const
{
    return m_conditions.contains(key) && m_conditions[key].contains(value);
}
