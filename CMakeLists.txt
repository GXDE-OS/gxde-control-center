cmake_minimum_required(VERSION 3.7)

project(gxde-control-center)

include(CTest)

find_package(Dtk6CMake REQUIRED)

# Install settings
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX /usr)
endif ()

if (NOT (${CMAKE_BUILD_TYPE} MATCHES "Debug"))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")

    # generate qm
    execute_process(COMMAND bash "translate_generation.sh"
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif ()

# Test architecture
execute_process(COMMAND dpkg-architecture -qDEB_BUILD_ARCH OUTPUT_VARIABLE ARCHITECTURE RESULT_VARIABLE EXIT_CODE)
if (${EXIT_CODE} EQUAL 0)
    string(STRIP ${ARCHITECTURE} ARCHITECTURE)

    if (${ARCHITECTURE} STREQUAL "sw_64")
        # add compiler flags -mieee for mathmatic
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mieee")
    endif()
endif()

set(STRONG_PASSWORD false)
set(AllowEnableMultiScaleRatio false)
set(AllowCloudSync false)

configure_file(files/gxde-control-center.conf.in files/gxde-control-center.conf @ONLY)

# Work in progress: the QML frontend in src/quick.
option(DCC_QML_FRONTEND "Build the QML frontend instead of the QWidget one" OFF)

add_subdirectory("src/frame")
if (DCC_QML_FRONTEND)
    add_subdirectory("src/quick")
endif()
add_subdirectory("src/dialogs")
add_subdirectory("src/helpers/gxdm-autologin")
add_subdirectory("src/helpers/blur-settings")

## qm files
file(GLOB QM_FILES "translations/*.qm")
install(FILES ${QM_FILES} DESTINATION share/gxde-control-center/translations)

if (DCC_QML_FRONTEND)
    # The QML frontend opens its window with no options, so the menu entry
    # runs it plainly and D-Bus starts it with --daemon.
    foreach(file gxde-control-center.desktop com.deepin.dde.ControlCenter.service)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${file})
        file(READ ${file} content)
        if (file MATCHES "\\.desktop$")
            string(REGEX REPLACE "\nExec=[^\n]*" "\nExec=gxde-control-center" content "${content}")
        else()
            string(REGEX REPLACE "\nExec=[^\n]*" "\nExec=/usr/bin/gxde-control-center --daemon" content "${content}")
        endif()
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${file} "${content}")
    endforeach()
    set(DESKTOP_FILE ${CMAKE_CURRENT_BINARY_DIR}/gxde-control-center.desktop)
    set(SERVICE_FILE ${CMAKE_CURRENT_BINARY_DIR}/com.deepin.dde.ControlCenter.service)
else()
    set(DESKTOP_FILE gxde-control-center.desktop)
    set(SERVICE_FILE com.deepin.dde.ControlCenter.service)
endif()

#desktop
install(FILES ${DESKTOP_FILE} DESTINATION share/applications)

#service
install(FILES ${SERVICE_FILE} DESTINATION share/dbus-1/services)

#datebases
install(FILES data/timezone.db DESTINATION share/gxde-control-center)

#internal testing group
if (NOT DEFINED DCC_DISABLE_TESTING_GROUP)
    install(DIRECTORY tools/gxde-control-center DESTINATION share/)
endif()

if (NOT DEFINED DCC_UPDATE_USE_PACKAGEKIT)
    install(PROGRAMS tools/gxde-control-center/gxde-update-worker.sh DESTINATION lib/gxde-control-center)
endif()


# conf file
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/files/gxde-control-center.conf DESTINATION share/gxde-control-center)

# polkit policy file
if (NOT DEFINED DCC_UPDATE_USE_PACKAGEKIT)
    install(FILES com.gxde.controlcenter.update.policy DESTINATION share/polkit-1/actions)
endif()
install(FILES com.gxde.controlcenter.gxdm-autologin.policy DESTINATION share/polkit-1/actions)
if (OS_DEEPIN_PROFESSIONAL)
    if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
        install(FILES com.deepin.controlcenter.addomain.policy DESTINATION share/polkit-1/actions)
    endif()
endif()
