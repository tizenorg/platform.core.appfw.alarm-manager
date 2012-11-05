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



#define _BSD_SOURCE /*for localtime_r*/
#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<signal.h>
#include<string.h>
#include<sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include<dbus/dbus.h>
#include<dbus/dbus-glib-lowlevel.h>
#include<glib.h>
#if !GLIB_CHECK_VERSION (2, 31, 0)
#include <glib/gmacros.h>
#else
#endif

/*#include "global-gconf.h"*/
#include"alarm.h"
#include"alarm-internal.h"

/*#include"limo-event-delivery.h"*/

#include <aul.h>
#include <bundle.h>
#include <heynoti.h>
#include <security-server.h>
#include <db-util.h>
#include <vconf.h>
#include <vconf-keys.h>

#define SIG_TIMER 0x32
#define WAKEUP_ALARM_APP_ID       "org.tizen.alarm.ALARM"
	/* alarm ui application's alarm's dbus_service name instead of 21
	   (alarm application's app_id) value */
#define WAKEUP_ALARMBOOTING_APP_ID	"org.tizen.alarmboot.ui.ALARM"
/*alrmbooting ui application's dbus_service name instead of 121(alarmbooting
	application's app_id) value */
/*
#include "tapi_misc_ext.h"
#include "TelMisc.h"
#include "tapi_misc_data.h"
#include "tapi_power.h"
*/

#include "pmapi.h"

__alarm_server_context_t alarm_context;
bool g_dummy_timer_is_set = FALSE;

GSList *g_scheduled_alarm_list = NULL;

GSList *g_expired_alarm_list = NULL;

#ifndef RTC_WKALM_BOOT_SET
#define RTC_WKALM_BOOT_SET _IOW('p', 0x80, struct rtc_wkalrm)
#endif

#ifdef __ALARM_BOOT
bool enable_power_on_alarm;
bool alarm_boot;
time_t ab_due_time = -1;
bool poweron_alarm_expired = false;
#endif

/*	2008. 6. 3 sewook7.park
       When the alarm becoms sleeping mode, alarm timer is not expired.
       So using RTC, phone is awaken before alarm rings.
*/
#define __WAKEUP_USING_RTC__
#ifdef __WAKEUP_USING_RTC__
#include<errno.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <fcntl.h>

static const char default_rtc[] = "/dev/rtc1";
static const char power_rtc[] = "/dev/rtc0";

#endif				/*__WAKEUP_USING_RTC__*/

static bool __alarm_add_to_list(__alarm_info_t *__alarm_info,
				alarm_id_t *alarm_id);
static bool __alarm_update_in_list(int pid, alarm_id_t alarm_id,
				   __alarm_info_t *__alarm_info,
				   int *error_code);
static bool __alarm_remove_from_list(int pid, alarm_id_t alarm_id,
				     int *error_code);
static bool __alarm_set_start_and_end_time(alarm_info_t *alarm_info,
					   __alarm_info_t *__alarm_info);
static bool __alarm_update_due_time_of_all_items_in_list(double diff_time);
static bool __alarm_create(alarm_info_t *alarm_info, alarm_id_t *alarm_id,
			   int pid, char *app_service_name, char *app_service_name_mod,
			   const char *dst_service_name, const char *dst_service_name_mod, int *error_code);
static bool __alarm_create_appsvc(alarm_info_t *alarm_info, alarm_id_t *alarm_id,
			   int pid, char *bundle_data, int *error_code);

static bool __alarm_delete(int pid, alarm_id_t alarm_id, int *error_code);
static bool __alarm_update(int pid, char *app_service_name, alarm_id_t alarm_id,
			   alarm_info_t *alarm_info, int *error_code);
static bool __alarm_power_on(int app_id, bool on_off, int *error_code);
static bool __alarm_power_off(int app_id, int *error_code);
static bool __alarm_check_next_duetime(int app_id, int *error_code);
static void __alarm_send_noti_to_application(const char *app_service_name,
					     alarm_id_t alarm_id);
static void __alarm_expired();
static gboolean __alarm_handler_idle();
static void __alarm_handler(int sigNum, siginfo_t *pSigInfo, void *pUContext);
static void __clean_registry();
static bool __alarm_manager_reset();
static void __on_system_time_changed(keynode_t *node, void *data);
static void __initialize_timer();
static void __initialize_alarm_list();
static void __initialize_scheduled_alarm_lsit();
static void __hibernation_leave_callback();
static bool __initialize_noti();

static bool __initialize_dbus();
static bool __initialize_db();
static void __initialize();
static bool __check_false_alarm();
static DBusHandlerResult __alarm_server_filter(DBusConnection *connection,
					       DBusMessage *message,
					       void *user_data);

static void __rtc_set()
{
#ifdef __WAKEUP_USING_RTC__

	const char *rtc = default_rtc;
	int fd = 0;
	struct rtc_time rtc_tm;
	struct rtc_wkalrm rtc_wk;
	struct tm due_tm;

#ifdef _SIMUL			/*if build is simulator, we don't need to set
				   RTC because RTC does not work in simulator.*/
	ALARM_MGR_EXCEPTION_PRINT("because it is simulator's mode, "
				  "we don't set RTC.\n");
	return;
#endif

	fd = open(rtc, O_RDONLY);
	if (fd == -1) {
		ALARM_MGR_EXCEPTION_PRINT("RTC open failed.\n");
		return;
	}

	/* Read the RTC time/date */
	int retval = ioctl(fd, RTC_RD_TIME, &rtc_tm);
	if (retval == -1) {
		ALARM_MGR_EXCEPTION_PRINT("RTC_RD_TIME ioctl failed");
		close(fd);
		return;
	}

	ALARM_MGR_LOG_PRINT("\n\nCurrent RTC date/time is %d-%d-%d, "
		"%02d:%02d:%02d.\n", rtc_tm.tm_mday, rtc_tm.tm_mon + 1, 
		rtc_tm.tm_year + 1900, rtc_tm.tm_hour, rtc_tm.tm_min, 
		rtc_tm.tm_sec);

	ALARM_MGR_LOG_PRINT("alarm_context.c_due_time is %d\n", \
			    alarm_context.c_due_time);

	if (alarm_context.c_due_time != -1) {
		time_t due_time = alarm_context.c_due_time - 1;
		gmtime_r(&due_time, &due_tm);

		rtc_tm.tm_mday = due_tm.tm_mday;
		rtc_tm.tm_mon = due_tm.tm_mon;
		rtc_tm.tm_year = due_tm.tm_year;
		rtc_tm.tm_hour = due_tm.tm_hour;
		rtc_tm.tm_min = due_tm.tm_min;
		rtc_tm.tm_sec = due_tm.tm_sec;
		memcpy(&rtc_wk.time, &rtc_tm, sizeof(rtc_tm));
		rtc_wk.enabled = 1;
		rtc_wk.pending = 0;

		ALARM_MGR_LOG_PRINT("\n\nSetted RTC Alarm date/time is "
			"%d-%d-%d, %02d:%02d:%02d.\n", rtc_tm.tm_mday, 
			rtc_tm.tm_mon + 1, rtc_tm.tm_year + 1900, 
			rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec);

		retval = ioctl(fd, RTC_WKALM_SET, &rtc_wk);
		if (retval == -1) {
			if (errno == ENOTTY) {
				ALARM_MGR_EXCEPTION_PRINT("\nAlarm IRQs not"
							  "supported.\n");
			}
			ALARM_MGR_EXCEPTION_PRINT("RTC_ALM_SET ioctl");
			close(fd);
			return;
		}
		ALARM_MGR_LOG_PRINT("[alarm-server]RTC alarm is setted");

		/* Enable alarm interrupts */
		retval = ioctl(fd, RTC_AIE_ON, 0);
		if (retval == -1) {
			ALARM_MGR_EXCEPTION_PRINT("RTC_AIE_ON ioctl failed");
			close(fd);
			return;
		}
		ALARM_MGR_LOG_PRINT("[alarm-server]RTC alarm is on");
	} else
		ALARM_MGR_LOG_PRINT("[alarm-server]alarm_context.c_due_time is"
				    "less than 10 sec. RTC alarm does not need to be set\n");

	close(fd);

#endif				/* __WAKEUP_USING_RTC__ */
}

int _set_rtc_time(time_t _time)
{
	int fd0 = 0;
	int fd1 = 0;
	int retval0 = 0;
	int retval1 = 0;
	struct rtc_time rtc_tm;
	const char *rtc0 = power_rtc;
	const char *rtc1 = default_rtc;
	struct tm *_tm;
	struct tm time_r;

	fd0 = open(rtc0, O_RDONLY);
	fd1 = open(rtc1, O_RDONLY);

	if (fd0 == -1) {
		ALARM_MGR_LOG_PRINT("error to open /dev/rtc0.");
		perror("\t");
	}

	if (fd1 == -1) {
		ALARM_MGR_LOG_PRINT("error to open /dev/rtc1.");
		perror("\t");
	}

	memset(&rtc_tm, 0, sizeof(struct rtc_time));


	_tm = gmtime_r(&_time, &time_r);

	/* Write the RTC time/date 2008:05:21 19:20:00 */


	rtc_tm.tm_mday = time_r.tm_mday;
	rtc_tm.tm_mon = time_r.tm_mon;
	rtc_tm.tm_year = time_r.tm_year;
	rtc_tm.tm_hour = time_r.tm_hour;
	rtc_tm.tm_min = time_r.tm_min;
	rtc_tm.tm_sec = 0;
	

	retval0 = ioctl(fd0, RTC_SET_TIME, &rtc_tm);

	if (retval0 == -1) {
		close(fd0);
		ALARM_MGR_LOG_PRINT("error to ioctl fd0.");
		perror("\t");
	}
	close(fd0);

	retval1 = ioctl(fd1, RTC_SET_TIME, &rtc_tm);

	if (retval1 == -1) {
		close(fd1);
		ALARM_MGR_LOG_PRINT("error to ioctl fd1.");
		perror("\t");
	}
	close(fd1);

	return 1;
}

bool __alarm_clean_list()
{
	GSList *iter = NULL;

	for (iter = alarm_context.alarms; iter != NULL;
	     iter = g_slist_next(iter)) {
		free(iter->data);
	}

	g_slist_free(alarm_context.alarms);
	return true;
}

