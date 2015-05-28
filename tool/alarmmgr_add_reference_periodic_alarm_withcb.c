#include<stdio.h>
#include<stdlib.h>
#include<glib.h>

#include "alarm.h"

int callback(alarm_id_t alarm_id, void* user_param)
{
	time_t current_time;
	time(&current_time);

	printf("Alarm[%d] has expired at %s\n", alarm_id, ctime(&current_time));

	return 0;
}

void create_test()
{
	int result = 0;
	alarm_id_t alarm_id;

	result = alarmmgr_add_reference_periodic_alarm_withcb(1, callback, NULL, &alarm_id);

	if (result < 0)
		printf("alarmmgr_add_reference_periodic_alarm_withcb : error_code : %d\n", result);
}

int main(int argc, char** argv)
{
	GMainLoop *mainloop;
	int result;

	g_type_init();

	mainloop = g_main_loop_new(NULL, FALSE);
	result = alarmmgr_init("org.tizen.alarmmgr.refperiodic");

	if (result < 0) {
		printf("fail to alarmmgr_init : error_code : %d\n", result);
	} else {
		create_test();
	}

	g_main_loop_run(mainloop);
}

