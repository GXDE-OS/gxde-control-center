cmake_minimum_required(VERSION 3.7)

set(VERSION 4.0)

set(BIN_NAME reboot-reminder-dialog)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_FLAGS "-g -Wall")

if (DEFINED ENABLE_MIEEE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mieee")
endif()

# Install settings
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX /usr)
endif ()

if(NOT DEFINED DISABLE_SYS_UPDATE)

set(SRCS
        main.cpp
        reminderdialog.h
        reminderdialog.cpp
)

# Find the library
find_package(Qt6 REQUIRED COMPONENTS Widgets DBus)
find_package(Dtk6Core REQUIRED)
find_package(Dtk2widget REQUIRED)
find_package(DFrameworkdbusQt6 REQUIRED)

add_executable(${BIN_NAME} ${SRCS})
target_include_directories(${BIN_NAME} PUBLIC
    ${DTK2WIDGET_INCLUDE_DIR}
    ${DtkCore_INCLUDE_DIRS}
    ${Qt6Gui_PRIVATE_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
)

target_link_libraries(${BIN_NAME} PRIVATE
    DFrameworkdbusQt6::DFrameworkdbusQt6
    ${Dtk2widget_LIBRARIES}
    Qt6::Widgets
    Qt6::DBus
    Dtk6::Core
)

# bin
install(TARGETS ${BIN_NAME} DESTINATION lib/gxde-control-center/)

endif()
