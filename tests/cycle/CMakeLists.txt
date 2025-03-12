function(add_test_helper)
    set(options FULL DELTA DIFF PERF)
    set(oneValueArgs NAME DIR)
    set(multiValueArgs)
    cmake_parse_arguments(""
            "${options}"
            "${oneValueArgs}"
            "${multiValueArgs}"
            ${ARGN}
    )
    set(args_list)
    if(_FULL)
        list(APPEND args_list "--full")
    endif()
    if(_DELTA)
        list(APPEND args_list "--delta")
    endif()
    if(_DIFF)
        list(APPEND args_list "--diff")
    endif()
    if(_PERF)
        list(APPEND args_list "--perf")
    endif()
#    message("ARGS: ${args_list}")
#    message("NAME: ${_NAME}")
#    message("CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME ${_NAME}
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_script.py --dir ${_DIR} ${args_list}
    )
    set_tests_properties(${_NAME} PROPERTIES
            ENVIRONMENT "BUILD_DIR=${CMAKE_BINARY_DIR};SOURCE_DIR=${CMAKE_SOURCE_DIR}")
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${_DIR}
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
endfunction()

add_test_helper(
        NAME cycle01
        FULL
        DIR 01
)


add_test_helper(
        NAME cycle02
        FULL
        DIR 02
)


add_test_helper(
        NAME cycle03_diamond
        FULL
        DIR 03_diamond
)


add_test_helper(
        NAME cycle04_diamond
        FULL
        DIR 04_diamond
)


add_test_helper(
        NAME cycle05_neg
        FULL
        DIR 05_neg
)
