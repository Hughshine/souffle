# Maintained regression suite for the incremental artifact branch.
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

add_regression_case(dred_mix dred_mix_naive_vs_full 240)
add_regression_case(dred_hub dred_hub_rederive_naive_vs_full 240)
add_regression_case(deterministic_combo deterministic_inc_naive_combo_vs_full 240)
add_regression_case(deterministic_regional deterministic_inc_regional_single_round_vs_full 180)
add_regression_case(inc_regional_calibration inc_regional_calibration_single_interface 180)
add_regression_case(inc_regional_shared_delta_join inc_regional_shared_delta_join_vs_full 180)
add_regression_case(inc_regional_independent_overlap_skip inc_regional_independent_overlap_skip_vs_full 180)
add_regression_case(deterministic_regional_multiturn deterministic_inc_regional_multiturn_state_machine 240)
add_regression_case(deterministic_regional_degenerate deterministic_inc_regional_multiturn_degenerate 180)
add_regression_case(deterministic_derivation_guard deterministic_recursive_derivation_guard_vs_full 180)
add_regression_case(negated_absent_tuple negated_absent_tuple_universe 180)
add_regression_case(nonrecursive_timestamp_views nonrecursive_mixed_timestamp_views 180)
add_regression_case(canonical_cli canonical_online_cli_surface 180)

if (NOT TARGET check-regression)
    add_custom_target(check-regression
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --progress -L regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL
        COMMENT "Run maintained incremental artifact regression tests"
    )
    add_dependencies(check-regression souffle)
endif()
