SET(this_target alarm)
ADD_DEFINITIONS("-DPREFIX=\"${PREFIX}\"")

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "\${prefix}")
SET(LIBDIR "\${prefix}/lib")
SET(INCLUDEDIR "\${prefix}/include")
SET(VERSION_MAJOR 1)
SET(VERSION "${VERSION_MAJOR}.1")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

SET(LIB_PKGS glib-2.0 gobject-2.0 dlog bundle appsvc gio-2.0 gio-unix-2.0 libtzplatform-config)

INCLUDE(FindPkgConfig)
pkg_check_modules(lib_pkgs REQUIRED ${LIB_PKGS})

FOREACH(flag ${lib_pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag} -Wall -Wno-unused")
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

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")

ADD_LIBRARY(${this_target} SHARED ${${this_target}_SOURCE_FILES} alarm-mgr-stub.c)

SET_TARGET_PROPERTIES(${this_target} PROPERTIES COMPILE_FLAGS "${EXTRA_CFLAGS}")
SET_TARGET_PROPERTIES(${this_target} PROPERTIES LINK_FLAGS "-Wl,--as-needed -Wl,--hash-style=both")
SET_TARGET_PROPERTIES(${this_target} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}")

SET_TARGET_PROPERTIES(${this_target}
	PROPERTIES
	VERSION 0.0.0
	SOVERSION ${MAJORVER}
	CLEAN_DIRECT_OUTPUT 1
)

TARGET_LINK_LIBRARIES(${this_target} ${lib_pkgs_LDFLAGS})

ADD_CUSTOM_COMMAND(TARGET ${this_target}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SHARED_LIBRARY_PREFIX}${this_target}${CMAKE_SHARED_LIBRARY_SUFFIX}.${FULLVER}${LIBRARY_OUTPUT_PATH}	${CMAKE_SHARED_LIBRARY_PREFIX}${this_target}${CMAKE_SHARED_LIBRARY_SUFFIX}.${MAJORVER}

)

# pkgconfig file
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/alarm-service.pc.in ${CMAKE_SOURCE_DIR}/alarm-service.pc @ONLY)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/alarm-service.pc DESTINATION lib/pkgconfig)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include/ FILES_MATCHING PATTERN "alarm.h")
INSTALL(TARGETS ${this_target} DESTINATION lib)

