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
#include <glib.h>
#include <glib-object.h>
#include <dlog/dlog.h>
#include <alarm.h>
#include <bundle.h>
#include <appsvc.h>


void create_test()
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

int main(int argc, char** argv)
{       
        GMainLoop *mainloop;

	g_type_init();

	mainloop = g_main_loop_new(NULL, FALSE);

	create_test();

	g_main_loop_run(mainloop);
		
	return 0;
}


