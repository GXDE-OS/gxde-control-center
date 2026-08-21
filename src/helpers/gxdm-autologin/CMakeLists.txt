set(BIN_NAME gxde-control-center-gxdm-autologin-helper)

add_executable(${BIN_NAME} main.cpp)
target_link_libraries(${BIN_NAME} PRIVATE Qt6::Core)

install(TARGETS ${BIN_NAME} DESTINATION lib/gxde-control-center)
