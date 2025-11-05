if(NOT DEFINED FILE1 OR NOT DEFINED FILE2 OR NOT DEFINED OUT)
    message(FATAL_ERROR "FILE1, FILE2, and OUT must be set")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E cat "${FILE1}" "${FILE2}"
    OUTPUT_FILE "${OUT}"
    RESULT_VARIABLE _rv
)
if(NOT _rv EQUAL 0)
    message(FATAL_ERROR "failed to concatenate ${FILE1} + ${FILE2}")
endif()
