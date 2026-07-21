cmake_minimum_required(VERSION 3.19)
project(OracleQtMES LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools Sql)

qt_standard_project_setup()

qt_add_executable(OracleQtMES
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    logindialog.h logindialog.cpp logindialog.ui

    dbconfigmanager.h dbconfigmanager.cpp
    Oracle/oracle_sajet.cpp Oracle/oracle_sajet.h
    Oracle/oracle_dt.h Oracle/oracle_dt.cpp
    Oracle/oracle_manager.h Oracle/oracle_manager.cpp
    Sajet/sajetmainwindow.h Sajet/sajetmainwindow.cpp Sajet/sajetmainwindow.ui
    Gedt/gedtmainwindow.h Gedt/gedtmainwindow.cpp Gedt/gedtmainwindow.ui

)

qt_add_translations(
    TARGETS OracleQtMES
    TS_FILES OracleQtMES_zh_CN.ts
)

target_link_libraries(OracleQtMES
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Sql
        Qt::
)
target_include_directories(OracleQtMES PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/oracle
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