static bool __alarm_add_to_list(__alarm_info_t *__alarm_info,
				alarm_id_t *alarm_id)
{

	bool unique_id = false;
	alarm_info_t *alarm_info = &__alarm_info->alarm_info;
	__alarm_info_t *entry = NULL;

	GSList *iter = NULL;

	/* FIXME: alarm id must be unique. */
	__alarm_info->alarm_id = (int)(void *)__alarm_info;
	ALARM_MGR_LOG_PRINT("__alarm_info->alarm_id is %d", \
			    __alarm_info->alarm_id);

	while (unique_id == false) {
		unique_id = true;

		for (iter = alarm_context.alarms; iter != NULL;
		     iter = g_slist_next(iter)) {
			entry = iter->data;
			if (entry->alarm_id == __alarm_info->alarm_id) {
				__alarm_info->alarm_id++;
				unique_id = false;
			}
		}

	}

	/* list alarms */
	ALARM_MGR_LOG_PRINT("[alarm-server]: before add\n");
	for (iter = alarm_context.alarms; iter != NULL;
	     iter = g_slist_next(iter)) {
		entry = iter->data;
		/*ALARM_MGR_LOG_PRINT("[alarm-server]: alarm_id(%d)\n", 
		   entry->alarm_id); */
	}

	alarm_context.alarms =
	    g_slist_append(alarm_context.alarms, __alarm_info);
	/*list alarms */
	ALARM_MGR_LOG_PRINT("[alarm-server]: after add\n");
	for (iter = alarm_context.alarms; iter != NULL;
	     iter = g_slist_next(iter)) {
		entry = iter->data;
		ALARM_MGR_LOG_PRINT("[alarm-server]: alarm_id(%d)\n",
				    entry->alarm_id);
	}

	if (!(alarm_info->alarm_type & ALARM_TYPE_VOLATILE)) {
		_save_alarms(__alarm_info);
	}

	*alarm_id = __alarm_info->alarm_id;

	return true;
}

static bool __alarm_update_in_list(int pid, alarm_id_t alarm_id,
				   __alarm_info_t *__alarm_info,
				   int *error_code)
{
	bool found = false;
	alarm_info_t *alarm_info = &__alarm_info->alarm_info;
	GSList *iter = NULL;
	__alarm_info_t *entry = NULL;

	for (iter = alarm_context.alarms; iter != NULL;
	     iter = g_slist_next(iter)) {
		entry = iter->data;
		if (entry->alarm_id == alarm_id) {

			found = true;
			__alarm_info->quark_app_unique_name =
			    entry->quark_app_unique_name;
			__alarm_info->quark_dst_service_name =
			    entry->quark_dst_service_name;
			memcpy(entry, __alarm_info, sizeof(__alarm_info_t));

			break;
		}
	}

	if (!found) {
		if (error_code)
			*error_code = ERR_ALARM_INVALID_ID;
		return false;
	}

	if (!(alarm_info->alarm_type & ALARM_TYPE_VOLATILE)) {
		_update_alarms(__alarm_info);
	}

	return true;
}

static bool __alarm_remove_from_list(int pid, alarm_id_t alarm_id,
				     int *error_code)
{
	bool found = false;

	alarm_info_t *alarm_info = NULL;

	GSList *iter = NULL;
	__alarm_info_t *entry = NULL;

	/*list alarms */
	ALARM_MGR_LOG_PRINT("[alarm-server]: before del : alarm id(%d)\n",
			    alarm_id);

	for (iter = alarm_context.alarms; iter != NULL;
	     iter = g_slist_next(iter)) {
		entry = iter->data;
		if (entry->alarm_id == alarm_id) {
			alarm_info = &entry->alarm_info;

			ALARM_MGR_LOG_PRINT("[alarm-server]: "
					    "__alarm_remove_from_list : alarm id(%d)\n",
					    entry->alarm_id);

			if (!(alarm_info->alarm_type & ALARM_TYPE_VOLATILE)) {
				_delete_alarms(alarm_id);
			}

			alarm_context.alarms =
			    g_slist_remove(alarm_context.alarms, iter->data);
			found = true;
			break;
		}

	}

	ALARM_MGR_LOG_PRINT("[alarm-server]: after del\n");

	if (!found) {
		if (error_code)
			*error_code = ERR_ALARM_INVALID_ID;
		return false;
	}

	return true;
}

static bool __alarm_set_start_and_end_time(alarm_info_t *alarm_info,
					   __alarm_info_t *__alarm_info)
{
	alarm_date_t *start = &alarm_info->start;
	alarm_date_t *end = &alarm_info->end;

	struct tm alarm_tm = { 0, };

	if (start->year != 0) {
		alarm_tm.tm_year = start->year - 1900;
		alarm_tm.tm_mon = start->month - 1;
		alarm_tm.tm_mday = start->day;

		alarm_tm.tm_hour = 0;
		alarm_tm.tm_min = 0;
		alarm_tm.tm_sec = 0;

		__alarm_info->start = mktime(&alarm_tm);
	} else {
		__alarm_info->start = 0;
	}

	if (end->year != 0) {
		alarm_tm.tm_year = end->year - 1900;
		alarm_tm.tm_mon = end->month - 1;
		alarm_tm.tm_mday = end->day;

		alarm_tm.tm_hour = 23;
		alarm_tm.tm_min = 59;
		alarm_tm.tm_sec = 59;

		__alarm_info->end = mktime(&alarm_tm);
	} else {
		__alarm_info->end = 0;
	}

	return true;
}

/*
static bool alarm_get_tz_info(int *gmt_idx, int *dst)
{
	GConfValue *value1 = NULL;
	GConfValue *value2 = NULL;
	GConfClient* gConfClient = NULL;
	GError* err = NULL;		
	
	gConfClient = gconf_client_get_default();

	if(gConfClient) {
		value1 = gconf_client_get(gConfClient, SETTINGS_CLOCKTIMEZONE, 
									&err);
		if (err) {
			ALARM_MGR_LOG_PRINT("__on_system_time_changed:
			gconf_client_get() failed:
			error:[%s]\n", err->message);
			g_error_free(err);
			err = NULL;
		}
		*gmt_idx = gconf_value_get_int(value1);
		ALARM_MGR_LOG_PRINT("gconf return gmt_idx =%d\n ", *gmt_idx);
	
		value2 = gconf_client_get(gConfClient, 
			SETTINGS_DAYLIGHTSTATUS, &err);
		if (err) {
			ALARM_MGR_LOG_PRINT("__on_system_time_changed:
		gconf_client_get() failed: error:[%s]\n", err->message);
		g_error_free(err);
		err = NULL;
	}
	
	*dst = gconf_value_get_int(value2);
	ALARM_MGR_LOG_PRINT("gconf return dst =%d\n ", *dst);
	
	if(gConfClient != NULL) {
		g_object_unref(gConfClient);
		gConfClient = NULL;
		}
	}
	else
		ALARM_MGR_LOG_PRINT("check the gconf setting failed!!!!! \n ");
	
	if(value1) {
		gconf_value_free(value1);
		value1 = NULL;
	}
	if(value2) {
		gconf_value_free(value2);
		value2 = NULL;
	}

	return true;
}
*/

static bool __alarm_update_due_time_of_all_items_in_list(double diff_time)
{
	time_t current_time;
	time_t min_time = -1;
	time_t due_time = 0;
	GSList *iter = NULL;
	__alarm_info_t *entry = NULL;
	struct tm *p_time = NULL ;
	struct tm due_time_result ;
	struct tm fixed_time ;

	for (iter = alarm_context.alarms; iter != NULL;
	     iter = g_slist_next(iter)) {
		entry = iter->data;
		alarm_info_t *alarm_info = &(entry->alarm_info);
		if (alarm_info->alarm_type & ALARM_TYPE_RELATIVE) {
			/*diff_time 贸府 */

			entry->due_time += diff_time;

			alarm_date_t *start = &alarm_info->start; /**< start 
							time of the alarm */
			alarm_date_t *end = &alarm_info->end;;
						/**< end time of the alarm */

			tzset();
			p_time = localtime_r(&entry->due_time, &due_time_result);
			if (p_time != NULL) {
				start->year = p_time->tm_year + 1900;
				start->month = p_time->tm_mon + 1;
				start->day = p_time->tm_mday;
				start->hour = p_time->tm_hour;
				start->min = p_time->tm_min;
				start->sec = p_time->tm_sec;

				end->year = p_time->tm_year + 1900;
				end->month = p_time->tm_mon + 1;
				end->day = p_time->tm_mday;

				
				memset(&fixed_time, 0, sizeof(fixed_time));
				fixed_time.tm_year = p_time->tm_year;
				fixed_time.tm_mon = p_time->tm_mon;
				fixed_time.tm_mday = p_time->tm_mday;
				fixed_time.tm_hour = 0;
				fixed_time.tm_min = 0;
				fixed_time.tm_sec = 0;
			}
			entry->start = mktime(&fixed_time);

			fixed_time.tm_hour = 23;
			fixed_time.tm_min = 59;
			fixed_time.tm_sec = 59;

			entry->end = mktime(&fixed_time);

			ALARM_MGR_LOG_PRINT("alarm_info->alarm_type is "
					    "ALARM_TYPE_RELATIVE\n");

			_update_alarms(entry);
		}

		_alarm_next_duetime(entry);
		ALARM_MGR_LOG_PRINT("entry->due_time is %d\n", entry->due_time);
	}

	time(&current_time);

	for (iter = alarm_context.alarms; iter != NULL;
	     iter = g_slist_next(iter)) {
		entry = iter->data;
		due_time = entry->due_time;

		double interval = 0;

		ALARM_MGR_LOG_PRINT("alarm[%d] with duetime(%u) at "
		"current(%u)\n", entry->alarm_id, due_time, current_time);
		if (due_time == 0) {	/* 0 means this alarm has been 
					   disabled */
			continue;
		}

		interval = difftime(due_time, current_time);

		if (interval <= 0) {
			ALARM_MGR_LOG_PRINT("this may be error.. alarm[%d]\n", \
					    entry->alarm_id);
			continue;
		}

		interval = difftime(due_time, min_time);

		if ((interval < 0) || min_time == -1) {
			min_time = due_time;
		}

	}

	alarm_context.c_due_time = min_time;

	return true;
}

