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



/**
 @ingroup SLP_PG
 @defgroup SLP_PG_ALARM_MANAGER Alarm
 @{

<h1 class="pg">Introduction</h1>

<h2 class="pg">Purpose of this document</h2>
 The purpose of this document is to describe how applications can use Alarm Manager APIs for handling alarms. This document gives programming guidelines to application engineers.

<h2 class="pg">Scope</h2>
The scope of this document is limited to Alarm Manager API usage.


<h1 class="pg">Architecture</h1>

<h2 class="pg">Architecture overview</h2>
The Alarm Manager provides functionality for applications to create, delete and configure alarms. Applications register alarm information and the Alarm Manager manages the alarm information, generates timer and schedule alarms. When the timer expires, the Alarm Manager sends an event to the applications. 

@image html SLP_Alarm_PG_overview.png

<h2 class="pg">SLP Features</h2>
- The Alarm Manager exposes a high level interface that is used by applications.
- The Alarm Manager and applications use D-Bus as its underlying IPC mechanism. 
- The Alarm Manager uses DB to store the alarm information internally.
- The Alarm Manager uses system timers for setting expiration time.


<h1 class="pg">Alarm manager properties</h1>

<h2 class="pg">Time</h2>
There are two types of alarm. The alarm type is distinguished by the following APIs
- alarmmgr_add_alarm_with_localtime 
	- The alarm expires at a specified time. The localtime is set by the application in the alarm_entry_t parameter through alarmmgr_set_time(). 
- alarmmgr_add_alarm 
	- The alarm expires after a specified interval (in seconds).  

<h2 class="pg">Repeat mode</h2>
- Repeat mode 
	- ONCE: In this mode, the alarm expires only once. After it expires, the alarm is removed from alarm server. 
	- REPEAT: In this mode, the alarm is expires repeatly. 
	- WEEKLY: In this mode, the alarm expires weekly on the days specified by the day_of_week parameter. 
	- MONTHLY: In this mode, the alarm expires once a month from the start date.
	- ANNUALY: In this mode, the alarm expires once a year from the start date.

- Repeat value 
	- ALARM_REPEAT_MODE_REPEAT : In this mode, the alarm is repeated at a interval specified in the interval parameter
	- ALARM_REPEAT_MODE_WEEKLY : In this mode, the alarm is repeated at a weekly interval


@code
typedef enum
{
	ALARM_WDAY_SUNDAY=0x01,
	ALARM_WDAY_MONDAY=0x02,
	ALARM_WDAY_TUESDAY=0x04,
	ALARM_WDAY_WEDNESDAY=0x08,
	ALARM_WDAY_THURSDAY=0x10,
	ALARM_WDAY_FRIDAY=0x20, 
	ALARM_WDAY_SATURDAY=0x40,
}alarm_day_of_week_t;
@endcode


<h2 class="pg">Alarm type</h2>
- ALARM_TYPE_DEFAULT : The alarm information is saved using DB. After a device reboot, the alarm still exists. 
- ALARM_TYPE_VOLATILE : The alarm information is not saved using DB. After a device reboot, the alarm no longer exists. 



<h1 class="pg">Alarm manager features with sample code</h3>

<h2 class="pg">Add an Alarm to the server</h2>
An application can add an alarm to the alarm server by using below two ways. 

1.  by alarmmgr_add_alarm
@code
int alarmmgr_add_alarm(int alarm_type, 
time_t trigger_at_time, 
time_t interval, 
const char* destination, 
alarm_id_t* alarm_id);
@endcode
As shown in the sample code below, alrmmgr_init is used to initalize the alarm library. alarmmgr_set_cb is called to set the callback for handling the alarm events. create_test function is called to create an alarm. In the create_test function the alarmmgr_add_alarm() function is called.

	
	- Note: 
		- Trigger_at_time is the interval after which the alarm is triggered. 
		- If the package that receives the events is different from the package to add an alarm, the application has to write the destination package name. 
		- If the interval is zero, the repeat_mode is ALARM_REPEAT_MODE_ONCE. 
		- If the interval is >0, the repeat_mode is ALARM_REPEAT_MODE_REPEAT. 

@code
#include<stdio.h>
#include<stdlib.h>
#include<glib.h>

#include <alarm.h>

int callback(alarm_id_t alarm_id,void* user_param)
{
        int error;
        time_t current_time;
        time(&current_time);

        printf("Alarm[%d] has expired at %s\n", alarm_id, ctime(&current_time));

        return 0;

}

void create_test()
{
		time_t current_time;
		alarm_id_t alarm_id;
 		int result;

		time(&current_time);

		printf("current time: %s\n", ctime(&current_time));
       

		alarmmgr_add_alarm(ALARM_TYPE_DEFAULT, 60, 10, NULL,& alarm_id);
		if(result != ALARMMGR_RESULT_SUCCESS)
			printf("fail to alarmmgr_create : error_code : %d\n",result);
        
}

int main(int argc, char** argv)
{       
        int error_code;
        GMainLoop *mainloop;
		int result;
        
	g_type_init();
        
        mainloop = g_main_loop_new(NULL, FALSE);
                
        result = alarmmgr_init("org.tizen.test");

		if(result != ALARMMGR_RESULT_SUCCESS)
		{
			printf("fail to alarmmgr_init : error_code : %d\n",result);
		}
		else{
			
     	   result = alarmmgr_set_cb(callback,NULL);

		  if(result != ALARMMGR_RESULT_SUCCESS)
			{
				printf("fail to alarmmgr_set_cb : error_code : %d\n",result);
			}
			else{
				create_test();
			}
		}

        g_main_loop_run(mainloop);
}
@endcode

2.  by alarmmgr_add_alarm_with_localtime
@code
alarm_entry_t* alarmmgr_create_alarm(void);

int alarmmgr_set_time(alarm_entry_t* alarm, alarm_date_t time);

int alarmmgr_set_repeat_mode(alarm_entry_t* alarm, 
alarm_repeat_mode_t repeat_mode, 
int repeat_value);

int alarmmgr_set_type(alarm_entry_t* alarm, int alarm_type);

int alarmmgr_add_alarm_with_localtime(alarm_entry_t* alarm, 
const char* destination, 
alarm_id_t* alarm_id);
 int alarmmgr_remove_alarm(alarm_id_t alarm_id);
@endcode
In this sample code, the application creates an alarm object using the alarmmgr_create_alarm function. Then the alarmmgr_set_*() functions are used to set the attributes of the alarm entry. Finally, the application adds an alarm to the alarm server using alarmmgr_add_alarm_with_localtime. If the application doesn't need the object (alarm entry), the application has to free the memory using the alarmmgr_remove_alarm() function.

	- Note: 
		- Time is localtime. For example, if the alarm is set to expire at 2010/10/3 15:00, even if the system time is changed or the timezone is changed, the alarm will expire at 2010/10/3 15:00. 
		- If the package that receives the events is different from the package to add an alarm, the application has to write the destination package name. 
		- ALARM_REPEAT_MODE_REPEAT : In this mode, the alarm is repeated at a interval specified in the interval parameter
		- ALARM_REPEAT_MODE_WEEKLY : In this mode, the alarm is repeated at a weekly interval

@code
#include<stdio.h>
#include<stdlib.h>
#include<glib.h>

#include <alarm.h>

int callback(alarm_id_t alarm_id,void* user_param)
{
        int error;
        time_t current_time;
        time(&current_time);

        printf("Alarm[%d] has expired at %s\n", alarm_id, ctime(&current_time));

        return 0;

}

void create_test()
{
        time_t current_time;
        struct tm current_tm;
        alarm_entry_t* alarm_info;
        alarm_id_t alarm_id;
        int result;
        alarm_date_t test_time;

        time(&current_time);

        printf("current time: %s\n", ctime(&current_time));
        localtime_r(&current_time, &current_tm);
        
        alarm_info = alarmmgr_create_alarm();
        
        test_time.year = 0;                 
				test_time.month = 0;                
				test_time.day = 0;                  
                                                 
				test_time.hour = current_tm.tm_hour;
				test_time.min = current_tm.tm_min+1;
				test_time.sec = 0;

        
        alarmmgr_set_time(alarm_info,test_time);
        alarmmgr_set_repeat_mode(alarm_info,ALARM_REPEAT_MODE_WEEKLY,ALARM_WDAY_MONDAY| \
        																		ALARM_WDAY_TUESDAY|ALARM_WDAY_WEDNESDAY| \
                                            ALARM_WDAY_THURSDAY|ALARM_WDAY_FRIDAY );

        alarmmgr_set_type(alarm_info,ALARM_TYPE_VOLATILE);
        alarmmgr_add_alarm_with_localtime(alarm_info,NULL,&alarm_id);
	if(result != ALARMMGR_RESULT_SUCCESS)
		printf("fail to alarmmgr_create : error_code : %d\n",result);
        
}

int main(int argc, char** argv)
{       
        int error_code;
        GMainLoop *mainloop;
		int result;
        
	g_type_init();
        
        mainloop = g_main_loop_new(NULL, FALSE);
                
        result = alarmmgr_init("org.tizen.test");

		if(result != ALARMMGR_RESULT_SUCCESS)
		{
			printf("fail to alarmmgr_init : error_code : %d\n",result);
		}
		else{
			
     	   result = alarmmgr_set_cb(callback,NULL);

		   if(result != ALARMMGR_RESULT_SUCCESS)
			{
				printf("fail to alarmmgr_set_cb : error_code : %d\n",result);
			}
			else{
				create_test();
			}
		}

        g_main_loop_run(mainloop);
}
@endcode

<h2 class="pg">Get alarm info</h2>
The application can retrieve alarm information from the alarm server using the alarmmgr_get_info & alarmmgr_enum_alarm_ids APIs


@code
#include<stdio.h>
#include<stdlib.h>
#include<glib.h>
#include "alarm.h"

int test_callback (alarm_id_t id, void* user_param)
{
 alarm_entry_t* alarm;
alarm = alarmmgr_create_alarm();
	int* n = (int*)user_param;
	printf("[%d]alarm id : %d\n",*n,id);
	(*n)++;
       alarmmgr_get_info(id, alarm);   //the application use this object(alarm)
}
sample_get_info()
{
	int n=1;
	alarmmgr_enum_alarm_ids(test_callback,(void*)&n);
}
@endcode


 @}
**/
