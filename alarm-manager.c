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
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <stdint.h>

#include <glib.h>
#if !GLIB_CHECK_VERSION (2, 31, 0)
#include <glib/gmacros.h>
#endif

#include "gio/gio.h"
#include "alarm.h"
#include "alarm-internal.h"
#include "alarm-mgr-stub.h"

#include <tzplatform_config.h>
#include <aul.h>
#include <bundle.h>
#include <db-util.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <dlfcn.h>
#include <pkgmgr-info.h>
#include <device/display.h>

#define SIG_TIMER 0x32
#define WAKEUP_ALARM_APP_ID       "org.tizen.alarm.ALARM"
	/* alarm ui application's alarm's dbus_service name instead of 21
	   (alarm application's app_id) value */

__alarm_server_context_t alarm_context;
bool g_dummy_timer_is_set = FALSE;

GSList *g_scheduled_alarm_list = NULL;
GSList *g_expired_alarm_list = NULL;

#ifndef RTC_WKALM_BOOT_SET
#define RTC_WKALM_BOOT_SET _IOW('p', 0x80, struct rtc_wkalrm)
#endif

/*	2008. 6. 3 sewook7.park
       When the alarm becoms sleeping mode, alarm timer is not expired.
       So using RTC, phone is awaken before alarm rings.
*/
#define __WAKEUP_USING_RTC__
#ifdef __WAKEUP_USING_RTC__
#include <errno.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define ALARM_RTC_WAKEUP	0

#define ALARM_IOW(c, type, size)            _IOW('a', (c) | ((type) << 4), size)
#define ALARM_SET(type)             ALARM_IOW(2, type, struct timespec)
#define ALARM_SET_RTC               _IOW('a', 5, struct timespec)
#define ALARM_CLEAR(type)           _IO('a', 0 | ((type) << 4))

#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
#define ALARMMGR_LOG_BUFFER_SIZE	10000
#define ALARMMGR_LOG_BUFFER_STRING_SIZE	200
#define ALARMMGR_LOG_TAG_SIZE		20
#define ALARMMGR_LOG_MESSAGE_SIZE	120
#define ALARMMGR_LOG_FILE_PATH	"/var/log/alarmmgr.log"
static int log_index = 0;
static int log_fd = 0;
#endif

// display lock and unlock
#define DEVICED_BUS_NAME "org.tizen.system.deviced"
#define DEVICED_PATH_DISPLAY		"/Org/Tizen/System/DeviceD/Display"
#define DEVICED_INTERFACE_DISPLAY	"org.tizen.system.deviced.display"
#define DEVICED_LOCK_STATE		"lockstate"
#define DEVICED_UNLOCK_STATE	"unlockstate"
#define DEVICED_DBUS_REPLY_TIMEOUT	(120*1000)
#define DEVICED_LCD_OFF		"lcdoff"
#define DEVICED_STAY_CUR_STATE	"staycurstate"
#define DEVICED_SLEEP_MARGIN		"sleepmargin"

// link path for timezone info
#define TIMEZONE_INFO_LINK_PATH	tzplatform_mkpath(TZ_SYS_ETC, "localtime")

static const char default_rtc[] = "/dev/alarm";

static int gfd = 0;

#endif				/*__WAKEUP_USING_RTC__*/

/*  GDBus Declaration */
#define ALARM_MGR_DBUS_PATH	"/org/tizen/alarm/manager"
#define ALARM_MGR_DBUS_NAME	"org.tizen.alarm.manager"
GDBusObjectManagerServer *alarmmgr_server = NULL;
static AlarmManager* interface = NULL;

sqlite3 *alarmmgr_db;
bool is_time_changed = false;	// for calculating next duetime

#define BILLION 1000000000	// for calculating nano seconds

static bool __alarm_add_to_list(__alarm_info_t *__alarm_info);
static void __alarm_generate_alarm_id(__alarm_info_t *__alarm_info, alarm_id_t *alarm_id);
static bool __alarm_update_in_list(int pid, alarm_id_t alarm_id,
				   __alarm_info_t *__alarm_info,
				   int *error_code);
static bool __alarm_remove_from_list(int pid, alarm_id_t alarm_id,
				     int *error_code);
static bool __alarm_set_start_and_end_time(alarm_info_t *alarm_info,
					   __alarm_info_t *__alarm_info);
static bool __alarm_update_due_time_of_all_items_in_list(double diff_time);
static bool __alarm_create(alarm_info_t *alarm_info, alarm_id_t *alarm_id,
			int pid, periodic_method_e method, long requested_interval, int is_ref,
			char *app_service_name, char *app_service_name_mod,
			const char *dst_service_name, const char *dst_service_name_mod,
			int *error_code);
static bool __alarm_create_appsvc(alarm_info_t *alarm_info, alarm_id_t *alarm_id,
			   int pid, char *bundle_data, int *error_code);

static bool __alarm_delete(int pid, alarm_id_t alarm_id, int *error_code);
static bool __alarm_update(int pid, char *app_service_name, alarm_id_t alarm_id,
			   alarm_info_t *alarm_info, int *error_code);
static void __alarm_send_noti_to_application(const char *app_service_name, alarm_id_t alarm_id);
static void __alarm_expired();
static gboolean __alarm_handler_idle(gpointer user_data);
static void __clean_registry();
static bool __alarm_manager_reset();
static void __on_system_time_external_changed(keynode_t *node, void *data);
static void __initialize_timer();
static void __initialize_alarm_list();
static void __initialize_scheduled_alarm_list();
static bool __initialize_noti();

static bool __initialize_dbus();
static bool __initialize_db();
static void __initialize();
void on_bus_name_owner_changed(GDBusConnection *connection, const gchar *sender_name, const gchar *object_path,
             const gchar *interface_name, const gchar *signal_name, GVariant *parameters, gpointer user_data);
bool __get_caller_unique_name(int pid, char *unique_name);

#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
static void __initialize_module_log(void);
static bool __save_module_log(const char *tag, const char *messgae);
#endif

int __display_lock_state(char *state, char *flag, unsigned int timeout);
int __display_unlock_state(char *state, char *flag);

int __set_time(time_t _time);

static void __rtc_set()
{
#ifdef __WAKEUP_USING_RTC__
	const char *rtc = default_rtc;
	struct tm due_tm;
	struct timespec alarm_time;
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	char log_message[ALARMMGR_LOG_MESSAGE_SIZE] = {0,};
#endif
#ifdef _SIMUL	// RTC does not work in simulator.
	ALARM_MGR_EXCEPTION_PRINT("because it is simulator's mode, we don't set RTC.");
	return;
#endif

	if (gfd == 0) {
		gfd = open(rtc, O_RDWR);
		if (gfd == -1) {
			ALARM_MGR_EXCEPTION_PRINT("RTC open failed.");
			return;
		}
	}

	/* Read the RTC time/date */
	int retval = 0;
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	char *timebuf = ctime(&alarm_context.c_due_time);
	timebuf[strlen(timebuf) - 1] = '\0';	// to avoid new line
	sprintf(log_message, "wakeup time: %d, %s", (int)alarm_context.c_due_time, timebuf);
#endif

	ALARM_MGR_LOG_PRINT("alarm_context.c_due_time is %d.", (int)alarm_context.c_due_time);

	if (alarm_context.c_due_time != -1) {
		retval = ioctl(gfd, ALARM_CLEAR(ALARM_RTC_WAKEUP));
		if (retval == -1) {
			if (errno == ENOTTY) {
				ALARM_MGR_EXCEPTION_PRINT("Alarm IRQs is not supported.");
			}
			ALARM_MGR_EXCEPTION_PRINT("ALARM_CLEAR ioctl is failed. errno = %s", strerror(errno));
			return;
		}
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]ALARM_CLEAR ioctl is successfully done.");

		time_t due_time = alarm_context.c_due_time;
		gmtime_r(&due_time, &due_tm);

		ALARM_MGR_EXCEPTION_PRINT("Setted RTC Alarm date/time is %d-%d-%d, %02d:%02d:%02d (UTC).",
			due_tm.tm_mday, due_tm.tm_mon + 1, due_tm.tm_year + 1900,
			due_tm.tm_hour, due_tm.tm_min, due_tm.tm_sec);

		alarm_time.tv_sec = due_time - 1;
		alarm_time.tv_nsec = 500000000;	// Wakeup is 500ms faster than expiring time to correct RTC error.
		retval = ioctl(gfd, ALARM_SET(ALARM_RTC_WAKEUP), &alarm_time);
		if (retval == -1) {
			if (errno == ENOTTY) {
				ALARM_MGR_EXCEPTION_PRINT("Alarm IRQs is not supported.");
			}
			ALARM_MGR_EXCEPTION_PRINT("RTC ALARM_SET ioctl is failed. errno = %s", strerror(errno));
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
			__save_module_log("FAIL: SET RTC", log_message);
#endif
			return;
		}
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]RTC ALARM_SET ioctl is successfully done.");
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		__save_module_log("SET RTC", log_message);
#endif
	}
	else {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]alarm_context.c_due_time is"
			"less than 10 sec. RTC alarm does not need to be set");
	}
#endif				/* __WAKEUP_USING_RTC__ */
	return;
}

int __set_time(time_t _time)
{
	// Using /dev/alarm, this function changes both OS time and RTC.
	int ret = 0;
	const char *rtc0 = default_rtc;
	struct timespec rtc_time;
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	char log_tag[ALARMMGR_LOG_TAG_SIZE] = {0,};
	char log_message[ALARMMGR_LOG_MESSAGE_SIZE] = {0,};
#endif

	if (gfd == 0) {
		gfd = open(rtc0, O_RDWR);
		if (gfd == -1) {
			ALARM_MGR_EXCEPTION_PRINT("Opening the /dev/alarm is failed.");
			perror("\t");
		}
	}

	rtc_time.tv_sec = _time;
	rtc_time.tv_nsec = 0;

	ret = ioctl(gfd, ALARM_SET_RTC, &rtc_time);
	if (ret == -1) {
		ALARM_MGR_EXCEPTION_PRINT("ALARM_SET_RTC ioctl is failed. errno = %s", strerror(errno));
	#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		strncpy(log_tag, "FAIL: SET RTC", strlen("FAIL: SET RTC"));
	#endif
		perror("\t");
	}
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	else {
		strncpy(log_tag, "SET RTC", strlen("SET RTC"));
	}

	char *timebuf = ctime(&_time);
	timebuf[strlen(timebuf) - 1] = '\0';    // to avoid new line
	sprintf(log_message, "RTC & OS =%d, %s", (int)_time, timebuf);

	__save_module_log(log_tag, log_message);
#endif

	return 1;
}

bool __alarm_clean_list()
{
	g_slist_free_full(alarm_context.alarms, g_free);
	return true;
}

static void __alarm_generate_alarm_id(__alarm_info_t *__alarm_info, alarm_id_t *alarm_id)
{
	bool unique_id = false;
	__alarm_info_t *entry = NULL;
	GSList *iter = NULL;

	srand((unsigned int)time(NULL));
	__alarm_info->alarm_id = (rand() % INT_MAX) + 1;
	ALARM_MGR_LOG_PRINT("__alarm_info->alarm_id is %d", __alarm_info->alarm_id);

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

	*alarm_id = __alarm_info->alarm_id;

	return;
}