static bool __alarm_create_appsvc(alarm_info_t *alarm_info, alarm_id_t *alarm_id,
			   int pid,char *bundle_data, int *error_code){

	time_t current_time;
	time_t due_time;
	struct tm ts_ret;
	char due_time_r[100] = { 0 };
	char proc_file[512] = { 0 };
	char process_name[512] = { 0 };
	char app_name[512] = { 0 };
	char *word = NULL;
	char *proc_name_ptr = NULL;
	int fd = 0;
	int ret = 0;
	int i = 0;

	__alarm_info_t *__alarm_info = NULL;

	__alarm_info = malloc(sizeof(__alarm_info_t));
	if (__alarm_info == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("Caution!! app_pid=%d, malloc "
					  "failed. it seems to be OOM\n", pid);
		*error_code = -1;	/* -1 means that system 
					   failed internally. */
		return false;
	}
	__alarm_info->pid = pid;
	__alarm_info->alarm_id = -1;


	/* we should consider to check whether  pid is running or Not
	 */
	memset(process_name, '\0', 512);
	memset(proc_file, '\0', 512);
	snprintf(proc_file, 512, "/proc/%d/cmdline", pid);

	fd = open(proc_file, O_RDONLY);
	if (fd > 0) {
		ret = read(fd, process_name, 512);
		close(fd);
		if (ret <=0)
		{
			ALARM_MGR_EXCEPTION_PRINT("Unable to get application name\n");
			*error_code = -1;
			free(__alarm_info);
			return false;
		}
		while (process_name[i] != '\0') {
			if (process_name[i] == ' ') {
				process_name[i] = '\0';
				break;
			}
			i++;
		}

		word = strtok_r(process_name, "/", &proc_name_ptr);
		while (word != NULL) {
			memset(app_name, 0, 512);
			snprintf(app_name, 512, "%s", word);
			word = strtok_r(NULL, "/", &proc_name_ptr);
		}
		__alarm_info->quark_app_unique_name =
		    g_quark_from_string(app_name);
	} else {		/* failure */
		ALARM_MGR_EXCEPTION_PRINT("Caution!! app_pid(%d) seems to be "
					  "killed, so we failed to get proc file(%s) and do not create "
					  "alarm_info\n", pid, proc_file);
		*error_code = -1;	/*-1 means that system failed 
							internally.*/
		free(__alarm_info);
		return false;
	}

	__alarm_info->quark_bundle=g_quark_from_string(bundle_data);
	__alarm_info->quark_app_service_name = g_quark_from_string("null");
	__alarm_info->quark_dst_service_name = g_quark_from_string("null");
	__alarm_info->quark_app_service_name_mod = g_quark_from_string("null");
	__alarm_info->quark_dst_service_name_mod = g_quark_from_string("null");

	__alarm_set_start_and_end_time(alarm_info, __alarm_info);
	memcpy(&(__alarm_info->alarm_info), alarm_info, sizeof(alarm_info_t));

	time(&current_time);

	if (alarm_context.c_due_time < current_time) {
		ALARM_MGR_EXCEPTION_PRINT("Caution!! alarm_context.c_due_time "
		"(%d) is less than current time(%d)", alarm_context.c_due_time,
					  current_time);
		alarm_context.c_due_time = -1;
	}

	due_time = _alarm_next_duetime(__alarm_info);
	if (__alarm_add_to_list(__alarm_info, alarm_id) == false) {
		free(__alarm_info);
		*error_code = -1;
		return false;
	}

	if (due_time == 0) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:Create a new alarm: "
		"due_time is 0, alarm(%d) \n", *alarm_id);
		return true;
	} else if (current_time == due_time) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:Create alarm: "
		     "current_time(%d) is same as due_time(%d)", current_time,
		     due_time);
		return true;
	}else if (difftime(due_time, current_time) <  0){
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]: Expired Due Time.[Due time=%d, Current Time=%d]!!!Do not add to schedule list\n", due_time, current_time);
		return true;
	}else {
		localtime_r(&due_time, &ts_ret);
		strftime(due_time_r, 30, "%c", &ts_ret);
		ALARM_MGR_LOG_PRINT("[alarm-server]:Create a new alarm: "
				    "alarm(%d) due_time(%s)", *alarm_id,
				    due_time_r);
	}

	ALARM_MGR_LOG_PRINT("[alarm-server]:alarm_context.c_due_time(%d), "
			    "due_time(%d)", alarm_context.c_due_time, due_time);

	if (alarm_context.c_due_time == -1
	    || due_time < alarm_context.c_due_time) {
		_clear_scheduled_alarm_list();
		_add_to_scheduled_alarm_list(__alarm_info);
		_alarm_set_timer(&alarm_context, alarm_context.timer, due_time,
				 *alarm_id);
		alarm_context.c_due_time = due_time;

	} else if (due_time == alarm_context.c_due_time) {
		_add_to_scheduled_alarm_list(__alarm_info);

	}

	__rtc_set();

#ifdef __ALARM_BOOT
	/*alarm boot */
	if (enable_power_on_alarm) {
		__alarm_power_on(0, enable_power_on_alarm, NULL);
	}
#endif

	return true;

}

static bool __alarm_create(alarm_info_t *alarm_info, alarm_id_t *alarm_id,
			   int pid, char *app_service_name, char *app_service_name_mod,
			   const char *dst_service_name,const char *dst_service_name_mod,  int *error_code)
{

	time_t current_time;
	time_t due_time;
	char proc_file[256] = { 0 };
	char process_name[512] = { 0 };
	char app_name[256] = { 0 };
	char *word = NULL;
	char *proc_name_ptr = NULL;

	__alarm_info_t *__alarm_info = NULL;

	__alarm_info = malloc(sizeof(__alarm_info_t));
	if (__alarm_info == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("Caution!! app_pid=%d, malloc "
					  "failed. it seems to be OOM\n", pid);
		*error_code = -1;	/* -1 means that system 
					   failed internally. */
		return false;
	}
	__alarm_info->pid = pid;
	__alarm_info->alarm_id = -1;

	/* we should consider to check whether  pid is running or Not
	 */
	memset(process_name, '\0', 512);
	memset(proc_file, '\0', 256);
	snprintf(proc_file, 256, "/proc/%d/cmdline", pid);

	int fd;
	int ret;
	int i = 0;
	fd = open(proc_file, O_RDONLY);
	if (fd > 0) {
		ret = read(fd, process_name, 512);
		close(fd);
		while (process_name[i] != '\0') {
			if (process_name[i] == ' ') {
				process_name[i] = '\0';
				break;
			}
			i++;
		}
		/* if (readlink(proc_file, process_name, 256)!=-1) */
		/*success */

		word = strtok_r(process_name, "/", &proc_name_ptr);
		while (word != NULL) {
			memset(app_name, 0, 256);
			snprintf(app_name, 256, "%s", word);
			word = strtok_r(NULL, "/", &proc_name_ptr);
		}
		__alarm_info->quark_app_unique_name =
		    g_quark_from_string(app_name);
	} else {		/* failure */

		__alarm_info->quark_app_unique_name =
		    g_quark_from_string("unknown");
		ALARM_MGR_EXCEPTION_PRINT("Caution!! app_pid(%d) seems to be "
					  "killed, so we failed to get proc file(%s) and do not create "
					  "alarm_info\n", pid, proc_file);
		*error_code = -1;	/*-1 means that system failed 
							internally.*/
		free(__alarm_info);
		return false;
	}

	__alarm_info->quark_app_service_name =
	    g_quark_from_string(app_service_name);
	__alarm_info->quark_app_service_name_mod =
	g_quark_from_string(app_service_name_mod);
	__alarm_info->quark_dst_service_name =
	    g_quark_from_string(dst_service_name);
	__alarm_info->quark_dst_service_name_mod =
	    g_quark_from_string(dst_service_name_mod);
	__alarm_info->quark_bundle = g_quark_from_string("null");


	__alarm_set_start_and_end_time(alarm_info, __alarm_info);
	memcpy(&(__alarm_info->alarm_info), alarm_info, sizeof(alarm_info_t));

	time(&current_time);

	ALARM_MGR_LOG_PRINT("[alarm-server]:pid=%d, app_unique_name=%s, "
		"app_service_name=%s,dst_service_name=%s, c_due_time=%d", \
		pid, g_quark_to_string(__alarm_info->quark_app_unique_name), \
		g_quark_to_string(__alarm_info->quark_app_service_name), \
		g_quark_to_string(__alarm_info->quark_dst_service_name), \
			    alarm_context.c_due_time);

	if (alarm_context.c_due_time < current_time) {
		ALARM_MGR_EXCEPTION_PRINT("Caution!! alarm_context.c_due_time "
		"(%d) is less than current time(%d)", alarm_context.c_due_time,
					  current_time);
		alarm_context.c_due_time = -1;
	}

	due_time = _alarm_next_duetime(__alarm_info);
	if (__alarm_add_to_list(__alarm_info, alarm_id) == false) {
		free(__alarm_info);
		return false;
	}

	if (due_time == 0) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:Create a new alarm: "
		"due_time is 0, alarm(%d) \n", *alarm_id);
		return true;
	} else if (current_time == due_time) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:Create alarm: "
		     "current_time(%d) is same as due_time(%d)", current_time,
		     due_time);
		return true;
	}else if (difftime(due_time, current_time) <  0){
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]: Expired Due Time.[Due time=%d, Current Time=%d]!!!Do not add to schedule list\n", due_time, current_time);
		return true;
	} else {
		char due_time_r[100] = { 0 };
		struct tm ts_ret;
		localtime_r(&due_time, &ts_ret);
		strftime(due_time_r, 30, "%c", &ts_ret);
		ALARM_MGR_LOG_PRINT("[alarm-server]:Create a new alarm: "
				    "alarm(%d) due_time(%s)", *alarm_id,
				    due_time_r);
	}

	ALARM_MGR_LOG_PRINT("[alarm-server]:alarm_context.c_due_time(%d), "
			    "due_time(%d)", alarm_context.c_due_time, due_time);

	if (alarm_context.c_due_time == -1
	    || due_time < alarm_context.c_due_time) {
		_clear_scheduled_alarm_list();
		_add_to_scheduled_alarm_list(__alarm_info);
		_alarm_set_timer(&alarm_context, alarm_context.timer, due_time,
				 *alarm_id);
		alarm_context.c_due_time = due_time;

	} else if (due_time == alarm_context.c_due_time) {
		_add_to_scheduled_alarm_list(__alarm_info);

	}

	__rtc_set();

#ifdef __ALARM_BOOT
	/*alarm boot */
	if (enable_power_on_alarm) {
		/* orginally first arg's value was 21(app_id, WAKEUP_ALARM_APP_ID) in a
		 * platform with app-server.because __alarm_power_on(..) fuction don't 
		 * use first parameter internally, we set this value to 0(zero)
		 */
		__alarm_power_on(0, enable_power_on_alarm, NULL);
	}
#endif
	return true;
}

