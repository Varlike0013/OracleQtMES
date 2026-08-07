#ifndef MANAGERSAJET_H
#define MANAGERSAJET_H

#include "oracle_manager.h"

class ManagerSajet
{
public:
    ManagerSajet();
    static bool is_SERIAL_NUMBER(const QString &serialNumber);
    static bool is_ReworkNo(const QString &rework);
    static bool is_CartonNo(const QString &input);
    static bool is_WorkOrderNo(const QString &input);
    static bool is_QcNo(const QString &input);
};

#endif // MANAGERSAJET_H
