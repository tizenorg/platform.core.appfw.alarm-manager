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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>

#include <glib.h>
#include <glib-object.h>
#include <dlog/dlog.h>

#include <alarm.h>
#include <bundle.h>
#include <appsvc.h>
#include <dlog.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "TEST_LOG"

#define TEST_APP_LOG_PRINT(FMT, ARG...) SLOGD(FMT, ##ARG);

#define AUTO_TEST 0		/*Flag to Enable or Disable auto testing */

#if AUTO_TEST

#define ITER_COUNT 200		/*Stress testing iteration count */

typedef enum {
	INTERVAL = 1,
	INTERVAL_APPSVC,
	CALENDAR,
	CALENDAR_APPSVC
} create_subtype;		/*alarm type */

int sub_type = 0;

/*Used to hold multiple alarm id created */
alarm_id_t alarm_id_arr[ITER_COUNT] = { 0, };

#endif

#define USER_INPUT_YES 1
#define USER_INPUT_NO 2

#define USER_INPUT_DEFAULT 1
#define USER_INPUT_VOLATILE 2

GMainLoop *mainloop = NULL;	/*gmain loop */

alarm_id_t alarm_id;		/*Alarm identificaiton no */

guint pgm_exit_time = 2000000;	/*gmainloop life in milli second for few scenarios */

bool cleanup_req = false;	/*tells whether test program cleanup required
				   or not before program exit */

bool multiple_alarm = false;

/* Local Functions*/
static int __alarm_callback(alarm_id_t alarm_id, void *user_param);

static int __alarm_callback1(alarm_id_t alarm_id, void *user_param);

static void __create_interval_alarm(void);

static void __create_app_svc_interval_alarm(void);

static void __create_calendar_alarm(void);

static void __create_app_svc_calendar_alarm(void);

static gboolean __timout_handler(gpointer user_data);

static void __test_pgm_clean_up(void);

static int __get_integer_input_data(void);

static void __test_alarm_manager_delete_alarm(void);

static void __test_alarm_manager_create_alarm(void);

static void __test_alarm_manager_get_information(void);

static void __test_alarm_manager_get_appsvc_information(void);

static void __test_alarm_manager_get_alarm_ids(void);

static void __print_api_return_result(int err_no, const char *api_name);

static void __test_alarm_manager_callback(void);

static int __get_alarm_persistance_type(void);

static char *__get_alarm_destination(void);

static int __get_alarm_repeat_mode(void);

static void __print_appsvc_bundle_data(const char *key, const char *val, void *data);


/*This function will be executed upon alarm expiry*/
static int __alarm_callback(alarm_id_t alarm_id_rec, void *user_param)
{
	time_t current_time = { 0, };
	time(&current_time);

	TEST_APP_LOG_PRINT("Alarm[%d] has expired at %s\n", alarm_id_rec,
			   ctime(&current_time));
	printf("Alarm[%d] has expired at %s\n", alarm_id_rec,
	       ctime(&current_time));

	return 0;

}

/*This function will be executed upon alarm expiry*/
static int __alarm_callback1(alarm_id_t alarm_id_rec, void *user_param)
{
	time_t current_time = { 0, };
	time(&current_time);
	printf("Callback function 2 executed\n");

	TEST_APP_LOG_PRINT("Alarm[%d] has expired at %s\n", alarm_id_rec,
			   ctime(&current_time));
	printf("Alarm[%d] has expired at %s\n", alarm_id_rec,
	       ctime(&current_time));

	return 0;

}

/*This function prints the result of Alarm manager API call*/
static void __print_api_return_result(int err_no, const char *api_name)
{

	printf("Alarm Manager API \"%s\" retuned with result: ", api_name);

	switch (err_no) {
	case ERR_ALARM_INVALID_PARAM:{
			printf
			    ("Alarm Manager error: \"ERR_ALARM_INVALID_PARAM\"\n");
			break;
		}
	case ERR_ALARM_INVALID_ID:{
			printf
			    ("Alarm Manager error: \"ERR_ALARM_INVALID_ID\"\n");
			break;
		}
	case ERR_ALARM_INVALID_REPEAT:{
			printf
			    ("Alarm Manager error: \"ERR_ALARM_INVALID_REPEAT\"\n");
			break;
		}
	case ERR_ALARM_INVALID_TIME:{
			printf
			    ("Alarm Manager error: \"ERR_ALARM_INVALID_TIME\"\n");
			break;
		}
	case ERR_ALARM_INVALID_DATE:{
			printf
			    ("Alarm Manager error: \"ERR_ALARM_INVALID_DATE\"\n");
			break;
		}
	case ERR_ALARM_NO_SERVICE_NAME:{
			printf
			    ("Alarm Manager error: \"ERR_ALARM_NO_SERVICE_NAME\"\n");
			break;
		}
	case ERR_ALARM_NO_PERMISSION:{
			printf
			    ("Alarm Manager error: \"ERR_ALARM_NO_PERMISSION\"\n");
			break;
		}
	case ERR_ALARM_INVALID_TYPE:{
			printf
			    ("Alarm Manager error: \"ERR_ALARM_INVALID_TYPE\"\n");
			break;
		}
	case ERR_ALARM_SYSTEM_FAIL:{
			printf
			    ("Alarm Manager error: \"ERR_ALARM_SYSTEM_FAIL\"\n");
			break;
		}
	case ALARMMGR_RESULT_SUCCESS:{
			printf
			    ("Alarm API Success: \"ALARMMGR_RESULT_SUCCESS\"\n");
			break;
		}
	default:{
			printf("Alarm Manager error: Unknow Error \n");
		}
	}
#if AUTO_TEST
	/*Depending upon the test scenario,
	   these errno are added to assert()! */
	assert(err_no == ALARMMGR_RESULT_SUCCESS
	       || err_no == ERR_ALARM_INVALID_ID
	       || err_no == ERR_ALARM_INVALID_PARAM);
#else
	assert(err_no == ALARMMGR_RESULT_SUCCESS);
#endif

	return;
}