static bool __alarm_update(int pid, char *app_service_name, alarm_id_t alarm_id,
			   alarm_info_t *alarm_info, int *error_code)
{
	time_t current_time;
	time_t due_time;

	__alarm_info_t *__alarm_info = NULL;
	bool result = false;

	__alarm_info = malloc(sizeof(__alarm_info_t));
	if (__alarm_info == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("Caution!! app_pid=%d, "
			"malloc failed. it seems to be OOM\n", pid);
		*error_code = -1;	/*-1 means that system failed 
						internally.*/
		return false;
	}

	__alarm_info->pid = pid;
	__alarm_info->alarm_id = alarm_id;

	/* we should consider to check whether  pid is running or Not
	 */

	__alarm_info->quark_app_service_name =
	    g_quark_from_string(app_service_name);
	__alarm_set_start_and_end_time(alarm_info, __alarm_info);
	memcpy(&(__alarm_info->alarm_info), alarm_info, sizeof(alarm_info_t));

	time(&current_time);

	if (alarm_context.c_due_time < current_time) {
		ALARM_MGR_EXCEPTION_PRINT("Caution!! alarm_context.c_due_time "
		"(%d) is less than current time(%d)", alarm_context.c_due_time,
					  current_time);
		alarm_context.c_due_time = -1;
	}

	due_time = _alarm_next_duetime(__alarm_info);
	if (!__alarm_update_in_list(pid, alarm_id, __alarm_info, error_code)) {
		free(__alarm_info);
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]: requested alarm_id "
		"(%d) does not exist. so this value is invalid id.", alarm_id);
		return false;
	}
	/* ALARM_MGR_LOG_PRINT("[alarm-server]:request_pid=%d, alarm_id=%d, 
	 * app_unique_name=%s, app_service_name=%s, dst_service_name=%s, 
	 * c_due_time=%d", pid, alarm_id, g_quark_to_string
	 * (__alarm_info->quark_app_unique_name), g_quark_to_string
	 * (__alarm_info->quark_app_service_name), g_quark_to_string
	 * (__alarm_info->quark_dst_service_name), alarm_context.c_due_time);
	 */

	result = _remove_from_scheduled_alarm_list(pid, alarm_id);

	if (result == true && g_slist_length(g_scheduled_alarm_list) == 0) {
		/*there is no scheduled alarm */
		_alarm_disable_timer(alarm_context);
		_alarm_schedule();

		ALARM_MGR_LOG_PRINT("[alarm-server]:Update alarm: alarm(%d)\n",
				    alarm_id);

		__rtc_set();

#ifdef __ALARM_BOOT
		/*alarm boot */
		if (enable_power_on_alarm) {
			/* orginally first arg's value was 21(app_id, WAKEUP_ALARM_APP_ID) in 
			 * a platform with app-server.because __alarm_power_on(..) fuction don't
			 * use first parameter internally, we set this value to 0(zero)
			 */
			__alarm_power_on(0, enable_power_on_alarm, NULL);
		}
#endif

		if (due_time == 0) {
			ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:Update alarm: "
					"due_time is 0\n");
		}
		free(__alarm_info);
		return true;
	}

	if (due_time == 0) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:Update alarm: "
				"due_time is 0, alarm(%d)\n", alarm_id);
		free(__alarm_info);
		return true;
	} else if (current_time == due_time) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:Update alarm: "
		"current_time(%d) is same as due_time(%d)", current_time,
		due_time);
		free(__alarm_info);
		return true;
	}else if (difftime(due_time, current_time)< 0){
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]: Expired Due Time.[Due time=%d, Current Time=%d]!!!Do not add to schedule list\n", due_time, current_time);
		return true;
	} else {
		char due_time_r[100] = { 0 };
		struct tm ts_ret;
		localtime_r(&due_time, &ts_ret);
		strftime(due_time_r, 30, "%c", &ts_ret);
		ALARM_MGR_LOG_PRINT("[alarm-server]:Update alarm: alarm(%d) "
				    "due_time(%s)\n", alarm_id, due_time_r);
	}

	ALARM_MGR_LOG_PRINT("[alarm-server]:alarm_context.c_due_time(%d), "
			    "due_time(%d)", alarm_context.c_due_time, due_time);

	if (alarm_context.c_due_time == -1
	    || due_time < alarm_context.c_due_time) {
		_clear_scheduled_alarm_list();
		_add_to_scheduled_alarm_list(__alarm_info);
		_alarm_set_timer(&alarm_context, alarm_context.timer, due_time,
				 alarm_id);
		alarm_context.c_due_time = due_time;
		ALARM_MGR_LOG_PRINT("[alarm-server1]:alarm_context.c_due_time "
		     "(%d), due_time(%d)", alarm_context.c_due_time, due_time);
	} else if (due_time == alarm_context.c_due_time) {
		_add_to_scheduled_alarm_list(__alarm_info);
		ALARM_MGR_LOG_PRINT("[alarm-server2]:alarm_context.c_due_time "
		     "(%d), due_time(%d)", alarm_context.c_due_time, due_time);
	}

	__rtc_set();

#ifdef __ALARM_BOOT
	/*alarm boot */
	if (enable_power_on_alarm) {
		/* orginally first arg's value was 21(app_id, WAKEUP_ALARM_APP_ID) 
		 * in a platform with app-server.because __alarm_power_on(..) fuction 
		 * don't use first parameter internally, we set this value to 0(zero)
		 */
		__alarm_power_on(0, enable_power_on_alarm, NULL);
	}
#endif

	return true;
}

static bool __alarm_delete(int pid, alarm_id_t alarm_id, int *error_code)
{
	bool result = false;

	ALARM_MGR_LOG_PRINT("[alarm-server]:delete alarm: alarm(%d) pid(%d)\n",\
			    alarm_id, pid);
	result = _remove_from_scheduled_alarm_list(pid, alarm_id);

	if (!__alarm_remove_from_list(pid, alarm_id, error_code)) {

		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:delete alarm: "
					  "alarm(%d) pid(%d) has failed with error_code(%d)\n",
					  alarm_id, pid, *error_code);
		return false;
	}

	if (result == true && g_slist_length(g_scheduled_alarm_list) == 0) {
		_alarm_disable_timer(alarm_context);
		_alarm_schedule();
	}

	__rtc_set();

#ifdef __ALARM_BOOT
	/*alarm boot */
	if (enable_power_on_alarm) {
		/* orginally first arg's value was 21(app_id, WAKEUP_ALARM_APP_ID) in a 
		 * platform with app-server.because __alarm_power_on(..) fuction don't 
		 * use first parameter internally, we set this value to 0(zero)
		 */
		__alarm_power_on(0, enable_power_on_alarm, NULL);
	}
#endif

	return true;
}

static bool __alarm_power_on(int app_id, bool on_off, int *error_code)
{
#ifdef __ALARM_BOOT
	time_t min_time = 0;
	time_t current_time = 0;
	struct tm *temp_info;
	struct rtc_time rtc_tm = { 0, };
	struct tm min_time_r;
	int fd = 0;
	int retval;

	enable_power_on_alarm = on_off;
	/*_update_power_on(on_off); */ /*currently its empty*/

	fd = open(power_rtc, O_RDONLY);
	if (fd < 0) {
		ALARM_MGR_EXCEPTION_PRINT("cannot open /dev/rtc0\n");
		return false;
	}

	if (on_off == true) {
		if (_alarm_find_mintime_power_on(&min_time) == true) {

			ab_due_time = min_time;

			min_time = min_time - 60;

			time(&current_time);

			if (min_time <= current_time)
				min_time = current_time + 5;

			temp_info = gmtime_r(&min_time, &min_time_r);

			ALARM_MGR_LOG_PRINT("__alarm_power_on : %d %d %d %d "
						"%d\n", \
					   min_time_r.tm_year,\
					   min_time_r.tm_mon,\
					   min_time_r.tm_mday,\
					   min_time_r.tm_hour,\
					   min_time_r.tm_min);
			ALARM_MGR_LOG_PRINT("__alarm_power_on : %d %d %d %d "
						"%d\n", \
					   min_time_r.tm_year,\
					   min_time_r.tm_mon,\
					   min_time_r.tm_mday,\
					   min_time_r.tm_hour,\
					   min_time_r.tm_min);

			rtc_tm.tm_mday = min_time_r.tm_mday;
			rtc_tm.tm_mon = min_time_r.tm_mon;
			rtc_tm.tm_year = min_time_r.tm_year;
			rtc_tm.tm_hour = min_time_r.tm_hour;
			rtc_tm.tm_min = min_time_r.tm_min;
			rtc_tm.tm_sec = min_time_r.tm_sec;
			/*set_info.time_zone = 0; */
			/*set_info.u_interval.day_of_week = 0; */

			/*ALARM_MGR_LOG_PRINT("####__alarm_power_on : %d %d 
			%d %d %d\n",set_info.year,set_info.month,set_info.day,
			set_info.hour,set_info.minute); */

			ALARM_MGR_LOG_PRINT("due_time : %d \n", ab_due_time);

			ALARM_MGR_LOG_PRINT("\n\nSetted RTC Alarm date/time is "
					    "%d-%d-%d, %02d:%02d:%02d.\n",
					    rtc_tm.tm_mday, rtc_tm.tm_mon + 1,
				rtc_tm.tm_year + 1900, rtc_tm.tm_hour, 
				rtc_tm.tm_min, rtc_tm.tm_sec);

			retval = ioctl(fd, RTC_ALM_SET, &rtc_tm);
			if (retval == -1) {
				if (errno == ENOTTY) {
					ALARM_MGR_EXCEPTION_PRINT(
					"\n...Alarm IRQs not supported.\n");
				}
				ALARM_MGR_EXCEPTION_PRINT("RTC_ALM_SET ioctl");
				close(fd);
				return false;
			}
			ALARM_MGR_LOG_PRINT("[alarm-server]RTC "
					    "alarm(POWER ON) is setted");

			/* Enable alarm interrupts */
			retval = ioctl(fd, RTC_AIE_ON, 0);
			if (retval == -1) {
				ALARM_MGR_EXCEPTION_PRINT(
				    "RTC_AIE_ON ioctl failed");
				close(fd);
				return false;
			}
			ALARM_MGR_LOG_PRINT("[alarm-server]RTC(POWER ON) "
					    "alarm is on");

		} else
			retval = ioctl(fd, RTC_AIE_OFF, 0);
	} else {
		ALARM_MGR_LOG_PRINT("__alarm_power_on : off\n");
		retval = ioctl(fd, RTC_AIE_OFF, 0);
	}

	close(fd);
#endif				/* #ifdef __ALARM_BOOT */

	return true;
}

static bool __alarm_power_off(int app_id, int *error_code)
{
#ifdef __ALARM_BOOT

#endif				/* #ifdef __ALARM_BOOT */
	return true;
}

static bool __alarm_check_next_duetime(int app_id, int *error_code)
{
#ifdef __ALARM_BOOT
	time_t current_time;
	time_t interval;

	time(&current_time);

	interval = ab_due_time - current_time;

	ALARM_MGR_LOG_PRINT("due_time : %d / current_time %d\n", ab_due_time,
			    current_time);

	if (interval > 0 && interval <= 60)
		return true;
	else
		return false;
#else				/* #ifdef __ALARM_BOOT */
	return true;
#endif
}

static void __alarm_send_noti_to_application(const char *app_service_name,
					     alarm_id_t alarm_id)
{


	char service_name[MAX_SERVICE_NAME_LEN];
	char object_name[MAX_SERVICE_NAME_LEN];

	DBusMessage *message;
	DBusMessageIter iter;

	if (app_service_name == NULL || strlen(app_service_name) == 0) {
		ALARM_MGR_EXCEPTION_PRINT("This alarm destination is "
					  "invalid\n");
		return;
	}
	memset(service_name, 0, MAX_SERVICE_NAME_LEN);
	memcpy(service_name, app_service_name, strlen(app_service_name));

	snprintf(object_name, MAX_SERVICE_NAME_LEN,
		 "/org/tizen/alarm/client");

	ALARM_MGR_LOG_PRINT("[alarm server][send expired_alarm(alarm_id=%d)to"
	"app_service_name(%s), object_name(%s), interface_name(%s)]\n",\
	alarm_id, service_name, object_name, "org.tizen.alarm.client");

	message = dbus_message_new_method_call(service_name,
					       object_name,
					       "org.tizen.alarm.client",
					       "alarm_expired");
	if (message == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm server] "
			"dbus_message_new_method_call faild. maybe OOM!.\n");
		ALARM_MGR_EXCEPTION_PRINT("[alarm server] so we cannot "
			"send expired alarm to %s\n", service_name);
		return;
	}

	dbus_message_set_no_reply(message, TRUE);
	/*      if(service_name[0]==':') */
	/* we don't need auto activation in a case that 
	   destination_app_service_name starts with a charactor like (:) */
	dbus_message_set_auto_start(message, FALSE);

	dbus_message_iter_init_append(message, &iter);
	if (!dbus_message_iter_append_basic 
		(&iter, DBUS_TYPE_INT32, &alarm_id)) {
		dbus_message_unref(message);
		ALARM_MGR_EXCEPTION_PRINT("[alarm server] "
		"dbus_message_iter_append_basic faild. maybe OOM!.\n");
		return;
	}

	dbus_connection_send(dbus_g_connection_get_connection
			     (alarm_context.bus), message, NULL);
	dbus_connection_flush(dbus_g_connection_get_connection
			      (alarm_context.bus));
	dbus_message_unref(message);

}

