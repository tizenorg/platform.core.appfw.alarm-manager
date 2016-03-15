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
#include<time.h>
#include<signal.h>
#include<string.h>
#include<sys/types.h>

#include<glib.h>

#include"alarm.h"
#include"alarm-internal.h"
#define WAKEUP_ALARM_APP_ID "org.tizen.alarm.ALARM"	/*alarm ui
							  application's alarm's dbus_service name instead of 21 value */
#define DST_TIME_DIFF 1

extern __alarm_server_context_t alarm_context;
extern GSList *g_scheduled_alarm_list;
extern bool is_time_changed;

static time_t __alarm_next_duetime_once(__alarm_info_t *__alarm_info);
static time_t __alarm_next_duetime_repeat(__alarm_info_t *__alarm_info);
static time_t __alarm_next_duetime_annually(__alarm_info_t *__alarm_info);
static time_t __alarm_next_duetime_monthly(__alarm_info_t *__alarm_info);
static time_t __alarm_next_duetime_weekly(__alarm_info_t *__alarm_info);
static bool __find_next_alarm_to_be_scheduled(time_t *min_due_time);
bool _alarm_schedule(void);

bool _clear_scheduled_alarm_list()
{
	g_slist_free_full(g_scheduled_alarm_list, g_free);
	g_scheduled_alarm_list = NULL;

	return true;
}

bool _init_scheduled_alarm_list()
{
	_clear_scheduled_alarm_list();

	return true;
}

bool _add_to_scheduled_alarm_list(__alarm_info_t *__alarm_info)
{
	/*
	 * 20080328. Sewook Park(sewook7.park@samsung.com)
	 * When multiple alarms are expired at same time, dbus rpc call for alarm
	 * ui should be invoked first.(Ui conflicting manager cannot manage the
	 * different kinds of alarm popups(wake up alarm/org alarm) correctly,
	 * when they are displayed at same time)So when arranging the schedule
	 * alarm list, wake up alarm element is located ahead.
	 */

	bool prior = false;
	gint count = 0;
	GSList *iter = NULL;
	__scheduled_alarm_t *alarm = NULL;
	__scheduled_alarm_t *entry = NULL;

	alarm = g_malloc(sizeof(__scheduled_alarm_t));
	if (alarm == NULL)
		return false;

	alarm->used = true;
	alarm->alarm_id = __alarm_info->alarm_id;
	alarm->uid = __alarm_info->uid;
	alarm->pid = __alarm_info->pid;
	alarm->__alarm_info = __alarm_info;

	SECURE_LOGD("%s :alarm->uid =%d, alarm->pid =%d, app_service_name=%s(%u)\n",
			__FUNCTION__, alarm->uid, alarm->pid,
			g_quark_to_string(alarm->__alarm_info->quark_app_service_name),
			alarm->__alarm_info->quark_app_service_name);

	if (alarm->__alarm_info->quark_app_service_name != g_quark_from_string(WAKEUP_ALARM_APP_ID)) {
		g_scheduled_alarm_list = g_slist_append(g_scheduled_alarm_list, alarm);
	} else {
		for (iter = g_scheduled_alarm_list; iter != NULL; iter = g_slist_next(iter)) {
			count++;
			entry = iter->data;
			if (entry->__alarm_info->quark_app_service_name != g_quark_from_string(WAKEUP_ALARM_APP_ID)) {
				prior = true;
				break;
			}
		}

		if (!prior) {
			g_scheduled_alarm_list = g_slist_append(g_scheduled_alarm_list, alarm);
			ALARM_MGR_LOG_PRINT("appended : prior is %d\tcount is %d\n", prior, count);
		} else {
			g_scheduled_alarm_list = g_slist_insert(g_scheduled_alarm_list, alarm, count - 1);
			ALARM_MGR_LOG_PRINT("appended : prior is %d\tcount is %d\n", prior, count);
		}
	}

	return true;
}