static void __print_appsvc_bundle_data(const char *key, const char *val, void *data){
	printf("Key-->[%s], Val-->[%s]\n",key,val);
	return;
}

/*This function is to take command line integer input from the user*/
static int __get_integer_input_data(void)
{
	char input_str[32] = { 0, };
	int data = 0;
	fflush(stdin);

	if (fgets(input_str, 1024, stdin) == NULL) {
		printf("Input buffer overflow....\n");
		return -1;
	}

	if (sscanf(input_str, "%d", &data) != 1) {
		printf("Input only integer option....\n");
		return -1;
	}

	return data;
}

/*Function to get alarm persistance type*/
static int __get_alarm_persistance_type(void)
{
	int type = ALARM_TYPE_VOLATILE;
	int alarm_type = 0;
	printf("Enter Type\n");
	printf("1. ALARM_TYPE_DEFAULT \n");
	printf("2. ALARM_TYPE_VOLATILE\n");
	printf("Enter Choice between [1-2]....\n");
	alarm_type = __get_integer_input_data();

	switch (alarm_type) {
	case USER_INPUT_DEFAULT:
		{
			type = ALARM_TYPE_DEFAULT;
			break;
		}
	case USER_INPUT_VOLATILE:
		{
			type = ALARM_TYPE_VOLATILE;
			break;
		}
	default:
		{
			printf("Invalid option....Try Again\n");
			exit(-1);
		}

	}
	return type;
}

static char *__get_alarm_destination(void)
{
	int alarm_type = 0;
	char *destination = NULL;
	printf("[Optional]: Do you want to provide Destination?\n");
	printf("1. YES\n");
	printf("2. NO \n");
	printf("Enter Choice between [1-2]....\n");
	alarm_type = __get_integer_input_data();
	switch (alarm_type) {
	case USER_INPUT_YES:
		{
			fflush(stdin);

			destination = (char *)malloc(1024);
			if (NULL == destination) {
				return NULL;
			}
			printf
			    ("Enter destination: [Ex:org.tizen.alarm-test "
			     "or org.tizen.gallery...]\n");
			if (fgets(destination, 1024, stdin) == NULL) {
				printf("Input buffer overflow....,try again\n");
				exit(-1);
			}

			destination[strlen(destination) - 1] = '\0';

			break;
		}
	case USER_INPUT_NO:
		{
			break;
		}
	default:
		{
			printf("Invalid option....Try Again\n");
			exit(-1);
		}
	}

	return destination;
}

static int __get_alarm_repeat_mode(void)
{
	int repeat_mode = 0;
	printf("Enter Repeat Mode:\n");
	printf("0. ALARM_REPEAT_MODE_ONCE\n");
	printf("1. ALARM_REPEAT_MODE_REPEAT\n");
	printf("2. ALARM_REPEAT_MODE_WEEKLY\n");
	printf("3. ALARM_REPEAT_MODE_MONTHLY\n");
	printf("4. ALARM_REPEAT_MODE_ANNUALLY\n");
	printf("5. ALARM_REPEAT_MODE_MAX\n");
	printf("Enter Choice between [0-5]....\n");
	repeat_mode = __get_integer_input_data();
	if (repeat_mode > 5 || repeat_mode < 0) {
		printf("Invalid option....Try Again\n");
		exit(-1);
	}
	return repeat_mode;
}

