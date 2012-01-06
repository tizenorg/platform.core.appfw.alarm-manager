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




#ifndef ALARM_LIB_H
#define ALARM_LIB_H

/**
 * @open
 * @addtogroup APPLICATION_FRAMEWORK
 * @{
 *
 * @defgroup Alarm Alarm
 * @version 0.4.2
 * 
 *
 * Alarm  supports APIs that add, delete, and update an alarm. 
 * @n An application can use alarm APIs by including @c alarm.h. The definitions
 * of APIs are defined as follows:
 *
 * @li @c #alarmmgr_init initialize alarm library
 * @li @c #alarmmgr_set_cb set the callback for an alarm event
 * @li @c #alarmmgr_create_alarm create an alarm entry
 * @li @c #alarmmgr_free_alarm free an alarm entry
 * @li @c #alarmmgr_set_time set a time will be expired
 * @li @c #alarmmgr_get_time get a time will be expired
 * @li @c #alarmmgr_set_repeat_mode set repeat mode
 * @li @c #alarmmgr_get_repeat_mode get repeat mode
 * @li @c #alarmmgr_set_type set type
 * @li @c #alarmmgr_get_type get type
 * @li @c #alarmmgr_add_alarm_with_localtime add an alarm with localtime
 * @li @c #alarmmgr_add_alarm add an alarm
 * @li @c #alarmmgr_remove_alarm remove an alarm from alarm server
 * @li @c #alarmmgr_enum_alarm_ids get the list of alarm ids
 * @li @c #alarmmgr_get_info get the information of an alarm
 * 
 *
 * The following code shows how to initialize alarm library, how to register the alarm handler, and how to add an alarm. It first calls alarm_init to initialize the alarm library and sets the callback to handle an alarm event it received. In create_test fucnction, the application add an alarm which will be expired in one minute from it execute and will expire everyday at same time. 
 *
 * 
 * @code
#include<stdio.h>
#include<stdlib.h>
#include<glib.h>

#include "alarm.h"

int callback(alarm_id_t alarm_id, void *user_param) 
{
	int error;
	time_t current_time;
	time(&current_time);
	
	printf("Alarm[%d] has expired at %s\n", alarm_id, ctime(&current_time));
	return 0;
}

void create_test()
{
	time_t current_time;
	struct tm current_tm;
	alarm_entry_t* alarm_info;
	alarm_id_t alarm_id;
	int result;
	alarm_date_t test_time;

	time(&current_time);

	printf("current time: %s\n", ctime(&current_time));
	localtime_r(&current_time, &current_tm);

	alarm_info = alarmmgr_create_alarm();

	test_time.year = current_tm.tm_year+1900;
	test_time.month = current_tm.tm_mon+1;
	test_time.day = current_tm.tm_mday;   
	test_time.hour = current_tm.tm_hour;
	test_time.min = current_tm.tm_min+1;
	test_time.sec = 0;

	alarmmgr_set_time(alarm_info,test_time);
	alarmmgr_set_repeat_mode(alarm_info,ALARM_REPEAT_MODE_WEEKLY,
	ALARM_WDAY_MONDAY| \
	ALARM_WDAY_TUESDAY|ALARM_WDAY_WEDNESDAY| \
	ALARM_WDAY_THURSDAY|ALARM_WDAY_FRIDAY );
	
	alarmmgr_set_type(alarm_info,ALARM_TYPE_VOLATILE);
	alarmmgr_add_alarm_with_localtime(alarm_info,NULL,&alarm_id);
	
	if(result != ALARMMGR_RESULT_SUCCESS)
		printf("fail to alarmmgr_create : error_code : %d\n",result);
       
}

int main(int argc, char** argv) 
{
	int error_code;
	GMainLoop *mainloop;
	int result;
	g_type_init();
	
	mainloop = g_main_loop_new(NULL, FALSE);
	result = alarmmgr_init("org.tizen.test");

	if(result != ALARMMGR_RESULT_SUCCESS) {
		printf("fail to alarmmgr_init : error_code : %d\n",result);
	} 
	else {
		result = alarmmgr_set_cb(callback,NULL);
		if(result != ALARMMGR_RESULT_SUCCESS) {
			printf("fail to alarmmgr_set_cb : error_code : 
							%d\n",result);
		}
		else {
			create_test();
		}
	}
	g_main_loop_run(mainloop);
}

* @endcode
*
*/

/**
 * @addtogroup Alarm
 * @{
 */

#include<sys/types.h>
#include<stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
* Type of an alarm id
*/
typedef int alarm_id_t;
/**
* The prototype of alarm handler.
* param [in] 	alarm_id the id of expired alarm
*/
typedef int (*alarm_cb_t) (alarm_id_t alarm_id, void *user_param);

typedef int (*alarm_enum_fn_t) (alarm_id_t alarm_id, void *user_param);

