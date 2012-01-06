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

#include "alarm.h"

int callback(alarm_id_t alarm_id)
{
    int error;
    time_t current_time;
    time(&current_time);

    printf("\nA: Alarm[%d] has expired at %s\n", alarm_id, ctime(&current_time));

    return 0;

}



void create_test()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;

    alarm_id_t alarm_id_1;
    alarm_id_t alarm_id_2;
    alarm_id_t alarm_id_3;

    int error_code;
	bool result = false;


    time(&current_time);

    printf("current time: %s\n", ctime(&current_time));
    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 0;
    alarm_info.start.month = 0;
    alarm_info.start.day = 0;

    alarm_info.end.year = 2008;
    alarm_info.end.month = 12;
    alarm_info.end.day = 31;

    alarm_info.start.hour = current_tm.tm_hour;
    alarm_info.start.min = current_tm.tm_min+1;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = ALARM_REPEAT_MODE_ONCE;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;
	
	alarm_info.alarm_type = LOCAL_TIME_FIXED;
	
	alarm_info.reserved_info = 20001;

    result = alarm_create(&alarm_info, &alarm_id_1, &error_code);
	if (!result)
	{
		printf("test 1 failed: %d\n", error_code);
	}

	alarm_info.reserved_info = 20002;

    result = alarm_create(&alarm_info, &alarm_id_2, &error_code);
	if (!result)
	{
		printf("test 2 failed: %d\n", error_code);
	}

	alarm_info.reserved_info = 0;

    result = alarm_create(&alarm_info, &alarm_id_3, &error_code);
	if (!result)
	{
		printf("test 3 failed: %d\n", error_code);
	}

}



int main(int argc, char** argv)
{
    int error_code;
    GMainLoop *mainloop;
    mainloop = g_main_loop_new(NULL, FALSE);

    alarm_init(&error_code);
    alarm_set_cb(callback, &error_code);

    create_test();

    g_main_loop_run(mainloop);

    alarm_fini(&error_code);
}
