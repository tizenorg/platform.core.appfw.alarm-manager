CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET (this_target alarm-server)

INCLUDE_DIRECTORIES(
	include
)

SET(DEPS_PKGS "glib-2.0 dlog aul bundle db-util appsvc pkgmgr-info vconf gio-2.0 gio-unix-2.0 capi-system-device libtzplatform-config libsystemd-login")

IF(_APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG)
ADD_DEFINITIONS("-D_APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG")
ENDIF(_APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG)

message("${DEPS_PKGS}")

INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED ${DEPS_PKGS})

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag} -Wall -Wno-unused")
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

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -fpie")

TARGET_LINK_LIBRARIES(${this_target} ${pkgs_LDFLAGS})
TARGET_LINK_LIBRARIES(${this_target} "-lrt -lm -pie")
TARGET_LINK_LIBRARIES(${this_target} alarm)

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(tool)

CONFIGURE_FILE(alarm-service.conf.in alarm-service.conf @ONLY)
INSTALL(TARGETS ${this_target} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
IF(_APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG)
	INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/alarmmgr_log_dump.sh DESTINATION ${SYSCONF_INSTALL_DIR}/dump.d/module.d/)
ENDIF(_APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/alarm-service.conf DESTINATION ${SYSCONF_INSTALL_DIR}/dbus-1/system.d/)

CONFIGURE_FILE(org.tizen.alarm.manager.service.in org.tizen.alarm.manager.service @ONLY)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.tizen.alarm.manager.service DESTINATION ${SHARE_INSTALL_PREFIX}/dbus-1/system-services/)