static bool __alarm_add_to_list(__alarm_info_t *__alarm_info)
{
	alarm_info_t *alarm_info = &__alarm_info->alarm_info;
	__alarm_info_t *entry = NULL;
	GSList *iter = NULL;

	ALARM_MGR_LOG_PRINT("[alarm-server]: Before add alarm_id(%d)", __alarm_info->alarm_id);

	alarm_context.alarms = g_slist_append(alarm_context.alarms, __alarm_info);
	ALARM_MGR_EXCEPTION_PRINT("[alarm-server]: After add alarm_id(%d)", __alarm_info->alarm_id);

	// alarm list
	for (iter = alarm_context.alarms; iter != NULL; iter = g_slist_next(iter)) {
		entry = iter->data;
		ALARM_MGR_LOG_PRINT("[alarm-server]: alarm_id(%d).", entry->alarm_id);
	}

	if (!(alarm_info->alarm_type & ALARM_TYPE_VOLATILE)) {
		if (!_save_alarms(__alarm_info)) {
			ALARM_MGR_EXCEPTION_PRINT("Saving alarm_id(%d) in DB is failed.", __alarm_info->alarm_id);
		}
	}

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
		if (!_update_alarms(__alarm_info)) {
			ALARM_MGR_EXCEPTION_PRINT("Updating alarm_id(%d) in DB is failed.", __alarm_info->alarm_id);
		}
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
	ALARM_MGR_LOG_PRINT("[alarm-server]: before del : alarm id(%d)", alarm_id);

	for (iter = alarm_context.alarms; iter != NULL; iter = g_slist_next(iter)) {
		entry = iter->data;
		if (entry->alarm_id == alarm_id) {
			alarm_info = &entry->alarm_info;

			ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:Remove alarm id(%d)", entry->alarm_id);

			if (!(alarm_info->alarm_type & ALARM_TYPE_VOLATILE)) {
				_delete_alarms(alarm_id);
			}

			alarm_context.alarms = g_slist_remove(alarm_context.alarms, iter->data);
			g_free(entry);
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

		alarm_tm.tm_hour = start->hour;
		alarm_tm.tm_min = start->min;
		alarm_tm.tm_sec = start->sec;
		alarm_tm.tm_isdst = -1;

		__alarm_info->start = mktime(&alarm_tm);
	} else {
		__alarm_info->start = 0;
	}

	if (end->year != 0) {
		alarm_tm.tm_year = end->year - 1900;
		alarm_tm.tm_mon = end->month - 1;
		alarm_tm.tm_mday = end->day;

		alarm_tm.tm_hour = end->hour;
		alarm_tm.tm_min = end->min;
		alarm_tm.tm_sec = end->sec;

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

gboolean __update_relative_alarms(gpointer user_data)
{
	GSList *iter = NULL;
	__alarm_info_t *entry = NULL;
	char *error_message = NULL;

	if (sqlite3_exec(alarmmgr_db, "BEGIN EXCLUSIVE", NULL, NULL, &error_message) != SQLITE_OK) {
		SECURE_LOGE("sqlite3_exec() is failed. error message = %s", error_message);
		return false;
	}

	for (iter = alarm_context.alarms; iter != NULL; iter = g_slist_next(iter)) {
		entry = iter->data;
		alarm_info_t *alarm_info = &(entry->alarm_info);
		if (alarm_info->alarm_type & ALARM_TYPE_RELATIVE) {
			_update_alarms(entry);
		}
	}

	if (sqlite3_exec(alarmmgr_db, "COMMIT", NULL, NULL, &error_message) != SQLITE_OK) {
		SECURE_LOGE("sqlite3_exec() is failed. error message = %s", error_message);
		return false;
	}

	return false;
}

static bool __alarm_update_due_time_of_all_items_in_list(double diff_time)
{
	time_t current_time;
	time_t min_time = -1;
	time_t due_time = 0;
	GSList *iter = NULL;
	__alarm_info_t *entry = NULL;
	struct tm *p_time = NULL ;
	struct tm due_time_result ;
	struct tm fixed_time = { 0, };
	is_time_changed = true;

	tzset();
	for (iter = alarm_context.alarms; iter != NULL; iter = g_slist_next(iter)) {
		entry = iter->data;
		alarm_info_t *alarm_info = &(entry->alarm_info);
		if (alarm_info->alarm_type & ALARM_TYPE_RELATIVE) {
			entry->due_time += diff_time;

			alarm_date_t *start = &alarm_info->start;	/**< start time of the alarm */
			alarm_date_t *end = &alarm_info->end;	/**< end time of the alarm */

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
		}
		_alarm_next_duetime(entry);
	}

	time(&current_time);

	for (iter = alarm_context.alarms; iter != NULL; iter = g_slist_next(iter)) {
		entry = iter->data;
		due_time = entry->due_time;

		double interval = 0;

		ALARM_MGR_LOG_PRINT("alarm[%d] with duetime(%u) at current(%u)", entry->alarm_id, due_time, current_time);
		if (due_time == 0) {	/* 0 means this alarm has been disabled */
			continue;
		}

		interval = difftime(due_time, current_time);

		if (interval < 0) {
			ALARM_MGR_EXCEPTION_PRINT("The duetime of alarm(%d) is OVER.", entry->alarm_id);
			continue;
		}

		interval = difftime(due_time, min_time);

		if ((interval < 0) || min_time == -1) {
			min_time = due_time;
		}
	}

	is_time_changed = false;
	alarm_context.c_due_time = min_time;

	g_idle_add(__update_relative_alarms, NULL);
	return true;
}

static bool __alarm_create_appsvc(alarm_info_t *alarm_info, alarm_id_t *alarm_id,
			   int pid, char *bundle_data, int *error_code)
{
	time_t current_time;
	time_t due_time;
	struct tm ts_ret;
	char due_time_r[100] = { 0 };
	char app_name[512] = { 0 };
	bundle *b;
	char caller_appid[256] = { 0 };
	const char* callee_appid = NULL;
	char* caller_pkgid = NULL;
	char* callee_pkgid = NULL;
	pkgmgrinfo_pkginfo_h caller_handle;
	pkgmgrinfo_pkginfo_h callee_handle;
	bundle_raw *b_data = NULL;
	int datalen = 0;

	__alarm_info_t *__alarm_info = NULL;

	__alarm_info = malloc(sizeof(__alarm_info_t));
	if (__alarm_info == NULL) {
		SECURE_LOGE("Caution!! app_pid=%d, malloc failed. it seems to be OOM.", pid);
		*error_code = ERR_ALARM_SYSTEM_FAIL;
		return false;
	}

	__alarm_info->pid = pid;
	__alarm_info->alarm_id = -1;

	if (!__get_caller_unique_name(pid, app_name)) {
		*error_code = ERR_ALARM_SYSTEM_FAIL;
		free(__alarm_info);
		return false;
	}
	__alarm_info->quark_app_unique_name = g_quark_from_string(app_name);

	// Get caller_appid and callee_appid to get each package id
	// caller
	__alarm_info->quark_caller_pkgid = g_quark_from_string("null");

	if (aul_app_get_appid_bypid(pid, caller_appid, 256) == AUL_R_OK) {
		if (pkgmgrinfo_appinfo_get_appinfo(caller_appid, &caller_handle) == PMINFO_R_OK) {
			if (pkgmgrinfo_appinfo_get_pkgid(caller_handle, &caller_pkgid) == PMINFO_R_OK) {
				if (caller_pkgid) {
					__alarm_info->quark_caller_pkgid = g_quark_from_string(caller_pkgid);
				}
			}
			pkgmgrinfo_appinfo_destroy_appinfo(caller_handle);
		}
	}

	// callee
	__alarm_info->quark_callee_pkgid = g_quark_from_string("null");

	b = bundle_decode((bundle_raw *)bundle_data, strlen(bundle_data));
	callee_appid = appsvc_get_appid(b);
	if (pkgmgrinfo_appinfo_get_appinfo(callee_appid, &callee_handle) == PMINFO_R_OK) {
		if (pkgmgrinfo_appinfo_get_pkgid(callee_handle, &callee_pkgid) == PMINFO_R_OK) {
			if (callee_pkgid) {
				__alarm_info->quark_callee_pkgid = g_quark_from_string(callee_pkgid);
			}
		}
		pkgmgrinfo_appinfo_destroy_appinfo(callee_handle);
	}

	SECURE_LOGD("caller_pkgid = %s, callee_pkgid = %s",
		g_quark_to_string(__alarm_info->quark_caller_pkgid), g_quark_to_string(__alarm_info->quark_callee_pkgid));

	bundle_encode(b, &b_data, &datalen);
	__alarm_info->quark_bundle=g_quark_from_string(b_data);
	__alarm_info->quark_app_service_name = g_quark_from_string("null");
	__alarm_info->quark_dst_service_name = g_quark_from_string("null");
	__alarm_info->quark_app_service_name_mod = g_quark_from_string("null");
	__alarm_info->quark_dst_service_name_mod = g_quark_from_string("null");

	bundle_free(b);
	if (b_data) {
		free(b_data);
		b_data = NULL;
	}

	__alarm_set_start_and_end_time(alarm_info, __alarm_info);
	memcpy(&(__alarm_info->alarm_info), alarm_info, sizeof(alarm_info_t));
	__alarm_generate_alarm_id(__alarm_info, alarm_id);

	time(&current_time);

	if (alarm_context.c_due_time < current_time) {
		ALARM_MGR_EXCEPTION_PRINT("Caution!! alarm_context.c_due_time "
		"(%d) is less than current time(%d)", alarm_context.c_due_time, current_time);
		alarm_context.c_due_time = -1;
	}

	due_time = _alarm_next_duetime(__alarm_info);
	if (__alarm_add_to_list(__alarm_info) == false) {
		free(__alarm_info);
		*error_code = ERR_ALARM_SYSTEM_FAIL;
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
	} else if (difftime(due_time, current_time) < 0) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]: Expired Due Time.[Due time=%d, Current Time=%d]!!!Do not add to schedule list\n", due_time, current_time);
		return true;
	} else {
		localtime_r(&due_time, &ts_ret);
		strftime(due_time_r, 30, "%c", &ts_ret);
		SECURE_LOGD("[alarm-server]:Create a new alarm: "
				    "alarm(%d) due_time(%s)", *alarm_id,
				    due_time_r);
	}

	ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:alarm_context.c_due_time(%d), due_time(%d)", alarm_context.c_due_time, due_time);

	if (alarm_context.c_due_time == -1 || due_time < alarm_context.c_due_time) {
		_clear_scheduled_alarm_list();
		_add_to_scheduled_alarm_list(__alarm_info);
		_alarm_set_timer(&alarm_context, alarm_context.timer, due_time);
		alarm_context.c_due_time = due_time;
	} else if (due_time == alarm_context.c_due_time) {
		_add_to_scheduled_alarm_list(__alarm_info);
	}

	__rtc_set();

	return true;
}

static bool __alarm_create(alarm_info_t *alarm_info, alarm_id_t *alarm_id,
			int pid, periodic_method_e method, long requested_interval, int is_ref,
			char *app_service_name, char *app_service_name_mod,
			const char *dst_service_name, const char *dst_service_name_mod,
			int *error_code)
{
	time_t current_time;
	time_t due_time;
	char app_name[512] = { 0 };
	char caller_appid[256] = { 0 };
	char* caller_pkgid = NULL;
	pkgmgrinfo_pkginfo_h caller_handle;

	__alarm_info_t *__alarm_info = NULL;

	__alarm_info = malloc(sizeof(__alarm_info_t));
	if (__alarm_info == NULL) {
		SECURE_LOGE("Caution!! app_pid=%d, malloc "
					  "failed. it seems to be OOM\n", pid);
		*error_code = ERR_ALARM_SYSTEM_FAIL;
		return false;
	}
	__alarm_info->pid = pid;
	__alarm_info->alarm_id = -1;
	__alarm_info->quark_caller_pkgid = g_quark_from_string("null");
	__alarm_info->method = method;
	__alarm_info->requested_interval = requested_interval;
	__alarm_info->is_ref = is_ref;

	// Get caller_appid to get caller's package id. There is no callee.
	if (aul_app_get_appid_bypid(pid, caller_appid, 256) == AUL_R_OK) {
		if (pkgmgrinfo_appinfo_get_appinfo(caller_appid, &caller_handle) == PMINFO_R_OK) {
			if (pkgmgrinfo_appinfo_get_pkgid(caller_handle, &caller_pkgid) == PMINFO_R_OK) {
				if (caller_pkgid) {
					__alarm_info->quark_caller_pkgid = g_quark_from_string(caller_pkgid);
				}
			}
			pkgmgrinfo_appinfo_destroy_appinfo(caller_handle);
		}
	}

	__alarm_info->quark_callee_pkgid = g_quark_from_string("null");
	SECURE_LOGD("caller_pkgid = %s, callee_pkgid = null", g_quark_to_string(__alarm_info->quark_caller_pkgid));

	if (!__get_caller_unique_name(pid, app_name)) {
		*error_code = ERR_ALARM_SYSTEM_FAIL;
		free(__alarm_info);
		return false;
	}

	__alarm_info->quark_app_unique_name = g_quark_from_string(app_name);
	__alarm_info->quark_app_service_name = g_quark_from_string(app_service_name);
	__alarm_info->quark_app_service_name_mod = g_quark_from_string(app_service_name_mod);
	__alarm_info->quark_dst_service_name = g_quark_from_string(dst_service_name);
	__alarm_info->quark_dst_service_name_mod = g_quark_from_string(dst_service_name_mod);
	__alarm_info->quark_bundle = g_quark_from_string("null");

	__alarm_set_start_and_end_time(alarm_info, __alarm_info);
	memcpy(&(__alarm_info->alarm_info), alarm_info, sizeof(alarm_info_t));
	__alarm_generate_alarm_id(__alarm_info, alarm_id);

	time(&current_time);

	SECURE_LOGD("[alarm-server]:pid=%d, app_unique_name=%s, "
		"app_service_name=%s,dst_service_name=%s, c_due_time=%d", \
		pid, g_quark_to_string(__alarm_info->quark_app_unique_name), \
		g_quark_to_string(__alarm_info->quark_app_service_name), \
		g_quark_to_string(__alarm_info->quark_dst_service_name), \
			    alarm_context.c_due_time);

	if (alarm_context.c_due_time < current_time) {
		ALARM_MGR_EXCEPTION_PRINT("Caution!! alarm_context.c_due_time "
		"(%d) is less than current time(%d)", alarm_context.c_due_time, current_time);
		alarm_context.c_due_time = -1;
	}

	due_time = _alarm_next_duetime(__alarm_info);
	if (__alarm_add_to_list(__alarm_info) == false) {
		free(__alarm_info);
		return false;
	}

	if (due_time == 0) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:Create a new alarm: due_time is 0, alarm(%d).", *alarm_id);
		return true;
	} else if (current_time == due_time) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:Create alarm: current_time(%d) is same as due_time(%d).",
			current_time, due_time);
		return true;
	} else if (difftime(due_time, current_time) <  0) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]: Expired Due Time.[Due time=%d, Current Time=%d]!!!Do not add to schedule list.",
			due_time, current_time);
		return true;
	} else {
		char due_time_r[100] = { 0 };
		struct tm ts_ret;
		localtime_r(&due_time, &ts_ret);
		strftime(due_time_r, 30, "%c", &ts_ret);
		SECURE_LOGD("[alarm-server]:Create a new alarm: alarm(%d) due_time(%s)", *alarm_id, due_time_r);
	}

	ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:alarm_context.c_due_time(%d), due_time(%d)", alarm_context.c_due_time, due_time);

	if (alarm_context.c_due_time == -1 || due_time < alarm_context.c_due_time) {
		_clear_scheduled_alarm_list();
		_add_to_scheduled_alarm_list(__alarm_info);
		_alarm_set_timer(&alarm_context, alarm_context.timer, due_time);
		alarm_context.c_due_time = due_time;
	} else if (due_time == alarm_context.c_due_time) {
		_add_to_scheduled_alarm_list(__alarm_info);
	}

	__rtc_set();

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
		SECURE_LOGE("Caution!! app_pid=%d, malloc failed. it seems to be OOM.", pid);
		*error_code = ERR_ALARM_SYSTEM_FAIL;
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
		"(%d) is less than current time(%d)", alarm_context.c_due_time, current_time);
		alarm_context.c_due_time = -1;
	}

	due_time = _alarm_next_duetime(__alarm_info);
	if (!__alarm_update_in_list(pid, alarm_id, __alarm_info, error_code)) {
		free(__alarm_info);
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]: requested alarm_id "
		"(%d) does not exist. so this value is invalid id.", alarm_id);
		return false;
	}

	result = _remove_from_scheduled_alarm_list(pid, alarm_id);

	if (result == true && g_slist_length(g_scheduled_alarm_list) == 0) {
		/*there is no scheduled alarm */
		_alarm_disable_timer(alarm_context);
		_alarm_schedule();

		ALARM_MGR_LOG_PRINT("[alarm-server]:Update alarm: alarm(%d).", alarm_id);

		__rtc_set();

		if (due_time == 0) {
			ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:Update alarm: due_time is 0.");
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
	} else if (difftime(due_time, current_time)< 0) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]: Expired Due Time.[Due time=%d, Current Time=%d]!!!Do not add to schedule list\n", due_time, current_time);
		free(__alarm_info);
		return true;
	} else {
		char due_time_r[100] = { 0 };
		struct tm ts_ret;
		localtime_r(&due_time, &ts_ret);
		strftime(due_time_r, 30, "%c", &ts_ret);
		SECURE_LOGD("[alarm-server]:Update alarm: alarm(%d) "
				    "due_time(%s)\n", alarm_id, due_time_r);
	}

	ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:alarm_context.c_due_time(%d), due_time(%d)", alarm_context.c_due_time, due_time);

	if (alarm_context.c_due_time == -1 || due_time < alarm_context.c_due_time) {
		_clear_scheduled_alarm_list();
		_add_to_scheduled_alarm_list(__alarm_info);
		_alarm_set_timer(&alarm_context, alarm_context.timer, due_time);
		alarm_context.c_due_time = due_time;
		ALARM_MGR_LOG_PRINT("[alarm-server1]:alarm_context.c_due_time "
		     "(%d), due_time(%d)", alarm_context.c_due_time, due_time);
	} else if (due_time == alarm_context.c_due_time) {
		_add_to_scheduled_alarm_list(__alarm_info);
		ALARM_MGR_LOG_PRINT("[alarm-server2]:alarm_context.c_due_time "
		     "(%d), due_time(%d)", alarm_context.c_due_time, due_time);
	}

	__rtc_set();

	free(__alarm_info);

	return true;
}