/*Function to create alarm in the calender input format*/
static void __create_calendar_alarm(void)
{
	char *destination = NULL;
	time_t current_time;
	struct tm current_tm;
	alarm_entry_t *alarm_info = NULL;
	int result = 0;
	alarm_date_t test_time;
	int type = 0;
	int year, month, day, hour, min, sec;
	int repeat_mode = 0;

	year = month = day = hour = min = sec = 0;

	time(&current_time);
	printf("current time: %s\n", ctime(&current_time));

	localtime_r(&current_time, &current_tm);
	alarm_info = alarmmgr_create_alarm();
	assert(alarm_info != NULL);

#if AUTO_TEST
	test_time.year = current_tm.tm_year;
	test_time.month = current_tm.tm_mon;
	test_time.day = current_tm.tm_mday;

	test_time.hour = current_tm.tm_hour;
	test_time.min = current_tm.tm_min + 1;
	test_time.sec = 5;
#else

	printf("Enter Year of Alarm Expiry: ");
	year = __get_integer_input_data();

	printf("Enter month of Alarm Expiry: ");
	month = __get_integer_input_data();

	printf("Enter day of Alarm Expiry: ");
	day = __get_integer_input_data();

	printf("Enter hour of Alarm Expiry: ");
	hour = __get_integer_input_data();

	printf("Enter minute of Alarm Expiry: ");
	min = __get_integer_input_data();

	printf("Enter second Alarm Expiry: ");
	sec = __get_integer_input_data();

	test_time.year = year;
	test_time.month = month;
	test_time.day = day;
	test_time.hour = hour;
	test_time.min = min;
	test_time.sec = sec;
	printf
	    ("Note: Test Program will exit After [Year:%d Month:%d Day:%d "
	     "at Hour:%d Min:%d Sec:%d]. Please wait....\n\n",
	     year, month, day, hour, min, sec);
#endif

	result = alarmmgr_set_time(alarm_info, test_time);
	__print_api_return_result(result, "alarmmgr_set_time");

#if AUTO_TEST
	repeat_mode = ALARM_REPEAT_MODE_WEEKLY;
#else
	repeat_mode = __get_alarm_repeat_mode();
#endif
	result = alarmmgr_set_repeat_mode(alarm_info, repeat_mode,
					  ALARM_WDAY_SUNDAY | ALARM_WDAY_MONDAY
					  | ALARM_WDAY_TUESDAY |
					  ALARM_WDAY_WEDNESDAY |
					  ALARM_WDAY_THURSDAY |
					  ALARM_WDAY_FRIDAY |
					  ALARM_WDAY_SATURDAY);
	__print_api_return_result(result, "alarmmgr_set_repeat_mode");

#if AUTO_TEST
	type = (random() % 2) + 1;
#else
	type = __get_alarm_persistance_type();
#endif
	result = alarmmgr_set_type(alarm_info, type);
	__print_api_return_result(result, "alarmmgr_set_type");

#if AUTO_TEST
	result =
	    alarmmgr_add_alarm_with_localtime(alarm_info, destination,
					      &alarm_id);
#else
	destination = __get_alarm_destination();
	result =
	    alarmmgr_add_alarm_with_localtime(alarm_info, destination,
					      &alarm_id);
	__print_api_return_result(result, "alarmmgr_add_alarm_with_localtime");
	printf("Created Alarm with Alarm ID %d\n", alarm_id);
#endif

	cleanup_req = true;

	pgm_exit_time = 70000;
	printf
	    ("Note: Test Program will exit After %f seconds..."
	     "Please wait....\n\n", (float)pgm_exit_time / 1000);

	return;
}

/*Function to create alarm with a delay for first expiry and
alarm expiry inteval for subsequent alarm*/
static void __create_interval_alarm(void)
{
	time_t current_time = { 0, };
	int result = 0;
	int delay = 0;
	int interval = 0;
	int type = 0;

	char *destination = NULL;

	time(&current_time);
#if AUTO_TEST
	int i = 0;
	delay = 1;
#else
	printf("current time: %s\n", ctime(&current_time));
	printf("Enter First Delay (in seconds) for alarm: \n");
	delay = __get_integer_input_data();
#endif

#if AUTO_TEST
	interval = 1;
#else
	printf("Enter Alarm repeat Interval in seconds (if 0, no repeat): \n");
	interval = __get_integer_input_data();
#endif

#if AUTO_TEST
	type = (random() % 2) + 1;
#else
	type = __get_alarm_persistance_type();
#endif

#if AUTO_TEST
	multiple_alarm = true;
	for (i = 0; i < ITER_COUNT; i++) {
		interval++;
		delay++;
		result =
		    alarmmgr_add_alarm(type, delay, interval,
				       destination, &alarm_id_arr[i]);
		__print_api_return_result(result, "alarmmgr_add_alarm");
	}
#else
	destination = __get_alarm_destination();
	result =
	    alarmmgr_add_alarm(type, delay, interval, destination, &alarm_id);
	__print_api_return_result(result, "alarmmgr_add_alarm");
	printf("Created Alarm with Alarm ID %d\n", alarm_id);
#endif

	if (interval > 0) {
		cleanup_req = true;
	}
	pgm_exit_time = ((delay + interval) * 1000) + 100;
	printf
	    ("Note: Test Program will exit After %f seconds..."
	     "Please wait....\n\n", (float)pgm_exit_time / 1000);

	return;
}

