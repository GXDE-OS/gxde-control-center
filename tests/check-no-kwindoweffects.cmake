# Keep the control center independent of the KWindowEffects library.
find_program(READELF readelf REQUIRED)
execute_process(COMMAND "${READELF}" --dynamic "${BINARY}"
    RESULT_VARIABLE result OUTPUT_VARIABLE dependencies ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect ${BINARY}: ${error}")
endif()
if(dependencies MATCHES "NEEDED[^\n]*libKF[56]WindowSystem")
    message(FATAL_ERROR "Unexpected KWindowSystem dependency: ${CMAKE_MATCH_0}")
endif()
