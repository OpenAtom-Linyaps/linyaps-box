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
    ./09-check-rlimit.json
    ./10-check-oom.json
    ./11-output-to-null.json
    ./12-bind-host-dev.json
    ./13-pid-extension.json)

foreach(test ${linyaps-box_SMOKE_TESTS})
  add_test(
    NAME "${test}"
    COMMAND
      "${CMAKE_CURRENT_SOURCE_DIR}/ll-box-st"
      "${CMAKE_BINARY_DIR}/${linyaps-box_APP}"
      "${CMAKE_CURRENT_BINARY_DIR}/st-data"
      "${CMAKE_CURRENT_SOURCE_DIR}/${test}")
  list(APPEND linyaps-box_TESTS "${test}")

  set_tests_properties(
    "${test}"
    PROPERTIES
      ENVIRONMENT
      "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/glibc_leaks.txt"
  )
endforeach()

if(NOT linyaps-box_ENABLE_COVERAGE)
  return()
endif()

set(COVERAGE_INFO "coverage.info")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set_tests_properties(
    ${linyaps-box_TESTS}
    PROPERTIES ENVIRONMENT
               "LLVM_PROFILE_FILE=${CMAKE_CURRENT_BINARY_DIR}/default.profraw")

  find_program(LLVM_PROFDATA llvm-profdata REQUIRED)

  add_custom_command(
    OUTPUT default.profdata
    DEPENDS "${linyaps-box_APP}" test
    COMMAND "${LLVM_PROFDATA}" merge -sparse default.profraw -o
            default.profdata)

  find_program(LLVM_COV llvm-cov REQUIRED)

  add_custom_command(
    OUTPUT coverage.info
    DEPENDS default.profdata
    COMMAND "${LLVM_COV}" show -instr-profile=default.profdata
            -object="$<TARGET_FILE_NAME:${linyaps-box_APP}>" > coverage.info)

  add_custom_command(
    OUTPUT coverage.tar.gz
    DEPENDS default.profdata
    COMMAND rm -rf coverage
    COMMAND
      "${LLVM_COV}" show -instr-profile=default.profdata -format=html
      -object="$<TARGET_FILE_NAME:${linyaps-box_APP}>" -output-dir=coverage
    COMMAND tar -czf coverage-html.tar.gz coverage)

  add_custom_target(coverage DEPENDS coverage.info coverage.tar.gz)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  find_program(LCOV lcov REQUIRED)
  find_program(SED sed REQUIRED)
  add_custom_target(
    coverage
    COMMAND "${LCOV}" --capture --directory . --output-file coverage.info
            --branch-coverage --rc geninfo_unexecuted_blocks=1
    COMMAND "${LCOV}" --remove coverage.info '/usr/*' --output-file
            coverage.info
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS "${linyaps-box_APP}" test)
else()
  message(FATAL_ERROR "Coverage is not supported for ${CMAKE_CXX_COMPILER_ID}.")
endif()