bool _remove_from_scheduled_alarm_list(uid_t uid, alarm_id_t alarm_id)
{
	bool result = false;
	GSList *iter = NULL;
	__scheduled_alarm_t *alarm = NULL;

	for (iter = g_scheduled_alarm_list; iter != NULL; iter = g_slist_next(iter)) {
		alarm = iter->data;
		if (alarm->uid == uid && alarm->alarm_id == alarm_id) {
			g_scheduled_alarm_list = g_slist_remove(g_scheduled_alarm_list, iter->data);
			g_free(alarm);
			result = true;
			break;
		}
	}

	if (g_slist_length(g_scheduled_alarm_list) == 0)
		alarm_context.c_due_time = -1;

	return result;
}

static time_t __alarm_next_duetime_once(__alarm_info_t *__alarm_info)
{
	time_t due_time = 0;
	time_t due_time_tmp = 0;
	time_t current_time = 0;
	struct tm duetime_tm;
	struct tm tmp_tm;
	int current_dst = 0;

	alarm_info_t *alarm_info = &__alarm_info->alarm_info;
	alarm_date_t *start = &alarm_info->start;

	tzset();
	time(&current_time);
	localtime_r(&current_time, &duetime_tm);
	duetime_tm.tm_hour = start->hour;
	duetime_tm.tm_min = start->min;
	duetime_tm.tm_sec = start->sec;

	current_dst = duetime_tm.tm_isdst;
	duetime_tm.tm_isdst = -1;

	if (start->year == 0 && start->month == 0 && start->day == 0) {
		/*any date */
		due_time = mktime(&duetime_tm);
		if (!(due_time > current_time))
			due_time = due_time + 60 * 60 * 24;
	} else	/*specific date*/ {
		duetime_tm.tm_year = start->year - 1900;
		duetime_tm.tm_mon = start->month - 1;
		duetime_tm.tm_mday = start->day;
		due_time = mktime(&duetime_tm);
	}

	if (due_time <= current_time) {
		ALARM_MGR_EXCEPTION_PRINT("duetime is less than or equal to current time. current_dst = %d", current_dst);
		duetime_tm.tm_isdst = 0; /* DST off */

		due_time_tmp = mktime(&duetime_tm);
		localtime_r(&due_time_tmp, &tmp_tm);

		ALARM_MGR_LOG_PRINT("%d:%d:%d. duetime = %d", tmp_tm.tm_hour, tmp_tm.tm_min, tmp_tm.tm_sec, due_time);
		if (tmp_tm.tm_hour == start->hour && tmp_tm.tm_min == start->min && tmp_tm.tm_sec == start->sec) {
			due_time = due_time_tmp;
			ALARM_MGR_EXCEPTION_PRINT("due_time = %d", due_time);
		}
	} else {
		localtime_r(&due_time, &tmp_tm);
		ALARM_MGR_LOG_PRINT("%d:%d:%d. current_dst = %d, duetime_dst = %d", tmp_tm.tm_hour, tmp_tm.tm_min, tmp_tm.tm_sec, current_dst, tmp_tm.tm_isdst);

		if (current_dst == 1 && tmp_tm.tm_isdst == 1 && tmp_tm.tm_hour == start->hour + 1) {
			/* When the calculated duetime is forwarded 1hour due to DST, Adds 23hours. */
			due_time += 60 * 60 * 23;
			localtime_r(&due_time, &duetime_tm);
			ALARM_MGR_EXCEPTION_PRINT("due_time = %d", due_time);
		}
	}

	ALARM_MGR_EXCEPTION_PRINT("Final due_time = %d, %s", due_time, ctime(&due_time));
	return due_time;
}

