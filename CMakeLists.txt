CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET (this_target alarm-server)
SET (EXPORT_API "__attribute__ ((visibility(\"default\")))")

ADD_DEFINITIONS("-DEXPORT_API=${EXPORT_API}")

SET(CMAKE_EXECUTABLE_SUFFIX "")
SET(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/output")

INCLUDE_DIRECTORIES(
	include
)

SET(DEPS_PKGS "glib-2.0 dlog aul bundle db-util appsvc pkgmgr-info vconf gio-2.0 gio-unix-2.0 capi-system-device libtzplatform-config")

IF(_APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG)
ADD_DEFINITIONS("-D_APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG")
ENDIF(_APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG)

message("${DEPS_PKGS}")

INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED ${DEPS_PKGS})

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag} -Wall -Wno-unused")
ENDFOREACH(flag)

FOREACH(flag ${pkgs_CFLAGS})
	SET(TEST_CFLAGS "${TEST_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET (${this_target}_SOURCE_FILES
	alarm-manager-registry.c
	alarm-manager-schedule.c
	alarm-manager-timer.c
	alarm-manager.c
	)

ADD_CUSTOM_COMMAND(
        WORKING_DIRECTORY
        OUTPUT alarm-mgr-stub.c
        COMMAND gdbus-codegen --interface-prefix org.tizen.
				--generate-c-code alarm-mgr-stub
				./alarm_mgr.xml
        COMMENT "Generating Server GDBus .c/.h")

ADD_EXECUTABLE (${this_target} ${${this_target}_SOURCE_FILES} alarm-mgr-stub.c)
ADD_DEPENDENCIES(${this_target} alarm)

SET(CMAKE_C_FLAGS "${OSP_DEBUG_FLAGS} ${OSP_OPT_FLAGS} ${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} ${OSP_COMPILER_FLAGS} -fpie")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -fvisibility=hidden")
SET(CMAKE_CXX_FLAGS "${OSP_DEBUG_FLAGS} ${OSP_OPT_FLAGS} ${CMAKE_CXX_FLAGS} ${EXTRA_CFLAGS} ${OSP_COMPILER_FLAGS}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TEST_CFLAGS}")

TARGET_LINK_LIBRARIES(${this_target} ${pkgs_LDFLAGS})
TARGET_LINK_LIBRARIES(${this_target} "-lrt -lm -pie")
TARGET_LINK_LIBRARIES(${this_target} alarm)

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(tool)

CONFIGURE_FILE(alarm-service.conf.in alarm-service.conf @ONLY)
INSTALL(TARGETS ${this_target} DESTINATION bin)
IF(_APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG)
	INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/alarmmgr_log_dump.sh DESTINATION ${SYSCONF_INSTALL_DIR}/dump.d/module.d/)
ENDIF(_APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/alarm-service.conf DESTINATION ${SYSCONF_INSTALL_DIR}/dbus-1/system.d/)