static void __alarm_expired()
{
	const char *destination_app_service_name = NULL;
	alarm_id_t alarm_id = -1;
	int app_pid = 0;
	__alarm_info_t *__alarm_info = NULL;
	char alarm_id_val[32]={0,};
	int b_len = 0;
	bundle *b = NULL;

	ALARM_MGR_LOG_PRINT("[alarm-server]: Enter \n");

	time_t current_time;
	double interval;

	time(&current_time);

	interval = difftime(alarm_context.c_due_time, current_time);
	ALARM_MGR_LOG_PRINT("[alarm-server]: c_due_time(%d), "
		"current_time(%d), interval(%d)\n", alarm_context.c_due_time,
		current_time, interval);

	if (alarm_context.c_due_time > current_time) {
		ALARM_MGR_LOG_PRINT("[alarm-server]: False Alarm\n");
		goto done;
	}

	GSList *iter = NULL;
	__scheduled_alarm_t *alarm = NULL;

	for (iter = g_scheduled_alarm_list; iter != NULL;
	     iter = g_slist_next(iter)) {
		alarm = iter->data;
		alarm_id = alarm->alarm_id;

		__alarm_info = alarm->__alarm_info;

		app_pid = __alarm_info->pid;

		if (strncmp
		    (g_quark_to_string(__alarm_info->quark_bundle),
		     "null", 4) != 0) {

				b_len = strlen(g_quark_to_string(__alarm_info->quark_bundle));

				b = bundle_decode((bundle_raw *)g_quark_to_string(__alarm_info->quark_bundle), b_len);

				if (NULL == b)
				{
					ALARM_MGR_EXCEPTION_PRINT("Error!!!..Unable to decode the bundle!!\n");
				}
				else
				{
					snprintf(alarm_id_val,31,"%d",alarm_id);

					if (bundle_add(b,"http://tizen.org/appcontrol/data/alarm_id", alarm_id_val)){
						ALARM_MGR_EXCEPTION_PRINT("Unable to add alarm id to the bundle\n");
					}
					else
					{
						if ( appsvc_run_service(b, 0, NULL, NULL) < 0)
						{
							ALARM_MGR_EXCEPTION_PRINT("Unable to run app svc\n");
						}
						else
						{
							ALARM_MGR_LOG_PRINT("Successfuly ran app svc\n");
						}
					}
					bundle_free(b);
				}

		}
		else
		{
			if (strncmp
			    (g_quark_to_string(__alarm_info->quark_dst_service_name),
			     "null",4) == 0) {
				ALARM_MGR_LOG_PRINT("[alarm-server]:destination is "
				"null, so we send expired alarm to %s(%u)\n",\
					g_quark_to_string(
					__alarm_info->quark_app_service_name),
					__alarm_info->quark_app_service_name); 
					destination_app_service_name = g_quark_to_string(
					__alarm_info->quark_app_service_name_mod);
			} else {
				ALARM_MGR_LOG_PRINT("[alarm-server]:destination "
						    ":%s(%u)\n",
					g_quark_to_string(
					__alarm_info->quark_dst_service_name), 
					__alarm_info->quark_dst_service_name);
					destination_app_service_name = g_quark_to_string(
						__alarm_info->quark_dst_service_name_mod);
			}

#ifdef __ALARM_BOOT
			/* orginally this code had if(__alarm_info->app_id==21) in a 
			   platform with app-server. */
			/*if(__alarm_info->quark_dst_service_name  == 
			   g_quark_from_string (WAKEUP_ALARM_APP_ID)) */
			if (strcmp
			    (g_quark_to_string(__alarm_info->quark_dst_service_name),
			     WAKEUP_ALARM_APP_ID) == 0) {
				int fd = 0;
				fd = open(power_rtc, O_RDONLY);
				if (fd < 0) {
					ALARM_MGR_LOG_PRINT("cannot open /dev/rtc0\n");
				} else {
					ioctl(fd, RTC_AIE_OFF, 0);
					close(fd);
				}
			}
#endif

			/* 
			 * we should consider a situation that 
			 * destination_app_service_name is owner_name like (:xxxx) and
			 * application's pid which registered this alarm was killed.In that case,
			 * we don't need to send the expire event because the process was killed.
			 * this causes needless message to be sent.
			 */
			ALARM_MGR_LOG_PRINT("[alarm-server]: "
					    "destination_app_service_name :%s, app_pid=%d\n",
					    destination_app_service_name, app_pid);
			/* the following is a code that checks the above situation. 
			   please verify this code. */

			if (dbus_bus_name_has_owner(
			     dbus_g_connection_get_connection(alarm_context.bus),
			     destination_app_service_name, NULL) == FALSE) {
				__expired_alarm_t *expire_info;
				char appid[MAX_SERVICE_NAME_LEN] = { 0, };
				char alarm_id_str[32] = { 0, };

				expire_info = malloc(sizeof(__expired_alarm_t));
				if (G_UNLIKELY(NULL == expire_info)){
					ALARM_MGR_ASSERT_PRINT("[alarm-server]:Malloc failed!Can't notify alarm expiry info\n");
					goto done;
				}
				strncpy(expire_info->service_name,
					destination_app_service_name,
					MAX_SERVICE_NAME_LEN);
				expire_info->alarm_id = alarm_id;
				g_expired_alarm_list =
				    g_slist_append(g_expired_alarm_list, expire_info);


				if (strncmp
			    		(g_quark_to_string(__alarm_info->quark_dst_service_name),
					     "null",4) == 0) {
					strncpy(appid,g_quark_to_string(__alarm_info->quark_app_service_name),strlen(g_quark_to_string(__alarm_info->quark_app_service_name))-6);
				}
				else
				{
					strncpy(appid,g_quark_to_string(__alarm_info->quark_dst_service_name),strlen(g_quark_to_string(__alarm_info->quark_dst_service_name))-6);
				}

				snprintf(alarm_id_str, 31, "%d", alarm_id);

				ALARM_MGR_LOG_PRINT("before aul_launch appid(%s) "
					"alarm_id_str(%s)\n", appid, alarm_id_str);

				bundle *kb;
				kb = bundle_create();
				bundle_add(kb, "__ALARM_MGR_ID", alarm_id_str);
				aul_launch_app(appid, kb);
				bundle_free(kb);
			} else {
				ALARM_MGR_LOG_PRINT(
					"before alarm_send_noti_to_application\n");
				__alarm_send_noti_to_application(
					     destination_app_service_name, alarm_id);
			}
		}
		ALARM_MGR_LOG_PRINT("after __alarm_send_noti_to_application\n");

/*		if( !(__alarm_info->alarm_info.alarm_type 
					& ALARM_TYPE_VOLATILE) ) {
			__alarm_remove_from_list(__alarm_info->pid, 
							alarm_id, NULL);
		}
		else */
		if (__alarm_info->alarm_info.mode.repeat
		    == ALARM_REPEAT_MODE_ONCE) {
/*                      _alarm_next_duetime(__alarm_info);*/
/*                      _update_alarms(__alarm_info);*/
			__alarm_remove_from_list(__alarm_info->pid, alarm_id,
						 NULL);
		} else {

			_alarm_next_duetime(__alarm_info);
/*                      _update_alarms(__alarm_info);*/
		}

	}

 done:
	_clear_scheduled_alarm_list();
	alarm_context.c_due_time = -1;

	ALARM_MGR_LOG_PRINT("[alarm-server]: Leave  \n");
}

static gboolean __alarm_handler_idle()
{
	if (g_dummy_timer_is_set == true) {
		ALARM_MGR_LOG_PRINT("dummy alarm timer has expired\n");
	} else {
		ALARM_MGR_LOG_PRINT("__alarm_handler \n");
		ALARM_MGR_LOG_PRINT("__alarm_handler \n");

		__alarm_expired();

	}

	_alarm_schedule();

	__rtc_set();

#ifdef __ALARM_BOOT
	/*alarm boot */
	if (enable_power_on_alarm) {
		/* orginally first arg's value was 21(app_id, WAKEUP_ALARM_APP_ID) 
		 *in a platform with app-server.because __alarm_power_on(..) fuction 
		 *don't use first parameter internally, we set this value to 0(zero)
		 */
		__alarm_power_on(0, enable_power_on_alarm, NULL);
	}
#endif
	pm_unlock_state(LCD_OFF, PM_SLEEP_MARGIN);
	return false;

}

static void __alarm_handler(int sigNum, siginfo_t *pSigInfo, void *pUContext)
{

	pm_lock_state(LCD_OFF, STAY_CUR_STATE, 0);

	/* we moved __alarm_expired() function to __alarm_handler_idle GSource
	   because of signal safety. */
	g_idle_add_full(G_PRIORITY_HIGH_IDLE, __alarm_handler_idle, NULL, NULL);
}

static void __clean_registry()
{

	/*TODO:remove all db entries */
}

static bool __alarm_manager_reset()
{

	_alarm_disable_timer(alarm_context);

	__alarm_clean_list();

	_clear_scheduled_alarm_list();
	__clean_registry();

	return true;
}

static void __on_system_time_changed(keynode_t *node, void *data)
{
	double diff_time;
	time_t _time;
	time_t before;

	_alarm_disable_timer(alarm_context);

	if (node) {
		_time = vconf_keynode_get_int(node);
	} else {
		vconf_get_int(VCONFKEY_SYSTEM_TIMECHANGE, (int *)&_time);
	}

	time(&before);
	diff_time = difftime(_time, before);

	tzset();

	ALARM_MGR_ASSERT_PRINT("diff_time is %f\n", diff_time);

	ALARM_MGR_LOG_PRINT("[alarm-server] System time has been changed\n");
	ALARM_MGR_LOG_PRINT("1.alarm_context.c_due_time is %d\n",
			    alarm_context.c_due_time);

	_set_time(_time);

	vconf_set_dbl(VCONFKEY_SYSTEM_TIMEDIFF, diff_time);
	vconf_set_int(VCONFKEY_SYSTEM_TIME_CHANGED,(int)diff_time);

	if (heynoti_publish(SYSTEM_TIME_CHANGED))
		ALARM_MGR_EXCEPTION_PRINT("alarm-server: Unable to publish heynoti for system time change\n");

	__alarm_update_due_time_of_all_items_in_list(diff_time);

	ALARM_MGR_LOG_PRINT("2.alarm_context.c_due_time is %d\n",
			    alarm_context.c_due_time);
	_clear_scheduled_alarm_list();
	_alarm_schedule();
	__rtc_set();
#ifdef __ALARM_BOOT
	/*alarm boot */
	if (enable_power_on_alarm) {
/* orginally first arg's value was 21(app_id, WAKEUP_ALARM_
APP_ID) in a platform with app-server. because _alarm_power_
on(..) fuction don't use first parameter internally, we set 
this value to 0(zero)
*/
		__alarm_power_on(0, enable_power_on_alarm, NULL);
	}
#endif
	return;
}