/**
* This enumeration has day of a week of alarm
*/
typedef enum {
	ALARM_WDAY_SUNDAY = 0x01, /**< enalbe alarm on Sunday*/
	ALARM_WDAY_MONDAY = 0x02, /**< enalbe alarm on Monday*/
	ALARM_WDAY_TUESDAY = 0x04, /**< enable alarm on Tuesday*/
	ALARM_WDAY_WEDNESDAY = 0x08, /**< enalbe alarm on Wednesday*/
	ALARM_WDAY_THURSDAY = 0x10, /**< enable alarm on Thursday*/
	ALARM_WDAY_FRIDAY = 0x20,  /**< enable alarm on Friday*/
	ALARM_WDAY_SATURDAY = 0x40,/**< enable alarm on Saturday*/
} alarm_day_of_week_t;

/**
* This enumeration has error codes of alarm
*/
	typedef enum {
		ERR_ALARM_INVALID_PARAM = -10,
				     /**<Invalid parameter*/
		ERR_ALARM_INVALID_ID,	/**<Invalid id*/
		ERR_ALARM_INVALID_REPEAT,
					/**<Invalid repeat mode*/
		ERR_ALARM_INVALID_TIME,	/**<Invalid time. */
		ERR_ALARM_INVALID_DATE,	/**<Invalid date. */
		ERR_ALARM_NO_SERVICE_NAME,
				    /**<there is no alarm service 
					for this applicaation. */
		ERR_ALARM_SYSTEM_FAIL = -1,
		ALARMMGR_RESULT_SUCCESS = 0,
	} alarm_error_t;

/**
*  This enumeration has repeat mode of alarm
*/
typedef enum {
	ALARM_REPEAT_MODE_ONCE = 0,	/**<once : the alarm will be expired 
					only one time. */
	ALARM_REPEAT_MODE_REPEAT,	/**<repeat : the alarm will be expired 
					repeatly*/
	ALARM_REPEAT_MODE_WEEKLY,	/**<weekly*/
	ALARM_REPEAT_MODE_MONTHLY,	/**< monthly*/
	ALARM_REPEAT_MODE_ANNUALLY,	/**< annually*/
	ALARM_REPEAT_MODE_MAX,
} alarm_repeat_mode_t;


#define ALARM_TYPE_DEFAULT	0x0	/*< non volatile */
#define ALARM_TYPE_VOLATILE	0x02	/*< volatile */


/**
* This struct has date information
*/
typedef struct {
	int year;		/**< specifies the year */
	int month;		/**< specifies the month */
	int day;		/**< specifies the day */
	int hour;		/**< specifies the hour */
	int min;		/**< specifies the minute*/
	int sec;		/**< specifies the second*/
} alarm_date_t;


typedef struct alarm_info_t alarm_entry_t;


/**
 *
 * This function initializes alarm library. It connects to system bus and registers the application's service name. 
 *
 * @param	[in]	pkg_name	a package of application
 *
 * @return On success, ALARMMGR_RESULT_SUCCESS is returned. On error, a negative number is returned 
 *
 * @pre None.
 * @post None.
 * @see None.
 * @remark An application must call this function before using other alarm APIs. 
 * @par Sample code:
 * @code
#include <alarm.h>

...
{
	int ret_val = ALARMMGR_RESULT_SUCCESS;
	const char* pkg_name = "org.tizen.test";

	g_type_init();

	ret_val =alarmmgr_init(pkg_name) ;
	if(ret_val == ALARMMGR_RESULT_SUCCESS)
	{
		 //alarmmgr_init() is successful
	}
	else
	{
		 //alarmmgr_init () failed
	}
}
 * @endcode
 * @limo
 */
int alarmmgr_init(const char *pkg_name);

void alarmmgr_fini();


/**
 * This function registers handler which handles an alarm event. An application should register the alarm handler
 * before it enters mainloop.
 *
 * @param	[in]	handler	Callback function
 * @param	[in]	user_param	User Parameter
 *
 * @return	This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre alarmmgr_init().
 * @post None.
 * @see None.
 * @remark	An application can have only one alarm handler. If an application 
 *          calls this function more than one times, the handler regitered during  the
 *          last call of this funiction will be called when an alarm event has occured. 
 * @par Sample code:
 * @code
#include <alarm.h>
...

// Call back function 
int callback(alarm_id_t alarm_id,void* user_param)
{
        time_t current_time;
        time(&current_time);

        printf("Alarm[%d] has expired at %s\n", alarm_id, ctime(&current_time));

        return 0;

}

...
{
	int ret_val = ALARMMGR_RESULT_SUCCESS;
	void *user_param = NULL;

	ret_val = alarmmgr_set_cb( callback, user_param);
	if(ret_val == ALARMMGR_RESULT_SUCCESS)
	{
		//alarmmgr_set_cb() is successful
	}
	else
	{
		 //alarmmgr_set_cb () failed
	}
}

 * @endcode
 * @limo
 */
