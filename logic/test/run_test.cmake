if(NOT DEFINED TEST_EXECUTABLE)
    message(FATAL_ERROR "TEST_EXECUTABLE is not set")
endif()

set(test_runner_args "$ENV{EMAP_TEST_RUNNER_ARGS}")
if(test_runner_args)
    separate_arguments(test_runner_args NATIVE_COMMAND "${test_runner_args}")
endif()

execute_process(
    COMMAND "${TEST_EXECUTABLE}" ${test_runner_args}
    RESULT_VARIABLE test_result
)

if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "Test runner exited with code ${test_result}")
endif()