gboolean alarm_manager_alarm_set_rtc_time(void *pObject, int pid,
				int year, int mon, int day,
				int hour, int min, int sec, char *e_cookie,
				int *return_code){
	guchar *cookie = NULL;
	gsize size;
	int retval = 0;
	gboolean result = true;
	gid_t call_gid;

	const char *rtc = power_rtc;
	int fd = 0;
	struct rtc_time rtc_tm = {0,};
	struct rtc_wkalrm rtc_wk;
	struct tm *alarm_tm = NULL;
	struct tm due_tm = {0,};


	if (return_code){
		*return_code = ALARMMGR_RESULT_SUCCESS;
	}

	cookie = g_base64_decode(e_cookie, &size);
	if (NULL == cookie)
	{
		if (return_code)
			*return_code = ERR_ALARM_NO_PERMISSION;
		ALARM_MGR_EXCEPTION_PRINT("Unable to decode cookie!!!\n");
		return true;
	}

	call_gid = security_server_get_gid("alarm");

	ALARM_MGR_LOG_PRINT("call_gid : %d\n", call_gid);

	retval = security_server_check_privilege((const char *)cookie, call_gid);
	if (retval < 0) {
		if (retval == SECURITY_SERVER_API_ERROR_ACCESS_DENIED) {
			ALARM_MGR_EXCEPTION_PRINT(
				"%s", "access has been denied\n");
		}
		ALARM_MGR_EXCEPTION_PRINT("Error has occurred in security_server_check_privilege()\n");
		if (return_code)
			*return_code = ERR_ALARM_NO_PERMISSION;
	}
	else {

		/*extract day of the week, day in the year &
		daylight saving time from system*/
		time_t ctime;
		ctime = time(NULL);
		alarm_tm = localtime(&ctime);

		alarm_tm->tm_year = year;
		alarm_tm->tm_mon = mon;
		alarm_tm->tm_mday = day;
		alarm_tm->tm_hour = hour;
		alarm_tm->tm_min = min;
		alarm_tm->tm_sec = sec;

		/*convert to calendar time representation*/
		time_t rtc_time = mktime(alarm_tm);

		/*convert to Coordinated Universal Time (UTC)*/
		gmtime_r(&rtc_time, &due_tm);

		fd = open(rtc, O_RDONLY);
		if (fd == -1) {
			ALARM_MGR_EXCEPTION_PRINT("RTC open failed.\n");
			if (return_code)
				*return_code = ERR_ALARM_SYSTEM_FAIL;
			return result;
		}

		/* Read the RTC time/date */
		retval = ioctl(fd, RTC_RD_TIME, &rtc_tm);
		if (retval == -1) {
			ALARM_MGR_EXCEPTION_PRINT("RTC_RD_TIME ioctl failed");
			close(fd);
			if (return_code)
				*return_code = ERR_ALARM_SYSTEM_FAIL;
			return result;
		}

		rtc_tm.tm_mday = due_tm.tm_mday;
		rtc_tm.tm_mon = due_tm.tm_mon;
		rtc_tm.tm_year = due_tm.tm_year;
		rtc_tm.tm_hour = due_tm.tm_hour;
		rtc_tm.tm_min = due_tm.tm_min;
		rtc_tm.tm_sec = due_tm.tm_sec;

		memcpy(&rtc_wk.time, &rtc_tm, sizeof(rtc_tm));

		rtc_wk.enabled = 1;
		rtc_wk.pending = 0;

		retval = ioctl(fd, RTC_WKALM_BOOT_SET, &rtc_wk);
		if (retval == -1) {
			if (errno == ENOTTY) {
				ALARM_MGR_EXCEPTION_PRINT("\nAlarm IRQs not"
							  "supported.\n");
			}
			ALARM_MGR_EXCEPTION_PRINT("RTC_ALM_SET ioctl");
			close(fd);
			if (return_code)
				*return_code = ERR_ALARM_SYSTEM_FAIL;
		}
		else{
			ALARM_MGR_LOG_PRINT("[alarm-server]RTC alarm is setted");
			/* Enable alarm interrupts */
			retval = ioctl(fd, RTC_AIE_ON, 0);
			if (retval == -1) {
				ALARM_MGR_EXCEPTION_PRINT("RTC_AIE_ON ioctl failed");
				if (return_code)
					*return_code = ERR_ALARM_SYSTEM_FAIL;
			}
			close(fd);
		}
	}

	if (cookie){
		g_free(cookie);
		cookie = NULL;
	}

	return result;

}

gboolean alarm_manager_alarm_create_appsvc(void *pObject, int pid,
				    int start_year,
				    int start_month, int start_day,
				    int start_hour, int start_min,
				    int start_sec, int end_year, int end_month,
				    int end_day, int mode_day_of_week,
				    int mode_repeat, int alarm_type,
				    int reserved_info,
				    char *bundle_data, char *e_cookie,
				    int *alarm_id, int *return_code)
{
	alarm_info_t alarm_info;
	guchar *cookie = NULL;
	gsize size;
	int retval = 0;
	gid_t call_gid;
	gboolean result = true;

	alarm_info.start.year = start_year;
	alarm_info.start.month = start_month;
	alarm_info.start.day = start_day;
	alarm_info.start.hour = start_hour;
	alarm_info.start.min = start_min;
	alarm_info.start.sec = start_sec;

	alarm_info.end.year = end_year;
	alarm_info.end.month = end_month;
	alarm_info.end.day = end_day;

	alarm_info.mode.u_interval.day_of_week = mode_day_of_week;
	alarm_info.mode.repeat = mode_repeat;

	alarm_info.alarm_type = alarm_type;
	alarm_info.reserved_info = reserved_info;

	*return_code = 0;

	cookie = g_base64_decode(e_cookie, &size);
	if (NULL == cookie)
	{
		*return_code = -1;
		ALARM_MGR_EXCEPTION_PRINT("Unable to decode cookie!!!\n");
		return false;
	}
	
	call_gid = security_server_get_gid("alarm");

	ALARM_MGR_LOG_PRINT("call_gid : %d\n", call_gid);

	retval = security_server_check_privilege((const char *)cookie, call_gid);
	if (retval < 0) {
		if (retval == SECURITY_SERVER_API_ERROR_ACCESS_DENIED) {
			ALARM_MGR_EXCEPTION_PRINT(
				"%s", "access has been denied\n");
		}
		ALARM_MGR_EXCEPTION_PRINT("Error has occurred in security_server_check_privilege()\n");
		*return_code = -1;
		result = false;
	}
	else {
		result = __alarm_create_appsvc(&alarm_info, alarm_id, pid,
			       bundle_data, return_code);
		if (false == result)
		{
			ALARM_MGR_EXCEPTION_PRINT("Unable to create alarm!\n");
		}
	}

	if (cookie){
		g_free(cookie);
		cookie = NULL;
	}

	return result;
}

gboolean alarm_manager_alarm_create(void *pObject, int pid,
				    char *app_service_name, char *app_service_name_mod,  int start_year,
				    int start_month, int start_day,
				    int start_hour, int start_min,
				    int start_sec, int end_year, int end_month,
				    int end_day, int mode_day_of_week,
				    int mode_repeat, int alarm_type,
				    int reserved_info,
				    char *reserved_service_name, char *reserved_service_name_mod, char *e_cookie,
				    int *alarm_id, int *return_code)
{
	alarm_info_t alarm_info;
	guchar *cookie;
	gsize size;
	int retval;
	gid_t call_gid;

	alarm_info.start.year = start_year;
	alarm_info.start.month = start_month;
	alarm_info.start.day = start_day;
	alarm_info.start.hour = start_hour;
	alarm_info.start.min = start_min;
	alarm_info.start.sec = start_sec;

	alarm_info.end.year = end_year;
	alarm_info.end.month = end_month;
	alarm_info.end.day = end_day;

	alarm_info.mode.u_interval.day_of_week = mode_day_of_week;
	alarm_info.mode.repeat = mode_repeat;

	alarm_info.alarm_type = alarm_type;
	alarm_info.reserved_info = reserved_info;

	*return_code = 0;

	cookie = g_base64_decode(e_cookie, &size);
	call_gid = security_server_get_gid("alarm");

	ALARM_MGR_LOG_PRINT("call_gid : %d\n", call_gid);

	retval = security_server_check_privilege((const char *)cookie, call_gid);
	if (retval < 0) {
		if (retval == SECURITY_SERVER_API_ERROR_ACCESS_DENIED) {
			ALARM_MGR_EXCEPTION_PRINT(
				"%s", "access has been denied\n");
		}
		ALARM_MGR_EXCEPTION_PRINT("%s", "Error has occurred\n");

		*return_code = -1;
	}

	else {
		/* return valule and return_code should be checked */
		__alarm_create(&alarm_info, alarm_id, pid, app_service_name,app_service_name_mod,
			       reserved_service_name, reserved_service_name_mod, return_code);
	}

	g_free(cookie);

	return true;
}

gboolean alarm_manager_alarm_delete(void *pObject, int pid, alarm_id_t alarm_id,
				    char *e_cookie, int *return_code)
{
	guchar *cookie;
	gsize size;
	int retval;
	gid_t call_gid;

	cookie = g_base64_decode(e_cookie, &size);
	call_gid = security_server_get_gid("alarm");

	ALARM_MGR_LOG_PRINT("call_gid : %d\n", call_gid);

	retval = security_server_check_privilege((const char *)cookie, call_gid);
	if (retval < 0) {
		if (retval == SECURITY_SERVER_API_ERROR_ACCESS_DENIED) {
			ALARM_MGR_EXCEPTION_PRINT("%s",
						  "access has been denied\n");
		}
		ALARM_MGR_EXCEPTION_PRINT("%s", "Error has occurred\n");

		*return_code = -1;
	}

	else {
		__alarm_delete(pid, alarm_id, return_code);
	}

	g_free(cookie);

	return true;
}

gboolean alarm_manager_alarm_power_on(void *pObject, int pid, bool on_off,
				      int *return_code)
{

	return __alarm_power_on(pid, on_off, return_code);
}

gboolean alarm_manager_alarm_power_off(void *pObject, int pid, int *return_code)
{

	return __alarm_power_off(pid, return_code);
}

bool alarm_manager_alarm_check_next_duetime(void *pObject, int pid,
					    int *return_code)
{
	return __alarm_check_next_duetime(pid, return_code);
}

gboolean alarm_manager_alarm_update(void *pObject, int pid,
				    char *app_service_name, alarm_id_t alarm_id,
				    int start_year, int start_month,
				    int start_day, int start_hour,
				    int start_min, int start_sec, int end_year,
				    int end_month, int end_day,
				    int mode_day_of_week, int mode_repeat,
				    int alarm_type, int reserved_info,
				    int *return_code)
{
	alarm_info_t alarm_info;
	alarm_info.start.year = start_year;
	alarm_info.start.month = start_month;
	alarm_info.start.day = start_day;
	alarm_info.start.hour = start_hour;
	alarm_info.start.min = start_min;
	alarm_info.start.sec = start_sec;

	alarm_info.end.year = end_year;
	alarm_info.end.month = end_month;
	alarm_info.end.day = end_day;

	alarm_info.mode.u_interval.day_of_week = mode_day_of_week;
	alarm_info.mode.repeat = mode_repeat;

	alarm_info.alarm_type = alarm_type;
	alarm_info.reserved_info = reserved_info;

	*return_code = 0;

	__alarm_update(pid, app_service_name, alarm_id, &alarm_info,
		       return_code);

	return true;
}