/*Function to create alarm (with app-svc information)
in the calender input format */
static void __create_app_svc_calendar_alarm(void)
{
	time_t current_time;
	struct tm current_tm;
	alarm_entry_t *alarm_info = NULL;
	int result;
	alarm_date_t test_time;
	int type = 0;
	bundle *b = NULL;
	int op_type = 0;
	int repeat_mode = 0;

	char pkgname[1024] = { 0, };

	memset(pkgname, '\0', 1024);

	int year, month, day, hour, min, sec;
	year = month = day = hour = min = sec = 0;

	time(&current_time);
	printf("current time: %s\n", ctime(&current_time));

	localtime_r(&current_time, &current_tm);
	alarm_info = alarmmgr_create_alarm();
	assert(alarm_info != NULL);

#if AUTO_TEST
	test_time.year = current_tm.tm_year;
	test_time.month = current_tm.tm_mon;
	test_time.day = current_tm.tm_mday;

	test_time.hour = current_tm.tm_hour;
	test_time.min = current_tm.tm_min + 1;
	test_time.sec = 5;
#else
	printf("Enter Year of Alarm Expiry: ");
	year = __get_integer_input_data();

	printf("Enter month of Alarm Expiry: ");
	month = __get_integer_input_data();

	printf("Enter day of Alarm Expiry: ");
	day = __get_integer_input_data();

	printf("Enter hour of Alarm Expiry: ");
	hour = __get_integer_input_data();

	printf("Enter minute of Alarm Expiry: ");
	min = __get_integer_input_data();

	printf("Enter second Alarm Expiry: ");
	sec = __get_integer_input_data();

	test_time.year = year;
	test_time.month = month;
	test_time.day = day;
	test_time.hour = hour;
	test_time.min = min;
	test_time.sec = sec;

	printf
	    ("Note: Test Program will exit After "
	     "[Year:%d Month:%d Day:%d at Hour:%d Min:%d Sec:%d]."
	     " Please wait....\n\n", year, month, day, hour, min, sec);
#endif
	result = alarmmgr_set_time(alarm_info, test_time);
	__print_api_return_result(result, "alarmmgr_set_time");

#if AUTO_TEST
	repeat_mode = ALARM_REPEAT_MODE_WEEKLY;
#else
	repeat_mode = __get_alarm_repeat_mode();
#endif
	result = alarmmgr_set_repeat_mode(alarm_info, repeat_mode,
					  ALARM_WDAY_SUNDAY | ALARM_WDAY_MONDAY
					  | ALARM_WDAY_TUESDAY |
					  ALARM_WDAY_WEDNESDAY |
					  ALARM_WDAY_THURSDAY |
					  ALARM_WDAY_FRIDAY |
					  ALARM_WDAY_SATURDAY);
	__print_api_return_result(result, "alarmmgr_set_repeat_mode");

#if AUTO_TEST
	type = (random() % 2) + 1;
#else
	type = __get_alarm_persistance_type();
#endif
	result = alarmmgr_set_type(alarm_info, type);
	__print_api_return_result(result, "alarmmgr_set_type");

	b = bundle_create();

	assert(b != NULL);

#if AUTO_TEST
	op_type = 1;
#else
	printf("Enter APPSVC Operation\n");
	printf("1. APPSVC_OPERATION_DEFAULT \n");
	printf("2. APPSVC_OPERATION_SEARCH\n");
	printf("3. No operation\n");
	op_type = __get_integer_input_data();
#endif
	switch (op_type) {
	case 1:{
			appsvc_set_operation(b, APPSVC_OPERATION_DEFAULT);
			break;
		}
	case 2:{
			appsvc_set_operation(b, APPSVC_OPERATION_SEARCH);
			break;
		}
	case 3:
		{
			printf("NO APPSVC OPERATION!!!\n");
			break;
		}
	default:{
			printf("Invalid option....Try Again\n");
			exit(-1);
		}

	}

#if AUTO_TEST
	sprintf(pkgname, "org.tizen.gallery");
#else
	fflush(stdin);

	printf
	    ("Enter Pkgname: [Ex:org.tizen.alarm-test "
	     "or org.tizen.gallery...]\n");
	if (fgets(pkgname, 1024, stdin) == NULL) {
		printf("Input buffer overflow....,try again\n");
		exit(-1);
	}

	pkgname[strlen(pkgname) - 1] = '\0';
#endif
	appsvc_set_pkgname(b, pkgname);

	result =
	    alarmmgr_add_alarm_appsvc_with_localtime(alarm_info, (void *)b,
						     &alarm_id);
	__print_api_return_result(result,
				  "alarmmgr_add_alarm_appsvc_with_localtime");

	printf("Alarm created with alarm id %d\n", alarm_id);

	cleanup_req = true;

	pgm_exit_time = 70000;

	printf
	    ("Note: Test Program will exit After %f "
	     "seconds...Please wait....\n\n", (float)pgm_exit_time / 1000);
	printf
	    ("Upon Alarm Expiry, %s application will be "
	     "launched (purpose:testing)\n", pkgname);

}

