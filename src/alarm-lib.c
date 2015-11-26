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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <glib.h>
#include <fcntl.h>

#include "alarm.h"
#include "alarm-internal.h"
#include "alarm-mgr-stub.h"
#include <bundle.h>
#include <appsvc.h>
#include <aul.h>
#include <gio/gio.h>
#include <pkgmgr-info.h>

#ifndef EXPORT_API
#define EXPORT_API __attribute__ ((visibility("default")))
#endif

static alarm_context_t alarm_context = { NULL, NULL, 0, NULL, NULL, -1 };

static bool b_initialized = false;
static bool sub_initialized = false;

pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;

static void __handle_expired_signal(GDBusConnection *conn,
		const gchar *name, const gchar *path, const gchar *interface,
		const gchar *signal_name, GVariant *param, gpointer user_data);

static int __alarm_validate_date(alarm_date_t *date, int *error_code);
static bool __alarm_validate_time(alarm_date_t *date, int *error_code);
static int __sub_init(void);
static int __alarmmgr_init_appsvc(void);

typedef struct _alarm_cb_info_t {
	alarm_id_t alarm_id;
	alarm_cb_t cb_func;
	void *priv_data;
	struct _alarm_cb_info_t *next;
} alarm_cb_info_t;

static alarm_cb_info_t *alarmcb_head = NULL;

static void __add_resultcb(alarm_id_t alarm_id, alarm_cb_t cb_func, void *data)
{
	alarm_cb_info_t *info;

	info = (alarm_cb_info_t *) malloc(sizeof(alarm_cb_info_t));
	if(info == NULL)
		return;
	info->alarm_id = alarm_id;
	info->cb_func = cb_func;
	info->priv_data = data;

	info->next = alarmcb_head;
	alarmcb_head = info;
}

static alarm_cb_info_t *__find_resultcb(alarm_id_t alarm_id)
{
	alarm_cb_info_t *tmp;

	tmp = alarmcb_head;
	while (tmp) {
		if (tmp->alarm_id == alarm_id) {
			ALARM_MGR_LOG_PRINT("matched alarm id =  %d", alarm_id);
			return tmp;
		}
		tmp = tmp->next;
	}
	return NULL;
}

static void __remove_resultcb(alarm_cb_info_t *info)
{
	alarm_cb_info_t *tmp;

	if (alarmcb_head == NULL || info == NULL)
		return;

	if (alarmcb_head == info) {
		alarmcb_head = info->next;
		free(info);
		return;
	}

	tmp = alarmcb_head;
	while (tmp) {
		if (tmp->next == info) {
			tmp->next = info->next;
			free(info);
			return;
		}
		tmp = tmp->next;
	}
}

static void __handle_expired_signal(GDBusConnection *conn,
		const gchar *name, const gchar *path, const gchar *interface,
		const gchar *signal_name, GVariant *param, gpointer user_data)
{
	gchar *package_name = NULL;
	alarm_id_t alarm_id = 0;
	alarm_cb_info_t *info;

	if (signal_name == NULL || strcmp(signal_name, "alarm_expired") != 0)
		ALARM_MGR_EXCEPTION_PRINT("[alarm-lib] : unexpected signal");

	g_variant_get(param, "(is)", &alarm_id, &package_name);
	ALARM_MGR_LOG_PRINT("[alarm-lib] : Alarm expired for [%s] : Alarm id [%d]", package_name, alarm_id);

	if (alarm_context.alarm_handler != NULL) {
		alarm_context.alarm_handler(alarm_id, alarm_context.user_param);
	}

	info = __find_resultcb(alarm_id);
	if (info && info->cb_func) {
		info->cb_func(alarm_id, info->priv_data);
	}
	g_free(package_name);
}

static int __alarm_validate_date(alarm_date_t *date, int *error_code)
{

	if (date->year == 0 && date->month == 0 && date->day == 0) {
		return true;
	}

	int year = date->year;
	int month = date->month;
	int day = date->day;

	if (month < 1 || month > 12) {
		if (error_code)
			*error_code = ERR_ALARM_INVALID_DATE;
		return false;
	}

	if ((month == 1 || month == 3 || month == 5 || month == 7 || month == 8
	     || month == 10 || month == 12)
	    && (day < 1 || day > 31)) {
		if (error_code)
			*error_code = ERR_ALARM_INVALID_DATE;
		return false;
	}

	if ((month == 4 || month == 6 || month == 9 || month == 11)
	    && (day < 1 || day > 30)) {
		if (error_code)
			*error_code = ERR_ALARM_INVALID_DATE;
		return false;
	}

	if (month == 2) {
		if ((year % 100 != 0 && year % 4 == 0) || (year % 400 == 0)) {
			if (day < 1 || day > 29) {
				if (error_code)
					*error_code = ERR_ALARM_INVALID_DATE;
				return false;
			}
		} else {
			if (day < 1 || day > 28) {
				if (error_code)
					*error_code = ERR_ALARM_INVALID_DATE;
				return false;
			}
		}

	}

	return true;

}

static bool __alarm_validate_time(alarm_date_t *date, int *error_code)
{
	if (date->hour < 0 || date->hour > 23) {
		if (error_code)
			*error_code = ERR_ALARM_INVALID_TIME;
		return false;
	}

	if (date->min < 0 || date->min > 59) {
		if (error_code)
			*error_code = ERR_ALARM_INVALID_TIME;
		return false;
	}

	return true;
}

static int __compare_api_version(int *result, uid_t uid) {
	int ret = 0;
	pkgmgrinfo_pkginfo_h pkginfo = NULL;
	char pkgid[512] = {0, };
	char *pkg_version;

	if (aul_app_get_pkgid_bypid_for_uid(getpid(), pkgid, sizeof(pkgid), uid) != AUL_R_OK) {
		ALARM_MGR_EXCEPTION_PRINT("aul_app_get_pkgid_bypid() is failed. PID %d may not be app.", getpid());
	} else {
		ret = pkgmgrinfo_pkginfo_get_usr_pkginfo(pkgid, uid, &pkginfo);
		if (ret != PMINFO_R_OK) {
			ALARM_MGR_EXCEPTION_PRINT("Failed to get pkginfo\n");
		} else {
			ret = pkgmgrinfo_pkginfo_get_api_version(pkginfo, &pkg_version);
			if (ret != PMINFO_R_OK) {
				ALARM_MGR_EXCEPTION_PRINT("Failed to check api version [%d]\n", ret);
			}
			*result = strverscmp(pkg_version, "2.4");
			pkgmgrinfo_pkginfo_destroy_pkginfo(pkginfo);
		}
	}
	return ret;
}

