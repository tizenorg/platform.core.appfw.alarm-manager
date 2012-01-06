/*
 *  alarm-manager
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Venkatesha Sarpangala <sarpangala.v@samsung.com>, Jayoun Lee <airjany@samsung.com>,
 * Sewook Park <sewook7.park@samsung.com>, Jaeho Lee <jaeho81.lee@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include<stdio.h>
#include<stdlib.h>
#include<glib.h>
#include<glib-object.h>
#include <dlog/dlog.h>
#include <alarm.h>
#include <bundle.h>
#include <appsvc.h>

void create_test()
{
	time_t current_time;
	struct tm current_tm;
	alarm_entry_t *alarm_info = NULL;
	alarm_id_t alarm_id;
	int result;
	alarm_date_t test_time;

	bundle *b = NULL;

	b = bundle_create();

	if (NULL == b) {
		printf("Unable to create bundle!!!\n");
		return;
	}

	appsvc_set_operation(b, APPSVC_OPERATION_DEFAULT);
	appsvc_set_pkgname(b, "org.tizen.alarm-test");

	time(&current_time);

	printf("current time: %s\n", ctime(&current_time));
	localtime_r(&current_time, &current_tm);

	alarm_info = alarmmgr_create_alarm();
	if (NULL == alarm_info) {
		printf("alarmmgr_create_alarm failed!!!\n");
		return;
	}

	test_time.year = current_tm.tm_year;
	test_time.month = current_tm.tm_mon;
	test_time.day = current_tm.tm_mday;

	test_time.hour = current_tm.tm_hour;
	test_time.min = current_tm.tm_min + 1;
	test_time.sec = 5;

	alarmmgr_set_time(alarm_info, test_time);
	alarmmgr_set_repeat_mode(alarm_info, ALARM_REPEAT_MODE_WEEKLY,
				 ALARM_WDAY_MONDAY | ALARM_WDAY_TUESDAY |
				 ALARM_WDAY_WEDNESDAY | ALARM_WDAY_THURSDAY |
				 ALARM_WDAY_FRIDAY);

	alarmmgr_set_type(alarm_info, ALARM_TYPE_DEFAULT);
	//alarmmgr_set_type(alarm_info,ALARM_TYPE_VOLATILE);
	if ((result =
	     alarmmgr_add_alarm_appsvc_with_localtime(alarm_info, (void *)b,
						      &alarm_id)) < 0) {
		printf("Alarm creation failed!!! Alrmgr error code is %d\n",
		       result);
	} else {
		printf("Alarm created succesfully with alarm id %d\n",
		       alarm_id);
	}

}

int main(int argc, char **argv)
{
	GMainLoop *mainloop;

	g_type_init();

	mainloop = g_main_loop_new(NULL, FALSE);

	create_test();

	g_main_loop_run(mainloop);

	return 0;
}
