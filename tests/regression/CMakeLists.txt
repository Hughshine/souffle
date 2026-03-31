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
add_regression_case(dred_mix dred_mix_naive_vs_full 240)
add_regression_case(dred_hub dred_hub_rederive_naive_vs_full 240)
add_regression_case(detopt_combo detopt_inc_naive_combo_vs_full 240)
add_regression_case(detopt_regional detopt_inc_regional_single_round_vs_full 180)
add_regression_case(detopt_derivation_guard detopt_recursive_derivation_guard_vs_full 180)
add_regression_case(nonrecursive_timestamp_views nonrecursive_mixed_timestamp_views 180)
add_regression_case(rewrite_split rewrite_split_modes_equiv 180)
add_regression_case(rewrite_dirty_detect rewrite_dirty_detect_equiv 180)
add_regression_case(full_det_modes full_det_modes 120)
add_regression_case(dump_contract dump_outputs_contract 180)
add_regression_case(full_const_negation_grounding full_const_negation_grounding 180)
add_regression_case(canonical_compile canonical_compile_defaults_contract 180)
add_regression_case(canonical_cli canonical_online_cli_surface 180)
add_regression_case(graph_query_canonical graph_query_canonical_surface 180)
add_regression_case(side_channel_full_pipeline side_channel_full_pipeline_rewrite 300)
add_regression_case(scbf_rewrite_lane scbf_rewrite_runtime_lane 180)
add_regression_case(side_channel_inc_pipeline side_channel_incremental_pipeline_modes 300)
add_regression_case(taint_pipeline taint_stage_pipeline_compile_smoke 420)
add_regression_case(datarace_pipeline datarace_stage_pipeline_smoke 240)

if (NOT TARGET check-regression)
    add_custom_target(check-regression
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --progress -L regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL
        COMMENT "Run maintained Souffle regression tests"
    )
    add_dependencies(check-regression souffle)
endif()