static int __bg_category_func(const char *name, void *user_data)
{
	bg_category_cb_info_t *info = (bg_category_cb_info_t *)user_data;
	ALARM_MGR_LOG_PRINT("appid[%s], bg name = %s", info->appid, name);
	if (name &&
			strncmp("enable", name, strlen(name)) && strncmp("disable", name, strlen(name))) {
		info->has_bg = true;
		return -1;
	}

	return 0;
}

static bool __is_permitted(const char *app_id, int alarm_type)
{
	if (app_id == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("app_id is NULL. Only expicit launch is permitted\n");
		return false;
	}

	pkgmgrinfo_appinfo_h handle = NULL;
	int ret;
	bool _return = false;

	ret = pkgmgrinfo_appinfo_get_usr_appinfo(app_id, getuid(), &handle);
	if (ret != PMINFO_R_OK) {
		ALARM_MGR_EXCEPTION_PRINT("Failed to get appinfo [%s]\n", app_id);
	} else {
		char *app_type = NULL;
		ret = pkgmgrinfo_appinfo_get_component_type(handle, &app_type);
		if (app_type && strcmp("uiapp", app_type) == 0) {
			ALARM_MGR_LOG_PRINT("[%s] is ui application. It is allowed", app_id);
			_return = true;
			goto out;
		} else if (app_type && strcmp("svcapp", app_type) == 0) {
			ALARM_MGR_LOG_PRINT("[%s] is service application.", app_id);

			bg_category_cb_info_t info = {
				.appid = app_id,
				.has_bg = false
			};

			if (alarm_type & ALARM_TYPE_INEXACT) {
				ret = pkgmgrinfo_appinfo_foreach_background_category(handle, __bg_category_func, &info);
				if (ret == PMINFO_R_OK && info.has_bg) {
					ALARM_MGR_LOG_PRINT("[%s] has background categories.", app_id);
					_return = true;
					goto out;
				} else {
					ALARM_MGR_EXCEPTION_PRINT("Failed to foreach background category. [%s] is not allowed\n", app_id);
				}
			}
		}
	}

out :
	if (handle)
		pkgmgrinfo_appinfo_destroy_appinfo(handle);
	return _return;
}