gboolean alarm_manager_alarm_get_number_of_ids(void *pObject, int pid,
					       int *num_of_ids,
					       int *return_code)
{
	GSList *gs_iter = NULL;
	GQuark quark_app_unique_name;	/* the fullpath of pid(pid) is 
					   converted  to quark value. */
	char proc_file[256] = { 0 };
	char process_name[512] = { 0 };
	char app_name[256] = { 0 };
	char *word = NULL;
	__alarm_info_t *entry = NULL;
	char *proc_name_ptr = NULL;

	*num_of_ids = 0;
	*return_code = 0;

	/* we should consider to check whether  pid is running or Not
	 */
	memset(process_name, '\0', 512);
	memset(proc_file, '\0', 256);
	snprintf(proc_file, 256, "/proc/%d/cmdline", pid);

	int fd;
	int ret;
	int i = 0;
	fd = open(proc_file, O_RDONLY);
	if (fd > 0) {
		ret = read(fd, process_name, 512);
		close(fd);
		while (process_name[i] != '\0') {
			if (process_name[i] == ' ') {
				process_name[i] = '\0';
				break;
			}
			i++;
		}
		/*if (readlink(proc_file, process_name, 256)!=-1) */
		/*success */

		word = strtok_r(process_name, "/", &proc_name_ptr);
		while (word != NULL) {
			memset(app_name, 0, 256);
			snprintf(app_name, 256, "%s", word);
			word = strtok_r(NULL, "/", &proc_name_ptr);
		}
		quark_app_unique_name = g_quark_from_string(app_name);
	} else {		/* failure */

		quark_app_unique_name = g_quark_from_string("unknown");
		memcpy(app_name, "unknown", strlen("unknown") + 1);

		ALARM_MGR_EXCEPTION_PRINT("Caution!! app_pid(%d) seems to be "
					  "killed, so we failed to get proc file(%s) \n",
					  pid, proc_file);
		*return_code = -1;	/* -1 means that system 
					   failed internally. */
		return true;
	}

	ALARM_MGR_LOG_PRINT("called for  app(pid:%d, name=%s)\n",
			    pid, app_name);

	for (gs_iter = alarm_context.alarms; gs_iter != NULL;
	     gs_iter = g_slist_next(gs_iter)) {
		entry = gs_iter->data;
		ALARM_MGR_LOG_PRINT("alarm_manager_alarm_get_number_of_ids(): "
				    "app_name=%s,quark_app_unique_name=%s\n",
		app_name, g_quark_to_string(entry->quark_app_unique_name));
		if (strcmp
		    (app_name,
		     g_quark_to_string(entry->quark_app_unique_name)) == 0
		    && strcmp(app_name, "unknown") != 0) {
			(*num_of_ids)++;
			ALARM_MGR_LOG_PRINT("inc number of alarms of app "
					    "(pid:%d, name:%s) is %d\n",
					    pid, app_name, *num_of_ids);
		}
	}
	*return_code = 0;
	ALARM_MGR_LOG_PRINT("number of alarms of app(pid:%d, name:%s) is %d\n",
			    pid, app_name, *num_of_ids);
	return true;
}

gboolean alarm_manager_alarm_get_list_of_ids(void *pObject, int pid,
					     int max_number_of_ids,
					     GArray **arr, int *num_of_ids,
					     int *return_code)
{
	GSList *gs_iter = NULL;
	GQuark quark_app_unique_name;	/* the fullpath of pid(pid) is converted
					   to quark value. */
	char proc_file[256] = { 0 };
	char process_name[512] = { 0 };
	char app_name[256] = { 0 };
	char *word = NULL;
	__alarm_info_t *entry = NULL;
	int index = 0;
	char *proc_name_ptr = NULL;
	int fd;
	int ret;
	int i = 0;
	GArray *garray = NULL;

	*return_code = 0;

	garray = g_array_new(false, true, sizeof(alarm_id_t));

	/* we should check that there is a resource leak.
	 * Now we don't have free code for g_array_new().
	 */
	if (max_number_of_ids <= 0) {
		*arr = garray;
		ALARM_MGR_EXCEPTION_PRINT("called for  pid(%d), but "
					  "max_number_of_ids(%d) is less than 0.\n",
					  pid, max_number_of_ids);
		return true;
	}

	/* we should consider to check whether  pid is running or Not
	 */
	memset(process_name, '\0', 512);
	memset(proc_file, '\0', 256);
	snprintf(proc_file, 256, "/proc/%d/cmdline", pid);

	fd = open(proc_file, O_RDONLY);
	if (fd > 0) {
		ret = read(fd, process_name, 512);
		close(fd);
		while (process_name[i] != '\0') {
			if (process_name[i] == ' ') {
				process_name[i] = '\0';
				break;
			}
			i++;
		}
		/* if (readlink(proc_file, process_name, 256)!=-1) */
		/*success */

		word = strtok_r(process_name, "/", &proc_name_ptr);
		while (word != NULL) {
			memset(app_name, 0, 256);
			snprintf(app_name, 256, "%s", word);
			word = strtok_r(NULL, "/", &proc_name_ptr);
		}
		quark_app_unique_name = g_quark_from_string(app_name);
	} else {		/* failure */

		quark_app_unique_name = g_quark_from_string("unknown");
		memcpy(app_name, "unknown", strlen("unknown") + 1);

		ALARM_MGR_EXCEPTION_PRINT("Caution!! app_pid(%d) seems to be "
		"killed, so we failed to get proc file(%s)\n", pid, proc_file);
		*return_code = -1;
		/* -1 means that system failed internally. */
		return true;
	}

	ALARM_MGR_LOG_PRINT("called for  app(pid:%d, name=%s)\n",
			    pid, app_name);

	for (gs_iter = alarm_context.alarms; gs_iter != NULL;
	     gs_iter = g_slist_next(gs_iter)) {
		entry = gs_iter->data;
		if (strcmp
		    (app_name,
		     g_quark_to_string(entry->quark_app_unique_name)) == 0
		    && strcmp(app_name, "unknown") != 0) {
			g_array_append_val(garray, entry->alarm_id);
		}
	}

	*num_of_ids = index;

	*arr = garray;

	return true;
}

gboolean alarm_manager_alarm_get_appsvc_info(void *pObject, int pid, alarm_id_t alarm_id,
				char *e_cookie, gchar **b_data, int *return_code)
{
	bool found = false;

	GSList *gs_iter = NULL;
	__alarm_info_t *entry = NULL;

	guchar *cookie = NULL;
	gsize size;
	int retval = 0;
	gid_t call_gid;

	ALARM_MGR_LOG_PRINT("called for  pid(%d) and alarm_id(%d)\n", pid,
			    alarm_id);

	cookie = g_base64_decode(e_cookie, &size);
	if (NULL == cookie)
	{
		if (return_code)
			*return_code = ERR_ALARM_SYSTEM_FAIL;
		ALARM_MGR_EXCEPTION_PRINT("Unable to decode cookie!!!\n");
		return true;
	}
	call_gid = security_server_get_gid("alarm");

	ALARM_MGR_LOG_PRINT("call_gid : %d\n", call_gid);

	retval = security_server_check_privilege((const char *)cookie, call_gid);
	if (retval < 0) {
		if (retval == SECURITY_SERVER_API_ERROR_ACCESS_DENIED) {
			ALARM_MGR_EXCEPTION_PRINT(
				"%s", "access has been denied\n");
		}
		ALARM_MGR_EXCEPTION_PRINT("%s", "Error has occurred\n");

		if (return_code)
			*return_code = ERR_ALARM_NO_PERMISSION;

		if (cookie)
			g_free(cookie);

		return true;
	}

	if (return_code)
		*return_code = 0;

	for (gs_iter = alarm_context.alarms; gs_iter != NULL;
	     gs_iter = g_slist_next(gs_iter)) {
		entry = gs_iter->data;
		if (entry->alarm_id == alarm_id) {
			found = true;
			*b_data = g_strdup(g_quark_to_string(entry->quark_bundle));
			break;
		}
	}

	if (found) {
		if ( *b_data && strlen(*b_data) == 4 && strncmp(*b_data,"null",4) == 0){
			ALARM_MGR_EXCEPTION_PRINT("Regular alarm,not svc alarm");
			if (return_code)
				*return_code = ERR_ALARM_INVALID_TYPE;
		}
	} else {
		if (return_code)
			*return_code = ERR_ALARM_INVALID_ID;
	}

	if (cookie)
		g_free(cookie);

	return true;
}

gboolean alarm_manager_alarm_get_info(void *pObject, int pid,
				      alarm_id_t alarm_id, int *start_year,
				      int *start_month, int *start_day,
				      int *start_hour, int *start_min,
				      int *start_sec, int *end_year,
				      int *end_month, int *end_day,
				      int *mode_day_of_week, int *mode_repeat,
				      int *alarm_type, int *reserved_info,
				      int *return_code)
{
	ALARM_MGR_LOG_PRINT("called for  pid(%d) and alarm_id(%d)\n", pid,
			    alarm_id);

	GSList *gs_iter = NULL;
	__alarm_info_t *entry = NULL;

	alarm_info_t *alarm_info = NULL;
	*return_code = 0;

	for (gs_iter = alarm_context.alarms; gs_iter != NULL;
	     gs_iter = g_slist_next(gs_iter)) {
		entry = gs_iter->data;
		if (entry->alarm_id == alarm_id) {
			alarm_info = &(entry->alarm_info);
			break;
		}
	}

	if (alarm_info == NULL)
	{
		ALARM_MGR_EXCEPTION_PRINT("alarm id(%d) was not found\n",
					  alarm_id);
		*return_code = ERR_ALARM_INVALID_ID;
	} else {
		ALARM_MGR_LOG_PRINT("alarm was found\n");
		*start_year = alarm_info->start.year;
		*start_month = alarm_info->start.month;
		*start_day = alarm_info->start.day;
		*start_hour = alarm_info->start.hour;
		*start_min = alarm_info->start.min;
		*start_sec = alarm_info->start.sec;

		*end_year = alarm_info->end.year;
		*end_year = alarm_info->end.month;
		*end_year = alarm_info->end.day;

		*mode_day_of_week = alarm_info->mode.u_interval.day_of_week;
		*mode_repeat = alarm_info->mode.repeat;

		*alarm_type = alarm_info->alarm_type;
		*reserved_info = alarm_info->reserved_info;

		*return_code = 0;
	}
	return true;
}

#include "alarm-skeleton.h"

typedef struct AlarmManagerObject AlarmManagerObject;
typedef struct AlarmManagerObjectClass AlarmManagerObjectClass;
GType Server_Object_get_type(void);
struct AlarmManagerObject {
	GObject parent;
};
struct AlarmManagerObjectClass {
	GObjectClass parent;
};

#define DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT 0

