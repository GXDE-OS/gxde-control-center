set(BIN_NAME gxde-blur-settings)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets DBus)

add_executable(${BIN_NAME}
    main.cpp
    blursettingswindow.cpp
    blursettingswindow.h
)

target_link_libraries(${BIN_NAME} PRIVATE
    Qt6::Widgets
    Qt6::DBus
)

# Keep the helper next to gxde-control-center in both the build and install trees.
set_target_properties(${BIN_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/frame"
)

install(TARGETS ${BIN_NAME} DESTINATION bin)
