# Maintained regression suite for this fork.
# Legacy tests under tests/ are intentionally not included here.

set(REGRESSION_RUNNER "${CMAKE_CURRENT_SOURCE_DIR}/run_regression_case.py")

function(add_regression_case test_name case_id timeout_sec)
    add_test(
        NAME "regression.${test_name}"
        COMMAND
            ${Python3_EXECUTABLE}
            "${REGRESSION_RUNNER}"
            --case "${case_id}"
            --souffle-bin "$<TARGET_FILE:souffle>"
            --work-root "${CMAKE_CURRENT_BINARY_DIR}"
    )
    set_tests_properties("regression.${test_name}" PROPERTIES
        LABELS "regression;${case_id}"
        TIMEOUT "${timeout_sec}"
    )
endfunction()

add_regression_case(smoke smoke_full_only 120)
add_regression_case(rewrite_split rewrite_split_modes_equiv 180)
add_regression_case(problog_string_roundtrip problog_string_roundtrip 180)
add_regression_case(problog_symbol_aggregate_roundtrip problog_symbol_aggregate_roundtrip 180)
add_regression_case(full_det_modes full_det_modes 120)
add_regression_case(dump_contract dump_outputs_contract 180)

if (NOT TARGET check-regression)
    add_custom_target(check-regression
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --progress -L regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL
        COMMENT "Run maintained Souffle regression tests"
    )
    add_dependencies(check-regression souffle)
endif()