/*Function to create alarm with a delay for first expiry and
alarm expiry inteval for subsequent alarm (with app-svc information)*/
static void __create_app_svc_interval_alarm(void)
{
	time_t current_time = { 0, };
	int result = 0;
	int delay = 0;
	int interval = 0;
	int type = 0;
	bundle *b = NULL;
	int op_type = 0;

	char pkgname[1024] = { 0, };

	memset(pkgname, '\0', 1024);

	time(&current_time);
#if AUTO_TEST
	int i = 0;
	delay = (random() % 9) + 1;
	interval = (random() % 9);
#else
	printf("current time: %s\n", ctime(&current_time));
	printf("Enter First Delay (in seconds) for alarm: \n");
	delay = __get_integer_input_data();
	printf("Enter Alarm repeat Interval in seconds (if 0, no repeat): \n");
	interval = __get_integer_input_data();
#endif
	if (interval > 0) {
		cleanup_req = true;
	}
	pgm_exit_time = ((delay + interval) * 1000) + 100;
	printf
	    ("Note: Test Program will exit After %f "
	     "seconds...Please wait....\n\n", (float)pgm_exit_time / 1000);

#if AUTO_TEST
	type = (random() % 2) + 1;
#else
	type = __get_alarm_persistance_type();
#endif

	b = bundle_create();

	assert(b != NULL);

#if AUTO_TEST
	op_type = 1;
#else
	printf("Enter APPSVC Operation\n");
	printf("1. APPSVC_OPERATION_DEFAULT \n");
	printf("2. APPSVC_OPERATION_SEARCH\n");
	printf("3. No operation\n");
	op_type = __get_integer_input_data();
#endif

	switch (op_type) {
	case 1:{
			appsvc_set_operation(b, APPSVC_OPERATION_DEFAULT);
			break;
		}
	case 2:{
			appsvc_set_operation(b, APPSVC_OPERATION_SEARCH);
			break;
		}
	case 3:
		{
			printf("NO APPSVC OPERATION!!!\n");
			break;
		}
	default:{
			printf("Invalid option....Try Again\n");
			exit(-1);
		}

	}

	fflush(stdin);

#if AUTO_TEST
	sprintf(pkgname, "org.tizen.gallery");
#else
	printf
	    ("Enter Pkgname: [Ex:org.tizen.alarm-test "
	     "or org.tizen.gallery...]\n");
	if (fgets(pkgname, 1024, stdin) == NULL) {
		printf("Input buffer overflow...Try Again.\n");
		exit(-1);
	}
	pkgname[strlen(pkgname) - 1] = '\0';
#endif
	appsvc_set_pkgname(b, pkgname);

#if AUTO_TEST
	multiple_alarm = true;
	for (i = 0; i < ITER_COUNT; i++) {
		interval++;
		delay++;
		result =
		    alarmmgr_add_alarm_appsvc(type, delay, interval, (void *)b,
					      &alarm_id_arr[i]);
		__print_api_return_result(result, "alarmmgr_add_alarm");
	}
#else
	result =
	    alarmmgr_add_alarm_appsvc(type, delay, interval, (void *)b,
				      &alarm_id);
	__print_api_return_result(result, "alarmmgr_add_alarm_appsvc");
	printf("Created Alarm with Alarm ID %d\n", alarm_id);
#endif

	if (interval > 0) {
		cleanup_req = true;
	}
	pgm_exit_time = ((delay + interval) * 1000) + 100;
	printf
	    ("Note: Test Program will exit After %f seconds..."
	     "Please wait....\n\n", (float)pgm_exit_time / 1000);

	return;
}

/*This function will be executed before program exit (if required)*/
static void __test_pgm_clean_up(void)
{
	int result = 0;
	printf("Cleaning up test program\n");
#if AUTO_TEST
	int i = 0;
	if (multiple_alarm == true) {
		for (i = 0; i < ITER_COUNT; i++) {
			result = alarmmgr_remove_alarm(alarm_id_arr[i]);
			__print_api_return_result(result,
						  "alarmmgr_remove_alarm");
		}
		multiple_alarm = false;
	} else {
		result = alarmmgr_remove_alarm(alarm_id);
		__print_api_return_result(result, "alarmmgr_remove_alarm");
	}
#else
	result = alarmmgr_remove_alarm(alarm_id);
	__print_api_return_result(result, "alarmmgr_remove_alarm");
#endif
	return;
}

/*Function to exit gmainloop*/
static gboolean __timout_handler(gpointer user_data)
{
	if (mainloop)
		g_main_loop_quit(mainloop);

	return false;
}

/*Main alarm create function which provides options
 for different types of alarm creation*/
static void __test_alarm_manager_callback(void)
{
	printf("Inside Alarm Callback test\n");
	int result;

	mainloop = g_main_loop_new(NULL, FALSE);
	assert(mainloop != NULL);

	result = alarmmgr_init("org.tizen.alarm-test");
	__print_api_return_result(result, "alarmmgr_init");

	result = alarmmgr_set_cb(__alarm_callback, NULL);
	__print_api_return_result(result, "alarmmgr_set_cb");
	result = alarmmgr_set_cb(__alarm_callback1, NULL);
	__print_api_return_result(result, "alarmmgr_set_cb");

	__create_interval_alarm();

	g_timeout_add(pgm_exit_time, __timout_handler, NULL);

	g_main_loop_run(mainloop);

	if (cleanup_req) {
		__test_pgm_clean_up();
	}

	return;
}