static int __sub_init()
{
	GError *error = NULL;
	int fd = 0;
	int ret = 0;

	pthread_mutex_lock(&init_lock);

	if (sub_initialized) {
		pthread_mutex_unlock(&init_lock);
		return ALARMMGR_RESULT_SUCCESS;
	}

#if !(GLIB_CHECK_VERSION(2, 32, 0))
	g_thread_init(NULL);
#endif
#if !(GLIB_CHECK_VERSION(2, 36, 0))
	g_type_init();
#endif

	alarm_context.connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (alarm_context.connection == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("g_bus_get_sync() is failed. error: %s", error->message);
		g_error_free(error);
		pthread_mutex_unlock(&init_lock);
		return ERR_ALARM_SYSTEM_FAIL;
	}

	alarm_context.proxy = g_dbus_proxy_new_sync(alarm_context.connection,
							G_DBUS_PROXY_FLAGS_NONE,
							NULL,
							"org.tizen.alarm.manager",
							"/org/tizen/alarm/manager",
							"org.tizen.alarm.manager",
							NULL,
							NULL);

	if (alarm_context.proxy == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("Creating a proxy is failed.");
		g_object_unref (alarm_context.connection);
		pthread_mutex_unlock(&init_lock);
		return ERR_ALARM_SYSTEM_FAIL;
	}

	sub_initialized = true;

	pthread_mutex_unlock(&init_lock);

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_init(const char *appid)
{
	SECURE_LOGD("Enter");
	char service_name[MAX_SERVICE_NAME_LEN] = { 0 };
	int ret;
	int len = 0;

	if (appid == NULL)
		return ERR_ALARM_INVALID_PARAM;

	if (strlen(appid) >= MAX_PKG_NAME_LEN)
		return ERR_ALARM_INVALID_PARAM;

	if (b_initialized) {
		SECURE_LOGD("alarm was already initialized. app_service_name=%s",
		     g_quark_to_string(alarm_context.quark_app_service_name));
		return ALARMMGR_RESULT_SUCCESS;
	}

	ret = __sub_init();
	if (ret < 0)
		return ret;

	alarm_context.sid = g_dbus_connection_signal_subscribe(
			alarm_context.connection,
			NULL,
			"org.tizen.alarm.manager",
			"alarm_expired",
			"/org/tizen/alarm/manager",
			NULL,
			G_DBUS_SIGNAL_FLAGS_NONE,
			__handle_expired_signal,
			NULL,
			NULL);

	len = strlen("ALARM.");
	strncpy(service_name, "ALARM.", len);
	strncpy(service_name + len, appid, strlen(appid));

	alarm_context.quark_app_service_name = g_quark_from_string(service_name);

	b_initialized = true;

	SECURE_LOGD("Leave");
	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API void alarmmgr_fini()
{
	SECURE_LOGD("Enter");

	g_dbus_connection_signal_unsubscribe(alarm_context.connection,
			alarm_context.sid);
	alarm_context.sid = 0;

	if (alarm_context.proxy) {
		g_object_unref(alarm_context.proxy);
		alarm_context.proxy = NULL;
	}

	if (alarm_context.connection) {
		g_object_unref(alarm_context.connection);
		alarm_context.connection = NULL;
	}

	b_initialized = false;
	sub_initialized = false;

	SECURE_LOGD("Leave");
}

EXPORT_API int alarmmgr_set_cb(alarm_cb_t handler, void *user_param)
{
	SECURE_LOGD("Enter");

	if (handler == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("callback is NULL.");
		return ERR_ALARM_INVALID_PARAM;
	}
	alarm_context.alarm_handler = handler;
	alarm_context.user_param = user_param;

	SECURE_LOGD("Leave");
	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API alarm_entry_t *alarmmgr_create_alarm(void)
{
	alarm_info_t *alarm = (alarm_info_t *) malloc(sizeof(alarm_info_t));

	if (NULL == alarm)
	{
		return NULL;
	}

	alarm->start.year = 0;
	alarm->start.month = 0;
	alarm->start.day = 0;
	alarm->start.hour = 0;
	alarm->start.min = 0;
	alarm->start.sec = 0;

	alarm->end.year = 0;
	alarm->end.month = 0;
	alarm->end.day = 0;
	alarm->end.hour = 0;
	alarm->end.min = 0;
	alarm->end.sec = 0;

	alarm->mode.repeat = ALARM_REPEAT_MODE_ONCE;
	alarm->mode.u_interval.interval = 0;

	alarm->alarm_type = ALARM_TYPE_DEFAULT;

	alarm->reserved_info = 0;

	return (alarm_entry_t *) alarm;
}

EXPORT_API int alarmmgr_free_alarm(alarm_entry_t *alarm)
{
	if (alarm == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}
	free(alarm);

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_set_time(alarm_entry_t *alarm, alarm_date_t time)
{
	alarm_info_t *alarm_info;	/*= (alarm_info_t*)alarm;*/
	int error_code;

	if (alarm == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	alarm_info = (alarm_info_t *) alarm;

	if (!__alarm_validate_date(&time, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("start date error\n");
		return error_code;
	}

	if (!__alarm_validate_time(&time, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("start time error\n");
		return error_code;
	}

	memcpy(&alarm_info->start, &time, sizeof(alarm_date_t));

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_get_time(const alarm_entry_t *alarm,
				 alarm_date_t *time)
{
	alarm_info_t *alarm_info = (alarm_info_t *) alarm;

	if (alarm == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (time != NULL)
		memcpy(time, &alarm_info->start, sizeof(alarm_date_t));

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_set_repeat_mode(alarm_entry_t *alarm,
					alarm_repeat_mode_t repeat,
					int interval)
{
	alarm_info_t *alarm_info = (alarm_info_t *) alarm;

	if (repeat >= ALARM_REPEAT_MODE_MAX) {
		return ERR_ALARM_INVALID_PARAM;
	}

	alarm_info->mode.repeat = repeat;

	if (repeat == ALARM_REPEAT_MODE_REPEAT
	    || repeat == ALARM_REPEAT_MODE_WEEKLY) {
		if (interval <= 0) {
			return ERR_ALARM_INVALID_PARAM;
		}
		alarm_info->mode.u_interval.interval = interval;
	}

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_get_repeat_mode(const alarm_entry_t *alarm,
					alarm_repeat_mode_t *repeat,
					int *interval)
{
	alarm_info_t *alarm_info = (alarm_info_t *) alarm;

	if (alarm == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (repeat != NULL)
		*repeat = alarm_info->mode.repeat;
	if (interval != NULL)
		*interval = alarm_info->mode.u_interval.interval;

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_set_type(alarm_entry_t *alarm, int alarm_type)
{
	alarm_info_t *alarm_info;	/*= (alarm_info_t*)alarm;*/

	if (alarm == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	alarm_info = (alarm_info_t *) alarm;

	alarm_info->alarm_type = alarm_type;
	alarm_info->alarm_type &= (~ALARM_TYPE_RELATIVE);

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_get_type(const alarm_entry_t *alarm, int *alarm_type)
{
	alarm_info_t *alarm_info = (alarm_info_t *) alarm;

	if (alarm == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (alarm_type != NULL)
		*alarm_type = alarm_info->alarm_type;

	return ALARMMGR_RESULT_SUCCESS;
}


static int __alarmmgr_init_appsvc(void)
{
	if (b_initialized) {
		ALARM_MGR_EXCEPTION_PRINT("alarm was already initialized.");
		return ALARMMGR_RESULT_SUCCESS;
	}

	int ret = __sub_init();
	if (ret < 0)
		return ret;

	b_initialized = true;
	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API void *alarmmgr_get_alarm_appsvc_info(alarm_id_t alarm_id, int *return_code){

	int ret = 0;

	ret = __sub_init();
	if (ret < 0){
		if (return_code) {
			*return_code = ret;
		}
		return NULL;
	}

	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarmmgr_get_alarm_appsvc_info() is called.");

	if (alarm_id <= 0) {
		if (return_code) {
			*return_code = ERR_ALARM_INVALID_ID;
		}
		return NULL;
	}

	return _send_alarm_get_appsvc_info(alarm_context, alarm_id, return_code);

}

EXPORT_API int alarmmgr_set_rtc_time(alarm_date_t *time){

	int ret = 0;
	int error_code = 0;

	if (!time){
		ALARM_MGR_EXCEPTION_PRINT("Invalid parameter time\n");
		return ERR_ALARM_INVALID_PARAM;
	}

	ret = __sub_init();
	if (ret < 0){
		return ret;
	}

	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarmmgr_set_rtc_time() is called\n");

	if (!__alarm_validate_date(time, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("RTC date error\n");
		return error_code;
	}

	if (!__alarm_validate_time(time, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("RTC time error\n");
		return error_code;
	}

	time->year-=1900;
	time->month-=1;

	if (!_send_alarm_set_rtc_time
		(alarm_context, time, &error_code)){
			return error_code;
	}

	return ALARMMGR_RESULT_SUCCESS;

}

EXPORT_API int alarmmgr_add_alarm_appsvc_with_localtime(alarm_entry_t *alarm, void *bundle_data, alarm_id_t *alarm_id)
{
	alarm_info_t *alarm_info = NULL;	/* = (alarm_info_t*)alarm; */
	const char *operation = NULL;
	int error_code = 0;
	const char *appid = NULL;
	int result;

	bundle *b=(bundle *)bundle_data;

	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarm_create() is called\n");

	if (alarm == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (NULL == b)
	{
		ALARM_MGR_EXCEPTION_PRINT("Invalid parameter bundle\n");
		return ERR_ALARM_INVALID_PARAM;
	}

	operation = appsvc_get_operation(b);

	if (NULL == operation)
	{
		appsvc_set_operation(b, APPSVC_OPERATION_DEFAULT);
	}

	if (__alarmmgr_init_appsvc() < 0)
	{
		ALARM_MGR_EXCEPTION_PRINT("Unable to initialize dbus!!!\n");
		return ERR_ALARM_SYSTEM_FAIL;
	}

	alarm_info = (alarm_info_t *) alarm;

	appid = appsvc_get_appid(b);

	if ( (NULL == appid && (alarm_info->alarm_type & ALARM_TYPE_NOLAUNCH)) ||
			(NULL == appid && operation && !strcmp(operation, APPSVC_OPERATION_DEFAULT)) )
	{
		ALARM_MGR_EXCEPTION_PRINT("Invalid parameter\n");
		return ERR_ALARM_INVALID_PARAM;
	}

	if (__compare_api_version(&result, getuid()) < 0)
		return ERR_ALARM_SYSTEM_FAIL;

	if (result >= 0 && !__is_permitted(appid, alarm_info->alarm_type)) {
		ALARM_MGR_EXCEPTION_PRINT("[%s] is not permitted \n", appid);
		return ERR_ALARM_NOT_PERMITTED_APP;
	}

	if (alarm_info == NULL || alarm_id == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("Invalid parameter\n");
		return ERR_ALARM_INVALID_PARAM;
	}
	alarm_mode_t *mode = &alarm_info->mode;

	ALARM_MGR_EXCEPTION_PRINT("start(%d-%d-%d, %02d:%02d:%02d), end(%d-%d-%d), repeat(%d), interval(%d), type(%d)",
		alarm_info->start.day, alarm_info->start.month, alarm_info->start.year,
		alarm_info->start.hour, alarm_info->start.min, alarm_info->start.sec,
		alarm_info->end.year, alarm_info->end.month, alarm_info->end.day,
		alarm_info->mode.repeat, alarm_info->mode.u_interval, alarm_info->alarm_type);

	/* TODO: This should be changed to > ALARM_REPEAT_MODE_MAX ? */
	if (mode->repeat >= ALARM_REPEAT_MODE_MAX) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (!__alarm_validate_date(&alarm_info->start, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("start date error\n");
		return error_code;
	}

	if (!__alarm_validate_time(&alarm_info->start, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("start time error\n");
		return error_code;
	}

	if (!__alarm_validate_date(&alarm_info->end, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("end date error\n");
		return error_code;
	}


	if (!_send_alarm_create_appsvc(alarm_context, alarm_info, alarm_id, b, &error_code)) {
		return error_code;
	}

	return ALARMMGR_RESULT_SUCCESS;
}




EXPORT_API int alarmmgr_add_alarm_with_localtime(alarm_entry_t *alarm,
						 const char *destination,
						 alarm_id_t *alarm_id)
{
	char dst_service_name[MAX_SERVICE_NAME_LEN] = { 0 };
	char dst_service_name_mod[MAX_SERVICE_NAME_LEN] = { 0 };
	alarm_info_t *alarm_info;	/* = (alarm_info_t*)alarm; */
	int ret;
	int i = 0;
	int j = 0;

	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarm_create() is called\n");

	if (alarm == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	alarm_info = (alarm_info_t *) alarm;
	if (alarm_info == NULL || alarm_id == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	int error_code;
	alarm_mode_t *mode = &alarm_info->mode;

	ret = __sub_init();
	if (ret < 0)
		return ret;

	ALARM_MGR_LOG_PRINT("start(%d-%d-%d, %02d:%02d:%02d), end(%d-%d-%d), repeat(%d), interval(%d), type(%d)",
		alarm_info->start.day, alarm_info->start.month, alarm_info->start.year,
		alarm_info->start.hour, alarm_info->start.min, alarm_info->start.sec,
		alarm_info->end.year, alarm_info->end.month, alarm_info->end.day,
		alarm_info->mode.repeat, alarm_info->mode.u_interval, alarm_info->alarm_type);

	/* TODO: This should be changed to > ALARM_REPEAT_MODE_MAX ? */
	if (mode->repeat >= ALARM_REPEAT_MODE_MAX) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (destination && strlen(destination) >= MAX_PKG_NAME_LEN){
		ALARM_MGR_EXCEPTION_PRINT("[alarm-lib]: destination name is too long!\n");
		return ERR_ALARM_INVALID_PARAM;
	}


	if (!__alarm_validate_date(&alarm_info->start, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("start date error\n");
		return error_code;
	}

	if (!__alarm_validate_time(&alarm_info->start, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("start time error\n");
		return error_code;
	}

	if (!__alarm_validate_date(&alarm_info->end, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("end date error\n");
		return error_code;
	}

	if (destination != NULL) {
		memset(dst_service_name, 0, strlen(destination) + strlen("ALARM.") + 2);
		snprintf(dst_service_name, MAX_SERVICE_NAME_LEN, "ALARM.%s", destination);
		memset(dst_service_name_mod, 'a', MAX_SERVICE_NAME_LEN-1);

		for (i=0; i<=strlen(dst_service_name); i++)
		{
			if (dst_service_name[i] == '.' )
			{
				dst_service_name_mod[j] = dst_service_name[i];
				j++;
			}
			else
			{
				dst_service_name_mod[j] = dst_service_name[i];
			}
			j++;
		}

		if (!_send_alarm_create(alarm_context, alarm_info, alarm_id, dst_service_name, dst_service_name_mod, &error_code)) {
			return error_code;
		}
	} else {
		if (!_send_alarm_create(alarm_context, alarm_info, alarm_id, "null", "null", &error_code)) {
			return error_code;
		}
	}

	return ALARMMGR_RESULT_SUCCESS;
}



EXPORT_API int alarmmgr_add_alarm_appsvc(int alarm_type, time_t trigger_at_time,
				  time_t interval, void *bundle_data,
				  alarm_id_t *alarm_id)
{
	int error_code = 0;
	int result;
	struct timeval current_time;
	struct tm duetime_tm;
	alarm_info_t alarm_info;
	const char *operation = NULL;
	const char *appid = NULL;

	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarm_create() is called\n");

	bundle *b=(bundle *)bundle_data;

	if (NULL == b)
	{
		ALARM_MGR_EXCEPTION_PRINT("Invalid parameter bundle\n");
		return ERR_ALARM_INVALID_PARAM;
	}
	operation = appsvc_get_operation(b);

	if (NULL == operation)
	{
		appsvc_set_operation(b, APPSVC_OPERATION_DEFAULT);
	}

	appid = appsvc_get_appid(b);

	if ( (NULL == appid && (alarm_type & ALARM_TYPE_NOLAUNCH)) ||
			(NULL == appid && operation && !strcmp(operation, APPSVC_OPERATION_DEFAULT)) )
	{
		ALARM_MGR_EXCEPTION_PRINT("Invalid parameter\n");
		return ERR_ALARM_INVALID_PARAM;
	}

	if (__alarmmgr_init_appsvc() < 0)
	{
		ALARM_MGR_EXCEPTION_PRINT("Unable to initialize dbus!!!\n");
		return ERR_ALARM_SYSTEM_FAIL;
	}

	if (alarm_id == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (trigger_at_time < 0) {
		return ERR_ALARM_INVALID_PARAM;
	}

	alarm_info.alarm_type = alarm_type;
	alarm_info.alarm_type |= ALARM_TYPE_RELATIVE;

	if (__compare_api_version(&result, getuid()) < 0)
		return ERR_ALARM_SYSTEM_FAIL;

	if (result < 0) {
		if (alarm_info.alarm_type & ALARM_TYPE_INEXACT) {
			alarm_info.alarm_type ^= ALARM_TYPE_INEXACT;
		}
	} else { //Since 2.4
		if (!__is_permitted(appid, alarm_info.alarm_type)) {
			ALARM_MGR_EXCEPTION_PRINT("[%s] is not permitted \n", appid);
			return ERR_ALARM_NOT_PERMITTED_APP;
		}
	}

	gettimeofday(&current_time, NULL);

	if (current_time.tv_usec > 500 * 1000)
	{
		// When the millisecond part of the current_time is bigger than 500ms,
		// the duetime increases by extra 1sec.
		current_time.tv_sec += (trigger_at_time + 1);
	}
	else
	{
		current_time.tv_sec += trigger_at_time;
	}

	tzset();	// Processes the TZ environment variable, and Set timezone, daylight, and tzname.
	localtime_r(&current_time.tv_sec, &duetime_tm);

	alarm_info.start.year = duetime_tm.tm_year + 1900;
	alarm_info.start.month = duetime_tm.tm_mon + 1;
	alarm_info.start.day = duetime_tm.tm_mday;

	alarm_info.end.year = 0;
	alarm_info.end.month = 0;
	alarm_info.end.day = 0;

	alarm_info.start.hour = duetime_tm.tm_hour;
	alarm_info.start.min = duetime_tm.tm_min;
	alarm_info.start.sec = duetime_tm.tm_sec;

	if ((alarm_info.alarm_type & ALARM_TYPE_INEXACT) && interval < MIN_INEXACT_INTERVAL) {
		interval = MIN_INEXACT_INTERVAL;
	}

	if (interval <= 0) {
		alarm_info.mode.repeat = ALARM_REPEAT_MODE_ONCE;
		alarm_info.mode.u_interval.interval = 0;
	} else {
		alarm_info.mode.repeat = ALARM_REPEAT_MODE_REPEAT;
		alarm_info.mode.u_interval.interval = interval;
	}

	ALARM_MGR_LOG_PRINT("trigger_at_time(%d), start(%d-%d-%d, %02d:%02d:%02d), repeat(%d), interval(%d), type(%d)",
		trigger_at_time, alarm_info.start.day, alarm_info.start.month, alarm_info.start.year,
		alarm_info.start.hour, alarm_info.start.min, alarm_info.start.sec,
		alarm_info.mode.repeat, alarm_info.mode.u_interval.interval, alarm_info.alarm_type);

	if (!_send_alarm_create_appsvc(alarm_context, &alarm_info, alarm_id, b, &error_code)) {
		return error_code;
	}

	return ALARMMGR_RESULT_SUCCESS;
}


EXPORT_API int alarmmgr_add_alarm(int alarm_type, time_t trigger_at_time,
				  time_t interval, const char *destination,
				  alarm_id_t *alarm_id)
{
	char dst_service_name[MAX_SERVICE_NAME_LEN] = { 0 };
	char dst_service_name_mod[MAX_SERVICE_NAME_LEN] = { 0 };
	int i = 0;
	int j = 0;
	int result;
	int error_code;
	struct timeval current_time;
	struct tm duetime_tm;
	alarm_info_t alarm_info;
	int ret;

	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarm_create() is called\n");

	ret = __sub_init();
	if (ret < 0)
		return ret;

	if (alarm_id == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (trigger_at_time < 0) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (destination && strlen(destination) >= MAX_PKG_NAME_LEN){
		ALARM_MGR_EXCEPTION_PRINT("[alarm-lib]: destination name is too long!\n");
		return ERR_ALARM_INVALID_PARAM;
	}

	alarm_info.alarm_type = alarm_type;
	alarm_info.alarm_type |= ALARM_TYPE_RELATIVE;

	gettimeofday(&current_time, NULL);

	if (current_time.tv_usec > 500 * 1000)
	{
		// When the millisecond part of the current_time is bigger than 500ms,
		// the duetime increases by extra 1sec.
		current_time.tv_sec += (trigger_at_time + 1);
	}
	else
	{
		current_time.tv_sec += trigger_at_time;
	}

	tzset();	// Processes the TZ environment variable, and Set timezone, daylight, and tzname.
	localtime_r(&current_time.tv_sec, &duetime_tm);

	alarm_info.start.year = duetime_tm.tm_year + 1900;
	alarm_info.start.month = duetime_tm.tm_mon + 1;
	alarm_info.start.day = duetime_tm.tm_mday;

	alarm_info.end.year = 0;
	alarm_info.end.month = 0;
	alarm_info.end.day = 0;

	alarm_info.start.hour = duetime_tm.tm_hour;
	alarm_info.start.min = duetime_tm.tm_min;
	alarm_info.start.sec = duetime_tm.tm_sec;

	if (interval <= 0) {
		alarm_info.mode.repeat = ALARM_REPEAT_MODE_ONCE;
		alarm_info.mode.u_interval.interval = 0;
	} else {
		alarm_info.mode.repeat = ALARM_REPEAT_MODE_REPEAT;
		alarm_info.mode.u_interval.interval = interval;
	}

	ALARM_MGR_LOG_PRINT("trigger_at_time(%d), start(%d-%d-%d, %02d:%02d:%02d), repeat(%d), interval(%d), type(%d)",
		trigger_at_time, alarm_info.start.day, alarm_info.start.month, alarm_info.start.year,
		alarm_info.start.hour, alarm_info.start.min, alarm_info.start.sec,
		alarm_info.mode.repeat, alarm_info.mode.u_interval, alarm_info.alarm_type);

	if (destination != NULL) {
		memset(dst_service_name, 0,
		       strlen(destination) + strlen("ALARM.") + 2);
		snprintf(dst_service_name, MAX_SERVICE_NAME_LEN, "ALARM.%s",
			 destination);
		memset(dst_service_name_mod,'a',MAX_SERVICE_NAME_LEN-1);

		j=0;

	        for(i=0;i<=strlen(dst_service_name);i++)
	        {
	                if (dst_service_name[i] == '.')
	                {
	                	dst_service_name_mod[j]=dst_service_name[i];
	                        j++;
	                }
	                else
	                {
				dst_service_name_mod[j]=dst_service_name[i];
	                }
	                j++;
	        }

		if (!_send_alarm_create
		    (alarm_context, &alarm_info, alarm_id, dst_service_name,dst_service_name_mod,
		     &error_code)) {
			return error_code;
		}
	} else
	    if (!_send_alarm_create
		(alarm_context, &alarm_info, alarm_id, "null","null", &error_code)) {
		return error_code;
	}

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_add_alarm_withcb(int alarm_type, time_t trigger_at_time,
				  time_t interval, alarm_cb_t handler, void *user_param, alarm_id_t *alarm_id)
{
	int error_code = 0;
	struct timeval current_time;
	struct tm duetime_tm;
	alarm_info_t alarm_info;
	int ret = 0;
	char appid[256] = {0,};

	if (aul_app_get_appid_bypid(getpid(), appid, sizeof(appid)) != AUL_R_OK) {
		ALARM_MGR_EXCEPTION_PRINT("aul_app_get_appid_bypid() is failed. PID %d may not be app.", getpid());
	}

	ret = alarmmgr_init(appid);
	if (ret < 0)
		return ret;

	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarmmgr_add_alarm_withcb() is called");

	if (alarm_id == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (trigger_at_time < 0) {
		return ERR_ALARM_INVALID_PARAM;
	}

	alarm_info.alarm_type = alarm_type;
	alarm_info.alarm_type |= ALARM_TYPE_RELATIVE;
	alarm_info.alarm_type |= ALARM_TYPE_WITHCB;

	gettimeofday(&current_time, NULL);

	if (current_time.tv_usec > 500 * 1000)
	{
		// When the millisecond part of the current_time is bigger than 500ms,
		// the duetime increases by extra 1sec.
		current_time.tv_sec += (trigger_at_time + 1);
	}
	else
	{
		current_time.tv_sec += trigger_at_time;
	}

	tzset();	// Processes the TZ environment variable, and Set timezone, daylight, and tzname.
	localtime_r(&current_time.tv_sec, &duetime_tm);

	alarm_info.start.year = duetime_tm.tm_year + 1900;
	alarm_info.start.month = duetime_tm.tm_mon + 1;
	alarm_info.start.day = duetime_tm.tm_mday;

	alarm_info.end.year = 0;
	alarm_info.end.month = 0;
	alarm_info.end.day = 0;

	alarm_info.start.hour = duetime_tm.tm_hour;
	alarm_info.start.min = duetime_tm.tm_min;
	alarm_info.start.sec = duetime_tm.tm_sec;

	if (interval <= 0) {
		alarm_info.mode.repeat = ALARM_REPEAT_MODE_ONCE;
		alarm_info.mode.u_interval.interval = 0;
	} else {
		alarm_info.mode.repeat = ALARM_REPEAT_MODE_REPEAT;
		alarm_info.mode.u_interval.interval = interval;
	}

	ALARM_MGR_LOG_PRINT("trigger_at_time(%d), start(%d-%d-%d, %02d:%02d:%02d), repeat(%d), interval(%d), type(%d)",
		trigger_at_time, alarm_info.start.day, alarm_info.start.month, alarm_info.start.year,
		alarm_info.start.hour, alarm_info.start.min, alarm_info.start.sec,
		alarm_info.mode.repeat, alarm_info.mode.u_interval.interval, alarm_info.alarm_type);

	if (!_send_alarm_create(alarm_context, &alarm_info, alarm_id, "null","null", &error_code)) {
		return error_code;
	}
	__add_resultcb(*alarm_id, handler, user_param);

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_remove_alarm(alarm_id_t alarm_id)
{
	int error_code;
	int ret;
	alarm_cb_info_t *info;
	alarm_info_t alarm;

	ret = __sub_init();
	if (ret < 0)
		return ret;

	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarm_delete(%d) is called\n", alarm_id);

	if (alarm_id <= 0) {
		return ERR_ALARM_INVALID_ID;
	}

	if (!_send_alarm_delete(alarm_context, alarm_id, &error_code))
		return error_code;

	info = __find_resultcb(alarm_id);
	__remove_resultcb(info);

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_remove_all(void)
{
	int error_code;
	int return_code = ALARMMGR_RESULT_SUCCESS;
	int ret = __sub_init();
	if (ret < 0)
	{
		return ret;
	}

	if (!_send_alarm_delete_all(alarm_context, &error_code))
		return error_code;

	return return_code;
}

EXPORT_API int alarmmgr_enum_alarm_ids(alarm_enum_fn_t fn, void *user_param)
{
	SECURE_LOGD("Enter");
	GError *error = NULL;
	GVariant *alarm_array = NULL;
	int return_code = 0;
	int maxnum_of_ids = 0;
	int num_of_ids = 0;
	alarm_id_t alarm_id = -1;
	int ret = 0;
	GVariantIter *iter = NULL;

	if (fn == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	ret = __sub_init();
	if (ret < 0) {
		ALARM_MGR_EXCEPTION_PRINT("__sub_init() is failed.");
		return ret;
	}

	SECURE_LOGD("alarm_manager_call_alarm_get_number_of_ids_sync() is called");
	if (!alarm_manager_call_alarm_get_number_of_ids_sync(
	    (AlarmManager*)alarm_context.proxy, &maxnum_of_ids, &return_code, NULL, &error)) {
		/* dbus error. error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		    "alarm_manager_call_alarm_get_number_of_ids_sync() is failed by dbus. return_code[%d], err message[%s] err code[%d]",
		    return_code, error->message, error->code);
		if (error->code == G_DBUS_ERROR_ACCESS_DENIED)
			ret = ERR_ALARM_NO_PERMISSION;
		else
			ret = ERR_ALARM_SYSTEM_FAIL;
		g_error_free(error);
		return ret;
	}

	if (return_code != ALARMMGR_RESULT_SUCCESS) {
		ALARM_MGR_EXCEPTION_PRINT("alarm_manager_call_alarm_get_number_of_ids_sync() is failed. return_code[%d]", return_code);
		return return_code;
	} else {
		ALARM_MGR_LOG_PRINT("maxnum_of_ids[%d]", maxnum_of_ids);
	}

	SECURE_LOGD("alarm_manager_call_alarm_get_list_of_ids_sync() is called");
	if (!alarm_manager_call_alarm_get_list_of_ids_sync(
		     (AlarmManager*)alarm_context.proxy, maxnum_of_ids, &alarm_array, &num_of_ids, &return_code, NULL, &error)) {
		/* dbus error. error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		    "alarm_manager_call_alarm_get_list_of_ids_sync() failed by dbus. num_of_ids[%d], return_code[%d]. err message[%s] err code[%d]", num_of_ids, return_code, error->message, error->code);
		if (error->code == G_DBUS_ERROR_ACCESS_DENIED)
			ret = ERR_ALARM_NO_PERMISSION;
		else
			ret = ERR_ALARM_SYSTEM_FAIL;
		g_error_free(error);
		return ret;
	}

	if (return_code != ALARMMGR_RESULT_SUCCESS) {
		return return_code;
	}

	if (error != NULL) {
		ALARM_MGR_EXCEPTION_PRINT("Alarm server is not ready dbus. error message %s.", error->message);
		return ERR_ALARM_SYSTEM_FAIL;
	}

	if (alarm_array == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("alarm server is not initilized.");
		return ERR_ALARM_SYSTEM_FAIL;
	}

	g_variant_get(alarm_array, "ai", &iter);
	while (g_variant_iter_loop(iter, "i", &alarm_id))
	{
		(*fn) (alarm_id, user_param);
		ALARM_MGR_LOG_PRINT("alarm_id (%d)", alarm_id);
	}
	g_variant_iter_free(iter);
	g_variant_unref(alarm_array);

	SECURE_LOGD("Leave");
	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_get_info(alarm_id_t alarm_id, alarm_entry_t *alarm)
{
	int error_code;
	alarm_info_t *alarm_info = (alarm_info_t *) alarm;

	int ret;

	ret = __sub_init();
	if (ret < 0)
		return ret;

	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarm_get_info() is called\n");

	if (alarm_id < 0 || alarm_info == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (!_send_alarm_get_info(alarm_context, alarm_id, alarm_info, &error_code)) {
		return error_code;
	}

	return ALARMMGR_RESULT_SUCCESS;
}

int alarmmgr_create(alarm_info_t *alarm_info, char *destination,
		    alarm_id_t *alarm_id)
{
	char dst_service_name[MAX_SERVICE_NAME_LEN] = { 0 };
	alarm_mode_t *mode = &alarm_info->mode;
	int error_code;

	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarm_create() is called\n");

	if (alarm_info == NULL || alarm_id == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	ALARM_MGR_LOG_PRINT("alarm_info->start.year(%d), "
			    "alarm_info->start.month(%d), alarm_info->start.day(%d)",
			    alarm_info->start.year, alarm_info->start.month,
			    alarm_info->start.day);

	/* TODO: This should be changed to > ALARM_REPEAT_MODE_MAX ? */
	if (mode->repeat >= ALARM_REPEAT_MODE_MAX) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (!__alarm_validate_date(&alarm_info->start, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("start date error\n");
		return error_code;
	}

	if (!__alarm_validate_time(&alarm_info->start, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("start time error\n");
		return error_code;
	}

	if (!__alarm_validate_date(&alarm_info->end, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("end date error\n");
		return error_code;
	}

	if (destination != NULL) {
		memset(dst_service_name, 0,
		       strlen(destination) + strlen("ALARM.") + 2);
		snprintf(dst_service_name, MAX_SERVICE_NAME_LEN, "ALARM.%s",
			 destination);
		if (!_send_alarm_create
		    (alarm_context, alarm_info, alarm_id, dst_service_name,"null",
		     &error_code)) {
			return error_code;
		}
	}
/*TODO: Currently this API is not exported. Hence not modifying*/
	if (!_send_alarm_create
	    (alarm_context, alarm_info, alarm_id, "null", "null", &error_code)) {
		return error_code;
	}

	return ALARMMGR_RESULT_SUCCESS;

}

int alarmmgr_get_number_of_ids(int *num_of_ids)
{
	int error_code;
	ALARM_MGR_LOG_PRINT("[alarm-lib]: alarm_get_number_of_ids() is called.");

	if (num_of_ids == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}
	ALARM_MGR_LOG_PRINT("call alarm_get_number_of_ids\n");
	if (!_send_alarm_get_number_of_ids(alarm_context, num_of_ids, &error_code)) {
		return error_code;
	}

	return ALARMMGR_RESULT_SUCCESS;
}

int alarmmgr_get_list_of_ids(int maxnum_of_ids, alarm_id_t *alarm_id,
			     int *num_of_ids)
{
	int error_code;
	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarm_get_list_of_ids() is called.");

	if (maxnum_of_ids < 0 || alarm_id == NULL || num_of_ids == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (maxnum_of_ids == 0) {
		*num_of_ids = 0;
		return ALARMMGR_RESULT_SUCCESS;
	}

	if (!_send_alarm_get_list_of_ids
	    (alarm_context, maxnum_of_ids, alarm_id, num_of_ids, &error_code)) {
		return error_code;
	}

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_get_next_duetime(alarm_id_t alarm_id, time_t* duetime)
{
	int error_code;
	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarmmgr_get_next_duetime() is called.");

	if (duetime == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (!_send_alarm_get_next_duetime(alarm_context, alarm_id, duetime, &error_code)) {
		return error_code;
	}

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_get_all_info(char **db_path)
{
	int error_code;
	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarmmgr_get_all_info() is called.");

	if (db_path == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (!_send_alarm_get_all_info(alarm_context, db_path, &error_code)) {
		return error_code;
	}

	ALARM_MGR_LOG_PRINT("[alarm-lib]: successfully save info in %s.", *db_path);
	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_add_periodic_alarm_withcb(int interval, periodic_method_e method,
        alarm_cb_t handler, void *user_param, alarm_id_t *alarm_id)
{
	int error_code = 0;
	alarm_info_t alarm_info;
	int ret = 0;
	char appid[256] = {0,};

	if (aul_app_get_appid_bypid(getpid(), appid, sizeof(appid)) != AUL_R_OK) {
		ALARM_MGR_EXCEPTION_PRINT("aul_app_get_appid_bypid() is failed. PID %d may not be app.",
			getpid());
	}

	ret = alarmmgr_init(appid);
	if (ret < 0)
		return ret;

	if (alarm_id == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (!_send_alarm_create_periodic(alarm_context, interval, 0, (int)method, alarm_id,
		&error_code)) {
		return error_code;
	}
	__add_resultcb(*alarm_id, handler, user_param);

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_add_reference_periodic_alarm_withcb(int interval,
        alarm_cb_t handler, void *user_param, alarm_id_t *alarm_id)
{
	int error_code = 0;
	alarm_info_t alarm_info;
	int ret = 0;
	char appid[256] = {0,};

	if (aul_app_get_appid_bypid(getpid(), appid, sizeof(appid)) != AUL_R_OK) {
		ALARM_MGR_EXCEPTION_PRINT("aul_app_get_appid_bypid() is failed. PID %d may not be app.",
			getpid());
	}

	ret = alarmmgr_init(appid);
	if (ret < 0)
		return ret;

	if (alarm_id == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (!_send_alarm_create_periodic(alarm_context, interval, 1, 0,
		alarm_id, &error_code)) {
		return error_code;
	}

	__add_resultcb(*alarm_id, handler, user_param);

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_set_systime(int new_time)
{
	int error_code;
	struct timespec req_time = {0,};
	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarmmgr_set_systime(%d) is called.", new_time);

	if (__sub_init() < 0) {
		return ERR_ALARM_SYSTEM_FAIL;
	}

	clock_gettime(CLOCK_REALTIME, &req_time);
	if (!_send_alarm_set_time_with_propagation_delay(alarm_context, new_time, 0, req_time.tv_sec, req_time.tv_nsec, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("Failed to set time with propagation delay. error: %d", error_code);
		return error_code;
	}

	ALARM_MGR_LOG_PRINT("[alarm-lib]: successfully set the time(%d) by pid(%d).", new_time, getpid());
	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_set_systime_with_propagation_delay(struct timespec new_time, struct timespec req_time)
{
	int error_code;
	ALARM_MGR_LOG_PRINT("[alarm-lib] New: %d(sec) %09d(nsec), Requested: %d(sec) %09d(nsec)",
		new_time.tv_sec, new_time.tv_nsec, req_time.tv_sec, req_time.tv_nsec);

	if (__sub_init() < 0) {
		return ERR_ALARM_SYSTEM_FAIL;
	}

	if (!_send_alarm_set_time_with_propagation_delay(alarm_context, new_time.tv_sec, new_time.tv_nsec, req_time.tv_sec, req_time.tv_nsec, &error_code)) {
		ALARM_MGR_EXCEPTION_PRINT("Failed to set time with propagation delay. error: %d", error_code);
		return error_code;
	}

	ALARM_MGR_LOG_PRINT("[alarm-lib]: successfully set the time by pid(%d).", getpid());
	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_set_timezone(char *tzpath_str)
{
	int error_code;
	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarmmgr_set_timezone() is called.");

	if (tzpath_str == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (__sub_init() < 0) {
		return ERR_ALARM_SYSTEM_FAIL;
	}

	if (!_send_alarm_set_timezone(alarm_context, tzpath_str, &error_code)) {
		return error_code;
	}

	ALARM_MGR_LOG_PRINT("[alarm-lib]: successfully set the timezone(%s) by pid(%d)", tzpath_str, getpid());
	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_set_global(const alarm_id_t alarm_id,
					bool global)
{
	int error_code;
	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarmmgr_set_global() is called.");

	if (!_send_alarm_set_global(alarm_context, alarm_id, global, &error_code)) {
		return error_code;
	}

	return ALARMMGR_RESULT_SUCCESS;
}


EXPORT_API int alarmmgr_get_global(const alarm_id_t alarm_id,
					bool *global)
{
	bool _global;
	int error_code;

	if (global == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (!_send_alarm_get_global(alarm_context, alarm_id, &_global, &error_code)) {
		return error_code;
	}

	return ALARMMGR_RESULT_SUCCESS;
}

