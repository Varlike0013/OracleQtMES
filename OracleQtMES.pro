# 临时项目文件，仅用于提取翻译
TEMPLATE = app
LANGUAGE = C++

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    logindialog.cpp \
    dbconfigmanager.cpp \
    Oracle/oracle_sajet.cpp \
    Oracle/oracle_dt.cpp \
    Oracle/oracle_manager.cpp \
    Sajet/sajetmainwindow.cpp \
    Gedt/gedtmainwindow.cpp

HEADERS += \
    mainwindow.h \
    logindialog.h \
    dbconfigmanager.h \
    Oracle/oracle_sajet.h \
    Oracle/oracle_dt.h \
    Oracle/oracle_manager.h \
    Sajet/sajetmainwindow.h \
    Gedt/gedtmainwindow.h

FORMS += \
    mainwindow.ui \
    logindialog.ui \
    Sajet/sajetmainwindow.ui \
    Gedt/gedtmainwindow.ui

TRANSLATIONS = OracleQtMES_zh_CN.ts