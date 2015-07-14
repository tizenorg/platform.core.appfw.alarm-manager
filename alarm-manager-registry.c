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
#include <db-util.h>
#if !GLIB_CHECK_VERSION (2, 31, 0)
#include <glib/gmacros.h>
#endif
#include"alarm.h"
#include"alarm-internal.h"

#define SIG_TIMER 0x32

#define MAX_GCONF_PATH_LEN 256
#define MAX_QUERY_LEN 4096

extern __alarm_server_context_t alarm_context;
extern sqlite3 *alarmmgr_db;

bool _save_alarms(__alarm_info_t *__alarm_info);
bool _update_alarms(__alarm_info_t *__alarm_info);
bool _delete_alarms(alarm_id_t alarm_id);
bool _load_alarms_from_registry(void);

bool _save_alarms(__alarm_info_t *__alarm_info)
{
	char *error_message = NULL;
	alarm_info_t *alarm_info =
	    (alarm_info_t *) &(__alarm_info->alarm_info);
	alarm_date_t *start = &alarm_info->start;
	alarm_mode_t *mode = &alarm_info->mode;

	char *query = sqlite3_mprintf("insert into alarmmgr( alarm_id, start,\
			end, uid, pid, caller_pkgid, callee_pkgid, app_unique_name, app_service_name, app_service_name_mod, bundle, year,\
			month, day, hour, min, sec, day_of_week, repeat,\
			alarm_type, reserved_info, dst_service_name, dst_service_name_mod)\
			values (%d,%d,%d,%d,%d,%Q,%Q,%Q,%Q,%Q,%Q,%d,%d,%d,%d,%d,%d,%d,%d,\
			%d,%d,%Q,%Q)",\
			__alarm_info->alarm_id,
			(int)__alarm_info->start,
			(int)__alarm_info->end,
			__alarm_info->uid,
			__alarm_info->pid,
			(char *)g_quark_to_string(__alarm_info->quark_caller_pkgid),
			(char *)g_quark_to_string(__alarm_info->quark_callee_pkgid),
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

	if (SQLITE_OK != sqlite3_exec(alarmmgr_db, query, NULL, NULL, &error_message)) {
		SECURE_LOGE("sqlite3_exec() is failed. query = %s, error message = %s", query, error_message);
		sqlite3_free(query);
		return false;
	}

	sqlite3_free(query);
	return true;
}

bool _update_alarms(__alarm_info_t *__alarm_info)
{
	char *error_message = NULL;
	alarm_info_t *alarm_info =
	    (alarm_info_t *) &(__alarm_info->alarm_info);
	alarm_date_t *start = &alarm_info->start;
	alarm_mode_t *mode = &alarm_info->mode;

	char *query = sqlite3_mprintf("update alarmmgr set start=%d, end=%d,\
			uid=%d, pid=%d, caller_pkgid=%Q, callee_pkgid=%Q, app_unique_name=%Q, app_service_name=%Q, app_service_name_mod=%Q,\
			bundle=%Q, year=%d, month=%d, day=%d, hour=%d, min=%d, sec=%d,\
			day_of_week=%d, repeat=%d, alarm_type=%d,\
			reserved_info=%d, dst_service_name=%Q, dst_service_name_mod=%Q\
			where alarm_id=%d",\
			(int)__alarm_info->start,
			(int)__alarm_info->end,
			__alarm_info->uid,
			__alarm_info->pid,
			(char *)g_quark_to_string(__alarm_info->quark_caller_pkgid),
			(char *)g_quark_to_string(__alarm_info->quark_callee_pkgid),
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

	if (SQLITE_OK != sqlite3_exec(alarmmgr_db, query, NULL, NULL, &error_message)) {
		SECURE_LOGE("sqlite3_exec() is failed. query = %s, error message = %s", query, error_message);
		sqlite3_free(query);
		return false;
	}

	sqlite3_free(query);
	return true;
}

bool _delete_alarms(alarm_id_t alarm_id)
{
	char *error_message = NULL;
	char *query = sqlite3_mprintf("delete from alarmmgr where alarm_id=%d", alarm_id);

	if (SQLITE_OK != sqlite3_exec(alarmmgr_db, query, NULL, NULL, &error_message)) {
		SECURE_LOGE("sqlite3_exec() is failed. query = %s, error message = %s", query, error_message);
		sqlite3_free(query);
		return false;
	}

	sqlite3_free(query);
	return true;
}

bool _load_alarms_from_registry()
{
	int i = 0;
	int col_idx;
	char query[MAX_QUERY_LEN] = {0,};
	sqlite3_stmt *stmt = NULL;
	const char *tail = NULL;
	alarm_info_t *alarm_info = NULL;
	__alarm_info_t *__alarm_info = NULL;
	alarm_date_t *start = NULL;
	alarm_mode_t *mode = NULL;
	char caller_pkgid[MAX_PKG_ID_LEN] = {0,};
	char callee_pkgid[MAX_PKG_ID_LEN] = {0,};
	char app_unique_name[MAX_SERVICE_NAME_LEN] = {0,};
	char app_service_name[MAX_SERVICE_NAME_LEN] = {0,};
	char app_service_name_mod[MAX_SERVICE_NAME_LEN] = {0,};
	char dst_service_name[MAX_SERVICE_NAME_LEN] = {0,};
	char dst_service_name_mod[MAX_SERVICE_NAME_LEN] = {0,};
	char bundle[MAX_BUNDLE_NAME_LEN] = {0,};

	snprintf(query, MAX_QUERY_LEN, "select * from alarmmgr");

	if (SQLITE_OK != sqlite3_prepare(alarmmgr_db, query, strlen(query), &stmt, &tail)) {
		ALARM_MGR_EXCEPTION_PRINT("sqlite3_prepare() is failed.");
		return false;
	}

	for (i = 0; SQLITE_ROW == sqlite3_step(stmt); i++) {
		col_idx = 0;
		__alarm_info = malloc(sizeof(__alarm_info_t));

		if (G_UNLIKELY(__alarm_info == NULL)) {
			ALARM_MGR_EXCEPTION_PRINT("Memory allocation failed.");
			return false;
		}
		alarm_info = (alarm_info_t *) &(__alarm_info->alarm_info);
		start = &alarm_info->start;
		mode = &alarm_info->mode;

		__alarm_info->alarm_id = sqlite3_column_int(stmt, col_idx++);
		__alarm_info->start = sqlite3_column_int(stmt, col_idx++);
		__alarm_info->end = sqlite3_column_int(stmt, col_idx++);
		__alarm_info->uid = sqlite3_column_int(stmt, col_idx++);
		__alarm_info->pid = sqlite3_column_int(stmt, col_idx++);

		strncpy(caller_pkgid, (const char *)sqlite3_column_text(stmt, col_idx++),
			MAX_PKG_ID_LEN - 1);
		strncpy(callee_pkgid, (const char *)sqlite3_column_text(stmt, col_idx++),
			MAX_PKG_ID_LEN - 1);
		strncpy(app_unique_name, (const char *)sqlite3_column_text(stmt, col_idx++),
			MAX_SERVICE_NAME_LEN - 1);
		strncpy(app_service_name, (const char *)sqlite3_column_text(stmt, col_idx++),
			MAX_SERVICE_NAME_LEN - 1);
		strncpy(app_service_name_mod, (const char *)sqlite3_column_text(stmt, col_idx++),
			MAX_SERVICE_NAME_LEN - 1);
		strncpy(bundle, (const char *)sqlite3_column_text(stmt, col_idx++),
			MAX_BUNDLE_NAME_LEN - 1);
		start->year = sqlite3_column_int(stmt, col_idx++);
		start->month = sqlite3_column_int(stmt, col_idx++);
		start->day = sqlite3_column_int(stmt, col_idx++);
		start->hour = sqlite3_column_int(stmt, col_idx++);
		start->min = sqlite3_column_int(stmt, col_idx++);
		start->sec = sqlite3_column_int(stmt, col_idx++);
		mode->u_interval.day_of_week = sqlite3_column_int(stmt, col_idx++);
		mode->repeat = sqlite3_column_int(stmt, col_idx++);
		alarm_info->alarm_type = sqlite3_column_int(stmt, col_idx++);
		alarm_info->reserved_info = sqlite3_column_int(stmt, col_idx++);
		strncpy(dst_service_name, (const char *)sqlite3_column_text(stmt, col_idx++),
			MAX_SERVICE_NAME_LEN - 1);
		strncpy(dst_service_name_mod, (const char *)sqlite3_column_text(stmt, col_idx++),
			MAX_SERVICE_NAME_LEN - 1);

		__alarm_info->quark_caller_pkgid = g_quark_from_string(caller_pkgid);
		__alarm_info->quark_callee_pkgid = g_quark_from_string(callee_pkgid);
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
		alarm_context.alarms = g_slist_append(alarm_context.alarms, __alarm_info);
	}


	_alarm_schedule();
	if (SQLITE_OK != sqlite3_finalize(stmt)) {
		ALARM_MGR_EXCEPTION_PRINT("sqlite3_finalize() is failed.");
		return false;
	}

	return true;
}
