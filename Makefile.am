ALARM_CFLAGS+=-DEXPORT_API="__attribute__ ((visibility(\"default\")))"
 
AM_CFLAGS = $(ALARM_MANAGER_CFLAGS)

lib_LTLIBRARIES = libalarm.la
alarmincludedir = $(includedir)
alarminclude_HEADERS = include/alarm.h

libalarm_la_SOURCES =  \
	alarm-lib.c \
	alarm-lib-stub.c 
libalarm_la_CPPFLAGS = $(ALARM_CFLAGS) $(ALARM_CPPFLAGS) $(ALARM_MANAGER_CFLAGS) $(ALARM_MANAGER_FEATURE_CFLAGS)\
		      -I$(srcdir)/include 
libalarm_la_LIBADD = $(ALARM_MANAGER_LIBS)

bin_PROGRAMS=alarm-server
alarm_server_SOURCES = \
	alarm-manager-registry.c \
	alarm-manager-schedule.c \
	alarm-manager-timer.c \
	alarm-manager.c

alarm_server_CPPFLAGS = $(ALARM_CFLAGS) $(ALARM_CPPFLAGS) $(ALARM_MANAGER_SERVER_CFLAGS) $(ALARM_MANAGER_FEATURE_CFLAGS)\
		      -I$(srcdir)/include 

alarm_server_LDADD = $(ALARM_MANAGER_SERVER_LIBS) -lrt

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = alarm-service.pc

