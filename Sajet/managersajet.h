#ifndef MANAGERSAJET_H
#define MANAGERSAJET_H

#include "oracle_manager.h"
#include "QFuture"

class ManagerSajet
{
public:
    ManagerSajet();
    static bool is_SERIAL_NUMBER(const QString &serialNumber);
    static bool is_ReworkNo(const QString &rework);
    static bool is_CartonNo(const QString &input);
    static bool is_WorkOrderNo(const QString &input);
    static bool is_QcNo(const QString &input);
    static QFuture<bool> exportGatewayData(const QString &filePath);
    static QFuture<QString> updateErpData();

private:
    static bool exportGatewayDataSync(const QString &filePath, QString &errorMsg);
    static QString updateErpDataSync();
};

#endif // MANAGERSAJET_H