int alarmmgr_set_cb(alarm_cb_t handler, void *user_param);


/**
 * This function creates a new alarm entry, will not be known to the server until alarmmgr_add_alarm is called.
 *
 * @return	This function returns the pointer of alarm_entry_t 
 *
 * @pre None.
 * @post None.
 * @see None.
 * @remark	After an application use this object, an application free this pointer through alarmmgr_free_alarm
 *
 * @par Sample code:
 * @code
#include <alarm.h>

...
{
	alarm_entry_t* alarm;

	alarm = alarmmgr_create_alarm() ;
	if(alarm != NULL)
	{
		 //alarmmgr_create_alarm() is successful
	}
	else
	{
		 //alarmmgr_create_alarm () failed 
	}
}


 * @endcode
 * @limo
 */
alarm_entry_t *alarmmgr_create_alarm(void);


/**
 * This function frees an alarm entry. 
 *
 * @param	[in]	alarm	alarm entry
 *
 * @return	This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see None.
 * @remark	None.
 *
 * @par Sample code:
 * @code
#include <alarm.h>
 
...
 {
	 int ret_val = ALARMMGR_RESULT_SUCCESS;
	 alarm_entry_t* alarm;
 
	 alarm = alarmmgr_create_alarm() ;
	 if(alarm == NULL)
	 {
		  //alarmmgr_create_alarm () failed 
	 }
	 else
		 {
 
			 ret_val = alarmmgr_free_alarm( alarm) ;
			 if(ret_val == ALARMMGR_RESULT_SUCCESS)
			 {
				  //alarmmgr_free_alarm() is successful
			 }
			 else
			 {
				 //alarmmgr_free_alarm() failed
			 }			 
		 }
 } 
 
 * @endcode
 * @limo
 */
int alarmmgr_free_alarm(alarm_entry_t *alarm);


/**
 * This function sets time that alarm should be expried.
 *
 * @param	[in]	alarm	alarm entry
 * @param	[in]	time		time the alarm should first go off
 *
 * @return	This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see None.
 * @remark  None.
 *
 * @par Sample code:
 * @code
#include <alarm.h>
  
 ...
  {
	  int ret_val = ALARMMGR_RESULT_SUCCESS;
	  alarm_entry_t* alarm;
	  time_t current_time;
	  struct tm current_tm;
	  alarm_date_t test_time;
  
  
	 time(&current_time);
	 localtime_r(&current_time, &current_tm);
		 
	 alarm = alarmmgr_create_alarm();
	  if(alarm == NULL)
	  {
		   //alarmmgr_create_alarm () failed 
	  }
	  else {
		test_time.year = current_tm.tm_year;
		test_time.month = current_tm.tm_mon;
		test_time.day = current_tm.tm_mday;

		test_time.hour = current_tm.tm_hour;
		test_time.min = current_tm.tm_min+1;
		test_time.sec = 0;
		
		ret_val=alarmmgr_set_time(alarm,test_time);
		if(ret_val == ALARMMGR_RESULT_SUCCESS)
		{
			//alarmmgr_set_time() is successful
		}
		else
		{
			//alarmmgr_set_time() failed
		}
			  alarmmgr_free_alarm( alarm) ;
	  }
 }
  
 * @endcode
 * @limo
 */
int alarmmgr_set_time(alarm_entry_t *alarm, alarm_date_t time);

/**
 * This function gives an application time that alarm should be expried.
 *
 * @param	[in]		alarm	alarm entry
 * @param	[out]	time		time the alarm should first go off
 *
 * @return	This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see None.
 * @remark	But an application does not need to specify year, month, and day field of alarm_info. If an application sets 
 * 			those fields with zero, the function sets them with proper values.
 *
 * @par Sample code:
 * @code
#include <alarm.h>
   
 ...
 {
	 int ret_val = ALARMMGR_RESULT_SUCCESS;
	 alarm_entry_t* alarm;
 
	 time_t current_time;
		struct tm current_tm;
	 alarm_date_t test_time;
	 alarm_date_t new_time;
 
 
		time(&current_time);
		localtime_r(&current_time, &current_tm);
		
		alarm = alarmmgr_create_alarm();
	 if(alarm == NULL) {
		  //alarmmgr_create_alarm () failed 
	 }
	 else {
		test_time.year = current_tm.tm_year;
		test_time.month = current_tm.tm_mon;
		test_time.day = current_tm.tm_mday;

		test_time.hour = current_tm.tm_hour;
		test_time.min = current_tm.tm_min+1;
		test_time.sec = 0;

		ret_val = alarmmgr_set_time(alarm,test_time);
		if(ret_val == ALARMMGR_RESULT_SUCCESS) {
			//alarmmgr_set_time() is successful
		}
		else {
			//alarmmgr_set_time() failed
		}

		ret_val = alarmmgr_get_time(alarm, &new_time);
		if(ret_val == ALARMMGR_RESULT_SUCCESS) {
			//alarmmgr_get_time() is successful
		}
		else {
			//alarmmgr_get_time() failed
		}
		alarmmgr_free_alarm( alarm) ;
	}
 }

 * @endcode
 * @limo
 */
