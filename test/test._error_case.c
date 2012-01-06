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
#include<noti.h>

#include "alarm.h"

int callback(alarm_id_t alarm_id)
{
    int error;
    time_t current_time;
    time(&current_time);

    printf("Alarm[%d] has expired at %s\n", alarm_id, ctime(&current_time));

    return 0;

}

void test_alarm_update_fail_case_normal()
{
	time_t current_time;
	struct tm current_tm;
	alarm_info_t alarm_info;
	alarm_id_t alarm_id;
	int error_code;
	bool result = false;


	time(&current_time);

	localtime_r(&current_time, &current_tm);

	alarm_info.start.year = 0;
	alarm_info.start.month = 0;
	alarm_info.start.day = 0;

	alarm_info.end.year = 0;
	alarm_info.end.month = 0;
	alarm_info.end.day = 0;

	alarm_info.start.hour = 0;
	alarm_info.start.min = 0;

	alarm_info.mode.day_of_week = 0;
	alarm_info.mode.repeat = 1;

	alarm_info.activation = true;
	alarm_info.auto_powerup = false;

	result = alarm_create(&alarm_info, &alarm_id, &error_code);
	// result must be false
	if ( result == true )
	{
		result = alarm_update(alarm_id, &alarm_info, &error_code);

		if ( result == true )
		{
			printf("Test Success: %s\n", __FUNCTION__);
			alarm_delete(alarm_id, &error_code);
		}		
		else
		{
			printf("Test Failed: %s: error(%d)\n", __FUNCTION__, error_code);
		}

	}
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
	}
}

void test_alarm_update_fail_case_invalid_id()
{
	time_t current_time;
	struct tm current_tm;
	alarm_info_t alarm_info;
	alarm_id_t alarm_id;
	int error_code;
	bool result = false;


	time(&current_time);

	localtime_r(&current_time, &current_tm);

		result = alarm_update(1234, &alarm_info, &error_code);
	// result must be false
	
		if ( result == false )
		{
			printf("Test Success: %s\n", __FUNCTION__);
		}		
		else
		{
			printf("Test Failed: %s: error(%d)\n", __FUNCTION__, error_code);
		}


}



void test_alarm_delete_fail_case_normal()
{
	time_t current_time;
	struct tm current_tm;
	alarm_info_t alarm_info;
	alarm_id_t alarm_id;
	int error_code;
	bool result = false;


	time(&current_time);

	localtime_r(&current_time, &current_tm);

	alarm_info.start.year = 0;
	alarm_info.start.month = 0;
	alarm_info.start.day = 0;

	alarm_info.end.year = 0;
	alarm_info.end.month = 0;
	alarm_info.end.day = 0;

	alarm_info.start.hour = 0;
	alarm_info.start.min = 0;

	alarm_info.mode.day_of_week = 0;
	alarm_info.mode.repeat = 1;

	alarm_info.activation = true;
	alarm_info.auto_powerup = false;

	result = alarm_create(&alarm_info, &alarm_id, &error_code);
	// result must be false
	if ( result == true )
	{
		result = alarm_delete(alarm_id, &error_code);

		if ( result == true )
		{
			printf("Test Success: %s\n", __FUNCTION__);
		}		
		else
		{
			printf("Test Failed: %s: error(%d)\n", __FUNCTION__, error_code);
		}

	}
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
	}
}

void test_alarm_delete_fail_case_invalid_id()
{
	time_t current_time;
	struct tm current_tm;
	alarm_info_t alarm_info;
	alarm_id_t alarm_id;
	int error_code;
	bool result = false;


	result = alarm_delete(1234, &error_code);

	if ( result == false)
	{
		printf("Test Success: %s\n", __FUNCTION__);
	}		
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
	}

}




void test_alarm_create_fail_case_start_start_1()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;
	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 0;
    alarm_info.start.month = 0;
    alarm_info.start.day = 0;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = 24;
    alarm_info.start.min = 1;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = 1;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);
// result must be false
	if ( result == false )
		printf("Test Success: %s\n", __FUNCTION__);
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}

void test_alarm_create_fail_case_start_start_2()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;
	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 0;
    alarm_info.start.month = 0;
    alarm_info.start.day = 0;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = -1;
    alarm_info.start.min = 1;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = 1;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);
// result must be false
	if ( result == false )
		printf("Test Success: %s\n", __FUNCTION__);
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}

void test_alarm_create_fail_case_start_start_3()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;
	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 0;
    alarm_info.start.month = 0;
    alarm_info.start.day = 0;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = 0;
    alarm_info.start.min = 60;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = 1;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);
// result must be false
	if ( result == false )
		printf("Test Success: %s\n", __FUNCTION__);
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}

void test_alarm_create_fail_case_start_start_4()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;
	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 0;
    alarm_info.start.month = 0;
    alarm_info.start.day = 0;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = 0;
    alarm_info.start.min = -1;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = 1;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);
// result must be false
	if ( result == false )
		printf("Test Success: %s\n", __FUNCTION__);
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}



void test_alarm_create_fail_case_start_day_1()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;
	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 0;
    alarm_info.start.month = 2;
    alarm_info.start.day = 31;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = current_tm.tm_hour;
    alarm_info.start.min = current_tm.tm_min+1;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = 1;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);
// result must be false
	if ( result == false )
		printf("Test Success: %s\n", __FUNCTION__);
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}

void test_alarm_create_fail_case_start_day_2()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;
	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 0;
    alarm_info.start.month = 2;
    alarm_info.start.day = 30;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = current_tm.tm_hour;
    alarm_info.start.min = current_tm.tm_min+1;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = 1;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);
