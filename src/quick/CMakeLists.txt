cmake_minimum_required(VERSION 3.16)

# The QML frontend. Modules still come from dcc-common in src/frame and
# keep the same ModuleInterface and FrameProxyInterface.

set(BIN_NAME gxde-control-center)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2)
find_package(GxdeBerneseQt6 0.1.1 REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_add_qml_module(${BIN_NAME}
    URI org.gxde.controlcenter
    VERSION 1.0
    # Code from qmlcachegen uses private Qt API, even with --only-bytecode.
    # Qt compiles the QML at run time and caches it on disk.
    NO_CACHEGEN
    QML_FILES
        qml/Main.qml
        qml/AllSettingsPage.qml
        qml/ContentHeader.qml
        qml/HomePage.qml
        qml/LegacyContentPage.qml
        qml/NavigationBar.qml
        qml/NavigationGrid.qml
        qml/PageStack.qml
        qml/SettingsWindow.qml
        qml/WindowModulePage.qml
    SOURCES
        controlcenter.cpp
        controlcenter.h
        controlcenteradaptor.cpp
        controlcenteradaptor.h
        homewidgets.cpp
        homewidgets.h
        legacywidgetitem.cpp
        legacywidgetitem.h
        modulehost.cpp
        modulehost.h
        userinfo.cpp
        userinfo.h
)

target_link_libraries(${BIN_NAME} PRIVATE
    dcc-common
    Qt6::Quick
    Qt6::QuickControls2
    GxdeBernese::Qt6
)

install(TARGETS ${BIN_NAME} DESTINATION bin)

if (BUILD_TESTING)
    add_test(NAME no-qt-private-api
        COMMAND ${CMAKE_COMMAND}
            -DBINARY=$<TARGET_FILE:${BIN_NAME}>
            -P ${PROJECT_SOURCE_DIR}/tests/check-qt-public.cmake)
    add_test(NAME no-kwindoweffects
        COMMAND ${CMAKE_COMMAND}
            -DBINARY=$<TARGET_FILE:${BIN_NAME}>
            -P ${PROJECT_SOURCE_DIR}/tests/check-no-kwindoweffects.cmake)
endif()
