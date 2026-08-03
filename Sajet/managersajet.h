#ifndef MANAGERSAJET_H
#define MANAGERSAJET_H

#include "oracle_manager.h"

class ManagerSajet
{
public:
    ManagerSajet();
    static bool is_SERIAL_NUMBER(const QString &serialNumber);
    static bool is_ReworkNo(const QString &rework);
};

#endif // MANAGERSAJET_H
