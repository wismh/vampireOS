if(NOT DEFINED FILE1 OR NOT DEFINED FILE2 OR NOT DEFINED OUT)
    message(FATAL_ERROR "FILE1, FILE2, and OUT must be set")
endif()

set(_files "${FILE1}" "${FILE2}")
if(DEFINED FILE3)
    list(APPEND _files "${FILE3}")
endif()
if(DEFINED FILE4)
    list(APPEND _files "${FILE4}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E cat ${_files}
    OUTPUT_FILE "${OUT}"
    RESULT_VARIABLE _rv
)
if(NOT _rv EQUAL 0)
    message(FATAL_ERROR "failed to concatenate disk image")
endif()
