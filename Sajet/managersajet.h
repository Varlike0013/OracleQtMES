#ifndef MANAGERSAJET_H
#define MANAGERSAJET_H

#include "oracle_manager.h"
#include "QFuture"
#include <QComboBox>

class ManagerSajet
{
public:
    ManagerSajet();
    static bool is_SERIAL_NUMBER(const QString &serialNumber);
    static bool is_ReworkNo(const QString &rework);
    static bool is_CartonNo(const QString &input);
    static bool is_WorkOrderNo(const QString &input);
    static bool is_QcNo(const QString &input);
    static bool is_RouteName(const QString &input);
    static QFuture<bool> exportGatewayData(const QString &filePath);
    static QFuture<QString> updateErpData();
    static QString insert_user_action(const QString &user_action,const QString &status = QString(),const QString &target = QString());
    static void loadPDline(QComboBox *comboBox);
    static void loadProcess(QComboBox *comboBox, const QString &line);
    static void loadRouteProcess(QComboBox *comboBox, const QString &route);
    static void loadTerminal(QComboBox *comboBox, const QString &line,const QString &process);

private:
    static bool exportGatewayDataSync(const QString &filePath, QString &errorMsg);
    static QString updateErpDataSync();
    static QString getLocalIP();

};

#endif // MANAGERSAJET_H