static bool __alarm_delete(int pid, alarm_id_t alarm_id, int *error_code)
{
	bool result = false;

	SECURE_LOGD("[alarm-server]:delete alarm: alarm(%d) pid(%d)\n", alarm_id, pid);
	result = _remove_from_scheduled_alarm_list(pid, alarm_id);

	if (!__alarm_remove_from_list(pid, alarm_id, error_code)) {

		SECURE_LOGE("[alarm-server]:delete alarm: "
					  "alarm(%d) pid(%d) has failed with error_code(%d)\n",
					  alarm_id, pid, *error_code);
		return false;
	}

	if (result == true && g_slist_length(g_scheduled_alarm_list) == 0) {
		_alarm_disable_timer(alarm_context);
		_alarm_schedule();
	}

	__rtc_set();

	return true;
}

static bool __can_skip_expired_cb(alarm_id_t alarm_id)
{
	GSList *gs_iter = NULL;
	__alarm_info_t *entry = NULL;
	alarm_info_t *alarm = NULL;

	for (gs_iter = alarm_context.alarms; gs_iter != NULL; gs_iter = g_slist_next(gs_iter)) {
		entry = gs_iter->data;
		if (entry->alarm_id == alarm_id) {
			alarm = &(entry->alarm_info);
			time_t ts = 0;
			struct tm ts_tm;
			int dur = entry->requested_interval;
			int from, to;

			if (!(alarm->alarm_type & ALARM_TYPE_PERIOD) || entry->method == CUT_OFF)
				return false;

			ts_tm.tm_hour = alarm->start.hour;
			ts_tm.tm_min = alarm->start.min;
			ts_tm.tm_sec = alarm->start.sec;

			ts_tm.tm_year = alarm->start.year - 1900;
			ts_tm.tm_mon = alarm->start.month - 1;
			ts_tm.tm_mday = alarm->start.day;
			ts_tm.tm_isdst = -1;

			ts = mktime(&ts_tm);

			from = (ts / dur) * dur;
			to = from + dur;

			if ( ts >= from && ts < to && from > ts - alarm->mode.u_interval.interval) {
				return false;
			}

			return true;
		}
	}

	return false;
}

static void __alarm_send_noti_to_application(const char *app_service_name, alarm_id_t alarm_id)
{
	char service_name[MAX_SERVICE_NAME_LEN] = {0,};

	if (app_service_name == NULL || strlen(app_service_name) == 0) {
		ALARM_MGR_EXCEPTION_PRINT("This alarm destination is invalid.");
		return;
	}

	if (__can_skip_expired_cb(alarm_id))
		return;

	memcpy(service_name, app_service_name, strlen(app_service_name));
	SECURE_LOGI("[alarm server][send expired_alarm(alarm_id=%d) to app_service_name(%s)]", alarm_id, service_name);

	g_dbus_connection_call(alarm_context.connection,
						service_name,
						"/org/tizen/alarm/client",
						"org.tizen.alarm.client",
						"alarm_expired",
						g_variant_new("(is)", alarm_id, service_name),
						NULL,
						G_DBUS_CALL_FLAGS_NONE,
						-1,
						NULL,
						NULL,
						NULL);
}

static int __get_caller_pid(const char *name)
{
	guint pid;
	GVariant *ret;
	GError *error = NULL;

	ret = g_dbus_connection_call_sync (alarm_context.connection,
	                                   "org.freedesktop.DBus",
	                                   "/org/freedesktop/DBus",
	                                   "org.freedesktop.DBus",
	                                   "GetConnectionUnixProcessID",
	                                   g_variant_new ("(s)", name),
	                                   NULL,
	                                   G_DBUS_CALL_FLAGS_NONE,
	                                   -1,
	                                   NULL,
	                                   &error);
	g_variant_get (ret, "(u)", &pid);
	g_variant_unref (ret);

	return pid;
}

static int __is_ui_app(const char *appid)
{
	if (appid == NULL)
		return 0;

	int ret = 0;
	pkgmgrinfo_appinfo_h appinfo_h = NULL;

	ret = pkgmgrinfo_appinfo_get_appinfo(appid, &appinfo_h);

	if (ret < 0 ) {
		return 0;
	}

	char *component = NULL;
	int found = 0;

	ret = pkgmgrinfo_appinfo_get_component_type(appinfo_h, &component);
	if (ret == 0 && component != NULL && strncmp(component, "uiapp", 5) == 0) {
		found = 1;
	}

	if (appinfo_h)
		pkgmgrinfo_appinfo_destroy_appinfo(appinfo_h);

	return found;
}

static int __iter_fn(const char* appid, void *data)
{
	int *found = data;

	if (__is_ui_app(appid)) {
		*found = 1;
		return 1;
	}

	return 0;
}

static int __have_ui_apps(bundle *b)
{
	int found = 0;
	appsvc_get_list(b, __iter_fn, &found);
	return found;
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
	char *appid = NULL;
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	char log_message[ALARMMGR_LOG_MESSAGE_SIZE] = {0,};
#endif
	GError *error = NULL;
	GVariant *result = NULL;
	gboolean name_has_owner_reply = false;

	ALARM_MGR_LOG_PRINT("[alarm-server]: Enter");

	time_t current_time;
	double interval;

	time(&current_time);

	interval = difftime(alarm_context.c_due_time, current_time);
	ALARM_MGR_LOG_PRINT("[alarm-server]: c_due_time(%d), current_time(%d), interval(%d)",
		alarm_context.c_due_time, current_time, interval);

	if (alarm_context.c_due_time > current_time + 1) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]: False Alarm. due time is (%d) seconds future",
			alarm_context.c_due_time - current_time);
		goto done;
	}
	// 10 seconds is maximum permitted delay from timer expire to this function
	if (alarm_context.c_due_time + 10 < current_time) {
		ALARM_MGR_EXCEPTION_PRINT("[alarm-server]: False Alarm. due time is (%d) seconds past.",
			current_time - alarm_context.c_due_time);
		goto done;
	}

	GSList *iter = NULL;
	__scheduled_alarm_t *alarm = NULL;

	for (iter = g_scheduled_alarm_list; iter != NULL; iter = g_slist_next(iter)) {
		alarm = iter->data;
		alarm_id = alarm->alarm_id;
		__alarm_info = alarm->__alarm_info;
		app_pid = __alarm_info->pid;

		// Case #1. The process is an application launched by app_control.
		// It registered an alarm using launch-based APIs like alarm_schedule_xxx, alarmmgr_xxx_appsvc.
		if (strncmp(g_quark_to_string(__alarm_info->quark_bundle), "null", 4) != 0) {
			b_len = strlen(g_quark_to_string(__alarm_info->quark_bundle));

			b = bundle_decode((bundle_raw *)g_quark_to_string(__alarm_info->quark_bundle), b_len);

			if (b == NULL)
			{
				ALARM_MGR_EXCEPTION_PRINT("Error!!!..Unable to decode the bundle!!\n");
			}
			else
			{
				snprintf(alarm_id_val,31,"%d",alarm_id);

				if (bundle_add_str(b,"http://tizen.org/appcontrol/data/alarm_id", alarm_id_val)){
					ALARM_MGR_EXCEPTION_PRINT("Unable to add alarm id to the bundle\n");
				}
				else
				{
					appid = (char *)appsvc_get_appid(b);
					if (appid && !__is_ui_app(appid)) {
							ALARM_MGR_EXCEPTION_PRINT("ui-application can only be launched\n");
					}
					else if( (__alarm_info->alarm_info.alarm_type & ALARM_TYPE_NOLAUNCH) && !aul_app_is_running(appid))
					{
						ALARM_MGR_EXCEPTION_PRINT("This alarm is ignored\n");
					}
					else
					{
						if (__have_ui_apps(b))
						{
							if ( appsvc_run_service(b, 0, NULL, NULL) < 0)
							{
								ALARM_MGR_EXCEPTION_PRINT("Unable to run app svc\n");
							}
							else
							{
								device_display_change_state(DISPLAY_STATE_NORMAL);
								ALARM_MGR_LOG_PRINT("Successfuly ran app svc\n");
							}
						}
						else
						{
							ALARM_MGR_EXCEPTION_PRINT("ui-application can only be launched\n");
						}

					}

				}
				bundle_free(b);
			}
		}
		else
		{
			char appid[MAX_SERVICE_NAME_LEN] = { 0, };
			pkgmgrinfo_appinfo_h appinfo_handle = NULL;

			if (strncmp(g_quark_to_string(__alarm_info->quark_dst_service_name), "null", 4) == 0) {
				SECURE_LOGD("[alarm-server]:destination is null, so we send expired alarm to %s(%u).",
					g_quark_to_string(__alarm_info->quark_app_service_name), __alarm_info->quark_app_service_name);
					destination_app_service_name = g_quark_to_string(__alarm_info->quark_app_service_name_mod);
			} else {
				SECURE_LOGD("[alarm-server]:destination :%s(%u)",
					g_quark_to_string(__alarm_info->quark_dst_service_name), __alarm_info->quark_dst_service_name);
					destination_app_service_name = g_quark_to_string(__alarm_info->quark_dst_service_name_mod);
			}

			/*
			 * we should consider a situation that
			 * destination_app_service_name is owner_name like (:xxxx) and
			 * application's pid which registered this alarm was killed.In that case,
			 * we don't need to send the expire event because the process was killed.
			 * this causes needless message to be sent.
			 */
			SECURE_LOGD("[alarm-server]: destination_app_service_name :%s, app_pid=%d", destination_app_service_name, app_pid);

			result = g_dbus_connection_call_sync(alarm_context.connection,
								"org.freedesktop.DBus",
								"/org/freedesktop/DBus",
								"org.freedesktop.DBus",
								"NameHasOwner",
								g_variant_new ("(s)", destination_app_service_name),
								G_VARIANT_TYPE ("(b)"),
								G_DBUS_CALL_FLAGS_NONE,
								-1,
								NULL,
								&error);
			if (result == NULL) {
				ALARM_MGR_EXCEPTION_PRINT("g_dbus_connection_call_sync() is failed. err: %s", error->message);
				g_error_free(error);
			} else {
				g_variant_get (result, "(b)", &name_has_owner_reply);
			}

			if (strncmp(g_quark_to_string(__alarm_info->quark_dst_service_name), "null",4) == 0) {
				strncpy(appid,g_quark_to_string(__alarm_info->quark_app_service_name)+6,strlen(g_quark_to_string(__alarm_info->quark_app_service_name))-6);
			} else {
				strncpy(appid,g_quark_to_string(__alarm_info->quark_dst_service_name)+6,strlen(g_quark_to_string(__alarm_info->quark_dst_service_name))-6);
			}

			pkgmgrinfo_appinfo_get_appinfo(appid, &appinfo_handle);
			ALARM_MGR_LOG_PRINT("appid : %s (%x)", appid, appinfo_handle);

			// Case #2. The process was killed && App type
			// This app is launched and owner of DBus connection is changed. and then, expiration noti is sent by DBus.
			if (name_has_owner_reply == false && appinfo_handle) {
				__expired_alarm_t *expire_info;
				char alarm_id_str[32] = { 0, };

				if (__alarm_info->alarm_info.alarm_type & ALARM_TYPE_WITHCB) {
					__alarm_remove_from_list(__alarm_info->pid, alarm_id, NULL);
					goto done;
				}

				expire_info = malloc(sizeof(__expired_alarm_t));
				if (G_UNLIKELY(NULL == expire_info)) {
					ALARM_MGR_ASSERT_PRINT("[alarm-server]:Malloc failed!Can't notify alarm expiry info\n");
					goto done;
				}
				memset(expire_info, '\0', sizeof(__expired_alarm_t));
				strncpy(expire_info->service_name, destination_app_service_name, MAX_SERVICE_NAME_LEN-1);
				expire_info->alarm_id = alarm_id;
				g_expired_alarm_list = g_slist_append(g_expired_alarm_list, expire_info);

				snprintf(alarm_id_str, 31, "%d", alarm_id);

				SECURE_LOGD("before aul_launch appid(%s) alarm_id_str(%s)", appid, alarm_id_str);

				bundle *kb;
				kb = bundle_create();
				bundle_add_str(kb, "__ALARM_MGR_ID", alarm_id_str);
				aul_launch_app(appid, kb);	// on_bus_name_owner_changed will be called.
				bundle_free(kb);
			} else {
				// Case #3. The process is alive or was killed && non-app type(daemon)
				// Expiration noti is sent by DBus. it makes the process alive. (dbus auto activation)
				ALARM_MGR_LOG_PRINT("before alarm_send_noti_to_application");
				ALARM_MGR_LOG_PRINT("WAKEUP pid: %d", __alarm_info->pid);

				/* TODO: implement aul_update_freezer_status */
				//aul_update_freezer_status(__alarm_info->pid, "wakeup");
				__alarm_send_noti_to_application(destination_app_service_name, alarm_id);	// dbus auto activation
				ALARM_MGR_LOG_PRINT("after __alarm_send_noti_to_application");
			}
		}

		ALARM_MGR_EXCEPTION_PRINT("alarm_id[%d] is expired.", alarm_id);

#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		sprintf(log_message, "alarmID: %d, pid: %d, duetime: %d", alarm_id, app_pid, (int)__alarm_info->due_time);
		__save_module_log("EXPIRED", log_message);
		memset(log_message, '\0', sizeof(log_message));
#endif

		if (__alarm_info->alarm_info.mode.repeat == ALARM_REPEAT_MODE_ONCE) {
			__alarm_remove_from_list(__alarm_info->pid, alarm_id, NULL);
		} else {
			_alarm_next_duetime(__alarm_info);
		}
	}

 done:
	_clear_scheduled_alarm_list();
	alarm_context.c_due_time = -1;

	ALARM_MGR_LOG_PRINT("[alarm-server]: Leave");
}

