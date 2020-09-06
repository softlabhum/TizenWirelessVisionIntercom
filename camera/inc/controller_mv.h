/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __CONTROLLER_MV_H__
#define __CONTROLLER_MV_H__
#include <mv_common.h>

typedef void (*movement_detected_cb)(int area_sum, int result[], int result_count, void *user_data);

mv_source_h controller_mv_create_source(
		unsigned char *buffer, unsigned int size,
		unsigned int width, unsigned int height, mv_colorspace_e colorspace);
void controller_mv_push_source(mv_source_h source);
int controller_mv_set_movement_detection_event_cb(movement_detected_cb movement_detected_cb, void *user_data);
void controller_mv_unset_movement_detection_event_cb(void);

#endif