#define ALARM_MANAGER_TYPE_OBJECT              (Server_Object_get_type())
#define ALARM_MANAGER_OBJECT(object)           (G_TYPE_CHECK_INSTANCE_CAST \
((object), ALARM_MANAGER_TYPE_OBJECT, AlarmManagerObject))
#define ALARM_MANAGER_OBJECT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST \
((klass), ALARM_MANAGER_TYPE_OBJECT, AlarmManagerObjectClass))
#define ALARM_MANAGER_IS_OBJECT(object)        (G_TYPE_CHECK_INSTANCE_TYPE \
((object), ALARM_MANAGER_TYPE_OBJECT))
#define ALARM_MANAGER_IS_OBJECT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE \
((klass), ALARM_MANAGER_TYPE_OBJECT))
#define ALARM_MANAGER_OBJECT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS \
((obj), ALARM_MANAGER_TYPE_OBJECT, AlarmManagerObjectClass))
G_DEFINE_TYPE(AlarmManagerObject, Server_Object, G_TYPE_OBJECT)
static void Server_Object_init(AlarmManagerObject * obj)
{
	;
}

static void Server_Object_class_init(AlarmManagerObjectClass *klass)
{
	;
}

static void __initialize_timer()
{
	struct sigaction sig_timer;
	sigemptyset(&sig_timer.sa_mask);
	sig_timer.sa_flags = SA_SIGINFO;
	sig_timer.sa_sigaction = (void *)__alarm_handler;
	sigaction(SIG_TIMER, &sig_timer, NULL);

	alarm_context.timer = _alarm_create_timer();

}

static void __initialize_alarm_list()
{

	alarm_context.alarms = NULL;
	alarm_context.c_due_time = -1;

	_load_alarms_from_registry();

	__rtc_set();	/*Set RTC1 Alarm with alarm due time for alarm-manager initialization*/

	/*alarm boot */
#ifdef __ALARM_BOOT
	/*alarm boot */
	if (enable_power_on_alarm) {
		/* orginally first arg's value was 21(app_id, WAKEUP_ALARM_APP_ID) in a
		 * platform with app-server. because __alarm_power_on(..) fuction don't 
		 * use first parameter internally, we set this value to 0(zero)
		 */
		__alarm_power_on(0, enable_power_on_alarm, NULL);
	}
#endif
}

static void __initialize_scheduled_alarm_lsit()
{
	_init_scheduled_alarm_list();
}


static void __hibernation_leave_callback()
{

	__initialize_scheduled_alarm_lsit();

	__alarm_clean_list();

	__initialize_alarm_list();
}

static bool __initialize_noti()
{

	int fd = heynoti_init();
	if (fd < 0) {
		ALARM_MGR_EXCEPTION_PRINT("fail to heynoti_init\n");
		return false;
	}
	heynoti_subscribe(fd, "HIBERNATION_LEAVE", __hibernation_leave_callback,
			  NULL);
	heynoti_attach_handler(fd);

	if (vconf_notify_key_changed
	    (VCONFKEY_SYSTEM_TIMECHANGE, __on_system_time_changed, NULL) < 0) {
		ALARM_MGR_LOG_PRINT(
			"Failed to add callback for time changing event\n");
	}
	/*system state change noti 贸府 */

	return true;
}


static DBusHandlerResult __alarm_server_filter(DBusConnection *connection,
					       DBusMessage *message,
					       void *user_data)
{

	if (dbus_message_is_signal
	    (message, DBUS_INTERFACE_DBUS, "NameOwnerChanged")) {
		char *service;
		char *old_owner;
		char *new_owner;
		GSList *entry;
		__expired_alarm_t *expire_info;

		dbus_message_get_args(message,
				      NULL,
				      DBUS_TYPE_STRING, &service,
				      DBUS_TYPE_STRING, &old_owner,
				      DBUS_TYPE_STRING, &new_owner,
				      DBUS_TYPE_INVALID);

		for (entry = g_expired_alarm_list; entry; entry = entry->next) {
			if (entry->data) {
				expire_info = (__expired_alarm_t *) entry->data;

				if (strcmp(expire_info->service_name, service)
				    == 0) {
					ALARM_MGR_EXCEPTION_PRINT(
					"__alarm_server_filter : "
					     "service name(%s) alarm_id (%d)\n",
					     expire_info->service_name,\
					     expire_info->alarm_id);

					__alarm_send_noti_to_application(
					     expire_info->service_name,
					     expire_info->alarm_id);
					g_expired_alarm_list =
					    g_slist_remove(g_expired_alarm_list,
							   entry->data);
					free(expire_info);
				}
			}
		}

		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static bool __initialize_dbus()
{
	GError *error = NULL;
	GObject *obj = NULL;
	DBusGConnection *connection = NULL;
	DBusError derror;
	int request_name_result = 0;

	dbus_g_object_type_install_info(ALARM_MANAGER_TYPE_OBJECT,
					&dbus_glib_alarm_manager_object_info);

	connection = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
	if (!connection) {

		ALARM_MGR_EXCEPTION_PRINT("dbus_g_bus_get failed\n");
		return false;
	}

	dbus_error_init(&derror);

	request_name_result =
	    dbus_bus_request_name(dbus_g_connection_get_connection(connection),
				  "org.tizen.alarm.manager",
				  DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT, &derror);
	if (dbus_error_is_set(&derror)) {	/* failure */
		ALARM_MGR_EXCEPTION_PRINT("Failed to dbus_bus_request_name "
		"(org.tizen.alarm.manager): %s\n", derror.message);
		dbus_error_free(&derror);
		return false;
	}

	if (!(obj = g_object_new(ALARM_MANAGER_TYPE_OBJECT, NULL))) {
		ALARM_MGR_EXCEPTION_PRINT("Could not allocate new object\n");
		return false;
	}

	dbus_g_connection_register_g_object(connection,
					    "/org/tizen/alarm/manager",
					    G_OBJECT(obj));

	/*dbus_bus_add_match (dbus_g_connection_get_connection(connection),
	   "type='signal',member='NameOwnerChanged'",NULL); */

	dbus_bus_add_match(dbus_g_connection_get_connection(connection),
			   "type='signal',sender='" DBUS_SERVICE_DBUS
			   "',path='" DBUS_PATH_DBUS
			   "',interface='" DBUS_INTERFACE_DBUS
			   "',member='NameOwnerChanged'", NULL);

	if (!dbus_connection_add_filter
	    (dbus_g_connection_get_connection(connection),
	     __alarm_server_filter, NULL, NULL)) {
		ALARM_MGR_EXCEPTION_PRINT("add __expire_alarm_filter failed\n");

		return false;
	}

	alarm_context.bus = connection;
	return true;
}

#define ALARMMGR_DB_FILE "/opt/dbspace/.alarmmgr.db"
sqlite3 *alarmmgr_db;
#define QUERY_CREATE_TABLE_ALARMMGR "create table alarmmgr \
				(alarm_id integer primary key,\
						start integer,\
						end integer,\
						pid integer,\
						app_unique_name text,\
						app_service_name text,\
						app_service_name_mod text,\
						bundle text, \
						year integer,\
						month integer,\
						day integer,\
						hour integer,\
						min integer,\
						sec integer,\
						day_of_week integer,\
						repeat integer,\
						alarm_type integer,\
						reserved_info integer,\
						dst_service_name text, \
						dst_service_name_mod text \
						)"

static bool __initialize_db()
{
	char *error_message = NULL;
	int ret;

	if (access("/opt/dbspace/.alarmmgr.db", F_OK) == 0) {
		ret =
		    db_util_open(ALARMMGR_DB_FILE, &alarmmgr_db,
				 DB_UTIL_REGISTER_HOOK_METHOD);

		if (ret != SQLITE_OK) {
			ALARM_MGR_EXCEPTION_PRINT(
			    "====>>>> connect menu_db [%s] failed!\n",
			     ALARMMGR_DB_FILE);
			return false;
		}

		return true;
	}

	ret =
	    db_util_open(ALARMMGR_DB_FILE, &alarmmgr_db,
			 DB_UTIL_REGISTER_HOOK_METHOD);

	if (ret != SQLITE_OK) {
		ALARM_MGR_EXCEPTION_PRINT(
		    "====>>>> connect menu_db [%s] failed!\n",
		     ALARMMGR_DB_FILE);
		return false;
	}

	if (SQLITE_OK !=
	    sqlite3_exec(alarmmgr_db, QUERY_CREATE_TABLE_ALARMMGR, NULL, NULL,\
			 &error_message)) {
		ALARM_MGR_EXCEPTION_PRINT("Don't execute query = %s, "
		"error message = %s\n", QUERY_CREATE_TABLE_ALARMMGR, 
					  error_message);
		return false;
	}

	return true;
}

static void __initialize()
{

	g_type_init();
#ifdef __ALARM_BOOT
	FILE *fp;
	char temp[2];
	int size;

	fp = fopen("/proc/alarm_boot", "r");
	if (fp == NULL)
		alarm_boot = 0;
	else {

		size = fread(&temp, 1, 1, fp);
		if (size != 1)
			alarm_boot = 0;
		else {
			temp[1] = 0;
			alarm_boot = atoi(temp);
		}
		fclose(fp);
	}
#endif

	int fd;
	int fd2;
	struct rtc_time rtc_tm;
	int retval;

	fd = open(power_rtc, O_RDONLY);
	if (fd < 0) {
		ALARM_MGR_EXCEPTION_PRINT("cannot open /dev/rtc0\n");
		return;
	}
	retval = ioctl(fd, RTC_RD_TIME, &rtc_tm);
	close(fd);

	fd2 = open(default_rtc, O_RDWR);
	if (fd2 < 0) {
		ALARM_MGR_EXCEPTION_PRINT("cannot open /dev/rtc1\n");
		return;
	}
	retval = ioctl(fd2, RTC_SET_TIME, &rtc_tm);
	close(fd2);

	__initialize_timer();
	if (__initialize_dbus() == false) {	/* because dbus's initialize 
					failed, we cannot continue any more. */
		ALARM_MGR_EXCEPTION_PRINT("because __initialize_dbus failed, "
					  "alarm-server cannot be runned.\n");
		exit(1);
	}
	__initialize_scheduled_alarm_lsit();
	__initialize_db();
	__initialize_alarm_list();
	__initialize_noti();

}

#ifdef __ALARM_BOOT
static bool __check_false_alarm()
{
	time_t current_time;
	time_t interval;

	time(&current_time);

	interval = ab_due_time - current_time;

	ALARM_MGR_LOG_PRINT("due_time : %d / current_time %d\n", \
			    alarm_context.c_due_time, current_time);

	if (interval > 0 && interval <= 30) {
		return true;
	} else if (!poweron_alarm_expired) {
		/* originally, first arguement's value was 121(app_id) which means 
		 * alarm_booting ui application.and this application's dbus-service 
		 * name had a org.tizen.alarmboot.ui in a platform with app-server.
		 * so we set "org.tizen.alarmboot.ui.ALARM" instead of 121.
		 */
		__alarm_send_noti_to_application(
			WAKEUP_ALARMBOOTING_APP_ID, -1);
		return false;
	}

	return true;
}
#endif

int main()
{
	GMainLoop *mainloop = NULL;

	ALARM_MGR_LOG_PRINT("Enter main loop\n");

	mainloop = g_main_loop_new(NULL, FALSE);

	__initialize();

#ifdef __ALARM_BOOT
	if (alarm_boot){
		__check_false_alarm();
	}
#endif

	g_main_loop_run(mainloop);

	return 0;
}
