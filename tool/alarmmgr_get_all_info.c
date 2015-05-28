/*
 *  alarm-manager
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
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
 */

#include<dlog.h>
#include<stdio.h>
#include<stdlib.h>
#include<glib.h>

#include "alarm.h"

extern int alarmmgr_get_all_info(char **db_path);

int main()
{
	printf("=== Hi :) I will save info of all registered alarms in /tmp/alarmmgr_{datetime}.db\n");

	int ret = alarmmgr_init("alarmmgr_tool");
	if (ret != ALARMMGR_RESULT_SUCCESS)
	{
		LOGE("alarmmgr_init() is failed. ret = %d", ret);
		printf("=== Failed to get all alarms's info :(\n");
	}

	char *db_path = NULL;
	ret = alarmmgr_get_all_info(&db_path);
	if (ret != ALARMMGR_RESULT_SUCCESS)
	{
		LOGE("alarmmgr_get_all_info() is failed. ret = %d", ret);
		printf("=== Failed to get all alarms's info :(\n");
	}
	else
	{
		LOGE("Getting all alarm's info is done successfully.");
		printf("=== Success :)\n    Please check %s\n", db_path);
	}

	if (db_path)
	{
		free(db_path);
	}

	return 0;
}