static gboolean __alarm_handler_idle(gpointer user_data)
{
	GPollFD *gpollfd = (GPollFD *) user_data;
	uint64_t exp;
	if (gpollfd == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("gpollfd is NULL");
		return false;
	}

	if (read(gpollfd->fd, &exp, sizeof(uint64_t)) < 0) {
		ALARM_MGR_EXCEPTION_PRINT("Reading the fd is failed.");
		return false;
	}

	ALARM_MGR_EXCEPTION_PRINT("Lock the display not to enter LCD OFF");
	if (__display_lock_state(DEVICED_LCD_OFF, DEVICED_STAY_CUR_STATE, 0) != ALARMMGR_RESULT_SUCCESS) {
		ALARM_MGR_EXCEPTION_PRINT("__display_lock_state() is failed");
	}

	if (g_dummy_timer_is_set == true) {
		ALARM_MGR_LOG_PRINT("dummy alarm timer has expired.");
	}
	else {
		ALARM_MGR_LOG_PRINT("__alarm_handler_idle");
		__alarm_expired();
	}

	_alarm_schedule();

	__rtc_set();

	ALARM_MGR_EXCEPTION_PRINT("Unlock the display from LCD OFF");
	if (__display_unlock_state(DEVICED_LCD_OFF, DEVICED_SLEEP_MARGIN) != ALARMMGR_RESULT_SUCCESS) {
		ALARM_MGR_EXCEPTION_PRINT("__display_unlock_state() is failed");
	}

	return false;
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

static void __on_system_time_external_changed(keynode_t *node, void *data)
{
	double diff_time = 0.0;
	time_t cur_time = 0;

	_alarm_disable_timer(alarm_context);

	if (node) {
		diff_time = vconf_keynode_get_dbl(node);
	} else {
		if (vconf_get_dbl(VCONFKEY_SYSTEM_TIMECHANGE_EXTERNAL, &diff_time) != 0) {
			ALARM_MGR_EXCEPTION_PRINT("Failed to get value of VCONFKEY_SYSTEM_TIMECHANGE_EXTERNAL.");
			return;
		}
	}

	tzset();
	time(&cur_time);

	ALARM_MGR_EXCEPTION_PRINT("diff_time is %f, New time is %s\n", diff_time, ctime(&cur_time));

	ALARM_MGR_LOG_PRINT("[alarm-server] System time has been changed externally\n");
	ALARM_MGR_LOG_PRINT("1.alarm_context.c_due_time is %d\n",
			    alarm_context.c_due_time);

	__set_time(cur_time);

	vconf_set_int(VCONFKEY_SYSTEM_TIME_CHANGED,(int)diff_time);

	__alarm_update_due_time_of_all_items_in_list(diff_time);

	ALARM_MGR_LOG_PRINT("2.alarm_context.c_due_time is %d\n",
			    alarm_context.c_due_time);
	_clear_scheduled_alarm_list();
	_alarm_schedule();
	__rtc_set();

	return;
}

static int __on_app_uninstalled(int req_id, const char *pkg_type,
				const char *pkgid, const char *key, const char *val,
				const void *pmsg, void *user_data)
{
	GSList* gs_iter = NULL;
	__alarm_info_t* entry = NULL;
	alarm_info_t* alarm_info = NULL;
	bool is_deleted = false;

	SECURE_LOGD("pkg_type(%s), pkgid(%s), key(%s), value(%s)", pkg_type, pkgid, key, val);

	if (strncmp(key, "end", 3) == 0 && strncmp(val, "ok", 2) == 0)
	{
		for (gs_iter = alarm_context.alarms; gs_iter != NULL; )
		{
			bool is_found = false;
			entry = gs_iter->data;

			const char* caller_pkgid = g_quark_to_string(entry->quark_caller_pkgid);
			const char* callee_pkgid = g_quark_to_string(entry->quark_callee_pkgid);

			if ((caller_pkgid && strncmp(pkgid, caller_pkgid, strlen(pkgid)) == 0) ||
				(callee_pkgid && strncmp(pkgid, callee_pkgid, strlen(pkgid)) == 0))
			{
				if (_remove_from_scheduled_alarm_list(entry->pid, entry->alarm_id))
				{
					is_deleted = true;
				}

				alarm_info = &entry->alarm_info;
				if (!(alarm_info->alarm_type & ALARM_TYPE_VOLATILE))
				{
					if(!_delete_alarms(entry->alarm_id))
					{
						SECURE_LOGE("_delete_alarms() is failed. pkgid[%s], alarm_id[%d]", pkgid, entry->alarm_id);
					}
				}
				is_found = true;
			}

			gs_iter = g_slist_next(gs_iter);

			if (is_found)
			{
				SECURE_LOGD("Remove pkgid[%s], alarm_id[%d]", pkgid, entry->alarm_id);
				alarm_context.alarms = g_slist_remove(alarm_context.alarms, entry);
				g_free(entry);
			}
		}

		if (is_deleted && (g_slist_length(g_scheduled_alarm_list) == 0))
		{
			_alarm_disable_timer(alarm_context);
			_alarm_schedule();
		}
	}

	__rtc_set();

	return ALARMMGR_RESULT_SUCCESS;
}

bool __get_caller_unique_name(int pid, char *unique_name)
{
	char caller_appid[256] = {0,};
	char caller_cmdline[512] = {0,};

	if (unique_name == NULL)
	{
		ALARM_MGR_EXCEPTION_PRINT("unique_name should not be NULL.");
		return false;
	}

	if (aul_app_get_appid_bypid(pid, caller_appid, sizeof(caller_appid)) == AUL_R_OK)
	{
		// When a caller is an application, the unique name is appID.
		strncpy(unique_name, caller_appid, strlen(caller_appid));
/*	}
	else if (aul_app_get_cmdline_bypid(pid, caller_cmdline, sizeof(caller_cmdline)) == AUL_R_OK)
	{
		strncpy(unique_name, caller_cmdline, strlen(caller_cmdline));
		ALARM_MGR_LOG_PRINT("unique_name is caller_cmdline : %s.", unique_name);
		*/
	} else {
		ALARM_MGR_EXCEPTION_PRINT("Failed to get caller_cmdline");
		return false;
	}

	SECURE_LOGD("unique_name= %s", unique_name);
	return true;
}

#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
static void __initialize_module_log(void)
{
	log_fd = open(ALARMMGR_LOG_FILE_PATH, O_CREAT | O_WRONLY, 0644);
	if (log_fd == -1) {
		ALARM_MGR_EXCEPTION_PRINT("Opening the file for alarmmgr log is failed. err: %s", strerror(errno));
		return;
	}

	int offset = lseek(log_fd, 0, SEEK_END);
	if (offset != 0) {
		log_index = (int)(offset / ALARMMGR_LOG_BUFFER_STRING_SIZE);
		if (log_index >= ALARMMGR_LOG_BUFFER_SIZE) {
			log_index = 0;
			lseek(log_fd, 0, SEEK_SET);
		}
	}
	return;
}

static bool __save_module_log(const char *tag, const char *message)
{
	char buffer[ALARMMGR_LOG_BUFFER_STRING_SIZE] = {0,};
	time_t now;
	int offset = 0;

	if (log_fd == -1) {
		ALARM_MGR_EXCEPTION_PRINT("The file is not ready.");
		return false;
	}

	if (log_index != 0) {
		offset = lseek(log_fd, 0, SEEK_CUR);
	} else {
		offset = lseek(log_fd, 0, SEEK_SET);
	}

	time(&now);
	snprintf(buffer, ALARMMGR_LOG_BUFFER_STRING_SIZE, "[%-6d] %-20s %-120s %d-%s", log_index, tag, message, (int)now, ctime(&now));

	int ret = write(log_fd, buffer, strlen(buffer));
	if (ret < 0) {
		ALARM_MGR_EXCEPTION_PRINT("Writing the alarmmgr log is failed. err: %s", strerror(errno));
		return false;
	}

	if (++log_index >= ALARMMGR_LOG_BUFFER_SIZE) {
		log_index = 0;
	}
	return true;
}
#endif	// _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG

int __display_lock_state(char *state, char *flag, unsigned int timeout)
{
	GDBusMessage *msg = NULL;
	GDBusMessage *reply = NULL;
	GVariant *body = NULL;
	int ret = ALARMMGR_RESULT_SUCCESS;
	int val = -1;

	msg = g_dbus_message_new_method_call(DEVICED_BUS_NAME, DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY, DEVICED_LOCK_STATE);
	if (!msg) {
		ALARM_MGR_EXCEPTION_PRINT("g_dbus_message_new_method_call() is failed. (%s:%s-%s)", DEVICED_BUS_NAME, DEVICED_INTERFACE_DISPLAY, DEVICED_LOCK_STATE);
		return ERR_ALARM_SYSTEM_FAIL;
	}

	g_dbus_message_set_body(msg, g_variant_new("(sssi)", state, flag, "NULL", timeout));

	reply =  g_dbus_connection_send_message_with_reply_sync(alarm_context.connection, msg, G_DBUS_SEND_MESSAGE_FLAGS_NONE, DEVICED_DBUS_REPLY_TIMEOUT, NULL, NULL, NULL);
	if (!reply) {
		ALARM_MGR_EXCEPTION_PRINT("No reply. g_dbus_connection_send_message_with_reply_sync() is failed.");
		ret = ERR_ALARM_SYSTEM_FAIL;
	} else {
		body = g_dbus_message_get_body(reply);
		if (!body) {
			ALARM_MGR_EXCEPTION_PRINT("g_dbus_message_get_body() is failed.");
			ret = ERR_ALARM_SYSTEM_FAIL;
		} else {
			g_variant_get(body, "(i)", &val);
			if (val != 0) {
				ALARM_MGR_EXCEPTION_PRINT("Failed to lock display");
				ret = ERR_ALARM_SYSTEM_FAIL;
			} else {
				ALARM_MGR_EXCEPTION_PRINT("Lock LCD OFF is successfully done");
			}
		}
	}

	g_dbus_connection_flush_sync(alarm_context.connection, NULL, NULL);
	g_object_unref(msg);
	g_object_unref(reply);

	return ret;
}

int __display_unlock_state(char *state, char *flag)
{
	GDBusMessage *msg = NULL;
	GDBusMessage *reply = NULL;
	GVariant *body = NULL;
	int ret = ALARMMGR_RESULT_SUCCESS;
	int val = -1;

	msg = g_dbus_message_new_method_call(DEVICED_BUS_NAME, DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY, DEVICED_UNLOCK_STATE);
	if (!msg) {
		ALARM_MGR_EXCEPTION_PRINT("g_dbus_message_new_method_call() is failed. (%s:%s-%s)", DEVICED_BUS_NAME, DEVICED_INTERFACE_DISPLAY, DEVICED_UNLOCK_STATE);
		return ERR_ALARM_SYSTEM_FAIL;
	}

	g_dbus_message_set_body(msg, g_variant_new("(ss)", state, flag ));

	reply =  g_dbus_connection_send_message_with_reply_sync(alarm_context.connection, msg, G_DBUS_SEND_MESSAGE_FLAGS_NONE, DEVICED_DBUS_REPLY_TIMEOUT, NULL, NULL, NULL);
	if (!reply) {
		ALARM_MGR_EXCEPTION_PRINT("No reply. g_dbus_connection_send_message_with_reply_sync() is failed.");
		ret = ERR_ALARM_SYSTEM_FAIL;
	} else {
		body = g_dbus_message_get_body(reply);
		if (!body) {
			ALARM_MGR_EXCEPTION_PRINT("g_dbus_message_get_body() is failed.");
			ret = ERR_ALARM_SYSTEM_FAIL;
		} else {
			g_variant_get(body, "(i)", &val);
			if (val != 0) {
				ALARM_MGR_EXCEPTION_PRINT("Failed to unlock display");
				ret = ERR_ALARM_SYSTEM_FAIL;
			} else {
				ALARM_MGR_EXCEPTION_PRINT("Unlock LCD OFF is successfully done");
			}
		}
	}

	g_dbus_connection_flush_sync(alarm_context.connection, NULL, NULL);
	g_object_unref(msg);
	g_object_unref(reply);

	return ret;
}

static long __get_proper_interval(long interval)
{
	GSList *gs_iter = NULL;
	__alarm_info_t *entry = NULL;
	long maxInterval = 60;

	for (gs_iter = alarm_context.alarms; gs_iter != NULL; gs_iter = g_slist_next(gs_iter)) {
		entry = gs_iter->data;
		if (entry->alarm_info.alarm_type & ALARM_TYPE_PERIOD) {
			if (entry->alarm_info.mode.u_interval.interval <= interval &&
					entry->alarm_info.mode.u_interval.interval > maxInterval) {
				maxInterval = entry->alarm_info.mode.u_interval.interval;
			}
		}
	}

	while (maxInterval * 2 <= interval) {
		maxInterval *= 2;
	}

	return maxInterval;
}

gboolean __alarm_expired_directly(gpointer user_data)
{
	if (g_scheduled_alarm_list == NULL || g_scheduled_alarm_list->data == NULL) {
		return false;
	}

	int time_sec = (int)user_data;
	__scheduled_alarm_t *alarm = g_scheduled_alarm_list->data;
	__alarm_info_t *alarm_info = alarm->__alarm_info;

	// Expire alarms with duetime equal to newtime by force
	if (alarm_info->due_time == time_sec) {
		if (__display_lock_state(DEVICED_LCD_OFF, DEVICED_STAY_CUR_STATE, 0) != ALARMMGR_RESULT_SUCCESS) {
			ALARM_MGR_EXCEPTION_PRINT("__display_lock_state() is failed");
		}

		if (g_dummy_timer_is_set == true) {
			ALARM_MGR_LOG_PRINT("dummy alarm timer has expired.");
		}
		else {
			ALARM_MGR_LOG_PRINT("due_time=%d is expired.", alarm_info->due_time);
			__alarm_expired();
		}

		_alarm_schedule();
		__rtc_set();

		if (__display_unlock_state(DEVICED_LCD_OFF, DEVICED_SLEEP_MARGIN) != ALARMMGR_RESULT_SUCCESS) {
			ALARM_MGR_EXCEPTION_PRINT("__display_unlock_state() is failed");
		}
	}

	return false;
}

void __reschedule_alarms_with_newtime(int cur_time, int new_time, double diff_time)
{
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	char log_message[ALARMMGR_LOG_MESSAGE_SIZE] = {0,};
#endif

	vconf_set_int(VCONFKEY_SYSTEM_TIME_CHANGED,(int)diff_time);

	__alarm_update_due_time_of_all_items_in_list(diff_time);	// Rescheduling alarms with ALARM_TYPE_RELATIVE
	ALARM_MGR_LOG_PRINT("Next duetime is %d", alarm_context.c_due_time);

	_clear_scheduled_alarm_list();
	_alarm_schedule();
	__rtc_set();

#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	char *timebuf = ctime(&new_time);
	timebuf[strlen(timebuf) - 1] = '\0';	// to avoid newline
	sprintf(log_message, "Current: %d, New: %d, %s, diff: %f", cur_time, new_time, timebuf, diff_time);
	__save_module_log("CHANGE TIME", log_message);
#endif

	g_idle_add(__alarm_expired_directly, (gpointer)new_time);	// Expire alarms with duetime equal to newtime directly
	return;
}

gboolean alarm_manager_alarm_set_rtc_time(AlarmManager *pObj, GDBusMethodInvocation *invoc,
				int year, int mon, int day,
				int hour, int min, int sec,
				gpointer user_data) {
	const char *rtc = default_rtc;
	struct timespec alarm_time;
	int retval = 0;
	int return_code = ALARMMGR_RESULT_SUCCESS;
	struct tm *alarm_tm = NULL;
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	char log_tag[ALARMMGR_LOG_TAG_SIZE] = {0,};
	char log_message[ALARMMGR_LOG_MESSAGE_SIZE] = {0,};
#endif
	/*extract day of the week, day in the year & daylight saving time from system*/
	time_t current_time;

	current_time = time(NULL);
	alarm_tm = localtime(&current_time);

	alarm_tm->tm_year = year;
	alarm_tm->tm_mon = mon;
	alarm_tm->tm_mday = day;
	alarm_tm->tm_hour = hour;
	alarm_tm->tm_min = min;
	alarm_tm->tm_sec = sec;

	/*convert to calendar time representation*/
	time_t rtc_time = mktime(alarm_tm);

	if (gfd == 0) {
		gfd = open(rtc, O_RDWR);
		if (gfd == -1) {
			ALARM_MGR_EXCEPTION_PRINT("RTC open failed.");
			return_code = ERR_ALARM_SYSTEM_FAIL;
			g_dbus_method_invocation_return_value(invoc, g_variant_new("(i)", return_code));
			return true;
		}
	}

	alarm_time.tv_sec = rtc_time;
	alarm_time.tv_nsec = 0;

	retval = ioctl(gfd, ALARM_SET(ALARM_RTC_WAKEUP), &alarm_time);
	if (retval == -1) {
		if (errno == ENOTTY) {
			ALARM_MGR_EXCEPTION_PRINT("Alarm IRQs is not supported.");
		}
		ALARM_MGR_EXCEPTION_PRINT("RTC ALARM_SET ioctl is failed. errno = %s", strerror(errno));
		return_code = ERR_ALARM_SYSTEM_FAIL;
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		strncpy(log_tag, "FAIL: SET RTC", strlen("FAIL: SET RTC"));
#endif
	}
	else{
		ALARM_MGR_LOG_PRINT("[alarm-server]RTC alarm is setted");
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		strncpy(log_tag, "SET RTC", strlen("SET RTC"));
#endif
	}

#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	sprintf(log_message, "wakeup rtc time: %d, %s", (int)rtc_time, ctime(&rtc_time));
	__save_module_log(log_tag, log_message);
#endif

	g_dbus_method_invocation_return_value(invoc, g_variant_new("(i)", return_code));
	return true;
}

static int accrue_msec = 0;	// To check a millisecond part of current time at changing the system time(sec)

gboolean alarm_manager_alarm_set_time(AlarmManager *pObj, GDBusMethodInvocation *invoc, int time_sec, gpointer user_data)
{
	double diff_time = 0.0;
	struct timeval cur_time = {0,};
	int return_code = ALARMMGR_RESULT_SUCCESS;

	_alarm_disable_timer(alarm_context);	// Disable the timer to reschedule the alarm before the time is changed.

	tzset();
	gettimeofday(&cur_time, NULL);

	accrue_msec += (cur_time.tv_usec / 1000);	// Accrue the millisecond to compensate the time
	if (accrue_msec > 500) {
		diff_time = difftime(time_sec, cur_time.tv_sec) - 1;
		accrue_msec -= 1000;
	} else {
		diff_time = difftime(time_sec, cur_time.tv_sec);
	}

	__set_time(time_sec);	// Change both OS time and RTC
	ALARM_MGR_EXCEPTION_PRINT("[TIMESTAMP]Current time(%d), New time(%d)(%s), diff_time(%f)",
									cur_time.tv_sec, time_sec, ctime(&time_sec), diff_time);

	__reschedule_alarms_with_newtime(cur_time.tv_sec, time_sec, diff_time);
	g_dbus_method_invocation_return_value(invoc, g_variant_new("(i)", return_code));
	return true;
}

gboolean alarm_manager_alarm_set_time_with_propagation_delay(AlarmManager *pObj, GDBusMethodInvocation *invoc,
	guint new_sec, guint new_nsec, guint req_sec, guint req_nsec, gpointer user_data)
{
	double diff_time = 0.0;
	struct timespec cur_time = {0,};
	struct timespec delay = {0,};
	struct timespec sleep_time = {0,};
	guint real_newtime = 0;
	accrue_msec = 0;		// reset accrued msec

	_alarm_disable_timer(alarm_context);	// Disable the timer to reschedule the alarm before the time is changed.

	tzset();
	clock_gettime(CLOCK_REALTIME, &cur_time);

	// Check validation of requested time
	if (req_sec > cur_time.tv_sec || (req_sec == cur_time.tv_sec && req_nsec > cur_time.tv_nsec)) {
		ALARM_MGR_EXCEPTION_PRINT("The requeted time(%d.%09d) must be equal to or less than current time(%d.%09d).",
			req_sec, req_nsec, cur_time.tv_sec, cur_time.tv_nsec);
		g_dbus_method_invocation_return_value(invoc, g_variant_new("(i)", ERR_ALARM_INVALID_PARAM));
		return true;
	}

	// Compensate propagation delay
	if (req_nsec > cur_time.tv_nsec) {
		delay.tv_sec = cur_time.tv_sec - 1 - req_sec;
		delay.tv_nsec = cur_time.tv_nsec + BILLION - req_nsec;
	} else {
		delay.tv_sec = cur_time.tv_sec - req_sec;
		delay.tv_nsec = cur_time.tv_nsec - req_nsec;
	}

	if (new_nsec + delay.tv_nsec >= BILLION) {
		real_newtime = new_sec + delay.tv_sec + 2;
		sleep_time.tv_nsec = BILLION - ((delay.tv_nsec + new_nsec) - BILLION);
	} else {
		real_newtime = new_sec + delay.tv_sec + 1;
		sleep_time.tv_nsec = BILLION - (delay.tv_nsec + new_nsec);
	}

	nanosleep(&sleep_time, NULL);	// Wait until 0 nsec to match both OS time and RTC(sec)

	__set_time(real_newtime);	// Change both OS time and RTC

	diff_time = difftime(real_newtime, cur_time.tv_sec);
	ALARM_MGR_EXCEPTION_PRINT("[TIMESTAMP]Current time(%d.%09d), New time(%d.%09d), Real Newtime(%d), diff_time(%f)",
		cur_time.tv_sec, cur_time.tv_nsec, new_sec, new_nsec, real_newtime, diff_time);
	ALARM_MGR_LOG_PRINT("Requested(%d.%09d) Delay(%d.%09d) Sleep(%09d)", req_sec, req_nsec, delay.tv_sec, delay.tv_nsec, sleep_time.tv_nsec);
	__reschedule_alarms_with_newtime(cur_time.tv_sec, real_newtime, diff_time);
	g_dbus_method_invocation_return_value(invoc, g_variant_new("(i)", ALARMMGR_RESULT_SUCCESS));
	return true;
}

gboolean alarm_manager_alarm_set_timezone(AlarmManager *pObject, GDBusMethodInvocation *invoc, char *tzpath_str, gpointer user_data)
{
	int retval = 0;
	int return_code = ALARMMGR_RESULT_SUCCESS;
	struct stat statbuf;
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	char log_tag[ALARMMGR_LOG_TAG_SIZE] = {0,};
	char log_message[ALARMMGR_LOG_MESSAGE_SIZE] = {0,};
#endif

	ALARM_MGR_EXCEPTION_PRINT("[TIMESTAMP]Set the timezone to %s.", tzpath_str);

	if (stat(tzpath_str, &statbuf) == -1 && errno == ENOENT) {
		ALARM_MGR_EXCEPTION_PRINT("Invalid tzpath, %s", tzpath_str);
		return_code = ERR_ALARM_INVALID_PARAM;
		goto done;
	}

	retval = stat(TIMEZONE_INFO_LINK_PATH, &statbuf);
	if (retval == 0 || (retval == -1 && errno != ENOENT)) {
		// unlink the current link
		if (unlink(TIMEZONE_INFO_LINK_PATH) < 0) {
			ALARM_MGR_EXCEPTION_PRINT("unlink() is failed.");
			return_code = ERR_ALARM_SYSTEM_FAIL;
			goto done;
		}
	}

	// create a new symlink when the /opt/etc/localtime is empty.
	if (symlink(tzpath_str, TIMEZONE_INFO_LINK_PATH) < 0) {
		ALARM_MGR_EXCEPTION_PRINT("Failed to create an symlink of %s.", tzpath_str);
		return_code = ERR_ALARM_SYSTEM_FAIL;
		goto done;
	}

	tzset();

	// Rescheduling alarms
	_alarm_disable_timer(alarm_context);
	__alarm_update_due_time_of_all_items_in_list(0);
	ALARM_MGR_LOG_PRINT("next expiring due_time is %d", alarm_context.c_due_time);

	_clear_scheduled_alarm_list();
	_alarm_schedule();
	__rtc_set();

	vconf_set_int(VCONFKEY_SYSTEM_TIME_CHANGED, 0);

done:
	g_dbus_method_invocation_return_value(invoc, g_variant_new("(i)", return_code));
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	if (return_code == ALARMMGR_RESULT_SUCCESS) {
		strncpy(log_tag, "SET TIMEZONE", strlen("SET TIMEZONE"));
	} else {
		strncpy(log_tag, "FAIL: SET TIMEZONE", strlen("FAIL: SET TIMEZONE"));
	}
	sprintf(log_message, "Set the timezone to %s.", tzpath_str);
	__save_module_log(log_tag, log_message);
#endif

	return true;
}

gboolean alarm_manager_alarm_create_appsvc(AlarmManager *pObject, GDBusMethodInvocation *invoc,
				    int start_year,
				    int start_month, int start_day,
				    int start_hour, int start_min,
				    int start_sec, int end_year, int end_month,
				    int end_day, int mode_day_of_week,
				    int mode_repeat, int alarm_type,
				    int reserved_info,
				    char *bundle_data,
				    gpointer user_data)
{
	alarm_info_t alarm_info;
	int retval = 0;
	int return_code = ALARMMGR_RESULT_SUCCESS;
	int alarm_id = 0;
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	char log_tag[ALARMMGR_LOG_TAG_SIZE] = {0,};
	char log_message[ALARMMGR_LOG_MESSAGE_SIZE] = {0,};
#endif
	bool ret = true;
	int pid;
	const char *name = g_dbus_method_invocation_get_sender(invoc);

	pid = __get_caller_pid(name);

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

	if (!__alarm_create_appsvc(&alarm_info, &alarm_id, pid, bundle_data, &return_code)) {
		ALARM_MGR_EXCEPTION_PRINT("Unable to create alarm! return_code[%d]", return_code);
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		strncpy(log_tag, "FAIL: CREATE", strlen("FAIL: CREATE"));
#endif
		ret = false;
	}
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	else {
		strncpy(log_tag, "CREATE", strlen("CREATE"));
	}
#endif

	g_dbus_method_invocation_return_value(invoc, g_variant_new("(ii)", alarm_id, return_code));

#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	sprintf(log_message, "alarmID: %d, pid: %d, duetime: %d-%d-%d %02d:%02d:%02d",
		alarm_id, pid, start_year, start_month, start_day, start_hour, start_min, start_sec);
	__save_module_log(log_tag, log_message);
#endif

	return ret;
}