int alarmmgr_get_time(const alarm_entry_t *alarm, alarm_date_t *time);

/**
 * This function sets an alarm repeat mode 
 *
 * @param	[in]	alarm	alarm entry
 * @param	[in]	repeat_mode	one of ALARM_REPEAT_MODE_ONCE, ALARM_REPEAT_MODE_REPEAT, 
 *								ALARM_REPEAT_MODE_WEEKLY, ALARM_REPEAT_MODE_MONTHLY or ALARM_REPEAT_MODE_ANNUALLY.
 * @param	[in]	repeat_value	the ALARM_REPEAT_MODE_REPEAT mode : interval between subsequent repeats of the alarm.
 *								the ALARM_REPEAT_MODE_WEEKLY mode : days of a week
 *								(ALARM_WDAY_SUNDAY, ALARM_WDAY_MONDAY, ALARM_WDAY_TUESDAY, 	ALARM_WDAY_WEDNESDAY, 
 *								ALARM_WDAY_THURSDAY, 	ALARM_WDAY_FRIDAY, ALARM_WDAY_SATURDAY)
 *								the others : this parameter is ignored.
 *
 * @return	This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see None.
 * @remark  None.
 *
 * @par Sample code:
 * @code
#include <alarm.h>
	
  ...
 {
	 int ret_val = ALARMMGR_RESULT_SUCCESS;
	 alarm_entry_t* alarm;	 
	 alarm_repeat_mode_t repeat_mode =ALARM_REPEAT_MODE_WEEKLY;  
	 int interval = ALARM_WDAY_MONDAY; //| ALARM_WDAY_TUESDAY|
		ALARM_WDAY_WEDNESDAY| ALARM_WDAY_THURSDAY|ALARM_WDAY_FRIDAY ;
 
 
	 alarm = alarmmgr_create_alarm();
	 if(alarm == NULL)
	 {
		  //alarmmgr_create_alarm () failed 
	 }
	 else
		 {
			   ret_val = alarmmgr_set_repeat_mode
					(alarm, repeat_mode,interval);
 
			 if(ret_val == ALARMMGR_RESULT_SUCCESS)
			 {
				  //alarmmgr_set_repeat_mode() is successful	
			 }
			 else
			 {
				 //alarmmgr_set_repeat_mode() failed
			 }
			 alarmmgr_free_alarm( alarm) ;
		 }
 }
 
 * @endcode
 * @limo
 */
int alarmmgr_set_repeat_mode(alarm_entry_t *alarm,
				     alarm_repeat_mode_t repeat_mode,
				     int repeat_value);

/**
 * This function gives an application an alarm mode 
 *
 * @param	[in]		alarm	alarm entry
 * @param	[out]	repeat_mode	one of ALARM_REPEAT_MODE_ONCE, ALARM_REPEAT_MODE_REPEAT, 
 *									ALARM_REPEAT_MODE_WEEKLY, ALARM_REPEAT_MODE_MONTHLY or ALARM_REPEAT_MODE_ANNUALLY.
 * @param	[out]	repeat_value	the ALARM_REPEAT_MODE_REPEAT mode : interval between subsequent repeats of the alarm.
 *									the ALARM_REPEAT_MODE_WEEKLY mode : days of a week
 *									(ALARM_WDAY_SUNDAY, ALARM_WDAY_MONDAY, ALARM_WDAY_TUESDAY, 	ALARM_WDAY_WEDNESDAY, 
 *									ALARM_WDAY_THURSDAY, 	ALARM_WDAY_FRIDAY, ALARM_WDAY_SATURDAY)
 *									the others : this parameter is ignored.
 *
 * @return	This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see None.
 * @remark  None.
 *
 * @par Sample code:
 * @code
#include <alarm.h>
	 
   ...
 {
	 int ret_val = ALARMMGR_RESULT_SUCCESS;
	 alarm_entry_t* alarm;
	 alarm_repeat_mode_t repeat;
	 int interval;
 
	 alarm = alarmmgr_create_alarm();
	 if(alarm == NULL)
	 {
		 //alarmmgr_create_alarm () failed 
	 }
	 else {
		ret_val =alarmmgr_get_repeat_mode
					(alarm, &repeat, &interval) ;
			 if(ret_val == ALARMMGR_RESULT_SUCCESS 
			&& repeat == ALARM_REPEAT_MODE_ONCE) {
				//alarmmgr_get_repeat_mode() is successful
			}
			else {
				//alarmmgr_get_repeat_mode() failed
			}
			alarmmgr_free_alarm(alarm) ;
	}
 }

 * @endcode
 * @limo
 */
