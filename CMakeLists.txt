cmake_minimum_required(VERSION 3.19)
project(OracleQtMES LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools Sql Network)
set(CMAKE_AUTOUIC ON)

qt_standard_project_setup()

qt_add_executable(OracleQtMES
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
    logindialog.cpp logindialog.h  logindialog.ui
    dbconfigmanager.h dbconfigmanager.cpp
    oracle_manager.h oracle_manager.cpp
    Sajet/sajetmainwindow.h Sajet/sajetmainwindow.cpp Sajet/sajetmainwindow.ui
    Gedt/gedtmainwindow.h Gedt/gedtmainwindow.cpp Gedt/gedtmainwindow.ui
    Sajet/statustag.h Sajet/statustag.cpp Sajet/statustag.ui
    Sajet/reworkquestion.h Sajet/reworkquestion.cpp Sajet/reworkquestion.ui
    Sajet/findroute.h Sajet/findroute.cpp Sajet/findroute.ui
    Sajet/checkmac.h Sajet/checkmac.cpp Sajet/checkmac.ui
    Sajet/managersajet.h Sajet/managersajet.cpp
    Sajet/websoaptest.h Sajet/websoaptest.cpp Sajet/websoaptest.ui
    Sajet/serverip.h Sajet/serverip.cpp Sajet/serverip.ui
    Sajet/pcbqrcode.h Sajet/pcbqrcode.cpp Sajet/pcbqrcode.ui
    Sajet/reworkform.h Sajet/reworkform.cpp Sajet/reworkform.ui
    Sajet/materialerp.h Sajet/materialerp.cpp Sajet/materialerp.ui
    Sajet/clearkeyparts.h Sajet/clearkeyparts.cpp Sajet/clearkeyparts.ui
    Sajet/checkroute.h Sajet/checkroute.cpp Sajet/checkroute.ui
    Sajet/workorderinfo.h Sajet/workorderinfo.cpp Sajet/workorderinfo.ui
    Sajet/tgsgroup.h Sajet/tgsgroup.cpp Sajet/tgsgroup.ui
    Sajet/tgsgrouptest.h Sajet/tgsgrouptest.cpp Sajet/tgsgrouptest.ui
    Sajet/changeinput.h Sajet/changeinput.cpp Sajet/changeinput.ui
    conditionmanager.h conditionmanager.cpp
    Sajet/exporttravels.h Sajet/exporttravels.cpp Sajet/exporttravels.ui

)

qt_add_translations(
    TARGETS OracleQtMES
    TS_FILES OracleQtMES_zh_CN.ts OracleQtMES_en.ts
    SOURCES ${UI_SOURCES}
)
target_link_libraries(OracleQtMES
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Sql
        Qt::Network
)
target_include_directories(OracleQtMES PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Sajet
    ${CMAKE_CURRENT_SOURCE_DIR}/Gedt
)

install(TARGETS OracleQtMES
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET OracleQtMES
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
