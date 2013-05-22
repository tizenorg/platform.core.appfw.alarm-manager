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
#include<sys/types.h>
#include<string.h>
#include<dbus/dbus.h>
#include<glib.h>

#include "alarm.h"
#include "alarm-internal.h"
#include "alarm-stub.h"
#include "security-server.h"

#define ALARM_SERVICE_NAME	"appframework.alarm"
#define ALARM_OBJECT_PATH	"/appframework/alarm"
#define ALARM_INTERFACE_NAME "appframework.alarm"


bool _send_alarm_create(alarm_context_t context, alarm_info_t *alarm_info,
			alarm_id_t *alarm_id, const char *dst_service_name, const char *dst_service_name_mod,
			int *error_code);
bool _send_alarm_create_appsvc(alarm_context_t context, alarm_info_t *alarm_info,
			alarm_id_t *alarm_id, bundle *b,int *error_code);
bool _send_alarm_delete(alarm_context_t context, alarm_id_t alarm_id,
			int *error_code);
#ifdef __ALARM_BOOT
bool _send_alarm_power_on(alarm_context_t context, bool on_off,
			  int *error_code);
bool _send_alarm_power_off(alarm_context_t context, int *error_code);
bool _send_alarm_check_next_duetime(alarm_context_t context, int *error_code);
#endif
bool _send_alarm_get_list_of_ids(alarm_context_t context, int maxnum_of_ids,
				 alarm_id_t *alarm_id, int *num_of_ids,
				 int *error_code);
bool _send_alarm_get_number_of_ids(alarm_context_t context, int *num_of_ids,
				   int *error_code);
bool _send_alarm_get_info(alarm_context_t context, alarm_id_t alarm_id,
			  alarm_info_t *alarm_info, int *error_code);



