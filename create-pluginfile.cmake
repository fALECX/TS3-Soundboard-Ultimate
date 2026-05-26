set(packageVersion "${RPSU_PLUGIN_PACKAGE_VERSION}")

if(NOT packageVersion)
    message(FATAL_ERROR "Could not determine plugin package version.")
endif()

file(MAKE_DIRECTORY "${RPSU_PLUGINFILE_OUTPUT_DIR}")

set(outputFile "${RPSU_PLUGINFILE_OUTPUT_DIR}/rp_soundboard_ultimate_${packageVersion}.ts3_plugin")

message("Creating final plugin package in ${RPSU_PLUGINFILE_OUTPUT_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar "cfv" "${outputFile}" --format=zip
            plugins package.ini
    WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
)