int alarmmgr_get_repeat_mode(const alarm_entry_t *alarm,
				     alarm_repeat_mode_t *repeat_mode,
				     int *repeat_value);

/**
 * This function sets an alarm mode 
 *
 * @param	[in]	alarm	alarm entry
 * @param	[in]	alarm_type	one of ALARM_TYPE_DEFAULT : After the device reboot, the alarm still works.
 * 							ALARM_TYPE_VOLATILE : After the device reboot, the alarm does not work.
 *
 * @return	This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see None.
 * @remark  None.
 *
 * @par Sample code:
 * @code
#include <alarm.h>

   ...	
 {
	 int ret_val = ALARMMGR_RESULT_SUCCESS;
	 alarm_entry_t* alarm;
	 int alarm_type = ALARM_TYPE_VOLATILE;
  
	 alarm = alarmmgr_create_alarm();
	 if(alarm == NULL)
	 {
		  //alarmmgr_create_alarm () failed 
	 }
	 else
		 {		 
			 ret_val = alarmmgr_set_type(alarm,  alarm_type);
			 if(ret_val == ALARMMGR_RESULT_SUCCESS)
			 {
				  //alarmmgr_set_type() is successful	
			 }
			 else
			 {
				  //alarmmgr_set_type() failed
			 }
			 alarmmgr_free_alarm( alarm) ;
		 }
 }
  
 * @endcode
 * @limo
 */
int alarmmgr_set_type(alarm_entry_t *alarm, int alarm_type);

/**
 * This function gives an application an alarm mode 
 *
 * @param	[in]		alarm	alarm entry
 * @param	[out]	alarm_type	one of ALARM_TYPE_DEFAULT, ALARM_TYPE_VOLATILE
 *
 * @return	This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see None.
 * @remark  None.
 *
 * @par Sample code:
 * @code
#include <alarm.h>

   ...		
 {
	int ret_val = ALARMMGR_RESULT_SUCCESS;
	alarm_entry_t* alarm; 
	int alarm_type;
 
	alarm = alarmmgr_create_alarm();
	if(alarm == NULL) {
		//alarmmgr_create_alarm () failed
	}
	else {
		ret_val = alarmmgr_get_type(  alarm, &alarm_type);
		if(ret_val == ALARMMGR_RESULT_SUCCESS && alarm_type 
						== ALARM_TYPE_DEFAULT ) {
			//alarmmgr_get_type() is successful
		}
		else {
			//alarmmgr_get_type() failed
		}
		alarmmgr_free_alarm( alarm) ;
	}
 }
   
 * @endcode
 * @limo
 */
int alarmmgr_get_type(const alarm_entry_t *alarm, int *alarm_type);

/**
 * This function adds an alarm entry to the server.	
 * Server will remember this entry, and generate alarm events for it when necessary.
 * Server will call app-svc interface to sent notification to destination application. Destination information
 * should be available in the input bundle.
 * Alarm entries registered with the server cannot be changed.  
 * Remove from server before changing.
 * Before the application calls alarmmgr_add_alarm_appsvc_with_localtime(), the application have to call alarmmgr_set_time().
 * The time set is localtime.
 * If the application does not call alarmmgr_set_repeat_mode, the default repeat_mode is ALARM_REPEAT_MODE_ONCE.
 * If the application does not call alarmmgr_set_type, the default alarm_type is ALARM_TYPE_DEFAULT.
 * The id of the new alarm will be copied to  alarm_id if the fuction successes to create an alarm.
 *
 * @param	[in]		alarm		the entry of an alarm to be created.
 * @param	[in]		bundle_data	bundle which contains information about the destination.
 * @param	[out] 		alarm_id	the id of the alarm added. 
 *
 * @return	This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see alarmmgr_add_alarm
 * @remark  None.
 *
 * @par Sample code:
 * @code
#include <alarm.h>

   ...	  
{
    time_t current_time;
    struct tm current_tm;
    alarm_entry_t* alarm_info;
    alarm_id_t alarm_id;
    int result;
    alarm_date_t test_time;


	bundle *b=NULL;

	b=bundle_create();

	if (NULL == b)
	{
		printf("Unable to create bundle!!!\n");
		return;
	}

	appsvc_set_operation(b,APPSVC_OPERATION_DEFAULT);
	appsvc_set_pkgname(b,"org.tizen.alarm-test");

    time(&current_time);

    printf("current time: %s\n", ctime(&current_time));
    localtime_r(&current_time, &current_tm);
    
    alarm_info = alarmmgr_create_alarm();
    
    test_time.year = current_tm.tm_year;                 
			test_time.month = current_tm.tm_mon;                
			test_time.day = current_tm.tm_mday;                  
                                             
			test_time.hour = current_tm.tm_hour;
			test_time.min = current_tm.tm_min+1;
			test_time.sec = 5;

        
    alarmmgr_set_time(alarm_info,test_time);
    alarmmgr_set_repeat_mode(alarm_info,ALARM_REPEAT_MODE_WEEKLY,ALARM_WDAY_MONDAY| \
		ALARM_WDAY_TUESDAY|ALARM_WDAY_WEDNESDAY| \
		ALARM_WDAY_THURSDAY|ALARM_WDAY_FRIDAY );

    alarmmgr_set_type(alarm_info, ALARM_TYPE_DEFAULT);
    //alarmmgr_set_type(alarm_info,ALARM_TYPE_VOLATILE);
    if ((result = alarmmgr_add_alarm_appsvc_with_localtime(alarm_info,(void *)b,&alarm_id)) < 0)
	{
		printf("Alarm creation failed!!! Alrmgr error code is %d\n",result);
	}
	else
	{
		printf("Alarm created succesfully with alarm id %d\n",alarm_id);
	}

}
 * @endcode
 * @limo
 */