/*Main alarm create function which provides options
 for different types of alarm creation*/
static void __test_alarm_manager_create_alarm(void)
{
	int result = 0;
	int alarm_type = 0;

#if AUTO_TEST
	alarm_type = sub_type;
#else
	char destination[1024] = { 0, };

	printf("Enter Alarm Type\n");
	printf("1. Interval with initial delay\n");
	printf("2. App-svc Interval alarm with initial delay\n");
	printf("3. Calendar Alarm\n");
	printf("4. App-svc Calendar Alarm\n");
	printf("Enter Choice between [1-4]....\n");
	alarm_type = __get_integer_input_data();
#endif
	mainloop = g_main_loop_new(NULL, FALSE);
	assert(mainloop != NULL);

	switch (alarm_type) {
	case 1:
		{
#if AUTO_TEST
			result = alarmmgr_init("org.tizen.1000");
#else
			fflush(stdin);
			printf
			    ("Enter pkgname (used to generate service name)"
			     "[Ex:org.tizen.alarm-test or "
			     "org.tizen.1000 etc]\n");
			if (fgets(destination, 1024, stdin) == NULL) {
				printf("Input buffer overflow....,try again\n");
				exit(-1);
			}

			destination[strlen(destination) - 1] = '\0';

			result = alarmmgr_init(destination);
#endif
			__print_api_return_result(result, "alarmmgr_init");

			result = alarmmgr_set_cb(__alarm_callback, NULL);
			__print_api_return_result(result, "alarmmgr_set_cb");

			__create_interval_alarm();

			break;
		}
	case 2:
		{
			printf("Inside APPSVC Interval Alarm test\n");
			__create_app_svc_interval_alarm();
			break;
		}
	case 3:
		{
			printf("Inside Calendar Alarm test\n");
#if AUTO_TEST
			result = alarmmgr_init("org.tizen.1000");
#else
			fflush(stdin);
			printf
			    ("Enter pkgname (used to generate service name)"
			     "[Ex:org.tizen.alarm-test or "
			     "org.tizen.1000 etc]\n");
			if (fgets(destination, 1024, stdin) == NULL) {
				printf("Input buffer overflow....,try again\n");
				exit(-1);
			}

			destination[strlen(destination) - 1] = '\0';

			result = alarmmgr_init(destination);
#endif
			__print_api_return_result(result, "alarmmgr_init");

			result = alarmmgr_set_cb(__alarm_callback, NULL);
			__print_api_return_result(result, "alarmmgr_set_cb");

			__create_calendar_alarm();
			break;
		}
	case 4:
		{
			printf("Inside APPSVC Calendar Alarm test\n");
			__create_app_svc_calendar_alarm();
			break;
		}
	default:
		{
			printf("Invalid option....Try Again\n");
			return;
		}
	}

	g_timeout_add(pgm_exit_time, __timout_handler, NULL);

	g_main_loop_run(mainloop);

	if (cleanup_req) {
		__test_pgm_clean_up();
	}

	return;
}

/*Main alarm get information function which provides
 different options of getting alarm information*/
static void __test_alarm_manager_get_information(void)
{

	printf("Inside Get Info Interval Alarm test\n");
	int option = 0;
	int result = 0;
	alarm_entry_t *alarm_info;
	int type = 0;

	alarm_date_t time = { 0, };

#if AUTO_TEST
	option = 2;
#else
	printf("1. Get Existing Alarm's Information\n");
	printf("2. Create Alarm & Get Information\n");
	option = __get_integer_input_data();
#endif
	alarm_info = alarmmgr_create_alarm();
	assert(alarm_info != NULL);

	switch (option) {
	case 1:
		{
			printf("Enter Alarm Id: \n");
			alarm_id = __get_integer_input_data();
			result = alarmmgr_get_info(alarm_id, alarm_info);
			__print_api_return_result(result, "alarmmgr_get_info");
			result = alarmmgr_get_time(alarm_info, &time);
			__print_api_return_result(result, "alarmmgr_get_time");
			printf("day is %d\n", time.day);
			printf("Alarm Get Info Success\n");
			break;
		}
	case 2:
		{
			result = alarmmgr_init("org.tizen.10000");
			__print_api_return_result(result, "alarmmgr_init");

#if AUTO_TEST
			type = (random() % 2) + 1;
#else
			type = __get_alarm_persistance_type();
#endif

			result =
			    alarmmgr_add_alarm(type, 2, 0, NULL, &alarm_id);
			__print_api_return_result(result, "alarmmgr_add_alarm");
			printf("Created Alarm with Alarm ID %d\n", alarm_id);

			pgm_exit_time = 3000;
			g_timeout_add(pgm_exit_time, __timout_handler, NULL);

			result = alarmmgr_get_info(alarm_id, alarm_info);
			__print_api_return_result(result, "alarmmgr_get_info");

			alarm_date_t time = { 0, };
			result = alarmmgr_get_time(alarm_info, &time);
			__print_api_return_result(result, "alarmmgr_get_time");

			printf("day is %d\n", time.day);
			printf("Alarm Get Info Success\n");

			__test_pgm_clean_up();

			break;
		}
	default:
		{
			printf("Invalid option\n");
			exit(-1);
		}
	}
	return;
}

