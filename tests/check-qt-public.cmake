# Check the linked program: internal functions can be declared in public headers.
find_program(READELF readelf REQUIRED)
execute_process(COMMAND "${READELF}" --dyn-syms --wide --demangle "${BINARY}"
    RESULT_VARIABLE result OUTPUT_VARIABLE symbols ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect ${BINARY}: ${error}")
endif()
if(symbols MATCHES "UND[^\n]*(PRIVATE_API|qt_blurImage|qt_findAtNxFile|platformNativeInterface|QNativeInterface::|QPlatformWindow::|QPlatformNativeInterface::)")
    message(FATAL_ERROR "Direct dependency on an unsupported Qt API: ${CMAKE_MATCH_0}")
endif()
