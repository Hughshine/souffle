# Maintained regression suite for this artifact branch.

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

add_regression_case(smoke smoke_exact_inference 300)
add_regression_case(rewrite_dispatch rewrite_dispatch_equiv 300)
add_regression_case(problog_string_roundtrip problog_string_roundtrip 300)
add_regression_case(problog_symbol_aggregate_roundtrip problog_symbol_aggregate_roundtrip 300)
add_regression_case(problog_fact_prob_alignment problog_fact_prob_alignment 300)
add_regression_case(problog_large_numeric_tuple_roundtrip problog_large_numeric_tuple_roundtrip 300)
add_regression_case(problog_query_named_variable_equality problog_query_named_variable_equality 300)
add_regression_case(problog_constraint_variable_equality_chain problog_constraint_variable_equality_chain 300)
add_regression_case(problog_sum_exact_roundtrip problog_sum_exact_roundtrip 300)
add_regression_case(exact_det_modes exact_det_modes 300)
add_regression_case(language_side_channel_mini language_side_channel_mini 300)
add_regression_case(language_taint_mini language_taint_mini 300)
add_regression_case(language_symbolization_mini language_symbolization_mini 300)
add_regression_case(dump_contract dump_outputs_contract 300)
add_regression_case(lifted_partial_outputs lifted_partial_outputs 300)
add_regression_case(lifted_unique_witness lifted_unique_witness 300)
add_regression_case(lifted_multi_witness_fallback lifted_multi_witness_fallback 300)
add_regression_case(lifted_multi_witness_direct lifted_multi_witness_direct 300)
add_regression_case(lifted_disconnected_witness_fallback lifted_disconnected_witness_fallback 300)
add_regression_case(lifted_boundary_shared_event lifted_boundary_shared_event 300)
add_regression_case(lifted_internal_template_ref lifted_internal_template_ref 300)

if (NOT TARGET check-regression)
    add_custom_target(check-regression
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --progress -L regression
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL
        COMMENT "Run maintained artifact regression tests"
    )
    add_dependencies(check-regression souffle)
endif()