gboolean alarm_manager_alarm_create(AlarmManager *obj, GDBusMethodInvocation *invoc,
				    char *app_service_name, char *app_service_name_mod,  int start_year,
				    int start_month, int start_day,
				    int start_hour, int start_min,
				    int start_sec, int end_year, int end_month,
				    int end_day, int mode_day_of_week,
				    int mode_repeat, int alarm_type,
				    int reserved_info,
				    char *reserved_service_name, char *reserved_service_name_mod,
				    gpointer user_data)
{
	alarm_info_t alarm_info;
	int retval = 0;
	int return_code = ALARMMGR_RESULT_SUCCESS;
	int alarm_id = 0;
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	char log_tag[ALARMMGR_LOG_TAG_SIZE] = {0,};
	char log_message[ALARMMGR_LOG_MESSAGE_SIZE] = {0,};
#endif
	bool ret = true;
	int pid;
	const char *name = g_dbus_method_invocation_get_sender(invoc);

	pid = __get_caller_pid(name);

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

	if (!__alarm_create(&alarm_info, &alarm_id, pid, 0, 0, 0, app_service_name,app_service_name_mod,
		       reserved_service_name, reserved_service_name_mod, &return_code)) {
		ALARM_MGR_EXCEPTION_PRINT("Unable to create alarm! return_code[%d]", return_code);
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		strncpy(log_tag, "FAIL: CREATE", strlen("FAIL: CREATE"));
#endif
		ret = false;
	}
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	else {
		strncpy(log_tag, "CREATE", strlen("CREATE"));
	}
#endif

	g_dbus_method_invocation_return_value(invoc, g_variant_new("(ii)", alarm_id, return_code));

#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	sprintf(log_message, "alarmID: %d, pid: %d, duetime: %d-%d-%d %02d:%02d:%02d",
		alarm_id, pid, start_year, start_month, start_day, start_hour, start_min, start_sec);
	__save_module_log(log_tag, log_message);