int alarmmgr_add_alarm_appsvc_with_localtime(alarm_entry_t *alarm,void *bundle_data, alarm_id_t *alarm_id);

/**
 * This function adds an alarm entry to the server.	
 * Server will remember this entry, and generate alarm events for it when necessary.
 * Alarm entries registered with the server cannot be changed.  
 * Remove from server before changing.
 * Before the application calls alarmmgr_add_alarm_with_localtime(), the application have to call alarmmgr_set_time().
 * The time set is localtime.
 * If the application does not call alarmmgr_set_repeat_mode, the default repeat_mode is ALARM_REPEAT_MODE_ONCE.
 * If the application does not call alarmmgr_set_type, the default alarm_type is ALARM_TYPE_DEFAULT.
 * The id of the new alarm will be copied to  alarm_id if the fuction successes to create an alarm.
 *
 * @param	[in]		alarm		the entry of an alarm to be created.
 * @param	[in]		destination	the packname of application that the alarm will be expired.
 * @param	[out] 	alarm_id		the id of the alarm added. 
 *
 * @return	This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see alarmmgr_add_alarm
 * @remark  None.
 *
 * @par Sample code:
 * @code
#include <alarm.h>

   ...	  
{
	int ret_val = ALARMMGR_RESULT_SUCCESS;
	alarm_entry_t* alarm; 
	const char* destination = NULL; 
	alarm_id_t alarm_id;
 
	time_t current_time;
	struct tm current_tm;
	alarm_date_t test_time;
 
	const char* pkg_name = "org.tizen.test";
 
	g_type_init();
 
	ret_val =alarmmgr_init(pkg_name) ;
	if(ret_val != ALARMMGR_RESULT_SUCCESS) {
		//alarmmgr_init () failed
		return;
	}
 
	time(&current_time);
 
	printf("current time: %s\n", ctime(&current_time));
	localtime_r(&current_time, &current_tm);
 
	alarm = alarmmgr_create_alarm();
	
	test_time.year = 0;
	test_time.month = 0;test_time.day = 0;
	
	test_time.hour = current_tm.tm_hour;
	test_time.min = current_tm.tm_min+1;
	test_time.sec = 0;
 
		 
	 alarmmgr_set_time(alarm,test_time);
	 alarmmgr_set_repeat_mode(alarm,ALARM_REPEAT_MODE_WEEKLY, \
					ALARM_WDAY_MONDAY);
	 alarmmgr_set_type(alarm,ALARM_TYPE_VOLATILE);
 
		 
	ret_val=alarmmgr_add_alarm_with_localtime(alarm,destination,&alarm_id);
 
	 if(ret_val == ALARMMGR_RESULT_SUCCESS)
	 {
		  //alarmmgr_add_alarm_with_localtime() is successful	
	 }
	 else
	 {
		  //alarmmgr_add_alarm_with_localtime() failed
	 }
	 alarmmgr_free_alarm( alarm) ;
 }
 * @endcode
 * @limo
 */
int alarmmgr_add_alarm_with_localtime(alarm_entry_t *alarm,
					      const char *destination,
					      alarm_id_t *alarm_id);