bool _send_alarm_create_appsvc(alarm_context_t context, alarm_info_t *alarm_info,
			alarm_id_t *alarm_id, bundle *b,
			int *error_code)
{
	GError *error = NULL;
	int return_code = 0;

	char cookie[256] = {0,};
	char *e_cookie = NULL;
	int size = 0;
	int retval = 0;

	bundle_raw *b_data = NULL;
	int datalen = 0;

	size = security_server_get_cookie_size();
	retval = security_server_request_cookie(cookie, size);

	if (retval < 0) {
		ALARM_MGR_EXCEPTION_PRINT(
			"security_server_request_cookie failed\n");
		if (error_code)
			*error_code = -1;	/* TODO: Need to redefine error codes */
		return false;
	}

	e_cookie = g_base64_encode((const guchar *)cookie, size);

	if (NULL == e_cookie)
	{
		ALARM_MGR_EXCEPTION_PRINT(
			"g_base64_encode failed\n");
		if (error_code)
			*error_code = -1;	/* TODO: Need to redefine error codes */
		return false;
	}

	if (bundle_encode(b, &b_data, &datalen))
	{
		ALARM_MGR_EXCEPTION_PRINT("Unable to encode the bundle data\n");
		if (error_code){
			*error_code = -1;	/* TODO: Need to redefine error codes*/
		}
		if (e_cookie)
		{
			g_free(e_cookie);
			e_cookie = NULL;
		}
		return false;
	}

	if (!org_tizen_alarm_manager_alarm_create_appsvc(context.proxy, context.pid,
						    alarm_info->start.year,
						    alarm_info->start.month,
						    alarm_info->start.day,
						    alarm_info->start.hour,
						    alarm_info->start.min,
						    alarm_info->start.sec,
						    alarm_info->end.year,
						    alarm_info->end.month,
						    alarm_info->end.day,
						    alarm_info->mode.u_interval.day_of_week,
						    alarm_info->mode.repeat,
						    alarm_info->alarm_type,
						    alarm_info->reserved_info,
						    (char *)b_data, e_cookie,
						    alarm_id, &return_code,
						    &error)) {
		/* dbus-glib error */
		/* error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		"org_tizen_alarm_manager_alarm_create()failed.alarm_id[%d], "
		"return_code[%d]\n", alarm_id, return_code);
		ALARM_MGR_EXCEPTION_PRINT("error->message is %s\n",
					  error->message);
	}

	if (e_cookie)
	{
		g_free(e_cookie);
		e_cookie = NULL;
	}

	if (b_data)
	{
		free(b_data);
		b_data = NULL;
	}

	if (return_code != 0) {
		if (error_code)
			*error_code = return_code;
		return false;
	}

	return true;
}



bool _send_alarm_create(alarm_context_t context, alarm_info_t *alarm_info,
			alarm_id_t *alarm_id, const char *dst_service_name, const char *dst_service_name_mod,
			int *error_code)
{
	GError *error = NULL;
	int return_code = 0;

	char cookie[256];
	char *e_cookie;
	int size;
	int retval;

	/*TODO: Dbus bus name validation is must & will be added to avoid alarm-server crash*/
	if (g_quark_to_string(context.quark_app_service_name) == NULL
		&& strlen(dst_service_name) == 4
		&& strncmp(dst_service_name, "null",4) == 0 ){
			ALARM_MGR_EXCEPTION_PRINT("Invalid arg. Provide valid destination or call alarmmgr_init()\n");
		if (error_code)
			*error_code = ERR_ALARM_INVALID_PARAM;
		return false;
	}

	size = security_server_get_cookie_size();
	retval = security_server_request_cookie(cookie, size);

	if (retval < 0) {
		ALARM_MGR_EXCEPTION_PRINT(
			"security_server_request_cookie failed\n");
		return false;
	}

	e_cookie = g_base64_encode((const guchar *)cookie, size);

	if (!org_tizen_alarm_manager_alarm_create(context.proxy, context.pid,
			g_quark_to_string(context.quark_app_service_name),
			g_quark_to_string(context.quark_app_service_name_mod),
						    alarm_info->start.year,
						    alarm_info->start.month,
						    alarm_info->start.day,
						    alarm_info->start.hour,
						    alarm_info->start.min,
						    alarm_info->start.sec,
						    alarm_info->end.year,
						    alarm_info->end.month,
						    alarm_info->end.day,
						    alarm_info->mode.u_interval.day_of_week,
						    alarm_info->mode.repeat,
						    alarm_info->alarm_type,
						    alarm_info->reserved_info,
						    dst_service_name, dst_service_name_mod, e_cookie,
						    alarm_id, &return_code,
						    &error)) {
		/* dbus-glib error */
		/* error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		"org_tizen_alarm_manager_alarm_create()failed.alarm_id[%d], "
		"return_code[%d]\n", alarm_id, return_code);
		ALARM_MGR_EXCEPTION_PRINT("error->message is %s\n",
					  error->message);
		if (error_code)
			*error_code = -1;	/* -1 means that system
						   failed internally. */
		return false;
	}

	g_free(e_cookie);

	if (return_code != 0) {
		if (error_code)
			*error_code = return_code;
		return false;
	}

	return true;
}
bundle *_send_alarm_get_appsvc_info(alarm_context_t context, alarm_id_t alarm_id, int *error_code)
{
	GError *error = NULL;
	int return_code = 0;

	bundle *b = NULL;

	char cookie[256] = {0,};
	char *e_cookie = NULL;
	int size = 0;
	int retval = 0;

	gchar *b_data = NULL;
	int len = 0;

	size = security_server_get_cookie_size();
	retval = security_server_request_cookie(cookie, size);

	if (retval < 0) {
		ALARM_MGR_EXCEPTION_PRINT(
			"security_server_request_cookie failed\n");
		if (error_code)
			*error_code = -1; /*TODO: define error*/
		return NULL;
	}

	e_cookie = g_base64_encode((const guchar *)cookie, size);

	if (NULL == e_cookie)
	{
		ALARM_MGR_EXCEPTION_PRINT(
			"g_base64_encode failed\n");
		if (error_code)
			*error_code = -1; /*TODO: define error*/
		return NULL;
	}


	if (!org_tizen_alarm_manager_alarm_get_appsvc_info
	    (context.proxy, context.pid, alarm_id, e_cookie, &b_data, &return_code, &error)) {
		/* dbus-glib error */
		/*error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		"org_tizen_alarm_manager_alarm_delete() failed. "
		     "alarm_id[%d], return_code[%d]\n", alarm_id, return_code);
		if (error_code)
			*error_code = ERR_ALARM_SYSTEM_FAIL; /*-1 means that system
								failed internally.*/
		if (e_cookie)
			g_free(e_cookie);
		if (b_data)
			g_free(b_data);

		return NULL;
	}

	if (return_code != 0){
		if (error_code)
			*error_code = return_code;
	} else {
		b = bundle_decode((bundle_raw *)b_data, len);
	}

	if (e_cookie)
		g_free(e_cookie);
	if (b_data)
		g_free(b_data);

	return b;
}


bool _send_alarm_set_rtc_time(alarm_context_t context, alarm_date_t *time, int *error_code){

	GError *error = NULL;
	int return_code = 0;

	char cookie[256] = {0,};
	char *e_cookie = NULL;
	int size = 0;
	int retval = 0;

	size = security_server_get_cookie_size();
	retval = security_server_request_cookie(cookie, size);

	if (retval < 0) {
		ALARM_MGR_EXCEPTION_PRINT(
			"security_server_request_cookie failed\n");
		if (error_code)
			*error_code = -1; /*TODO: define error*/
		return false;
	}

	e_cookie = g_base64_encode((const guchar *)cookie, size);

	if (NULL == e_cookie)
	{
		ALARM_MGR_EXCEPTION_PRINT(
			"g_base64_encode failed\n");
		if (error_code)
			*error_code = -1; /*TODO: define error*/
		return false;
	}

	if (!org_tizen_alarm_manager_alarm_set_rtc_time
	    (context.proxy, context.pid,
		time->year, time->month, time->day,
		 time->hour, time->min, time->sec,
		  e_cookie, &return_code, &error)) {
		/* dbus-glib error */
		/*error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		"org_tizen_alarm_manager_alarm_set_rtc_time() failed. "
		     "return_code[%d]\n", return_code);
		if (error_code)
			*error_code = ERR_ALARM_SYSTEM_FAIL; /*-1 means that system
								failed internally.*/
		if (e_cookie)
			g_free(e_cookie);

		return false;
	}
	if (e_cookie)
	{
		g_free(e_cookie);
		e_cookie = NULL;
	}

	if (return_code != 0) {
		if (error_code)
			*error_code = return_code;
		return false;
	}

	return true;

}

bool _send_alarm_delete(alarm_context_t context, alarm_id_t alarm_id,
			int *error_code)
{
	GError *error = NULL;
	int return_code = 0;

	char cookie[256];
	char *e_cookie;
	int size;
	int retval;

	size = security_server_get_cookie_size();
	retval = security_server_request_cookie(cookie, size);

	if (retval < 0) {
		ALARM_MGR_EXCEPTION_PRINT(
			"security_server_request_cookie failed\n");
		return false;
	}

	e_cookie = g_base64_encode((const guchar *)cookie, size);

	if (!org_tizen_alarm_manager_alarm_delete
	    (context.proxy, context.pid, alarm_id, e_cookie, &return_code,
	     &error)) {
		/* dbus-glib error */
		/*error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		"org_tizen_alarm_manager_alarm_delete() failed. " 
		     "alarm_id[%d], return_code[%d]\n", alarm_id, return_code);
		if (error_code)
			*error_code = -1;	/*-1 means that system 
							failed internally.*/

		return false;
	}

	g_free(e_cookie);

	if (return_code != 0) {
		if (error_code)
			*error_code = return_code;
		return false;
	}

	return true;

}

#ifdef __ALARM_BOOT
bool _send_alarm_power_on(alarm_context_t context, bool on_off,
			   int *error_code)
{
	GError *error = NULL;
	int return_code = 0;

	if (!org_tizen_alarm_manager_alarm_power_on
	    (context.proxy, context.pid, on_off, &return_code, &error)) {
		/* dbus-glib error */
		/* error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		"org_tizen_alarm_manager_alarm_power_on failed. "
		     "return_code[%d]\n", return_code);
		if (error_code)
			*error_code = -1;	/*-1 means that system 
							failed internally*/
		return false;
	}

	if (return_code != 0) {
		if (error_code)
			*error_code = return_code;
		return false;
	}

	return true;

}

bool _send_alarm_power_off(alarm_context_t context, int *error_code)
{
	GError *error = NULL;
	int return_code = 0;

	if (!org_tizen_alarm_manager_alarm_power_off(context.proxy,
				context.pid, &return_code, &error)) {
		/* dbus-glib error */
		/* error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		"org_tizen_alarm_manager_alarm_power_off failed. "
		     "return_code[%d]\n", return_code);
		if (error_code)
			*error_code = -1;	/*-1 means that system 
						failed internally.*/
		return false;
	}

	if (return_code != 0) {
		if (error_code)
			*error_code = return_code;
		return false;
	}

	return true;

}

bool _send_alarm_check_next_duetime(alarm_context_t context, int *error_code)
{
	GError *error = NULL;
	int return_code = 0;

	if (!org_tizen_alarm_manager_alarm_check_next_duetime(context.proxy,
					context.pid, &return_code, &error)) {
		/*dbus-glib error */
		/*error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		"org_tizen_alarm_manager_alarm_power_check_next_duetime's "
		"return value is false. return_code[%d]\n", return_code);
		if (error_code)
			*error_code = -1;	/*-1 means that system 
						failed internally*/
		return false;
	}

	if (return_code != 0) {
		if (error_code)
			*error_code = return_code;
		return false;
	}

	return true;

}

#endif				/* __ALARM_BOOT */

bool _send_alarm_get_list_of_ids(alarm_context_t context, int maxnum_of_ids,
				 alarm_id_t *alarm_id, int *num_of_ids,
				 int *error_code)
{

	GError *error = NULL;
	GArray *alarm_array = NULL;
	int return_code = 0;
	int i = 0;

	if (!org_tizen_alarm_manager_alarm_get_list_of_ids(context.proxy,
			     context.pid, maxnum_of_ids, &alarm_array,
			     num_of_ids, &return_code, &error)) {
		/*dbus-glib error */
		/*error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		"org_tizen_alarm_manager_alarm_get_list_of_ids() failed. "
		     "alarm_id[%d], return_code[%d]\n", alarm_id, return_code);
		if (error_code)
			*error_code = -1;	/*-1 means that system 
							failed internally.*/

		return false;
	}

	if (return_code != 0) {
		if (error_code)
			*error_code = return_code;
		return false;
	} else {
		for (i = 0; i < alarm_array->len && i < maxnum_of_ids; i++) {
			alarm_id[i] = g_array_index(alarm_array, alarm_id_t, i);
			ALARM_MGR_LOG_PRINT(" alarm_id(%d)\n", alarm_id[i]);
		}

		*num_of_ids = alarm_array->len;
		g_array_free(alarm_array, true);
	}

	return true;
}

bool _send_alarm_get_number_of_ids(alarm_context_t context, int *num_of_ids,
				   int *error_code)
{
	GError *error = NULL;
	gint return_code = 0;

	if (!org_tizen_alarm_manager_alarm_get_number_of_ids(context.proxy,
			     context.pid, num_of_ids, &return_code, &error)) {
		/* dbus-glib error */
		/* error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		"org_tizen_alarm_manager_alarm_get_number_of_ids() failed. "
		"return_code[%d], return_code[%s]\n", \
			return_code, error->message);
		if (error_code)
			*error_code = -1;	/*-1 means that system 
							failed internally*/
		return false;
	}

	if (return_code != 0) {
		if (error_code)
			*error_code = return_code;
		return false;
	}

	return true;

}

bool _send_alarm_get_info(alarm_context_t context, alarm_id_t alarm_id,
			  alarm_info_t *alarm_info, int *error_code)
{
	GError *error = NULL;
	int return_code = 0;

	if (!org_tizen_alarm_manager_alarm_get_info(context.proxy,
		     context.pid, alarm_id, &alarm_info->start.year,
		     &alarm_info->start.month, &alarm_info->start.day,
		     &alarm_info->start.hour, &alarm_info->start.min,
		     &alarm_info->start.sec, &alarm_info->end.year,
		     &alarm_info->end.month, &alarm_info->end.day,
                     &alarm_info->mode.u_interval.day_of_week,
                     (gint *)&alarm_info->mode.repeat,
		     &alarm_info->alarm_type, &alarm_info->reserved_info,
						      &return_code, &error)) {
		/*dbus-glib error */
		/* error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		"org_tizen_alarm_manager_alarm_get_info() failed. "
		     "alarm_id[%d], return_code[%d]\n", alarm_id, return_code);
		if (error_code)
			*error_code = -1;	/*-1 means that system failed 
								internally.*/
		return false;
	}

	if (return_code != 0) {
		if (error_code)
			*error_code = return_code;
		return false;
	}
	return true;

}

bool _send_alarm_get_next_duetime(alarm_context_t context,
				 alarm_id_t alarm_id, time_t* duetime,
				 int *error_code)
{
	GError *error = NULL;
	int return_code = 0;

	if (!com_samsung_alarm_manager_alarm_get_next_duetime(context.proxy,
			     context.pid, alarm_id, duetime, &return_code, &error)) {
		/*dbus-glib error */
		/*error_code should be set */
		ALARM_MGR_EXCEPTION_PRINT(
		"com_samsung_alarm_manager_alarm_get_next_duetime() failed. "
		     "alarm_id[%d], return_code[%d]\n", alarm_id, return_code);
		if (error_code)
			*error_code = -1;	/*-1 means that system
							failed internally.*/

		return false;
	}

	if (return_code != 0) {
		if (error_code)
			*error_code = return_code;
		return false;
	}
	return true;
}