static time_t __alarm_next_duetime_repeat(__alarm_info_t *__alarm_info)
{
	time_t due_time = 0;
	time_t current_time = 0;
	struct tm duetime_tm;

	alarm_info_t *alarm_info = &__alarm_info->alarm_info;
	alarm_date_t *start = &alarm_info->start;

	time(&current_time);
	/*localtime_r(&current_time, &duetime_tm); */

	duetime_tm.tm_hour = start->hour;
	duetime_tm.tm_min = start->min;
	duetime_tm.tm_sec = start->sec;

	duetime_tm.tm_year = start->year - 1900;
	duetime_tm.tm_mon = start->month - 1;
	duetime_tm.tm_mday = start->day;
	duetime_tm.tm_isdst = -1;

	if (alarm_info->alarm_type & ALARM_TYPE_PERIOD &&
			alarm_info->mode.u_interval.interval > 0) {
		/* For minimize 'while loop'
		 * Duetime should be "periodic_standard_time + (interval * x) >= current" */
		time_t periodic_standard_time = _get_periodic_alarm_standard_time();
		time_t temp;
		temp = (current_time - periodic_standard_time) / alarm_info->mode.u_interval.interval;
		due_time = periodic_standard_time + (temp * alarm_info->mode.u_interval.interval);
	} else {
		due_time = mktime(&duetime_tm);
	}

	while (__alarm_info->start > due_time || current_time > due_time || ((!is_time_changed) && (current_time == due_time)))
		due_time += alarm_info->mode.u_interval.interval;

	if (due_time - current_time < 10)
		due_time += alarm_info->mode.u_interval.interval;

	localtime_r(&due_time, &duetime_tm);

	start->year = duetime_tm.tm_year + 1900;
	start->month = duetime_tm.tm_mon + 1;
	start->day = duetime_tm.tm_mday;
	start->hour = duetime_tm.tm_hour;
	start->min = duetime_tm.tm_min;
	start->sec = duetime_tm.tm_sec;

	return due_time;

}

static time_t __alarm_next_duetime_annually(__alarm_info_t *__alarm_info)
{
	time_t due_time = 0;
	time_t current_time = 0;
	struct tm duetime_tm;

	alarm_info_t *alarm_info = &__alarm_info->alarm_info;
	alarm_date_t *start = &alarm_info->start;

	time(&current_time);
	localtime_r(&current_time, &duetime_tm);
	duetime_tm.tm_hour = start->hour;
	duetime_tm.tm_min = start->min;
	duetime_tm.tm_sec = start->sec;

	if (start->year != 0)
		duetime_tm.tm_year = start->year - 1900;

	duetime_tm.tm_mon = start->month - 1;
	duetime_tm.tm_mday = start->day;

	due_time = mktime(&duetime_tm);

	while (__alarm_info->start > due_time || current_time > due_time || ((!is_time_changed) && (current_time == due_time))) {
		duetime_tm.tm_year += 1;
		due_time = mktime(&duetime_tm);
	}

	return due_time;
}

static time_t __alarm_next_duetime_monthly(__alarm_info_t *__alarm_info)
{
	time_t due_time = 0;
	time_t current_time = 0;
	struct tm duetime_tm;

	alarm_info_t *alarm_info = &__alarm_info->alarm_info;
	alarm_date_t *start = &alarm_info->start;

	time(&current_time);
	localtime_r(&current_time, &duetime_tm);
	duetime_tm.tm_hour = start->hour;
	duetime_tm.tm_min = start->min;
	duetime_tm.tm_sec = start->sec;

	if (start->year != 0)
		duetime_tm.tm_year = start->year - 1900;

	if (start->month != 0)
		duetime_tm.tm_mon = start->month - 1;

	duetime_tm.tm_mday = start->day;

	due_time = mktime(&duetime_tm);

	while (__alarm_info->start > due_time || current_time > due_time || ((!is_time_changed) && (current_time == due_time))) {
		duetime_tm.tm_mon += 1;
		if (duetime_tm.tm_mon == 12) {
			duetime_tm.tm_mon = 0;
			duetime_tm.tm_year += 1;
		}
		due_time = mktime(&duetime_tm);
	}

	return due_time;
}

