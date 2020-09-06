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

#ifndef __RESOURCE_CAMERA_H__
#define __RESOURCE_CAMERA_H__

typedef struct __image_buffer_data_s {
    unsigned char *buffer;
	unsigned int buffer_size;
	unsigned int image_width;
	unsigned int image_height;
	camera_pixel_format_e format;
	void *user_data;
} image_buffer_data_s;

typedef void (*preview_image_buffer_created_cb)(void *buffedata);
typedef void (*capture_completed_cb)(const void *image, unsigned int size, void *user_data);

int resource_camera_init(preview_image_buffer_created_cb preview_image_buffer_created_cb, void *user_data);
int resource_camera_start_preview(void);
int resource_camera_stop_preview(void);
int resource_camera_capture(capture_completed_cb capture_completed_cb, void *data);
void resource_camera_close(void);

#endif
