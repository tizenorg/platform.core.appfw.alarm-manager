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
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <tet_api.h>
#include <sys/types.h>
#include <unistd.h>

#include "alarm.h"

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_ApplicationFW_alarmmgr_add_alarm_with_localtime_func_01(void);
static void utc_ApplicationFW_alarmmgr_add_alarm_with_localtime_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_ApplicationFW_alarmmgr_add_alarm_with_localtime_func_01, POSITIVE_TC_IDX },
	{ utc_ApplicationFW_alarmmgr_add_alarm_with_localtime_func_02, NEGATIVE_TC_IDX },
	{ NULL , 0 }
};

static int pid;

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of alarmmgr_add_alarm_with_localtime()
 */
static void utc_ApplicationFW_alarmmgr_add_alarm_with_localtime_func_01(void)
{
	int ret_val = ALARMMGR_RESULT_SUCCESS;
	alarm_entry_t* alarm; 
	const char* destination = NULL; 
	alarm_id_t alarm_id;

 	time_t current_time;
	struct tm current_tm;
	alarm_date_t test_time;

	const char* pkg_name = "org.tizen.test";

	g_type_init();

	ret_val =alarmmgr_init(pkg_name) ;
	if(ret_val != ALARMMGR_RESULT_SUCCESS)
	{
		 tet_infoline("\nAlarm Manager : call to alarmmgr_init () failed \n");
		 tet_result(TET_FAIL);
		 return;
	}

	time(&current_time);

	printf("current time: %s\n", ctime(&current_time));
	localtime_r(&current_time, &current_tm);

	alarm = alarmmgr_create_alarm();

	test_time.year = 0;                 
	test_time.month = 0;                
	test_time.day = 0;                  
                                     
	test_time.hour = current_tm.tm_hour;
	test_time.min = current_tm.tm_min+1;
	test_time.sec = 0;

        
	alarmmgr_set_time(alarm,test_time);
	alarmmgr_set_repeat_mode(alarm,ALARM_REPEAT_MODE_WEEKLY, ALARM_WDAY_MONDAY);
	alarmmgr_set_type(alarm,ALARM_TYPE_VOLATILE);

		
       ret_val=alarmmgr_add_alarm_with_localtime(alarm,destination,&alarm_id);

	if(ret_val == ALARMMGR_RESULT_SUCCESS)
	{
		 tet_infoline("\nAlarm Manager : call to alarmmgr_add_alarm_with_localtime() is successful \n");
		 tet_result(TET_PASS);
	}
	else
	{
		 tet_infoline("\nAlarm Manager : call to alarmmgr_add_alarm_with_localtime() failed \n");
		 tet_result(TET_FAIL);
	}
	alarmmgr_free_alarm( alarm) ;
}

/**
 * @brief Negative test case of ug_init alarmmgr_add_alarm_with_localtime()
 */
static void utc_ApplicationFW_alarmmgr_add_alarm_with_localtime_func_02(void)
{
	int ret_val = ALARMMGR_RESULT_SUCCESS;
	alarm_entry_t* alarm = NULL; 
	const char* destination = NULL; 
	alarm_id_t* alarm_id = NULL;

	const char* pkg_name = "org.tizen.test";

	g_type_init();

	ret_val =alarmmgr_init(pkg_name) ;
	if(ret_val != ALARMMGR_RESULT_SUCCESS)
	{
		 tet_infoline("\nAlarm Manager : call to alarmmgr_init () failed \n");
		 tet_result(TET_FAIL);
		 return;
	}
	
	ret_val = alarmmgr_add_alarm_with_localtime(alarm,  destination, alarm_id);
	if(ret_val == ERR_ALARM_INVALID_PARAM)
	{
		 tet_infoline("\nAlarm Manager : call to alarmmgr_add_alarm_with_localtime()  with invalid parameter  is successful \n");
		 tet_result(TET_PASS);
	}
	else
	{
		 tet_infoline("\nAlarm Manager : call to alarmmgr_add_alarm_with_localtime() failed \n");
		 tet_result(TET_FAIL);
	}
}