/**
 * This function adds an alarm entry to the server.	
 * Server will remember this entry, and generate alarm events for it when necessary.
 * Server will call app-svc interface to sent notification to destination application. Destination information
 * should be available in the input bundle.
 * Alarm entries registered with the server cannot be changed.  
 * Remove from server before changing.
 * After the trigger_at_time seconds from now, the alarm will be expired.
 * If the interval is zero, the repeat_mode is ALARM_REPEAT_MODE_ONCE.
 * If the interval is >0, the repeat_mode is ALARM_REPEAT_MODE_REPEAT.
 * The id of the new alarm will be copied to  alarm_id if the fuction successes to create an alarm.
 *
 * @param	[in]		alarm_type		one of ALARM_TYPE_DEFAULT, ALARM_TYPE_VOLATILE
 * @param	[in]		trigger_at_time	time interval to be triggered from now(sec). an alarm also will be expired at triggering time.	
 * @param	[in]		interval		Interval between subsequent repeats of the alarm	
 * @param	[in]		bundle_data		bundle which contains information about the destination.
 * @param	[out] 	alarm_id			the id of the alarm added. 
 *
 * @return	This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see alarmmgr_add_alarm_with_localtime alarmmgr_remove_alarm
 * @remark  None.
 *
 * @par Sample code:
 * @code
#include <alarm.h>

 ...	  
 {
 		int result;
		alarm_id_t alarm_id;

		bundle *b=NULL;

		b=bundle_create();

		if (NULL == b)
		{
			printf("Unable to create bundle!!!\n");
			return;
		}

		appsvc_set_pkgname(b,"org.tizen.alarm-test");
		//appsvc_set_operation(b,APPSVC_OPERATION_SEND_TEXT);
		appsvc_set_operation(b,APPSVC_OPERATION_DEFAULT);

		if ((result = alarmmgr_add_alarm_appsvc(ALARM_TYPE_DEFAULT, 10, 0, (void *)b ,&alarm_id)))
			printf("Unable to add alarm. Alarmmgr alarm no is %d\n", result);
		else
			printf("Alarm added successfully. Alarm Id is %d\n", alarm_id);
		return;	

 }

 * @endcode
 * @limo
 */
int alarmmgr_add_alarm_appsvc(int alarm_type, time_t trigger_at_time,
			       time_t interval, void *bundle_data,
			       alarm_id_t *alarm_id);


/**
 * This function adds an alarm entry to the server.	
 * Server will remember this entry, and generate alarm events for it when necessary.
 * Alarm entries registered with the server cannot be changed.  
 * Remove from server before changing.
 * After the trigger_at_time seconds from now, the alarm will be expired.
 * If the interval is zero, the repeat_mode is ALARM_REPEAT_MODE_ONCE.
 * If the interval is >0, the repeat_mode is ALARM_REPEAT_MODE_REPEAT.
 * The id of the new alarm will be copied to  alarm_id if the fuction successes to create an alarm.
 *
 * @param	[in]		alarm_type		one of ALARM_TYPE_DEFAULT, ALARM_TYPE_VOLATILE
 * @param	[in]		trigger_at_time	time interval to be triggered from now(sec). an alarm also will be expired at triggering time.	
 * @param	[in]		interval			Interval between subsequent repeats of the alarm	
 * @param	[in]		destination		the packname of application that the alarm will be expired.
 * @param	[out] 	alarm_id			the id of the alarm added. 
 *
 * @return	This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see alarmmgr_add_alarm_with_localtime alarmmgr_remove_alarm
 * @remark  None.
 *
 * @par Sample code:
 * @code
#include <alarm.h>

 ...	  
 {
	 int ret_val = ALARMMGR_RESULT_SUCCESS;
	 
	 int alarm_type = ALARM_TYPE_VOLATILE;
	 time_t trigger_at_time = 10; 
	 time_t interval = 10; 
	 const char* destination = NULL;
	 alarm_id_t alarm_id;
 
	 const char* pkg_name = "org.tizen.test";
 
	 g_type_init();
 
	 ret_val =alarmmgr_init(pkg_name) ;
	 if(ret_val != ALARMMGR_RESULT_SUCCESS)
	 {
		  //alarmmgr_init () failed
		  return;
	 }
 
	 ret_val = alarmmgr_add_alarm( alarm_type, trigger_at_time, interval, 
					destination, &alarm_id);
	 if(ret_val == ALARMMGR_RESULT_SUCCESS)
	 {
		  //alarmmgr_add_alarm() is successful	
	 }
	 else
	 {
		   //alarmmgr_add_alarm() failed
	 }
	 alarmmgr_remove_alarm( alarm_id) ;
 }

 * @endcode
 * @limo
 */
int alarmmgr_add_alarm(int alarm_type, time_t trigger_at_time,
			       time_t interval, const char *destination,
			       alarm_id_t *alarm_id);