#endif

	return ret;
}

gboolean alarm_manager_alarm_create_periodic(AlarmManager *obj, GDBusMethodInvocation *invoc,
		char *app_service_name, char *app_service_name_mod, int interval,
		int is_ref, int method, gpointer user_data)
{
	alarm_info_t alarm_info;
	int retval = 0;
	int return_code = ALARMMGR_RESULT_SUCCESS;
	int alarm_id = 0;
	char log_tag[ALARMMGR_LOG_TAG_SIZE] = {0,};
	char log_message[ALARMMGR_LOG_MESSAGE_SIZE] = {0,};
	bool ret = true;
	int pid;
	const char *name = g_dbus_method_invocation_get_sender(invoc);

	pid = __get_caller_pid(name);

	if (retval != ALARMMGR_RESULT_SUCCESS) {
		return_code = retval;
		g_dbus_method_invocation_return_value(invoc,
			g_variant_new("(ii)", alarm_id, return_code));
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		if (is_ref)
			sprintf(log_message,
				"pid: %d, Smack denied (alarm-server::alarm-ref-periodic, w)", pid);
		else
			sprintf(log_message,
				"pid: %d, Smack denied (alarm-server::alarm-periodic, w)", pid);

		__save_module_log("FAIL: CREATE", log_message);
#endif
		return true;
	}

	alarm_info.start.year = 1900;
	alarm_info.start.month = 1;
	alarm_info.start.day = 0;
	alarm_info.start.hour = 0;
	alarm_info.start.min = 0;
	alarm_info.start.sec = 0;

	alarm_info.end.year = 0;
	alarm_info.end.month = 0;
	alarm_info.end.day = 0;

	alarm_info.alarm_type = ALARM_TYPE_VOLATILE;
	alarm_info.alarm_type |= ALARM_TYPE_RELATIVE;
	alarm_info.alarm_type |= ALARM_TYPE_WITHCB;
	alarm_info.alarm_type |= ALARM_TYPE_PERIOD;
	alarm_info.reserved_info = 0;

	if (interval <= 0) {
		alarm_info.mode.repeat = ALARM_REPEAT_MODE_ONCE;
		alarm_info.mode.u_interval.interval = 0;
	} else {
		alarm_info.mode.repeat = ALARM_REPEAT_MODE_REPEAT;
		if (is_ref)
			alarm_info.mode.u_interval.interval = interval * 60;
		else
			alarm_info.mode.u_interval.interval = __get_proper_interval(interval * 60);
	}

	if (!__alarm_create(&alarm_info, &alarm_id, pid, method, interval * 60, is_ref,
	                    app_service_name, app_service_name_mod,
	                    "null", "null", &return_code)) {
		ALARM_MGR_EXCEPTION_PRINT("Unable to create alarm! return_code[%d]", return_code);
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		strncpy(log_tag, "FAIL: CREATE", strlen("FAIL: CREATE"));
#endif
		ret = false;
	} else {
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		strncpy(log_tag, "CREATE", strlen("CREATE"));
#endif
		ret = true;
	}

	g_dbus_method_invocation_return_value(invoc,
			g_variant_new("(ii)", alarm_id, return_code));
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	sprintf(log_message, "alarmID: %d, pid: %d, duetime: %d-%d-%d %02d:%02d:%02d",
			alarm_id, pid, alarm_info.start.year, alarm_info.start.month,
			alarm_info.start.day, alarm_info.start.hour,
			alarm_info.start.min, alarm_info.start.sec);
	__save_module_log(log_tag, log_message);
#endif
	return ret;
}

gboolean alarm_manager_alarm_delete(AlarmManager *obj, GDBusMethodInvocation *invoc,
		alarm_id_t alarm_id, gpointer user_data)
{
	int retval = 0;
	int return_code = ALARMMGR_RESULT_SUCCESS;
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	char log_tag[ALARMMGR_LOG_TAG_SIZE] = {0,};
	char log_message[ALARMMGR_LOG_MESSAGE_SIZE] = {0,};
#endif
	bool ret = true;
	int pid;
	const char *name = g_dbus_method_invocation_get_sender(invoc);

	pid = __get_caller_pid(name);


	if (!__alarm_delete(pid, alarm_id, &return_code)) {
		ALARM_MGR_EXCEPTION_PRINT("Unable to delete the alarm! alarm_id[%d], return_code[%d]", alarm_id, return_code);
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		strncpy(log_tag, "FAIL: DELETE", strlen("FAIL: DELETE"));
#endif
		ret = false;
	} else {
		ALARM_MGR_EXCEPTION_PRINT("alarm_id[%d] is removed.", alarm_id);
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		strncpy(log_tag, "DELETE", strlen("DELETE"));
#endif
	}

	g_dbus_method_invocation_return_value(invoc, g_variant_new("(i)", return_code));

#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	sprintf(log_message, "alarmID: %d, pid: %d", alarm_id, pid);
	__save_module_log(log_tag, log_message);
#endif

	return ret;
}

gboolean alarm_manager_alarm_delete_all(AlarmManager *obj, GDBusMethodInvocation *invoc,
					gpointer user_data)
{
	GSList* gs_iter = NULL;
	char app_name[512] = { 0 };
	alarm_info_t* alarm_info = NULL;
	__alarm_info_t* entry = NULL;
	bool is_deleted = false;
	int retval = 0;
	int return_code = ALARMMGR_RESULT_SUCCESS;
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	char log_message[ALARMMGR_LOG_MESSAGE_SIZE] = {0,};
#endif
	int pid;
	const char *name = g_dbus_method_invocation_get_sender(invoc);

	pid = __get_caller_pid(name);

	if (!__get_caller_unique_name(pid, app_name)) {
		return_code = ERR_ALARM_SYSTEM_FAIL;
		g_dbus_method_invocation_return_value(invoc, g_variant_new("(i)", return_code));
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		sprintf(log_message, "pid: %d. Can not get the unique_name.", pid);
		__save_module_log("FAIL: DELETE ALL", log_message);
#endif
		return true;
	}

	SECURE_LOGD("Called by process (pid:%d, unique_name=%s)", pid, app_name);

	for (gs_iter = alarm_context.alarms; gs_iter != NULL; )
	{
		bool is_found = false;
		entry = gs_iter->data;
		const char* tmp_appname = g_quark_to_string(entry->quark_app_unique_name);
		SECURE_LOGD("Try to remove app_name[%s], alarm_id[%d]\n", tmp_appname, entry->alarm_id);
		if (tmp_appname && strncmp(app_name, tmp_appname, strlen(tmp_appname)) == 0)
		{
			if (_remove_from_scheduled_alarm_list(pid, entry->alarm_id))
			{
				is_deleted = true;
			}

			alarm_info = &entry->alarm_info;
			if (!(alarm_info->alarm_type & ALARM_TYPE_VOLATILE))
			{
				if(!_delete_alarms(entry->alarm_id))
				{
					SECURE_LOGE("_delete_alarms() is failed. pid[%d], alarm_id[%d]", pid, entry->alarm_id);
				}
			}
			is_found = true;
		}

		gs_iter = g_slist_next(gs_iter);

		if (is_found)
		{
			ALARM_MGR_EXCEPTION_PRINT("alarm_id[%d] is removed.", entry->alarm_id);
			SECURE_LOGD("Removing is done. app_name[%s], alarm_id [%d]\n", tmp_appname, entry->alarm_id);
			alarm_context.alarms = g_slist_remove(alarm_context.alarms, entry);
			g_free(entry);
		}
	}

	if (is_deleted && (g_slist_length(g_scheduled_alarm_list) == 0))
	{
		_alarm_disable_timer(alarm_context);
		_alarm_schedule();
	}

#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	sprintf(log_message, "pid: %d, unique_name: %s", pid, app_name);
	__save_module_log("DELETE ALL", log_message);
#endif

	__rtc_set();
	g_dbus_method_invocation_return_value(invoc, g_variant_new("(i)", return_code));
	return true;
}

