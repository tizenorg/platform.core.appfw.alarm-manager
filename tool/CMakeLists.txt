# Test executable
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TEST_CFLAGS} -fpie")

add_executable(alarmmgr_get_all_info alarmmgr_get_all_info.c)
target_link_libraries(alarmmgr_get_all_info alarm ${pkgs_LDFLAGS} "-pie")
INSTALL(TARGETS alarmmgr_get_all_info DESTINATION bin)

add_executable(alarmmgr_add_reference_periodic_alarm_withcb alarmmgr_add_reference_periodic_alarm_withcb.c)
target_link_libraries(alarmmgr_add_reference_periodic_alarm_withcb alarm ${pkgs_LDFLAGS} "-pie")
INSTALL(TARGETS alarmmgr_add_reference_periodic_alarm_withcb DESTINATION bin)

add_executable(alarmmgr_add_periodic_alarm_withcb alarmmgr_add_periodic_alarm_withcb.c)
target_link_libraries(alarmmgr_add_periodic_alarm_withcb alarm ${pkgs_LDFLAGS} "-pie")
INSTALL(TARGETS alarmmgr_add_periodic_alarm_withcb DESTINATION bin)

