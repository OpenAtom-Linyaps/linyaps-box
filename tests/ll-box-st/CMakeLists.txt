if(NOT linyaps-box_ENABLE_SMOKE_TESTS)
    return()
endif()

set(linyaps-box_SMOKE_TESTS
    ./01-run-whoami.json
    ./02-check-procfs.json
    ./03-check-mounts.json
    ./04-check-noNewPrivs.json
    ./05-check-env.json
    ./06-check-cwd.json
    ./07-check-capability.json
    ./08-check-umask.json
    ./09-check-rlimits.json
    ./10-check-oom.json
    ./11-output-to-null.json
    ./12-bind-host-dev.json
    ./13-pid-extension.json
    ./14-console-size.json
)

foreach(test ${linyaps-box_SMOKE_TESTS})
    add_test(
        NAME "${test}"
        COMMAND
            "${CMAKE_CURRENT_SOURCE_DIR}/ll-box-st"
            "${CMAKE_BINARY_DIR}/${linyaps-box_APP}"
            "${CMAKE_CURRENT_BINARY_DIR}/st-data"
            "${CMAKE_CURRENT_SOURCE_DIR}/${test}"
    )
    list(APPEND linyaps-box_TESTS "${test}")

    set_tests_properties(
        "${test}"
        PROPERTIES
            RESOURCE_LOCK st-data
            ENVIRONMENT
                "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/glibc_leaks.txt"
    )
endforeach()

if(NOT linyaps-box_ENABLE_COVERAGE)
    return()
endif()

set(COVERAGE_INFO "coverage.info")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Drop stale profiles before the tests run
    add_test(
        NAME coverage-clean-profraw
        COMMAND
            bash -c
            "find '${CMAKE_CURRENT_BINARY_DIR}' -name 'llvm-*.profraw' -delete"
    )

    set_tests_properties(
        coverage-clean-profraw
        PROPERTIES FIXTURES_SETUP coverage-profraw-clean
    )

    set_tests_properties(
        ${linyaps-box_TESTS}
        PROPERTIES
            FIXTURES_REQUIRED coverage-profraw-clean
            ENVIRONMENT
                "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/glibc_leaks.txt;LLVM_PROFILE_FILE=${CMAKE_CURRENT_BINARY_DIR}/llvm-%p.profraw"
    )

    find_program(LLVM_PROFDATA llvm-profdata REQUIRED)

    add_custom_command(
        OUTPUT default.profdata
        DEPENDS "${linyaps-box_APP}" test
        COMMAND
            "${CMAKE_COMMAND}" "-DPROFILE_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DPROF_OUT=${CMAKE_CURRENT_BINARY_DIR}/default.profdata"
            "-DLLVM_PROFDATA=${LLVM_PROFDATA}" -P
            "${CMAKE_CURRENT_SOURCE_DIR}/../merge-llvm-profiles.cmake"
    )

    find_program(LLVM_COV llvm-cov REQUIRED)

    # Sources are recorded project-relative, so llvm-cov must run from
    # the project root output paths therefore have to be absolute.
    add_custom_command(
        OUTPUT coverage.info
        DEPENDS default.profdata
        COMMAND
            "${LLVM_COV}" show
            "-instr-profile=${CMAKE_CURRENT_BINARY_DIR}/default.profdata"
            -object="$<TARGET_FILE:${linyaps-box_APP}>" >
            "${CMAKE_CURRENT_BINARY_DIR}/coverage.info"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )

    add_custom_command(
        OUTPUT coverage.tar.gz
        DEPENDS default.profdata
        COMMAND rm -rf "${CMAKE_CURRENT_BINARY_DIR}/coverage"
        COMMAND
            "${LLVM_COV}" show
            "-instr-profile=${CMAKE_CURRENT_BINARY_DIR}/default.profdata"
            -format=html -object="$<TARGET_FILE:${linyaps-box_APP}>"
            "-output-dir=${CMAKE_CURRENT_BINARY_DIR}/coverage"
        COMMAND
            tar -czf "${CMAKE_CURRENT_BINARY_DIR}/coverage.tar.gz" -C
            "${CMAKE_CURRENT_BINARY_DIR}" coverage
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )

    add_custom_target(coverage DEPENDS coverage.info coverage.tar.gz)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    find_program(GCOVR gcovr REQUIRED)
    add_custom_target(
        coverage
        COMMAND bash -c "find '${CMAKE_BINARY_DIR}' -name '*.gcda' -delete"
        COMMAND
            "${CMAKE_COMMAND}" -E chdir "${CMAKE_CURRENT_BINARY_DIR}"
            "${CMAKE_CTEST_COMMAND}" --output-on-failure
        COMMAND
            "${GCOVR}" -r . --object-directory
            "${CMAKE_BINARY_DIR}/CMakeFiles/linyaps-box.dir" --object-directory
            "${CMAKE_BINARY_DIR}/CMakeFiles/${linyaps-box_APP}.dir" --filter
            "src/linyaps_box" --txt --html --html-details -o coverage.html
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        DEPENDS "${linyaps-box_APP}"
    )
else()
    message(
        FATAL_ERROR
        "Coverage is not supported for ${CMAKE_CXX_COMPILER_ID}."
    )
endif()