gboolean alarm_manager_alarm_update(AlarmManager *pObj, GDBusMethodInvocation *invoc,
				    char *app_service_name, alarm_id_t alarm_id,
				    int start_year, int start_month,
				    int start_day, int start_hour,
				    int start_min, int start_sec, int end_year,
				    int end_month, int end_day,
				    int mode_day_of_week, int mode_repeat,
				    int alarm_type, int reserved_info,
				    gpointer user_data)
{
	int return_code = ALARMMGR_RESULT_SUCCESS;
	alarm_info_t alarm_info;
	bool ret = true;
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	char log_tag[ALARMMGR_LOG_TAG_SIZE] = {0,};
	char log_message[ALARMMGR_LOG_MESSAGE_SIZE] = {0,};
#endif
	int pid;
	const char *name = g_dbus_method_invocation_get_sender(invoc);

	pid = __get_caller_pid(name);

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

	if (!__alarm_update(pid, app_service_name, alarm_id, &alarm_info, &return_code)) {
		ALARM_MGR_EXCEPTION_PRINT("Unable to update the alarm! alarm_id[%d], return_code[%d]", alarm_id, return_code);
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
		strncpy(log_tag, "FAIL: UPDATE", strlen("FAIL: UPDATE"));
#endif
		ret = false;
	}
#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	else {
		strncpy(log_tag, "UPDATE", strlen("UPDATE"));
	}
#endif

	g_dbus_method_invocation_return_value(invoc, g_variant_new("(i)", return_code));

#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	sprintf(log_message, "alarmID: %d, appname: %s, pid: %d, duetime: %d-%d-%d %02d:%02d:%02d",
		alarm_id, app_service_name, pid, start_year, start_month, start_day, start_hour, start_min, start_sec);
	__save_module_log(log_tag, log_message);
#endif

	return ret;
}

gboolean alarm_manager_alarm_get_number_of_ids(AlarmManager *pObject, GDBusMethodInvocation *invoc,
					       gpointer user_data)
{
	GSList *gs_iter = NULL;
	char app_name[256] = { 0 };
	__alarm_info_t *entry = NULL;
	int retval = 0;
	int num_of_ids = 0;
	int return_code = ALARMMGR_RESULT_SUCCESS;
	int pid;
	const char *name = g_dbus_method_invocation_get_sender(invoc);

	pid = __get_caller_pid(name);

	if (!__get_caller_unique_name(pid, app_name)) {
		return_code = ERR_ALARM_SYSTEM_FAIL;
		g_dbus_method_invocation_return_value(invoc, g_variant_new("(ii)", num_of_ids, return_code));
		return true;
	}

	SECURE_LOGD("Called by process (pid:%d, unique_name:%s)", pid, app_name);

	for (gs_iter = alarm_context.alarms; gs_iter != NULL; gs_iter = g_slist_next(gs_iter)) {
		entry = gs_iter->data;
		SECURE_LOGD("app_name=%s, quark_app_unique_name=%s", app_name, g_quark_to_string(entry->quark_app_unique_name));
		if (strncmp(app_name, g_quark_to_string(entry->quark_app_unique_name), strlen(app_name)) == 0) {
			(num_of_ids)++;
			SECURE_LOGD("inc number of alarms of app (pid:%d, unique_name:%s) is %d.", pid, app_name, num_of_ids);
		}
	}

	SECURE_LOGD("number of alarms of the process (pid:%d, unique_name:%s) is %d.", pid, app_name, num_of_ids);
	g_dbus_method_invocation_return_value(invoc, g_variant_new("(ii)", num_of_ids, return_code));
	return true;
}

gboolean alarm_manager_alarm_get_list_of_ids(AlarmManager *pObject, GDBusMethodInvocation *invoc,
					     int max_number_of_ids, gpointer user_data)
{
	GSList *gs_iter = NULL;
	char app_name[512] = { 0 };
	__alarm_info_t *entry = NULL;
	int index = 0;
	GVariant* arr = NULL;
	GVariantBuilder* builder = NULL;
	int num_of_ids = 0;
	int return_code = ALARMMGR_RESULT_SUCCESS;
	int pid;
	const char *name = g_dbus_method_invocation_get_sender(invoc);

	pid = __get_caller_pid(name);

	if (max_number_of_ids <= 0) {
		SECURE_LOGE("called for pid(%d), but max_number_of_ids(%d) is less than 0.", pid, max_number_of_ids);
		g_dbus_method_invocation_return_value(invoc, g_variant_new("(@aiii)", g_variant_new("ai", NULL), num_of_ids, return_code));
		return true;
	}

	if (!__get_caller_unique_name(pid, app_name)) {
		return_code = ERR_ALARM_SYSTEM_FAIL;
		g_dbus_method_invocation_return_value(invoc, g_variant_new("(@aiii)", g_variant_new("ai", NULL), num_of_ids, return_code));
		return true;
	}

	SECURE_LOGD("Called by process (pid:%d, unique_name=%s).", pid, app_name);

	builder = g_variant_builder_new(G_VARIANT_TYPE ("ai"));
	for (gs_iter = alarm_context.alarms; gs_iter != NULL; gs_iter = g_slist_next(gs_iter)) {
		entry = gs_iter->data;
		if (strncmp(app_name, g_quark_to_string(entry->quark_app_unique_name), strlen(app_name)) == 0) {
			g_variant_builder_add (builder, "i", entry->alarm_id);
			index ++;
			SECURE_LOGE("called for alarmid(%d), but max_number_of_ids(%d) index %d.", entry->alarm_id, max_number_of_ids, index);
		}
	}

	arr = g_variant_new("ai", builder);
	num_of_ids = index;

	SECURE_LOGE("Called by pid (%d), but max_number_of_ids(%d) return code %d.", pid, num_of_ids, return_code);
	g_dbus_method_invocation_return_value(invoc, g_variant_new("(@aiii)", arr, num_of_ids, return_code));

	g_variant_builder_unref(builder);
	return true;
}

gboolean alarm_manager_alarm_get_appsvc_info(AlarmManager *pObject, GDBusMethodInvocation *invoc,
				alarm_id_t alarm_id, gpointer user_data)
{
	bool found = false;
	GSList *gs_iter = NULL;
	__alarm_info_t *entry = NULL;
	int retval = 0;
	int return_code = ALARMMGR_RESULT_SUCCESS;
	gchar *b_data = NULL;

	SECURE_LOGD("called for alarm_id(%d)\n", alarm_id);

	for (gs_iter = alarm_context.alarms; gs_iter != NULL; gs_iter = g_slist_next(gs_iter)) {
		entry = gs_iter->data;
		if (entry->alarm_id == alarm_id) {
			found = true;
			b_data = g_strdup(g_quark_to_string(entry->quark_bundle));
			break;
		}
	}

	if (found) {
		if (b_data && strlen(b_data) == 4 && strncmp(b_data, "null", 4) == 0) {
			ALARM_MGR_EXCEPTION_PRINT("The alarm(%d) is an regular alarm, not svc alarm.", alarm_id);
			return_code = ERR_ALARM_INVALID_TYPE;
		}
	} else {
		ALARM_MGR_EXCEPTION_PRINT("The alarm(%d) is not found.", alarm_id);
		return_code = ERR_ALARM_INVALID_ID;
	}

	g_dbus_method_invocation_return_value(invoc, g_variant_new("(si)", b_data, return_code));
	g_free(b_data);
	return true;
}

gboolean alarm_manager_alarm_get_info(AlarmManager *pObject, GDBusMethodInvocation *invoc,
				      alarm_id_t alarm_id, gpointer user_data)
{
	GSList *gs_iter = NULL;
	__alarm_info_t *entry = NULL;
	alarm_info_t *alarm_info = NULL;
	int retval = 0;
	int return_code = ALARMMGR_RESULT_SUCCESS;

	SECURE_LOGD("called for alarm_id(%d)\n", alarm_id);
	for (gs_iter = alarm_context.alarms; gs_iter != NULL; gs_iter = g_slist_next(gs_iter)) {
		entry = gs_iter->data;
		if (entry->alarm_id == alarm_id) {
			alarm_info = &(entry->alarm_info);
			break;
		}
	}

	if (alarm_info == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("The alarm(%d) is not found.", alarm_id);
		return_code = ERR_ALARM_INVALID_ID;
		g_dbus_method_invocation_return_value(invoc, g_variant_new("(iiiiiiiiiiiiii)", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, return_code));
	} else {
		ALARM_MGR_LOG_PRINT("The alarm(%d) is found.", alarm_id);
		g_dbus_method_invocation_return_value(invoc, g_variant_new("(iiiiiiiiiiiiii)", alarm_info->start.year, alarm_info->start.month,
							alarm_info->start.day, alarm_info->start.hour, alarm_info->start.min, alarm_info->start.sec, alarm_info->end.year, alarm_info->end.month,
							alarm_info->end.day, alarm_info->mode.u_interval.day_of_week, alarm_info->mode.repeat, alarm_info->alarm_type, alarm_info->reserved_info, return_code));
	}

	return true;
}

gboolean alarm_manager_alarm_get_next_duetime(AlarmManager *pObject, GDBusMethodInvocation *invoc,
				      alarm_id_t alarm_id, gpointer user_data)
{
	GSList *gs_iter = NULL;
	__alarm_info_t *entry = NULL;
	__alarm_info_t *find_item = NULL;
	int retval = 0;
	int return_code = ALARMMGR_RESULT_SUCCESS;
	time_t duetime = 0;

	SECURE_LOGD("called for alarm_id(%d)\n", alarm_id);
	for (gs_iter = alarm_context.alarms; gs_iter != NULL; gs_iter = g_slist_next(gs_iter)) {
		entry = gs_iter->data;
		if (entry->alarm_id == alarm_id) {
			find_item = entry;
			break;
		}
	}

	if (find_item == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("The alarm(%d) is not found.", alarm_id);
		return_code = ERR_ALARM_INVALID_ID;
		g_dbus_method_invocation_return_value(invoc, g_variant_new("(ii)", duetime, return_code));
		return true;
	}

	duetime = _alarm_next_duetime(find_item);
	ALARM_MGR_LOG_PRINT("Next duetime : %s", ctime(&duetime));

	g_dbus_method_invocation_return_value(invoc, g_variant_new("(ii)", duetime, return_code));
	return true;
}

gboolean alarm_manager_alarm_get_all_info(AlarmManager *pObject, GDBusMethodInvocation *invoc, gpointer user_data)
{
	sqlite3 *alarmmgr_tool_db;
	char *db_path = NULL;
	char db_path_tmp[50] = {0,};
	time_t current_time = 0;
	struct tm current_tm;
	const char *query_for_creating_table =  "create table alarmmgr_tool \
					(alarm_id integer primary key,\
							duetime_epoch integer,\
							duetime text,\
							start_epoch integer,\
							end_epoch integer,\
							pid integer,\
							caller_pkgid text,\
							callee_pkgid text,\
							app_unique_name text,\
							app_service_name text,\
							dst_service_name text,\
							day_of_week integer,\
							repeat integer,\
							alarm_type integer)";
	const char *query_for_deleting_table = "drop table alarmmgr_tool";
	int return_code = ALARMMGR_RESULT_SUCCESS;
	GSList *gs_iter = NULL;
	__alarm_info_t *entry = NULL;
	char *error_message = NULL;

	// Open a DB
	time(&current_time);
	localtime_r(&current_time, &current_tm);
	sprintf(db_path_tmp, "/tmp/alarmmgr_%d%d%d_%02d%02d%02d.db",
		current_tm.tm_year + 1900, current_tm.tm_mon + 1, current_tm.tm_mday, current_tm.tm_hour, current_tm.tm_min, current_tm.tm_sec);
	db_path = strdup(db_path_tmp);

	if (db_util_open(db_path, &alarmmgr_tool_db, DB_UTIL_REGISTER_HOOK_METHOD) != SQLITE_OK) {
		ALARM_MGR_EXCEPTION_PRINT("Opening [%s] failed", db_path);
		return_code = ERR_ALARM_SYSTEM_FAIL;
		g_dbus_method_invocation_return_value(invoc, g_variant_new("(si)", db_path, return_code));
		free(db_path);
		return true;
	}

	// Drop a table
	if (sqlite3_exec(alarmmgr_tool_db, query_for_deleting_table, NULL, NULL, &error_message) != SQLITE_OK) {
		ALARM_MGR_EXCEPTION_PRINT("Deleting the table is failed. error message = %s", error_message);
	}

	// Create a table if it does not exist
	if (sqlite3_exec(alarmmgr_tool_db, query_for_creating_table, NULL, NULL, &error_message) != SQLITE_OK) {
		ALARM_MGR_EXCEPTION_PRINT("Creating the table is failed. error message = %s", error_message);
		sqlite3_close(alarmmgr_tool_db);
		return_code = ERR_ALARM_SYSTEM_FAIL;
		g_dbus_method_invocation_return_value(invoc, g_variant_new("(si)", db_path, return_code));
		free(db_path);
		return true;
	}

	// Get information of all alarms and save those into the DB.
	int index = 0;
	for (gs_iter = alarm_context.alarms; gs_iter != NULL; gs_iter = g_slist_next(gs_iter)) {
		entry = gs_iter->data;
		++index;
		SECURE_LOGD("#%d alarm id[%d] app_name[%s] duetime[%d]",
			index, entry->alarm_id, g_quark_to_string(entry->quark_app_unique_name), entry->start);

		alarm_info_t *alarm_info = (alarm_info_t *) &(entry->alarm_info);
		alarm_mode_t *mode = &alarm_info->mode;

		char *query = sqlite3_mprintf("insert into alarmmgr_tool( alarm_id, duetime_epoch, duetime, start_epoch,\
				end_epoch, pid, caller_pkgid, callee_pkgid, app_unique_name, app_service_name, dst_service_name, day_of_week, repeat, alarm_type)\
				values (%d,%d,%Q,%d,%d,%d,%Q,%Q,%Q,%Q,%Q,%d,%d,%d)",
				entry->alarm_id,
				(int)entry->due_time,
				ctime(&(entry->due_time)),
				(int)entry->start,
				(int)entry->end,
				(int)entry->pid,
				(char *)g_quark_to_string(entry->quark_caller_pkgid),
				(char *)g_quark_to_string(entry->quark_callee_pkgid),
				(char *)g_quark_to_string(entry->quark_app_unique_name),
				(char *)g_quark_to_string(entry->quark_app_service_name),
				(char *)g_quark_to_string(entry->quark_dst_service_name),
				mode->u_interval.day_of_week,
				mode->repeat,
				entry->alarm_info.alarm_type);

		if (sqlite3_exec(alarmmgr_tool_db, query, NULL, NULL, &error_message) != SQLITE_OK) {
			SECURE_LOGE("sqlite3_exec() is failed. error message = %s", error_message);
		}

		sqlite3_free(query);
	}

	sqlite3_close(alarmmgr_tool_db);

	return_code = ALARMMGR_RESULT_SUCCESS;
	g_dbus_method_invocation_return_value(invoc, g_variant_new("(si)", db_path, return_code));
	free(db_path);
	return true;
}