/**
 * This function deletes the alarm associated with the given alarm_id.
 *
 * @param	[in]	alarm_id	Specifies the ID of the alarm to be deleted. 
 *
 * @return	This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see alarmmgr_add_alarm_with_localtime alarmmgr_add_alarm
 * @remark  None.
 *
 * @par Sample code:
 * @code
#include <alarm.h>

 ...	 
 {
	int ret_val = ALARMMGR_RESULT_SUCCESS;
	int alarm_type = ALARM_TYPE_VOLATILE;
	time_t trigger_at_time = 10; 
	time_t interval = 10; 
	const char* destination = NULL;
	alarm_id_t alarm_id;
 
	const char* pkg_name = "org.tizen.test";
 
	g_type_init();
 
	ret_val =alarmmgr_init(pkg_name) ;
	if(ret_val != ALARMMGR_RESULT_SUCCESS) {
		//alarmmgr_init () failed
		return;
	}

	alarmmgr_add_alarm( alarm_type,  trigger_at_time, interval, 
						destination, &alarm_id);
 
	ret_val =alarmmgr_remove_alarm( alarm_id) ;
	if(ret_val == ALARMMGR_RESULT_SUCCESS) {
		/alarmmgr_remove_alarm() is successful
	}
	else {
		//alarmmgr_remove_alarm() failed
	}
 }
 
 * @endcode
 * @limo
 */
int alarmmgr_remove_alarm(alarm_id_t alarm_id);

/**
 * This function gives a list of alarm ids that the application adds to the server.
 *
 * @param	[in]	fn			a user callback function
 * @param	[in]	user_param	user parameter
 *
 * @return			This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see alarm_get_info
 * @remark  None.
 *
 * @par Sample code:
 * @code
#include <alarm.h>
 
 int callback_2(alarm_id_t id, void* user_param)
 {
	 int* n = (int*)user_param;
	 printf("[%d]alarm id : %d\n",*n,id);
	 (*n)++;
	 return 0;
 }

... 
 {
	 int ret_val = ALARMMGR_RESULT_SUCCESS;
	 int n = 1;
 
	 const char* pkg_name = "org.tizen.test";
 
	 g_type_init();
 
	 ret_val =alarmmgr_init(pkg_name) ;
	 if(ret_val != ALARMMGR_RESULT_SUCCESS)
	 {
		  //alarmmgr_init() failed
		  return;
	 }
 
	 ret_val = alarmmgr_enum_alarm_ids( callback_2, (void*)&n) ;
	 if(ret_val == ALARMMGR_RESULT_SUCCESS)
	 {
		   //alarmmgr_enum_alarm_ids() is successful	
	 }
	 else
	 {
		 //alarmmgr_enum_alarm_ids() failed
	 }
 }

 * @endcode
 * @limo
 */
int alarmmgr_enum_alarm_ids(alarm_enum_fn_t fn, void *user_param);


/**
 * This function gets the information of the alarm assosiated with alarm_id to alarm_info. The application
 * must allocate alarm_info before calling this function.  
 *
 * @param	[in] 	alarm_id		the id of the alarm
 * @param	[out] 	alarm	the buffer alarm informaiton will be copied to
 * 
 * @return			This function returns ALARMMGR_RESULT_SUCCESS on success or a negative number on failure. 
 *
 * @pre None.
 * @post None.
 * @see alarmmgr_enum_alarm_ids
 * @remark  None.
 *
 * @par Sample code:
 * @code
#include <alarm.h>

...
 {
	int ret_val = ALARMMGR_RESULT_SUCCESS;
	int alarm_type = ALARM_TYPE_VOLATILE;
	time_t trigger_at_time = 10; 
	time_t interval = ALARM_WDAY_SUNDAY; 
	const char* destination = NULL;
	alarm_id_t alarm_id;

	alarm_entry_t *alarm;

	const char* pkg_name = "org.tizen.test_get_info1";
	 
	g_type_init();
 
	ret_val =alarmmgr_init(pkg_name) ;
	if(ret_val != ALARMMGR_RESULT_SUCCESS) {
		//alarmmgr_init() failed
		return;
	}	 
	ret_val = alarmmgr_add_alarm( alarm_type,trigger_at_time,interval,
			destination, &alarm_id);

	if(ret_val != ALARMMGR_RESULT_SUCCESS) {
		//alarmmgr_add_alarm() failed
		return;
	} 
	ret_val = alarmmgr_get_info(alarm_id, alarm);
	if(ret_val == ALARMMGR_RESULT_SUCCESS) {
		//alarmmgr_get_info() is successful
	}
	else {
		//alarmmgr_get_info() failed
	}
	alarmmgr_remove_alarm( alarm_id) ;
 }
 * @endcode
 * @limo
 */
int alarmmgr_get_info(alarm_id_t alarm_id, alarm_entry_t *alarm);

/**
 * @}
 */


/**
 * @}
 */


int alarmmgr_power_on(bool on_off);

#ifdef __cplusplus
}
#endif


#endif/* ALARM_LIB_H*/
