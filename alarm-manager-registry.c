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

#include<dbus/dbus.h>
#include<glib.h>
#include <db-util.h>
#include <glib/gmacros.h>

#include"alarm.h"
#include"alarm-internal.h"

#define SIG_TIMER 0x32

#define MAX_GCONF_PATH_LEN 256
#define MAX_QUERY_LEN 4096

extern __alarm_server_context_t alarm_context;
extern sqlite3 *alarmmgr_db;

#ifdef __ALARM_BOOT
extern bool enable_power_on_alarm;
#endif

bool _save_alarms(__alarm_info_t *__alarm_info);
bool _update_alarms(__alarm_info_t *__alarm_info);
bool _delete_alarms(alarm_id_t alarm_id);
#ifdef __ALARM_BOOT
bool _update_power_on(bool on_off);
#endif
bool _load_alarms_from_registry(void);

bool _save_alarms(__alarm_info_t *__alarm_info)
{
	char query[MAX_QUERY_LEN] = {0,};
	char *error_message = NULL;
	alarm_info_t *alarm_info =
	    (alarm_info_t *) &(__alarm_info->alarm_info);
	alarm_date_t *start = &alarm_info->start;
	alarm_mode_t *mode = &alarm_info->mode;

	snprintf(query, MAX_QUERY_LEN, "insert into alarmmgr( alarm_id, start,\
			end, pid, app_unique_name, app_service_name, app_service_name_mod, bundle, year,\
			month, day, hour, min, sec, day_of_week, repeat,\
			alarm_type, reserved_info, dst_service_name, dst_service_name_mod)\
			values (%d,%d,%d,%d,'%s','%s','%s','%s',%d,%d,%d,%d,%d,%d,%d,%d,\
			%d,%d,'%s','%s')",\
			__alarm_info->alarm_id,
			(int)__alarm_info->start,
			(int)__alarm_info->end,
			__alarm_info->pid,
			(char *)g_quark_to_string(
				__alarm_info->quark_app_unique_name),
			(char *)g_quark_to_string(
				__alarm_info->quark_app_service_name),
			(char *)g_quark_to_string(
				__alarm_info->quark_app_service_name_mod),
			(char *)g_quark_to_string(
				__alarm_info->quark_bundle),
			start->year,
			start->month,
			start->day,
			start->hour,
			start->min,
			start->sec,
			mode->u_interval.day_of_week,
			mode->repeat,
			alarm_info->alarm_type,
			alarm_info->reserved_info,
			(char *)g_quark_to_string(
			__alarm_info->quark_dst_service_name),
			(char *)g_quark_to_string(
			__alarm_info->quark_dst_service_name_mod));

	if (SQLITE_OK !=
	    sqlite3_exec(alarmmgr_db, query, NULL, NULL, &error_message)) {
		ALARM_MGR_EXCEPTION_PRINT(
		    "Don't execute query = %s, error message = %s\n", query,
		     error_message);
		return false;
	}

	return true;
}

bool _update_alarms(__alarm_info_t *__alarm_info)
{
	char query[MAX_QUERY_LEN] = {0,};
	char *error_message = NULL;
	alarm_info_t *alarm_info =
	    (alarm_info_t *) &(__alarm_info->alarm_info);
	alarm_date_t *start = &alarm_info->start;
	alarm_mode_t *mode = &alarm_info->mode;

	snprintf(query, MAX_QUERY_LEN, "update alarmmgr set start=%d, end=%d,\
			pid=%d, app_unique_name='%s', app_service_name='%s', app_service_name_mod='%s',\
			bundle='%s', year=%d, month=%d, day=%d, hour=%d, min=%d, sec=%d,\
			day_of_week=%d, repeat=%d, alarm_type=%d,\
			reserved_info=%d, dst_service_name='%s', dst_service_name_mod='%s',\
			where alarm_id=%d",\
			(int)__alarm_info->start,
			(int)__alarm_info->end,
			__alarm_info->pid,
			(char *)g_quark_to_string(
				__alarm_info->quark_app_unique_name),
			(char *)g_quark_to_string(
				__alarm_info->quark_app_service_name),
			(char *)g_quark_to_string(
				__alarm_info->quark_app_service_name_mod),
			(char *)g_quark_to_string(
				__alarm_info->quark_bundle),				
			start->year,
			start->month,
			start->day,
			start->hour,
			start->min,
			start->sec,
			mode->u_interval.day_of_week,
			mode->repeat,
			alarm_info->alarm_type,
			alarm_info->reserved_info,
			(char *)g_quark_to_string(
				__alarm_info->quark_dst_service_name),
			(char *)g_quark_to_string(
				__alarm_info->quark_dst_service_name_mod),
			__alarm_info->alarm_id);

	if (SQLITE_OK !=
	    sqlite3_exec(alarmmgr_db, query, NULL, NULL, &error_message)) {
		ALARM_MGR_EXCEPTION_PRINT(
		    "Don't execute query = %s, error message = %s\n", query,
		     error_message);
		return false;
	}

	return true;

}

bool _delete_alarms(alarm_id_t alarm_id)
{
	char query[MAX_QUERY_LEN] = {0};
	char *error_message = NULL;

	snprintf(query, MAX_QUERY_LEN, "delete from alarmmgr where alarm_id=%d",
		 alarm_id);

	if (SQLITE_OK !=
	    sqlite3_exec(alarmmgr_db, query, NULL, NULL, &error_message)) {
		ALARM_MGR_EXCEPTION_PRINT(
		    "Don't execute query = %s, error message = %s\n", query,
		     error_message);
		return false;
	}

	return true;

}

