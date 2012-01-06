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

static void utc_ApplicationFW_alarmmgr_set_cb_func_01(void);
static void utc_ApplicationFW_alarmmgr_set_cb_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_ApplicationFW_alarmmgr_set_cb_func_01, POSITIVE_TC_IDX },
	{ utc_ApplicationFW_alarmmgr_set_cb_func_02, NEGATIVE_TC_IDX },
	{ NULL , 0 }
};

static int pid;

static void startup(void)
{
}

static void cleanup(void)
{
}

/* Call back function */

int callback(alarm_id_t alarm_id,void* user_param)
{
        time_t current_time;
        time(&current_time);

        printf("Alarm[%d] has expired at %s\n", alarm_id, ctime(&current_time));

        return 0;

}

/**
 * @ alarmmgr_set_cb :Successfull  : return 0 : ALARMMGR_RESULT_SUCCESS
 */
static void utc_ApplicationFW_alarmmgr_set_cb_func_01(void)
{
	int ret_val = ALARMMGR_RESULT_SUCCESS;
	void *user_param = NULL;

	ret_val = alarmmgr_set_cb( callback, user_param);
	if(ret_val == ALARMMGR_RESULT_SUCCESS)
	{
		 tet_infoline("\nAlarm Manager : call to alarmmgr_set_cb() is successful \n");
		 tet_result(TET_PASS);
	}
	else
	{
		 tet_infoline("\nAlarm Manager : call to alarmmgr_set_cb () failed \n");
		 tet_result(TET_FAIL);
	}
}

/**
 * @brief Negative test case of ug_init alarmmgr_set_cb()
 */
static void utc_ApplicationFW_alarmmgr_set_cb_func_02(void)
{
	int ret_val = ALARMMGR_RESULT_SUCCESS;
	void *user_param = NULL;
	
	ret_val =alarmmgr_set_cb( NULL, user_param) ;
	if(ret_val == ERR_ALARM_INVALID_PARAM)
	{
		 tet_infoline("\nAlarm Manager : call to alarmmgr_set_cb() with invalid parameter is successful \n");
		 tet_result(TET_PASS);
	}
	else
	{
		 tet_infoline("\nAlarm Manager : call to alarmmgr_set_cb() failed \n");
		 tet_result(TET_FAIL);
	}
}
