all:
	gcc -Wall -o alarmmgr_add_alarm_appsvc.exe alarmmgr_add_alarm_appsvc.c `pkg-config --libs --cflags glib-2.0 alarm-service dlog bundle appsvc appcore-efl glib-2.0`
	gcc -Wall -o alarmmgr_add_alarm_appsvc_with_localtime.exe alarmmgr_add_alarm_appsvc_with_localtime.c  `pkg-config --libs --cflags glib-2.0 alarm-service dlog bundle appsvc appcore-efl glib-2.0`
	gcc -Wall -o alarmmgr_get_info_appsvc.exe alarmmgr_get_info_appsvc.c `pkg-config --libs --cflags glib-2.0 alarm-service dlog bundle appsvc appcore-efl glib-2.0`
	gcc -Wall -o delete_alarm_appsvc.exe delete_alarm_appsvc.c  `pkg-config --libs --cflags glib-2.0 alarm-service dlog bundle appsvc appcore-efl glib-2.0`

clean:
	rm -rf *.exe





