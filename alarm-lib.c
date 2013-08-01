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
#include<errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include<sys/types.h>
#include<string.h>
#include<dbus/dbus.h>
#include<dbus/dbus-glib.h>
#include<glib.h>
#include <fcntl.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "alarm.h"
#include "alarm-internal.h"
#include "alarm-stub.h"
#include <bundle.h>
#include <appsvc.h>
#include <aul.h>

#define MAX_KEY_SIZE 256

static alarm_context_t alarm_context = { NULL, NULL, NULL, NULL, -1 };

static bool b_initialized = false;
static bool sub_initialized = false;

#define MAX_OBJECT_PATH_LEN 256
#define DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT 0

static DBusHandlerResult __expire_alarm_filter(DBusConnection *connection,
					       DBusMessage *message,
					       void *user_data);
static int __alarm_validate_date(alarm_date_t *date, int *error_code);
static bool __alarm_validate_time(alarm_date_t *date, int *error_code);
static int __sub_init(void);
static int __alarmmgr_init_appsvc(void);
bool alarm_power_off(int *error_code);
int alarmmgr_check_next_duetime(void);

typedef struct _alarm_cb_info_t {
	int alarm_id;
	alarm_cb_t cb_func;
	void *priv_data;
	struct _alarm_cb_info_t *next;
} alarm_cb_info_t;

static alarm_cb_info_t *alarmcb_head = NULL;

static void __add_resultcb(int alarm_id, alarm_cb_t cb_func,
			 void *data)
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