#ifdef __ALARM_BOOT
bool _update_power_on(bool on_off)
{
/*	GConfClient* pGCC;
	char key[MAX_GCONF_PATH_LEN];
	GError* error = NULL;
	
	g_type_init();
	pGCC = gconf_client_get_default();

	if(!pGCC)
	{
		ALARM_MGR_EXCEPTION_PRINT(" gconf get failed.. \n");
		return false;
	}

	sprintf(key,"/Services/AlarmMgr/Auto_poweron");	

	
	if(!gconf_client_set_bool(pGCC, key, on_off, &error))
	{
		ALARM_MGR_EXCEPTION_PRINT("set string has failed...\n");
		return false;
	}

	gconf_client_suggest_sync(pGCC, NULL);
	g_object_unref(pGCC);
*/
	return false;
}
#endif

bool _load_alarms_from_registry()
{
	int i = 0;
	char query[MAX_QUERY_LEN] = {0,};
	sqlite3_stmt *stmt = NULL;
	const char *tail = NULL;
	alarm_info_t *alarm_info = NULL;
	__alarm_info_t *__alarm_info = NULL;
	alarm_date_t *start = NULL;
	alarm_mode_t *mode = NULL;
	char app_unique_name[MAX_SERVICE_NAME_LEN] = {0,};
	char app_service_name[MAX_SERVICE_NAME_LEN] = {0,};
	char app_service_name_mod[MAX_SERVICE_NAME_LEN] = {0,};
	char dst_service_name[MAX_SERVICE_NAME_LEN] = {0,};
	char dst_service_name_mod[MAX_SERVICE_NAME_LEN] = {0,};
	char bundle[MAX_BUNDLE_NAME_LEN] = {0,};

#ifdef __ALARM_BOOT
	/*sprintf(path, "/Services/AlarmMgr/Auto_poweron"); */

	enable_power_on_alarm = 0;
	/*gconf_client_get_bool(pGCC, path, NULL); */
#endif

	snprintf(query, MAX_QUERY_LEN, "select * from alarmmgr");

	if (SQLITE_OK !=
	    sqlite3_prepare(alarmmgr_db, query, strlen(query), &stmt, &tail)) {
		ALARM_MGR_EXCEPTION_PRINT("sqlite3_prepare error\n");
		return false;
	}

	for (i = 0; SQLITE_ROW == sqlite3_step(stmt); i++) {
		__alarm_info = malloc(sizeof(__alarm_info_t));

		if (G_UNLIKELY(__alarm_info == NULL)){
			ALARM_MGR_EXCEPTION_PRINT("[alarm-server]:Malloc failed\n");
			return false;
		}
		alarm_info = (alarm_info_t *) &(__alarm_info->alarm_info);
		start = &alarm_info->start;
		mode = &alarm_info->mode;

		memset(app_service_name, 0, MAX_SERVICE_NAME_LEN);
		memset(app_service_name_mod, 0, MAX_SERVICE_NAME_LEN);
		memset(dst_service_name, 0, MAX_SERVICE_NAME_LEN);
		memset(dst_service_name_mod, 0, MAX_SERVICE_NAME_LEN);

		__alarm_info->alarm_id = sqlite3_column_int(stmt, 0);
		__alarm_info->start = sqlite3_column_int(stmt, 1);
		__alarm_info->end = sqlite3_column_int(stmt, 2);
		__alarm_info->pid = sqlite3_column_int(stmt, 3);
		strncpy(app_unique_name, (const char *)sqlite3_column_text(stmt, 4),
			MAX_SERVICE_NAME_LEN - 1);
		strncpy(app_service_name, (const char *)sqlite3_column_text(stmt, 5),
			MAX_SERVICE_NAME_LEN - 1);
		strncpy(app_service_name_mod, (const char *)sqlite3_column_text(stmt, 6),
			MAX_SERVICE_NAME_LEN - 1);
		strncpy(bundle, (const char *)sqlite3_column_text(stmt, 7),
			MAX_BUNDLE_NAME_LEN - 1);		
		start->year = sqlite3_column_int(stmt, 8);
		start->month = sqlite3_column_int(stmt, 9);
		start->day = sqlite3_column_int(stmt, 10);
		start->hour = sqlite3_column_int(stmt, 11);
		start->min = sqlite3_column_int(stmt, 12);
		start->sec = sqlite3_column_int(stmt, 13);
		mode->u_interval.day_of_week = sqlite3_column_int(stmt, 14);
		mode->repeat = sqlite3_column_int(stmt, 15);
		alarm_info->alarm_type = sqlite3_column_int(stmt, 16);
		alarm_info->reserved_info = sqlite3_column_int(stmt, 17);
		strncpy(dst_service_name, (const char *)sqlite3_column_text(stmt, 18),
			MAX_SERVICE_NAME_LEN - 1);
		strncpy(dst_service_name_mod, (const char *)sqlite3_column_text(stmt, 19),
			MAX_SERVICE_NAME_LEN - 1);

		__alarm_info->quark_app_unique_name =
		    g_quark_from_string(app_unique_name);
		__alarm_info->quark_app_service_name =
		    g_quark_from_string(app_service_name);
		__alarm_info->quark_app_service_name_mod=
		    g_quark_from_string(app_service_name_mod);
		__alarm_info->quark_dst_service_name =
		    g_quark_from_string(dst_service_name);
		__alarm_info->quark_dst_service_name_mod=
		    g_quark_from_string(dst_service_name_mod);
		__alarm_info->quark_bundle = g_quark_from_string(bundle);

		_alarm_next_duetime(__alarm_info);
		alarm_context.alarms =
		    g_slist_append(alarm_context.alarms, __alarm_info);
	}

	if (SQLITE_OK != sqlite3_finalize(stmt)) {
		ALARM_MGR_EXCEPTION_PRINT("error : sqlite3_finalize\n");
		return false;
	}

	_alarm_schedule();

	return true;
}
