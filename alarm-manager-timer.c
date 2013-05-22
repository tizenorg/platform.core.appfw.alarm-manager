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




#define _BSD_SOURCE		/*gmtime_r requires */

#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<signal.h>
#include<string.h>
#include<sys/types.h>
#include<errno.h>

#include<dbus/dbus.h>
#include<glib.h>

#include"alarm.h"
#include"alarm-internal.h"

extern bool g_dummy_timer_is_set;

bool _alarm_destory_timer(timer_t timer)
{
	if (timer_delete(timer) == 0)
		return true;
	else
		return false;
}

timer_t _alarm_create_timer()
{
	timer_t timer = (timer_t) -1;
	struct sigevent timer_event;

	ALARM_MGR_LOG_PRINT("[alarm-server][timer]\n");

	timer_event.sigev_notify = SIGEV_SIGNAL;
	timer_event.sigev_signo = SIG_TIMER;
	timer_event.sigev_value.sival_ptr = (void *)timer;

	if (timer_create(CLOCK_REALTIME, &timer_event, &timer) < 0) {
		perror("create timer has failed\n");
		exit(1);
	}

	return timer;

}

bool _alarm_disable_timer(__alarm_server_context_t alarm_context)
{
	struct itimerspec time_spec;

	time_spec.it_value.tv_sec = 0;
	time_spec.it_value.tv_nsec = 0;
	time_spec.it_interval.tv_sec = time_spec.it_interval.tv_nsec = 0;

	if (timer_settime(alarm_context.timer, 0, &time_spec, NULL) < 0) {
		perror("disable timer has failed\n");
		return false;
	}

	return true;
}

bool _alarm_set_timer(__alarm_server_context_t *alarm_context, timer_t timer,
		      time_t due_time, alarm_id_t id)
{
	struct itimerspec time_spec;
	time_t current_time;
	double interval;
	char due_time_r[100] = { 0 };
	struct tm ts_ret;
	
	extern int errno;

	time(&current_time);

	interval = difftime(due_time, current_time);
	ALARM_MGR_LOG_PRINT("[alarm-server][timer]: remain time from "
			    "current is %f , due_time is %d\n", interval,
			    due_time);

	/*set timer as absolute time */
	/*we create dummy timer when the interval is longer than one day. */
	/*the timer will be expired in half day. */

	localtime_r(&due_time, &ts_ret);

	if (interval > 60 * 60 * 24) {
		interval = 60 * 60 * 12;
		g_dummy_timer_is_set = true;
		strftime(due_time_r, 30, "%c", &ts_ret);
		ALARM_MGR_LOG_PRINT("create dummy alarm timer(%d), "
				    "due_time(%s) \n", timer, due_time_r);
	} else
		g_dummy_timer_is_set = false;

	time_spec.it_value.tv_sec = due_time;
	time_spec.it_value.tv_nsec = 0;
	time_spec.it_interval.tv_sec = time_spec.it_interval.tv_nsec = 0;

	if (interval > 0
	    && timer_settime(timer, TIMER_ABSTIME, &time_spec, NULL) != 0) {
		ALARM_MGR_EXCEPTION_PRINT("set timer has failed : timer(%d), "
					  "due_time(%u) , errno(%d)\n", timer,
					  due_time, errno);
		return false;
	}
	/* we set c_due_time to due_time due to allow newly created alarm can 
	   be schedlued when its interval is less than the current alarm */
	alarm_context->c_due_time = due_time;
	return true;
}

int _set_sys_time(time_t _time)
{
	struct tm result;
	/* Ignore return value of gmtime_r(). */
	(void) gmtime_r(&_time, &result);

	stime(&_time);

	return 1;
}

int _set_time(time_t _time)
{
	ALARM_MGR_LOG_PRINT("ENTER FUNC _set_time(%d)", _time);
	_set_rtc_time(_time);
	_set_sys_time(_time);

	/* inoti (broadcasting without data 
	 * send system noti that time has changed */

	return 0;
}