static void __timer_glib_finalize(GSource *src)
{
	GSList *fd_list;
	GPollFD *tmp;

	fd_list = src->poll_fds;
	do {
		tmp = (GPollFD *) fd_list->data;
		g_free(tmp);

		fd_list = fd_list->next;
	} while (fd_list);

	return;
}

static gboolean __timer_glib_check(GSource *src)
{
	GSList *fd_list;
	GPollFD *tmp;

	fd_list = src->poll_fds;
	do {
		tmp = (GPollFD *) fd_list->data;
		if (tmp->revents & (POLLIN | POLLPRI)) {
			return TRUE;
		}
		fd_list = fd_list->next;
	} while (fd_list);

	return FALSE;
}

static gboolean __timer_glib_dispatch(GSource *src, GSourceFunc callback,
				  gpointer data)
{
	callback(data);
	return TRUE;
}

static gboolean __timer_glib_prepare(GSource *src, gint *timeout)
{
	return FALSE;
}

GSourceFuncs funcs = {
	.prepare = __timer_glib_prepare,
	.check = __timer_glib_check,
	.dispatch = __timer_glib_dispatch,
	.finalize = __timer_glib_finalize
};

static void __initialize_timer()
{
	int fd;
	GSource *src;
	GPollFD *gpollfd;
	int ret;

	fd = timerfd_create(CLOCK_REALTIME, 0);
	if (fd == -1) {
		ALARM_MGR_EXCEPTION_PRINT("timerfd_create() is failed.\n");
		exit(1);
	}
	src = g_source_new(&funcs, sizeof(GSource));

	gpollfd = (GPollFD *) g_malloc(sizeof(GPollFD));
	gpollfd->events = POLLIN;
	gpollfd->fd = fd;

	g_source_add_poll(src, gpollfd);
	g_source_set_callback(src, (GSourceFunc) __alarm_handler_idle,
			      (gpointer) gpollfd, NULL);
	g_source_set_priority(src, G_PRIORITY_HIGH);

	ret = g_source_attach(src, NULL);
	if (ret == 0) {
		ALARM_MGR_EXCEPTION_PRINT("g_source_attach() is failed.\n");
		return;
	}

	g_source_unref(src);

	alarm_context.timer = fd;
}

static void __initialize_alarm_list()
{
	alarm_context.alarms = NULL;
	alarm_context.c_due_time = -1;

	_load_alarms_from_registry();

	__rtc_set();	/*Set RTC1 Alarm with alarm due time for alarm-manager initialization*/
}

static void __initialize_scheduled_alarm_list()
{
	_init_scheduled_alarm_list();
}

static bool __initialize_noti()
{
	// VCONFKEY_SYSTEM_TIMECHANGE_EXTERNAL is set by OSP app-service.
	if (vconf_notify_key_changed
	    (VCONFKEY_SYSTEM_TIMECHANGE_EXTERNAL, __on_system_time_external_changed, NULL) < 0) {
		ALARM_MGR_LOG_PRINT("Failed to add callback for time external changing event.");
	}

	// If the caller or callee app is uninstalled, all registered alarms will be canceled.
	int event_type = PMINFO_CLIENT_STATUS_UNINSTALL;
	pkgmgrinfo_client* pc = pkgmgrinfo_client_new(PMINFO_LISTENING);
	pkgmgrinfo_client_set_status_type(pc, event_type);
	pkgmgrinfo_client_listen_status(pc, __on_app_uninstalled, NULL);

	return true;
}

void on_bus_name_owner_changed(GDBusConnection  *connection,
										const gchar		*sender_name,
										const gchar		*object_path,
										const gchar		*interface_name,
										const gchar		*signal_name,
										GVariant			*parameters,
										gpointer			user_data)
{
	// On expiry, the killed app can be launched by aul. Then, the owner of the bus name which was registered by the app is changed.
	// In this case, "NameOwnerChange" signal is broadcasted.
	if (signal_name && strcmp(signal_name , "NameOwnerChanged") == 0) {
		GSList *entry = NULL;
		__expired_alarm_t *expire_info = NULL;
		char *service_name = NULL;
		g_variant_get(parameters, "(sss)", &service_name, NULL, NULL);

		for (entry = g_expired_alarm_list; entry; entry = entry->next) {
			if (entry->data) {
				expire_info = (__expired_alarm_t *) entry->data;
				SECURE_LOGD("expired service(%s), owner changed service(%s)", expire_info->service_name, service_name);

				if (strcmp(expire_info->service_name, service_name) == 0) {
					SECURE_LOGE("expired service name(%s) alarm_id (%d)", expire_info->service_name, expire_info->alarm_id);
					__alarm_send_noti_to_application(expire_info->service_name, expire_info->alarm_id);
					g_expired_alarm_list = g_slist_remove(g_expired_alarm_list, entry->data);
					g_free(expire_info);
				}
			}
		}
		g_free(service_name);
	}
}

static void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	ALARM_MGR_EXCEPTION_PRINT("on_bus_acquired");

	interface = alarm_manager_skeleton_new();
	if (interface == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("Creating a skeleton object is failed.");
		return;
	}

	g_signal_connect(interface, "handle_alarm_create", G_CALLBACK(alarm_manager_alarm_create), NULL);
	g_signal_connect(interface, "handle_alarm_create_periodic", G_CALLBACK(alarm_manager_alarm_create_periodic), NULL);
	g_signal_connect(interface, "handle_alarm_create_appsvc", G_CALLBACK(alarm_manager_alarm_create_appsvc), NULL);
	g_signal_connect(interface, "handle_alarm_delete", G_CALLBACK(alarm_manager_alarm_delete), NULL);
	g_signal_connect(interface, "handle_alarm_delete_all", G_CALLBACK(alarm_manager_alarm_delete_all), NULL);
	g_signal_connect(interface, "handle_alarm_get_appsvc_info", G_CALLBACK(alarm_manager_alarm_get_appsvc_info), NULL);
	g_signal_connect(interface, "handle_alarm_get_info", G_CALLBACK(alarm_manager_alarm_get_info), NULL);
	g_signal_connect(interface, "handle_alarm_get_list_of_ids", G_CALLBACK(alarm_manager_alarm_get_list_of_ids), NULL);
	g_signal_connect(interface, "handle_alarm_get_next_duetime", G_CALLBACK(alarm_manager_alarm_get_next_duetime), NULL);
	g_signal_connect(interface, "handle_alarm_get_number_of_ids", G_CALLBACK(alarm_manager_alarm_get_number_of_ids), NULL);
	g_signal_connect(interface, "handle_alarm_set_rtc_time", G_CALLBACK(alarm_manager_alarm_set_rtc_time), NULL);
	g_signal_connect(interface, "handle_alarm_set_time", G_CALLBACK(alarm_manager_alarm_set_time), NULL);
	g_signal_connect(interface, "handle_alarm_set_timezone", G_CALLBACK(alarm_manager_alarm_set_timezone), NULL);
	g_signal_connect(interface, "handle_alarm_update", G_CALLBACK(alarm_manager_alarm_update), NULL);
	g_signal_connect(interface, "handle_alarm_get_all_info", G_CALLBACK(alarm_manager_alarm_get_all_info), NULL);
	g_signal_connect(interface, "handle_alarm_set_time_with_propagation_delay", G_CALLBACK(alarm_manager_alarm_set_time_with_propagation_delay), NULL);

	guint subsc_id = g_dbus_connection_signal_subscribe(connection, "org.freedesktop.DBus", "org.freedesktop.DBus",
							"NameOwnerChanged", "/org/freedesktop/DBus", NULL, G_DBUS_SIGNAL_FLAGS_NONE,
							on_bus_name_owner_changed, NULL, NULL);
	if (subsc_id == 0) {
		ALARM_MGR_EXCEPTION_PRINT("Subscribing to signal for invoking callback is failed.");
		g_object_unref(interface);
		interface = NULL;
		return;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(interface), connection, ALARM_MGR_DBUS_PATH, NULL)) {
		ALARM_MGR_EXCEPTION_PRINT("Exporting the interface is failed.");
		g_object_unref(interface);
		interface = NULL;
		return;
	}

	alarm_context.connection = connection;
	g_dbus_object_manager_server_set_connection(alarmmgr_server, alarm_context.connection);
}

static bool __initialize_dbus()
{
	ALARM_MGR_LOG_PRINT("__initialize_dbus Enter");

	alarmmgr_server = g_dbus_object_manager_server_new(ALARM_MGR_DBUS_PATH);
	if (alarmmgr_server == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("Creating a new server object is failed.");
		return false;
	}

	guint owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM, ALARM_MGR_DBUS_NAME,
				G_BUS_NAME_OWNER_FLAGS_NONE, on_bus_acquired, NULL, NULL, NULL, NULL);

	if (owner_id == 0) {
		ALARM_MGR_EXCEPTION_PRINT("Acquiring the own name is failed.");
		g_object_unref(alarmmgr_server);
		return false;
	}

	return true;
}

#define ALARMMGR_DB_FILE tzplatform_mkpath(TZ_SYS_DB, ".alarmmgr.db")
#define QUERY_CREATE_TABLE_ALARMMGR "create table alarmmgr \
				(alarm_id integer primary key,\
						start integer,\
						end integer,\
						pid integer,\
						caller_pkgid text,\
						callee_pkgid text,\
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

	if (access(ALARMMGR_DB_FILE, F_OK) == 0) {
		ret = db_util_open(ALARMMGR_DB_FILE, &alarmmgr_db, DB_UTIL_REGISTER_HOOK_METHOD);

		if (ret != SQLITE_OK) {
			ALARM_MGR_EXCEPTION_PRINT("====>>>> connect menu_db [%s] failed", ALARMMGR_DB_FILE);
			return false;
		}

		return true;
	}

	ret = db_util_open(ALARMMGR_DB_FILE, &alarmmgr_db, DB_UTIL_REGISTER_HOOK_METHOD);

	if (ret != SQLITE_OK) {
		ALARM_MGR_EXCEPTION_PRINT("====>>>> connect menu_db [%s] failed", ALARMMGR_DB_FILE);
		return false;
	}

	if (SQLITE_OK != sqlite3_exec(alarmmgr_db, QUERY_CREATE_TABLE_ALARMMGR, NULL, NULL, &error_message)) {
		ALARM_MGR_EXCEPTION_PRINT("Don't execute query = %s, error message = %s", QUERY_CREATE_TABLE_ALARMMGR, error_message);
		return false;
	}

	return true;
}

static void __initialize()
{
	g_type_init();

	__initialize_timer();
	if (__initialize_dbus() == false) {	/* because dbus's initialize
					failed, we cannot continue any more. */
		ALARM_MGR_EXCEPTION_PRINT("because __initialize_dbus failed, "
					  "alarm-server cannot be runned.\n");
		exit(1);
	}

#ifdef _APPFW_FEATURE_ALARM_MANAGER_MODULE_LOG
	__initialize_module_log();	// for module log
#endif

	__initialize_scheduled_alarm_list();
	__initialize_db();
	__initialize_alarm_list();
	__initialize_noti();
}

int main()
{
	GMainLoop *mainloop = NULL;

	ALARM_MGR_LOG_PRINT("Enter main loop\n");

	mainloop = g_main_loop_new(NULL, FALSE);

	__initialize();

	g_main_loop_run(mainloop);

	return 0;
}
