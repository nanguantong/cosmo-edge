# copy_directory dereferences the executable links in node_modules/.bin. Restore
# them from the cached dependency tree when one is available before invoking npm.

set(SOURCE_BIN_DIR "${WEB_SRC_DIR}/node_modules/.bin")
set(STAGING_NODE_MODULES "${WEB_STAGING_DIR}/node_modules")

if(EXISTS "${SOURCE_BIN_DIR}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${STAGING_NODE_MODULES}/.bin"
        RESULT_VARIABLE remove_result)
    if(NOT remove_result EQUAL 0)
        message(FATAL_ERROR "Failed to remove copied web command links")
    endif()

    execute_process(
        COMMAND /bin/cp -a "${SOURCE_BIN_DIR}" "${STAGING_NODE_MODULES}/"
        RESULT_VARIABLE copy_result)
    if(NOT copy_result EQUAL 0)
        message(FATAL_ERROR "Failed to restore web command links")
    endif()
endif()