static time_t __alarm_next_duetime_weekly(__alarm_info_t *__alarm_info)
{
	time_t due_time = 0;
	time_t current_time = 0;
	struct tm duetime_tm;
	struct tm tmp_tm;
	int wday;
	int current_dst = 0;
	struct tm before_tm;
	struct tm after_tm;

	alarm_info_t *alarm_info = &__alarm_info->alarm_info;
	alarm_date_t *start = &alarm_info->start;
	alarm_mode_t *mode = &alarm_info->mode;

	tzset();
	time(&current_time);
	localtime_r(&current_time, &duetime_tm);
	wday = duetime_tm.tm_wday;
	duetime_tm.tm_hour = start->hour;
	duetime_tm.tm_min = start->min;
	duetime_tm.tm_sec = start->sec;
	current_dst = duetime_tm.tm_isdst;

	duetime_tm.tm_isdst = -1;

	if (__alarm_info->start != 0) {
		if (__alarm_info->start >= current_time)	{
			duetime_tm.tm_year = start->year - 1900;
			duetime_tm.tm_mon = start->month - 1;
			duetime_tm.tm_mday = start->day;
		}
	}

	due_time = mktime(&duetime_tm);
	localtime_r(&due_time, &tmp_tm);
	ALARM_MGR_LOG_PRINT("%d:%d:%d. duetime = %d, isdst = %d", tmp_tm.tm_hour, tmp_tm.tm_min, tmp_tm.tm_sec, due_time, tmp_tm.tm_isdst);

	if (due_time <= current_time) {
		ALARM_MGR_EXCEPTION_PRINT("duetime is less than or equal to current time. current_dst = %d", current_dst);
		duetime_tm.tm_isdst = 0;

		due_time = mktime(&duetime_tm);
		localtime_r(&due_time, &tmp_tm);

		SECURE_LOGD("%d:%d:%d. duetime = %d", tmp_tm.tm_hour, tmp_tm.tm_min, tmp_tm.tm_sec, due_time);
		if (tmp_tm.tm_hour != start->hour || tmp_tm.tm_min != start->min || tmp_tm.tm_sec != start->sec) {
			duetime_tm.tm_hour = start->hour;
			duetime_tm.tm_min = start->min;
			duetime_tm.tm_sec = start->sec;
			duetime_tm.tm_isdst = -1;
			due_time = mktime(&duetime_tm);
			ALARM_MGR_LOG_PRINT("due_time = %d", due_time);
		}
	} else {
		if (current_dst == 1 && tmp_tm.tm_isdst == 1 && tmp_tm.tm_hour == start->hour + 1) {
			/* When the calculated duetime is forwarded 1hour due to DST, Adds 23hours. */
			due_time += 60 * 60 * 23;
			localtime_r(&due_time, &duetime_tm);
			ALARM_MGR_LOG_PRINT("due_time = %d", due_time);
		}
	}

	/* Gets the dst before calculating the duedate as interval */
	localtime_r(&due_time, &before_tm);
	SECURE_LOGD("before_dst = %d", before_tm.tm_isdst);

	wday = duetime_tm.tm_wday;

	ALARM_MGR_LOG_PRINT("current_time(%d) due_time(%d)", current_time, due_time);

	/* CQ defect(72810) : only one time alarm function is not working
	   under all recurrence_disabled. */
	if (due_time > current_time && mode->u_interval.day_of_week == 0)
		return due_time;

	if (current_time > due_time || !(mode->u_interval.day_of_week & 1 << wday) || ((!is_time_changed) && (current_time == due_time))) {
		int day = wday + 1;
		int interval = 1;
		/*this week */

		if (day == 7)
			day = 0;

		while (!(mode->u_interval.day_of_week & 1 << day) && interval < 8) {
			day += 1;
			interval += 1;

			if (day == 7)
				day = 0;
		}

		ALARM_MGR_LOG_PRINT("interval : %d\n", interval);
		due_time += 60 * 60 * 24 * interval;
	}

	/* Gets the dst after calculating the duedate as interval */
	localtime_r(&due_time, &after_tm);
	SECURE_LOGD("after_dst = %d", after_tm.tm_isdst);

	/* Revise the duetime as difference in tm_isdst */
	if (before_tm.tm_isdst == 1 && after_tm.tm_isdst == 0)
		due_time += 60 * 60;	/* Add an hour */
	else if (before_tm.tm_isdst == 0 && after_tm.tm_isdst == 1)
		due_time -= 60 * 60;	/* Subtract an hour */

	ALARM_MGR_LOG_PRINT("Final due_time = %d", due_time);
	return due_time;
}