// result must be false
	if ( result == false )
		printf("Test Success: %s\n", __FUNCTION__);
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}

void test_alarm_create_fail_case_start_day_3()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;
	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 2000;
    alarm_info.start.month = 2;
    alarm_info.start.day = 29;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = current_tm.tm_hour;
    alarm_info.start.min = current_tm.tm_min+1;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = 1;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);
	// result must be false
	if ( result == false )
		printf("Test Failed: %s\n", __FUNCTION__);
	else
	{
		printf("Test Success: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}

void test_alarm_create_fail_case_start_day_4()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;
	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 1900;
    alarm_info.start.month = 2;
    alarm_info.start.day = 29;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = current_tm.tm_hour;
    alarm_info.start.min = current_tm.tm_min+1;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = 1;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);
	// result must be false
	if ( result == false )
	{
		printf("Test Success: %s\n", __FUNCTION__);
	}
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}

void test_alarm_create_fail_case_start_day_5()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;
	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 1900;
    alarm_info.start.month = 1;
    alarm_info.start.day = 32;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = current_tm.tm_hour;
    alarm_info.start.min = current_tm.tm_min+1;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = 1;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);
	// result must be false
	if ( result == false )
	{
		printf("Test Success: %s\n", __FUNCTION__);
	}
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}

void test_alarm_create_fail_case_start_day_6()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;
	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 1900;
    alarm_info.start.month = 4;
    alarm_info.start.day = 31;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = current_tm.tm_hour;
    alarm_info.start.min = current_tm.tm_min+1;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = 1;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);
	// result must be false
	if ( result == false )
	{
		printf("Test Success: %s\n", __FUNCTION__);
	}
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}



void test_alarm_create_fail_case_start_month_1()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;
	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 0;
    alarm_info.start.month = -1;
    alarm_info.start.day = 0;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = current_tm.tm_hour;
    alarm_info.start.min = current_tm.tm_min+1;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = 1;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);
// result must be false
	if ( result == false )
		printf("Test Success: %s\n", __FUNCTION__);
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}

void test_alarm_create_fail_case_start_month_2()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;
	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 0;
    alarm_info.start.month = 13;
    alarm_info.start.day = 0;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = current_tm.tm_hour;
    alarm_info.start.min = current_tm.tm_min+1;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = 1;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);
// result must be false
	if ( result == false )
		printf("Test Success: %s\n", __FUNCTION__);
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}

// Invalid day of week 
void test_alarm_create_fail_case_day_of_week()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;
	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 0;
    alarm_info.start.month = 0;
    alarm_info.start.day = 0;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = current_tm.tm_hour;
    alarm_info.start.min = current_tm.tm_min+1;

    alarm_info.mode.day_of_week = ALARM_WDAY_SATURDAY << 1;
    alarm_info.mode.repeat = ALARM_REPEAT_MODE_ANNUALLY;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);

	// result must be false
	if ( result == false )
		printf("Test Success: %s\n", __FUNCTION__);
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}


/* Invalid repeat mode */
void test_alarm_create_fail_case_repeat_mode()
{
    time_t current_time;
    struct tm current_tm;
    alarm_info_t alarm_info;
    alarm_id_t alarm_id;
    int error_code;

	bool result = false;


    time(&current_time);

    localtime_r(&current_time, &current_tm);

    alarm_info.start.year = 0;
    alarm_info.start.month = 0;
    alarm_info.start.day = 0;

    alarm_info.end.year = 0;
    alarm_info.end.month = 0;
    alarm_info.end.day = 0;

    alarm_info.start.hour = current_tm.tm_hour;
    alarm_info.start.min = current_tm.tm_min+1;

    alarm_info.mode.day_of_week = 0;
    alarm_info.mode.repeat = ALARM_REPEAT_MODE_ANNUALLY + 1;

    alarm_info.activation = true;
    alarm_info.auto_powerup = false;

    result = alarm_create(&alarm_info, &alarm_id, &error_code);

	// result must be false
	if ( result == false )
		printf("Test Success: %s\n", __FUNCTION__);
	else
	{
		printf("Test Failed: %s\n", __FUNCTION__);
		alarm_delete(alarm_id, &error_code);
	}
}


int main(int argc, char** argv)
{
    int error_code;
    GMainLoop *mainloop;
    mainloop = g_main_loop_new(NULL, FALSE);
	noti_init(&error_code);

    alarm_init(&error_code);
    alarm_set_cb(callback, &error_code);

	test_alarm_create_fail_case_repeat_mode();
	test_alarm_create_fail_case_day_of_week();
	test_alarm_create_fail_case_start_month_1();
	test_alarm_create_fail_case_start_month_2();
	test_alarm_create_fail_case_start_day_1();
	test_alarm_create_fail_case_start_day_2();
	test_alarm_create_fail_case_start_day_3();
	test_alarm_create_fail_case_start_day_4();
	test_alarm_create_fail_case_start_day_5();
	test_alarm_create_fail_case_start_day_6();
	test_alarm_create_fail_case_start_start_1();
	test_alarm_create_fail_case_start_start_2();
	test_alarm_create_fail_case_start_start_3();
	test_alarm_create_fail_case_start_start_4();
	test_alarm_delete_fail_case_normal();
	test_alarm_delete_fail_case_invalid_id();
	test_alarm_update_fail_case_normal();
	test_alarm_update_fail_case_invalid_id();

//    g_main_loop_run(mainloop);

	noti_finish(&error_code);
    alarm_fini(&error_code);
}
