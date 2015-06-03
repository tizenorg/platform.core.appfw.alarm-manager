SET(this_target alarm)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

SET(LIB_PKGS glib-2.0 gobject-2.0 dlog bundle appsvc gio-2.0 gio-unix-2.0 libtzplatform-config)

INCLUDE(FindPkgConfig)
pkg_check_modules(lib_pkgs REQUIRED ${LIB_PKGS})

FOREACH(flag ${lib_pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(${this_target}_SOURCE_FILES
	alarm-lib.c
	alarm-lib-stub.c
)

ADD_CUSTOM_COMMAND(
        WORKING_DIRECTORY
        OUTPUT alarm-mgr-stub.c
        COMMAND gdbus-codegen --interface-prefix org.tizen.
				--generate-c-code alarm-mgr-stub
				../alarm_mgr.xml
        COMMENT "Generating Server GDBus .c/.h")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -Wall -Wno-unused -fvisibility=hidden")

ADD_LIBRARY(${this_target} SHARED ${${this_target}_SOURCE_FILES} alarm-mgr-stub.c)

SET_TARGET_PROPERTIES(${this_target} PROPERTIES COMPILE_FLAGS "${EXTRA_CFLAGS}")
SET_TARGET_PROPERTIES(${this_target} PROPERTIES LINK_FLAGS "-Wl,--as-needed -Wl,--hash-style=both")
SET_TARGET_PROPERTIES(${this_target} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}")

SET_TARGET_PROPERTIES(${this_target}
	PROPERTIES
	VERSION ${FULLVER}
	SOVERSION ${MAJORVER}
	CLEAN_DIRECT_OUTPUT 1
)

TARGET_LINK_LIBRARIES(${this_target} ${lib_pkgs_LDFLAGS})

# pkgconfig file
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/alarm-service.pc.in ${CMAKE_SOURCE_DIR}/alarm-service.pc @ONLY)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/alarm-service.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include/ FILES_MATCHING PATTERN "alarm.h")
INSTALL(TARGETS ${this_target} DESTINATION ${CMAKE_INSTALL_LIBDIR})