time_t _alarm_next_duetime(__alarm_info_t *__alarm_info)
{
	int is_dst = 0;
	time_t current_time = 0;
	time_t due_time = 0;
	struct tm tm, *cur_tm = NULL ;
	struct tm *due_tm = NULL ;

	alarm_info_t *alarm_info = &__alarm_info->alarm_info;
	alarm_mode_t *mode = &alarm_info->mode;

	time(&current_time);
	cur_tm = localtime_r(&current_time, &tm);
	if (cur_tm && cur_tm->tm_isdst > 0)
		is_dst = 1;

	ALARM_MGR_LOG_PRINT("mode->repeat is %d\n", mode->repeat);

	if (mode->repeat == ALARM_REPEAT_MODE_ONCE) {
		due_time = __alarm_next_duetime_once(__alarm_info);
	} else if (mode->repeat == ALARM_REPEAT_MODE_REPEAT) {
		due_time = __alarm_next_duetime_repeat(__alarm_info);
	} else if (mode->repeat == ALARM_REPEAT_MODE_ANNUALLY) {
		due_time = __alarm_next_duetime_annually(__alarm_info);
	} else if (mode->repeat == ALARM_REPEAT_MODE_MONTHLY) {
		due_time = __alarm_next_duetime_monthly(__alarm_info);
	} else if (mode->repeat == ALARM_REPEAT_MODE_WEEKLY) {
		due_time = __alarm_next_duetime_weekly(__alarm_info);
	} else {
		ALARM_MGR_EXCEPTION_PRINT("repeat mode(%d) is wrong\n",
				mode->repeat);
		return 0;
	}

	if (mode->repeat != ALARM_REPEAT_MODE_WEEKLY && mode->repeat != ALARM_REPEAT_MODE_ONCE) {
		due_tm = localtime_r(&due_time, &tm);
		if (is_dst == 0 && due_tm && due_tm->tm_isdst == 1) {
			ALARM_MGR_LOG_PRINT("DST alarm found, enable\n");
			due_tm->tm_hour = due_tm->tm_hour - DST_TIME_DIFF;
		} else if (is_dst == 1 && due_tm && due_tm->tm_isdst == 0) {
			ALARM_MGR_LOG_PRINT("DST alarm found. disable\n");
			due_tm->tm_hour = due_tm->tm_hour + DST_TIME_DIFF;
		}
		if (due_tm)
			due_time = mktime(due_tm);
	}

	ALARM_MGR_LOG_PRINT("alarm_id: %d, next duetime: %d", __alarm_info->alarm_id, due_time);

	if (__alarm_info->end != 0 && __alarm_info->end < due_time) {
		ALARM_MGR_LOG_PRINT("due time > end time");
		__alarm_info->due_time = 0;
		return 0;
	}
	__alarm_info->due_time = due_time;

	return due_time;
}

static bool __find_next_alarm_to_be_scheduled(time_t *min_due_time)
{
	time_t current_time;
	time_t min_time = -1;
	time_t due_time;
	GSList *iter = NULL;
	__alarm_info_t *entry = NULL;

	time(&current_time);

	for (iter = alarm_context.alarms; iter != NULL;
			iter = g_slist_next(iter)) {
		entry = iter->data;
		due_time = entry->due_time;

		double interval = 0;

		SECURE_LOGD("alarm[%d] with duetime(%u) at current(%u) pid: (%d)\n",
				entry->alarm_id, due_time, current_time, entry->pid);
		if (due_time == 0)	/*0 means this alarm has been disabled*/ {
			continue;
		}

		interval = difftime(due_time, current_time);

		if (interval < 0)	/*2008.08.06 when the alarm expires, it may makes an error.*/ {
			ALARM_MGR_EXCEPTION_PRINT("The duetime of alarm(%d) is OVER.", entry->alarm_id);
			continue;
		}

		interval = difftime(due_time, min_time);

		if ((interval < 0) || min_time == -1)
			min_time = due_time;
	}

	*min_due_time = min_time;
	return true;
}

bool _alarm_schedule()
{
	time_t due_time = 0;
	time_t min_time = 0;
	GSList *iter = NULL;
	__alarm_info_t *entry = NULL;

	__find_next_alarm_to_be_scheduled(&min_time);

	if (min_time == -1) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server][schedule]: There is no alarm to be scheduled.");
	} else {
		for (iter = alarm_context.alarms; iter != NULL; iter = g_slist_next(iter)) {
			entry = iter->data;
			due_time = entry->due_time;

			if (due_time == min_time)
				_add_to_scheduled_alarm_list(entry);
		}
		_alarm_set_timer(&alarm_context, alarm_context.timer, min_time);
	}

	return true;
}