static void __test_alarm_manager_get_appsvc_information(void)
{


	printf("Inside Get Info Interval Alarm test\n");
	int option = 0;
	int result = 0;
	alarm_entry_t *alarm_info;
	bundle *b = NULL;

#if AUTO_TEST
	option = 2;
#else
	printf("1. Get Existing Alarm's Information\n");
	printf("2. Create Alarm & Get Information\n");
	option = __get_integer_input_data();
#endif
	alarm_info = alarmmgr_create_alarm();
	assert(alarm_info != NULL);

	switch (option) {
	case 1:
		{
			printf("Enter Alarm ID:\n");
			alarm_id = __get_integer_input_data();

			b = alarmmgr_get_alarm_appsvc_info(alarm_id, &result);

			if (b)
				bundle_iterate(b, __print_appsvc_bundle_data, NULL);

			__print_api_return_result(result, "alarmmgr_get_alarm_appsvc_info");

			break;
		}
	case 2:
		{
			mainloop = g_main_loop_new(NULL, FALSE);
			assert(mainloop != NULL);

			__create_app_svc_interval_alarm();

			b = alarmmgr_get_alarm_appsvc_info(alarm_id, &result);
			__print_api_return_result(result, "alarmmgr_get_alarm_appsvc_info");
			if (b)
				bundle_iterate(b, __print_appsvc_bundle_data, NULL);

			g_timeout_add(2000, __timout_handler, NULL);

			g_main_loop_run(mainloop);

			if (cleanup_req) {
				__test_pgm_clean_up();

			break;
		}
	default:
		{
			printf("Invalid option\n");
			exit(-1);
		}
	}
	}

	return;
}

static int __alarm_id_retrive_callback(alarm_id_t id, void *user_param)
{
	int *n = NULL;
	n = (int *)user_param;
	if (n == NULL) {
		printf("Invalid Parameter in test program");
		return -1;
	}
	printf("[%d]Alarm ID is %d\n", *n, id);
	(*n)++;

	return 0;
}

static void __test_alarm_manager_get_alarm_ids(void)
{
	printf("Inside Get Ids Alarm test\n");
	int option = 0;
	int result = 0;
	int n = 1;
	int type = 0;
#if AUTO_TEST
	option = 2;
#else
	printf("1. Get Existing Alarm's ID\n");
	printf("2. Create Alarm & Get ID\n");

	option = __get_integer_input_data();
#endif
	switch (option) {
	case 1:
		{
			result = alarmmgr_init("org.tizen.1000");
			__print_api_return_result(result, "alarmmgr_init");
			result =
			    alarmmgr_enum_alarm_ids(__alarm_id_retrive_callback,
						    (void *)&n);
			__print_api_return_result(result,
						  "alarmmgr_enum_alarm_ids");
			break;
		}
	case 2:
		{
			result = alarmmgr_init("org.tizen.test");
			__print_api_return_result(result, "alarmmgr_init");

#if AUTO_TEST
			type = (random() % 2) + 1;
#else
			type = __get_alarm_persistance_type();
#endif
			result = alarmmgr_add_alarm(type, 2, 0,
						    NULL, &alarm_id);
			printf("Created Alarm with Alarm ID %d\n", alarm_id);

			__print_api_return_result(result, "alarmmgr_add_alarm");

			result = alarmmgr_add_alarm(type, 3, 0,
						    NULL, &alarm_id);
			__print_api_return_result(result, "alarmmgr_add_alarm");
			printf("Created Alarm with Alarm ID %d\n", alarm_id);

			pgm_exit_time = 5100;
			g_timeout_add(pgm_exit_time, __timout_handler, NULL);

			result =
			    alarmmgr_enum_alarm_ids(__alarm_id_retrive_callback,
						    (void *)&n);
			__print_api_return_result(result,
						  "alarmmgr_enum_alarm_ids");

			break;
		}
	default:
		{
			printf("Invalid option\n");
			exit(-1);
		}
	}
	return;
}

/*Main alarm delete test function which provides options
 for different types of alarm deletion*/