static alarm_cb_info_t *__find_resultcb(int alarm_id)
{
	alarm_cb_info_t *tmp;

	tmp = alarmcb_head;
	while (tmp) {
		if (tmp->alarm_id == alarm_id)
			return tmp;
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

static DBusHandlerResult __expire_alarm_filter(DBusConnection *connection,
					       DBusMessage *message,
					       void *user_data)
{
	alarm_cb_info_t *info;

	if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_METHOD_CALL) {
		const char *method_name = dbus_message_get_member(message);
		/*"alarm_expired" */

		if (strcmp(method_name, "alarm_expired") == 0) {
			DBusMessageIter iter;
			alarm_id_t alarm_id;
			const char *service_name =
			    dbus_message_get_destination(message);
			const char *object_path =
			    dbus_message_get_path(message);
			/* "/org/tizen/alarm/client" */
			const char *interface_name =
			    dbus_message_get_interface(message);
			/* "org.tizen.alarm.client" */

			dbus_message_iter_init(message, &iter);
			dbus_message_iter_get_basic(&iter, &alarm_id);

			ALARM_MGR_LOG_PRINT("[alarm-lib]:service_name=%s, "
			"object_path=%s, interface_name=%s, method_name=%s, "
			"alarm_id=%d, handler=%s\n",
			service_name ? service_name : "no name",
			object_path ? object_path : "no path",
			interface_name ? interface_name : "no interface",
			method_name ? method_name : "no method", alarm_id,
			alarm_context.alarm_handler ? "ok" : "no handler");

			if (alarm_context.alarm_handler != NULL)
				/* alarm_context.alarm_handler(alarm_id); */
				alarm_context.alarm_handler(alarm_id,
					alarm_context.user_param);
			info = __find_resultcb(alarm_id);

			if( info && info->cb_func ) {
				info->cb_func(alarm_id, info->priv_data);
			//	__remove_resultcb(info);
			}

			return DBUS_HANDLER_RESULT_HANDLED;
		}
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
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

static int __sub_init()
{
	GError *error = NULL;

	if (sub_initialized) {
		//ALARM_MGR_LOG_PRINT("__sub_init was already called.\n");
		return ALARMMGR_RESULT_SUCCESS;
	}

	g_thread_init(NULL);
	dbus_g_thread_init();

	alarm_context.bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
	if (alarm_context.bus == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("dbus bus get failed\n");

		return ERR_ALARM_SYSTEM_FAIL;
	}

	alarm_context.proxy = dbus_g_proxy_new_for_name(alarm_context.bus,
							"org.tizen.alarm.manager",
							"/org/tizen/alarm/manager",
							"org.tizen.alarm.manager");
	if (alarm_context.proxy == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("dbus bus proxy get failed\n");

		return ERR_ALARM_SYSTEM_FAIL;
	}

	alarm_context.pid = getpid();	/*this running appliction's process id*/

	sub_initialized = true;

	return ALARMMGR_RESULT_SUCCESS;
}

bool alarm_power_off(int *error_code)
{
	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarm_power_off() is called\n");

#ifdef __ALARM_BOOT
	return _send_alarm_power_off(alarm_context, error_code);
#else
	ALARM_MGR_LOG_PRINT(
			"[alarm-lib]:ALARM_BOOT feature is not supported. "
			    "so we return false.\n");
	if (error_code)
		*error_code = -1;	/*-1 means that system failed
							internally.*/
	return false;
#endif
}

int alarmmgr_check_next_duetime()
{
	int error_code;
	ALARM_MGR_LOG_PRINT(
	    "[alarm-lib]:alarm_check_next_duetime() is called\n");

#ifdef __ALARM_BOOT
	if (!_send_alarm_check_next_duetime(alarm_context, &error_code))
		return error_code;
#else
	ALARM_MGR_LOG_PRINT(
		    "[alarm-lib]:ALARM_BOOT feature is not supported. "
			    "so we return false.\n");
	return ERR_ALARM_SYSTEM_FAIL;
#endif

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_init(const char *appid)
{
	DBusError derror;
	int request_name_result = 0;
	char service_name[MAX_SERVICE_NAME_LEN] = { 0 };
	char service_name_mod[MAX_SERVICE_NAME_LEN]= { 0 };

	int ret;
	int i = 0;
	int j = 0;
	int len = 0;

	if (appid == NULL)
		return ERR_ALARM_INVALID_PARAM;

	if (strlen(appid) >= MAX_PKG_NAME_LEN)
		return ERR_ALARM_INVALID_PARAM;

	if (b_initialized) {
		ALARM_MGR_EXCEPTION_PRINT(
		     "alarm was already initialized. app_service_name=%s\n",
		     g_quark_to_string(alarm_context.quark_app_service_name));
		return ALARMMGR_RESULT_SUCCESS;
	}

	ret = __sub_init();
	if (ret < 0)
		return ret;

	memset(service_name_mod, 'a', MAX_SERVICE_NAME_LEN-1);

	len = strlen("ALARM.");
	strncpy(service_name, "ALARM.", len);
	strncpy(service_name + len, appid, strlen(appid));

	j=0;

        for(i=0;i<=strlen(service_name);i++)
        {
                if (service_name[i] == '.' )
                {
			service_name_mod[j] = service_name[i];
                        j++;
                }
		else{
			service_name_mod[j] = service_name[i];
		}
                j++;
        }

	ALARM_MGR_LOG_PRINT("[alarm-lib]: service_name %s\n", service_name);
	ALARM_MGR_LOG_PRINT("[alarm-lib]: service_name_mod %s\n", service_name_mod);

	dbus_error_init(&derror);

	request_name_result = dbus_bus_request_name(
			  dbus_g_connection_get_connection(alarm_context.bus),
			  service_name_mod, 0, &derror);
	if (dbus_error_is_set(&derror))	/*failure*/ {
		ALARM_MGR_EXCEPTION_PRINT(
		     "Failed to dbus_bus_request_name(%s): %s\n", service_name,
		     derror.message);
		dbus_error_free(&derror);

		return ERR_ALARM_SYSTEM_FAIL;
	}
	alarm_context.quark_app_service_name =
	    g_quark_from_string(service_name);
	alarm_context.quark_app_service_name_mod=
	    g_quark_from_string(service_name_mod);


	if (!dbus_connection_add_filter(
	     dbus_g_connection_get_connection(alarm_context.bus),
	     __expire_alarm_filter, NULL, NULL)) {
		ALARM_MGR_EXCEPTION_PRINT("add __expire_alarm_filter failed\n");

		return ERR_ALARM_SYSTEM_FAIL;
	}

	b_initialized = true;
	return ALARMMGR_RESULT_SUCCESS;

}

EXPORT_API void alarmmgr_fini()
{
	dbus_connection_remove_filter(dbus_g_connection_get_connection
				      (alarm_context.bus),
				      __expire_alarm_filter, NULL);
}

EXPORT_API int alarmmgr_set_cb(alarm_cb_t handler, void *user_param)
{
	ALARM_MGR_LOG_PRINT("alarm_set_cb is called\n");

	if (handler == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}
	alarm_context.alarm_handler = handler;
	alarm_context.user_param = user_param;
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
	int ret;

	if (b_initialized) {
		ALARM_MGR_EXCEPTION_PRINT("alarm was already initialized\n");
		return ALARMMGR_RESULT_SUCCESS;
	}

	g_thread_init(NULL);

	dbus_g_thread_init();

	ret = __sub_init();
	if (ret < 0)
		return ret;

	b_initialized = true;

	return ALARMMGR_RESULT_SUCCESS;

}

EXPORT_API void *alarmmgr_get_alarm_appsvc_info(alarm_id_t alarm_id, int *return_code){

	int ret = 0;

	ret = __sub_init();
	if (ret < 0){
		if (return_code)
			*return_code = ret;
		return NULL;
	}

	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarmmgr_get_alarm_appsvc_info() is called\n");

	if (alarm_id <= 0) {
		if (return_code)
			*return_code = ERR_ALARM_INVALID_ID;
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
	char *appid = NULL;

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
		ALARM_MGR_EXCEPTION_PRINT("Invalid parameter bundle [appsvc operation not present]\n");
		return ERR_ALARM_INVALID_PARAM;
	}

	if (__alarmmgr_init_appsvc() < 0)
	{
		ALARM_MGR_EXCEPTION_PRINT("Unable to initialize dbus!!!\n");
		return ERR_ALARM_SYSTEM_FAIL;
	}

	alarm_info = (alarm_info_t *) alarm;

	appid = appsvc_get_appid(b);

	if (NULL == appid && (alarm_info->alarm_type & ALARM_TYPE_NOLAUNCH) )
	{
		ALARM_MGR_EXCEPTION_PRINT("Invalid parameter\n");
		return ERR_ALARM_INVALID_PARAM;
	}

	if (alarm_info == NULL || alarm_id == NULL) {
		ALARM_MGR_EXCEPTION_PRINT("Invalid parameter\n");
		return ERR_ALARM_INVALID_PARAM;
	}
	alarm_mode_t *mode = &alarm_info->mode;

	ALARM_MGR_LOG_PRINT("start(%d-%d-%d, %02d:%02d:%02d), end(%d-%d-%d), repeat(%d), interval(%d), type(%d)",
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


	if (!_send_alarm_create_appsvc
	    (alarm_context, alarm_info, alarm_id, b,
	     &error_code)) {
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
		memset(dst_service_name, 0,
		       strlen(destination) + strlen("ALARM.") + 2);
		snprintf(dst_service_name, MAX_SERVICE_NAME_LEN, "ALARM.%s",
			 destination);

		memset(dst_service_name_mod,'a',MAX_SERVICE_NAME_LEN-1);

		j=0;

	        for(i=0; i<=strlen(dst_service_name); i++)
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

		if (!_send_alarm_create
		    (alarm_context, alarm_info, alarm_id, dst_service_name, dst_service_name_mod,
		     &error_code)) {
			return error_code;
		}
	} else
	    if (!_send_alarm_create
		(alarm_context, alarm_info, alarm_id, "null", "null", &error_code)) {
		return error_code;
	}

	return ALARMMGR_RESULT_SUCCESS;
}



EXPORT_API int alarmmgr_add_alarm_appsvc(int alarm_type, time_t trigger_at_time,
				  time_t interval, void *bundle_data,
				  alarm_id_t *alarm_id)
{
	int error_code = 0;;
	time_t current_time;
	struct tm duetime_tm;
	alarm_info_t alarm_info;
	const char *operation = NULL;
	char *appid = NULL;

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
		ALARM_MGR_EXCEPTION_PRINT("Invalid parameter bundle [appsvc operation not present]\n");
		return ERR_ALARM_INVALID_PARAM;
	}

	appid = appsvc_get_appid(b);

	if (NULL == appid && (alarm_type & ALARM_TYPE_NOLAUNCH) )
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

	time(&current_time);

	current_time += trigger_at_time;

	localtime_r(&current_time, &duetime_tm);

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

	if (!_send_alarm_create_appsvc
	    (alarm_context, &alarm_info, alarm_id, b,
	     &error_code)) {
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
	int error_code;
	time_t current_time;
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

	time(&current_time);

	current_time += trigger_at_time;

	localtime_r(&current_time, &duetime_tm);

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
	char dst_service_name[MAX_SERVICE_NAME_LEN] = { 0 };
	char dst_service_name_mod[MAX_SERVICE_NAME_LEN] = { 0 };
	int i = 0;
	int j = 0;
	int error_code;
	time_t current_time;
	struct tm duetime_tm;
	alarm_info_t alarm_info;
	int ret;
	char appid[256];

        ret = aul_app_get_appid_bypid(getpid(), appid, sizeof(appid));
        if (ret != AUL_R_OK)
        	return ERR_ALARM_SYSTEM_FAIL;

	ret = alarmmgr_init(appid);
	if (ret < 0)
		return ret;

	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarmmgr_add_alarm_withcb() is called\n");

	ALARM_MGR_LOG_PRINT("interval(%d)", interval);

	if (alarm_id == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (trigger_at_time < 0) {
		return ERR_ALARM_INVALID_PARAM;
	}

	alarm_info.alarm_type = alarm_type;
	alarm_info.alarm_type |= ALARM_TYPE_RELATIVE;
	alarm_info.alarm_type |= ALARM_TYPE_WITHCB;

	time(&current_time);

	current_time += trigger_at_time;

	localtime_r(&current_time, &duetime_tm);

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

EXPORT_API int alarmmgr_enum_alarm_ids(alarm_enum_fn_t fn, void *user_param)
{
	GError *error = NULL;
	GArray *alarm_array = NULL;
	int return_code = 0;
	int i = 0;
	int maxnum_of_ids;
	int num_of_ids;
	int alarm_id = -1;
	int ret;

	if (fn == NULL)
		return ERR_ALARM_INVALID_PARAM;

	ret = __sub_init();
	if (ret < 0)
		return ret;

	if (!org_tizen_alarm_manager_alarm_get_number_of_ids(
	    alarm_context.proxy, alarm_context.pid, &maxnum_of_ids,
	       &return_code, &error)) {
		/* dbus-glib error */
		/* error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		    "org_tizen_alarm_manager_alarm_get_number_of_ids() "
		    "failed. return_code[%d], return_code[%s]\n",
		return_code, error->message);
	}

	if (return_code != 0) {
		return return_code;
	}

	if (!org_tizen_alarm_manager_alarm_get_list_of_ids(
		     alarm_context.proxy, alarm_context.pid, maxnum_of_ids,
	     &alarm_array, &num_of_ids, &return_code, &error)) {
		/*dbus-glib error */
		/* error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		    "org_tizen_alarm_manager_alarm_get_list_of_ids() "
		    "failed. alarm_id[%d], return_code[%d]\n",
		     alarm_id, return_code);
	}

	if (return_code != 0) {
		return return_code;
	} else {
		if (error != NULL) {
			ALARM_MGR_LOG_PRINT(
				"Alarm server not ready dbus error message %s\n", error->message);
			return ERR_ALARM_SYSTEM_FAIL;
		}
		if (NULL == alarm_array) {
			ALARM_MGR_LOG_PRINT(
				"alarm server not initilized\n");
			return ERR_ALARM_SYSTEM_FAIL;
		}
		for (i = 0; i < alarm_array->len && i < maxnum_of_ids; i++) {
			alarm_id = g_array_index(alarm_array, alarm_id_t, i);
			(*fn) (alarm_id, user_param);
			ALARM_MGR_LOG_PRINT(" alarm_id(%d)\n", alarm_id);
		}

		g_array_free(alarm_array, true);
	}

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

	if (!_send_alarm_get_info(alarm_context, alarm_id, alarm_info,
				  &error_code))
		return error_code;

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_power_on(bool on_off)
{
	int error_code;
	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarm_power_on() is called\n");

#ifdef __ALARM_BOOT
	if (!_send_alarm_power_on(alarm_context, on_off, &error_code))
		return error_code;
#else
	ALARM_MGR_LOG_PRINT("[alarm-lib]:ALARM_BOOT feature is not supported. "
			    "so we return false.\n");
	return ERR_ALARM_SYSTEM_FAIL;
#endif

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
	ALARM_MGR_LOG_PRINT("[alarm-lib]:"
			    "alarm_get_number_of_ids() is called\n");

	if (num_of_ids == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}
	ALARM_MGR_LOG_PRINT("call alarm_get_number_of_ids\n");
	if (!_send_alarm_get_number_of_ids(alarm_context, num_of_ids,
					   &error_code))
		return error_code;

	return ALARMMGR_RESULT_SUCCESS;
}

int alarmmgr_get_list_of_ids(int maxnum_of_ids, alarm_id_t *alarm_id,
			     int *num_of_ids)
{
	int error_code;
	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarm_get_list_of_ids() is called\n");

	if (maxnum_of_ids < 0 || alarm_id == NULL || num_of_ids == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (maxnum_of_ids == 0) {
		*num_of_ids = 0;
		return ALARMMGR_RESULT_SUCCESS;
	}

	if (!_send_alarm_get_list_of_ids
	    (alarm_context, maxnum_of_ids, alarm_id, num_of_ids, &error_code))
		return error_code;

	return ALARMMGR_RESULT_SUCCESS;
}

EXPORT_API int alarmmgr_get_next_duetime(alarm_id_t alarm_id, time_t* duetime)
{
	int error_code;
	ALARM_MGR_LOG_PRINT("[alarm-lib]:alarmmgr_get_next_duetime() is called\n");

	if (duetime == NULL) {
		return ERR_ALARM_INVALID_PARAM;
	}

	if (!_send_alarm_get_next_duetime
		(alarm_context, alarm_id, duetime, &error_code))
		return error_code;

	return ALARMMGR_RESULT_SUCCESS;
}
