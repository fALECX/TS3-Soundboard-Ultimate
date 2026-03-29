execute_process(
    COMMAND git describe --tags
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE gitVersionRaw
    ERROR_QUIET
    RESULT_VARIABLE gitDescribeResult
)

if(gitDescribeResult EQUAL 0)
    string(STRIP "${gitVersionRaw}" packageVersion)
else()
    set(packageVersion "${RPSU_PLUGIN_PACKAGE_VERSION}")
endif()

if(NOT packageVersion)
    set(packageVersion "0.2.0")
endif()

file(MAKE_DIRECTORY "${RPSU_PLUGINFILE_OUTPUT_DIR}")

set(outputFile "${RPSU_PLUGINFILE_OUTPUT_DIR}/rp_soundboard_ultimate_${packageVersion}.ts3_plugin")

message("Creating final plugin package in ${RPSU_PLUGINFILE_OUTPUT_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar "cfv" "${outputFile}" --format=zip .
    WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
)