static void __test_alarm_manager_delete_alarm(void)
{
	int option = 0;
	int result = 0;
	int type = 0;
	printf("Inside Delete Alarm test\n");

#if AUTO_TEST
	option = 2;
#else
	printf("1. Delete Existing Alarm\n");
	printf("2. Create Alarm & Delete\n");

	option = __get_integer_input_data();
#endif
	switch (option) {
	case 1:
		{
			printf("Enter Alarm Id: \n");
			option = __get_integer_input_data();

			result = alarmmgr_remove_alarm(option);
			__print_api_return_result(result,
						  "alarmmgr_remove_alarm");
			break;
		}
	case 2:
		{
			result = alarmmgr_init("org.tizen.test");
			__print_api_return_result(result, "alarmmgr_init");

#if AUTO_TEST
			type = (random() % 2) + 1;
#else
			type = __get_alarm_persistance_type();
#endif
			result = alarmmgr_add_alarm(type, 2, 0,
						    NULL, &alarm_id);
			__print_api_return_result(result, "alarmmgr_add_alamr");
			printf("Created Alarm with Alarm ID %d\n", alarm_id);

			result = alarmmgr_remove_alarm(alarm_id);
			__print_api_return_result(result,
						  "alarmmgr_remove_alarm");

			break;
		}
	default:
		{
			printf("Invalid Option\n");
			break;
		}
	}
	return;
}

/*Entry point of the test program*/
int main(int argc, char **argv)
{
#if AUTO_TEST
	int i = 0;
	int k = 0;
	int iteration = 0;

	int fd = 0;

	/*test result will be written into below file */
	fd = open("/var/alarm_test.log", O_CREAT | O_RDWR | O_APPEND, 0666);
	if (fd < 0) {
		printf("Unable to create log file\n");
		return -1;
	}
	if (argc < 3) {
		printf("Insufficient arguments\n");
		printf
		    ("Example: ./test_alarmmgr.exe {all|create_interval|"
		     "create_svc_interval|create_calendar|create_svc_calendar|"
		     "delete|get_id|callback} <iteration>");
		return -1;
	}

	close(1);
	close(2);
	dup(fd);
	dup(fd);

	char *test_job = NULL;

	test_job = argv[1];
	iteration = atoi(argv[2]);

	printf("##################Alarm Test for %s Starts##################\n",
	       test_job);
	g_type_init();

	for (i = 0; i < iteration; i++) {
		printf("########ITERATION %d #############\n", i);
		if (strncmp(test_job, "create_interval", 15) == 0) {
			sub_type = INTERVAL;
			__test_alarm_manager_create_alarm();
		} else if (strncmp(test_job, "create_svc_interval", 19) == 0) {
			sub_type = INTERVAL_APPSVC;
			__test_alarm_manager_create_alarm();
		} else if (strncmp(test_job, "create_calendar", 15) == 0) {
			sub_type = CALENDAR;
			__test_alarm_manager_create_alarm();
		} else if (strncmp(test_job, "create_svc_calendar", 19) == 0) {
			sub_type = CALENDAR_APPSVC;
			__test_alarm_manager_create_alarm();
		} else if (strncmp(test_job, "delete", 6) == 0) {
			__test_alarm_manager_delete_alarm();
		} else if (strncmp(test_job, "get_info", 8) == 0) {
			__test_alarm_manager_get_information();
		} else if (strncmp(test_job, "get_id", 6) == 0) {
			__test_alarm_manager_get_alarm_ids();
		} else if (strncmp(test_job, "callback", 8) == 0) {
			__test_alarm_manager_callback();
		} else if (strncmp(test_job, "all", 3) == 0) {
			for (k = INTERVAL; k <= CALENDAR_APPSVC; k++) {
				sub_type = k;
				__test_alarm_manager_create_alarm();
			}
			__test_alarm_manager_delete_alarm();
			__test_alarm_manager_get_information();
			__test_alarm_manager_get_alarm_ids();
			__test_alarm_manager_get_alarm_ids();
			__test_alarm_manager_callback();

		} else {
			printf("Invalid Option\n");
			return -1;
		}
	}
	printf("##################Alarm Test for %s Ends##################\n\n",
	       test_job);
	close(fd);

#else
	/*Compile with disabling AUTO_TEST flag for individual testing */
	int option = 0;

	g_type_init();

	printf("*********ALARM MANAGER TESTING************\n");
	printf("1. Test Alarm-Manager: Create Alarm\n");
	printf("2. Test Alarm-Manager: Delete Alarm\n");
	printf("3. Test Alarm-Manager: Get Alarm Information\n");
	printf("4. Test Alarm-Manager: Get Alarm ID\n");
	printf("5. Test Alarm-Manager: Callback\n");
	printf("6. Test Alarm-Manager: Get Alarm app-svc(bundle) Information");
	printf("Enter Choice between [1-5]....\n");

	option = __get_integer_input_data();
	switch (option) {
	case 1:
		{
			__test_alarm_manager_create_alarm();
			break;
		}
	case 2:
		{
			__test_alarm_manager_delete_alarm();
			break;
		}
	case 3:
		{
			__test_alarm_manager_get_information();
			break;
		}
	case 4:
		{
			__test_alarm_manager_get_alarm_ids();
			break;
		}
	case 5:
		{
			__test_alarm_manager_callback();
			break;
		}
	case 6:
		{
			__test_alarm_manager_get_appsvc_information();
			break;
		}
	default:
		{
			printf("Invalid Input!!! Retry again\n");
		}

	}
	printf("****************************************\n");
#endif
	return 0;
}
