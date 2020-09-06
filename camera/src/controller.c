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

#include <glib.h>
#include <Ecore.h>
#include <tizen.h>
#include <service_app.h>
#include <camera.h>
#include <mv_common.h>
#include <pthread.h>
#include "controller.h"
#include "controller_mv.h"
#include "controller_image.h"
#include "controller_telegram.h"
#include "log.h"
#include "resource_camera.h"

#define THRESHOLD_VALID_EVENT_COUNT 5
#define VALID_EVENT_INTERVAL_MS 200
#define TELEGRAM_EVENT_INTERVAL_MS 5000

#define IMAGE_FILE_PREFIX "CAM_"

//#define TEMP_IMAGE_FILENAME "/opt/usr/home/owner/apps_rw/org.tizen.smart-surveillance-camera/shared/data/tmp.jpg"
//#define LATEST_IMAGE_FILENAME "/opt/usr/home/owner/apps_rw/org.tizen.smart-surveillance-camera/shared/data/latest.jpg"

typedef struct app_data_s {
	long long int last_valid_event_time;
	int valid_event_count;

	unsigned int latest_image_width;
	unsigned int latest_image_height;
	char *latest_image_info;
	unsigned char *latest_image_buffer;
	unsigned char *latest_encoded_image_buffer;
	unsigned int latest_encoded_image_buffer_size;

	Ecore_Thread *image_writter_thread;
	pthread_mutex_t mutex;

	char* temp_image_filename;
	char* latest_image_filename;

	Ecore_Thread *telegram_thread;
	char* telegram_message;
	unsigned char* telegram_image_buffer;
	unsigned long long telegram_image_buffer_size;
} app_data;

static long long int __get_monotonic_ms(void)
{
	long long int ret_time = 0;
	struct timespec time_s;

	if (0 == clock_gettime(CLOCK_MONOTONIC, &time_s))
		ret_time = time_s.tv_sec* 1000 + time_s.tv_nsec / 1000000;
	else
		_E("Failed to get time");

	return ret_time;
}

static mv_colorspace_e __convert_colorspace_from_cam_to_mv(camera_pixel_format_e format)
{
	mv_colorspace_e colorspace = MEDIA_VISION_COLORSPACE_INVALID;
	switch (format) {
	case CAMERA_PIXEL_FORMAT_NV12:
		colorspace = MEDIA_VISION_COLORSPACE_NV12;
		break;
	case CAMERA_PIXEL_FORMAT_NV21:
		colorspace = MEDIA_VISION_COLORSPACE_NV21;
		break;
	case CAMERA_PIXEL_FORMAT_YUYV:
		colorspace = MEDIA_VISION_COLORSPACE_YUYV;
		break;
	case CAMERA_PIXEL_FORMAT_UYVY:
		colorspace = MEDIA_VISION_COLORSPACE_UYVY;
		break;
	case CAMERA_PIXEL_FORMAT_422P:
		colorspace = MEDIA_VISION_COLORSPACE_422P;
		break;
	case CAMERA_PIXEL_FORMAT_I420:
		colorspace = MEDIA_VISION_COLORSPACE_I420;
		break;
	case CAMERA_PIXEL_FORMAT_YV12:
		colorspace = MEDIA_VISION_COLORSPACE_YV12;
		break;
	case CAMERA_PIXEL_FORMAT_RGB565:
		colorspace = MEDIA_VISION_COLORSPACE_RGB565;
		break;
	case CAMERA_PIXEL_FORMAT_RGB888:
		colorspace = MEDIA_VISION_COLORSPACE_RGB888;
		break;
	case CAMERA_PIXEL_FORMAT_RGBA:
		colorspace = MEDIA_VISION_COLORSPACE_RGBA;
		break;
	case CAMERA_PIXEL_FORMAT_NV12T:
	case CAMERA_PIXEL_FORMAT_NV16:
	case CAMERA_PIXEL_FORMAT_ARGB:
	case CAMERA_PIXEL_FORMAT_JPEG:
	case CAMERA_PIXEL_FORMAT_H264:
	case CAMERA_PIXEL_FORMAT_INVALID:
	default :
		colorspace = MEDIA_VISION_COLORSPACE_INVALID;
		_E("unsupported format : %d", format);
		break;
	}

	return colorspace;
}

static void __terminate_telegram_thread(void *data)
{
	app_data *ad = (app_data *)data;
	ad->telegram_thread = NULL;
	_D("Telegram Thread Terminated!");

}
static void __thread_telegram_task(void *data, Ecore_Thread *th)
{
	app_data *ad = (app_data *)data;
	_D("Telegram Thread Start!");
	controller_telegram_send_message(ad->telegram_message);
	controller_telegram_send_image(ad->telegram_image_buffer, ad->telegram_image_buffer_size);
}

static void __thread_telegram_task_end_cb(void *data, Ecore_Thread *th)
{
	_D("Telegram Thread End!");
	ecore_main_loop_thread_safe_call_async(__terminate_telegram_thread, (app_data *)data);
}

static void __send_telegram_message(const char* msg, app_data *ad)
{
	if (!msg)
		return;

	static long long int last_event_time = 0;;
	long long int now = __get_monotonic_ms();

	if (now < last_event_time + TELEGRAM_EVENT_INTERVAL_MS) {
		return;
	}

	last_event_time = now;

	if (ad->telegram_message)
		free(ad->telegram_message);

	ad->telegram_message = strdup(msg);

	free(ad->telegram_image_buffer);

	pthread_mutex_lock(&ad->mutex);
	ad->telegram_image_buffer = ad->latest_encoded_image_buffer;
	ad->latest_encoded_image_buffer = NULL;
	ad->telegram_image_buffer_size = ad->latest_encoded_image_buffer_size;
	pthread_mutex_unlock(&ad->mutex);

	if (!ad->telegram_thread) {
		ad->telegram_thread = ecore_thread_run(__thread_telegram_task,
			__thread_telegram_task_end_cb,
			__thread_telegram_task_end_cb,
			ad);
	} else {
		_E("Telegram Thread is running NOW");
	}
}

static void __thread_write_image_file(void *data, Ecore_Thread *th)
{
	app_data *ad = (app_data *)data;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned char *buffer = NULL;
	unsigned char *encoded_buffer = NULL;
	unsigned long long encoded_size = 0;
	char *image_info = NULL;
	int ret = 0;

	pthread_mutex_lock(&ad->mutex);
	width = ad->latest_image_width;
	height = ad->latest_image_height;
	buffer = ad->latest_image_buffer;
	ad->latest_image_buffer = NULL;
	if (ad->latest_image_info) {
		image_info = ad->latest_image_info;
		ad->latest_image_info = NULL;
	} else {
		image_info = strdup("00");
	}
	pthread_mutex_unlock(&ad->mutex);

	ret = controller_image_save_image_file(ad->temp_image_filename, width, height, buffer,
		&encoded_buffer, &encoded_size, image_info, strlen(image_info));
	if (ret) {
		_E("failed to save image file");
	} else {
		ret = rename(ad->temp_image_filename, ad->latest_image_filename);
		if (ret != 0 )
			_E("Rename fail");
	}

	pthread_mutex_lock(&ad->mutex);
	unsigned char *temp = ad->latest_encoded_image_buffer;
	ad->latest_encoded_image_buffer = encoded_buffer;
	ad->latest_encoded_image_buffer_size = encoded_size;
	pthread_mutex_unlock(&ad->mutex);

	free(temp);
	free(image_info);
	free(buffer);
}

static void __thread_write_image_file_end_cb(void *data, Ecore_Thread *th)
{
	app_data *ad = (app_data *)data;

	pthread_mutex_lock(&ad->mutex);
	ad->image_writter_thread = NULL;
	pthread_mutex_unlock(&ad->mutex);
}

static void __thread_write_image_file_cancel_cb(void *data, Ecore_Thread *th)
{
	app_data *ad = (app_data *)data;
	unsigned char *buffer = NULL;

	_E("Thread %p got cancelled.\n", th);
	pthread_mutex_lock(&ad->mutex);
	buffer = ad->latest_image_buffer;
	ad->latest_image_buffer = NULL;
	ad->image_writter_thread = NULL;
	pthread_mutex_unlock(&ad->mutex);

	free(buffer);
}

static void __copy_image_buffer(image_buffer_data_s *image_buffer, app_data *ad)
{
	unsigned char *buffer = NULL;

	pthread_mutex_lock(&ad->mutex);
	ad->latest_image_height = image_buffer->image_height;
	ad->latest_image_width = image_buffer->image_width;

	buffer = ad->latest_image_buffer;
	ad->latest_image_buffer = image_buffer->buffer;
	pthread_mutex_unlock(&ad->mutex);

	free(buffer);
}

static void __preview_image_buffer_created_cb(void *data)
{
	image_buffer_data_s *image_buffer = data;
	app_data *ad = (app_data *)image_buffer->user_data;
	mv_source_h source = NULL;
	mv_colorspace_e image_colorspace = MEDIA_VISION_COLORSPACE_INVALID;
	char *info = NULL;

	ret_if(!image_buffer);
	ret_if(!ad);

	image_colorspace = __convert_colorspace_from_cam_to_mv(image_buffer->format);
	goto_if(image_colorspace == MEDIA_VISION_COLORSPACE_INVALID, FREE_ALL_BUFFER);

	__copy_image_buffer(image_buffer, ad);

	source = controller_mv_create_source(image_buffer->buffer,
		image_buffer->buffer_size,
		image_buffer->image_width,
		image_buffer->image_height,
		image_colorspace);

	pthread_mutex_lock(&ad->mutex);
	info = ad->latest_image_info;
	ad->latest_image_info = NULL;
	pthread_mutex_unlock(&ad->mutex);
	free(info);

	if (source)
		controller_mv_push_source(source);

	free(image_buffer);

	pthread_mutex_lock(&ad->mutex);
	if (!ad->image_writter_thread) {
		ad->image_writter_thread = ecore_thread_run(__thread_write_image_file,
			__thread_write_image_file_end_cb,
			__thread_write_image_file_cancel_cb,
			ad);
	} else {
		_E("Thread is running NOW");
	}
	pthread_mutex_unlock(&ad->mutex);

	return;

FREE_ALL_BUFFER:
	free(image_buffer->buffer);
	free(image_buffer);
}

static void __set_result_info(int result[], int result_count, app_data *ad, int image_result_type)
{
	char image_info[IMAGE_INFO_MAX + 1] = {'\0', };
	char *current_position;
	int current_index = 0;
	int string_count = 0;
	int i = 0;
	char *latest_image_info = NULL;
	char *info = NULL;

	current_position = image_info;

	current_position += snprintf(current_position, IMAGE_INFO_MAX, "%02d", image_result_type);
	string_count += 2;

	current_position += snprintf(current_position, IMAGE_INFO_MAX - string_count, "%02d", result_count);
	string_count += 2;

	for (i = 0; i < result_count; i++) {
		current_index = i * 4;
		if (IMAGE_INFO_MAX - string_count < 8)
			break;

		current_position += snprintf(current_position, IMAGE_INFO_MAX - string_count, "%02d%02d%02d%02d"
			, result[current_index], result[current_index + 1], result[current_index + 2], result[current_index + 3]);
		string_count += 8;
	}

	latest_image_info = strdup(image_info);

	pthread_mutex_lock(&ad->mutex);
	info = ad->latest_image_info;
	ad->latest_image_info = latest_image_info;
	pthread_mutex_unlock(&ad->mutex);
	free(info);
}

static void __mv_detection_event_cb(int area_sum, int result[], int result_count, void *user_data)
{
	app_data *ad = (app_data *)user_data;
	long long int now = __get_monotonic_ms();

	if (now < ad->last_valid_event_time + VALID_EVENT_INTERVAL_MS) {
		ad->valid_event_count++;
	} else {
		ad->valid_event_count = 1;
	}

	ad->last_valid_event_time = now;

	if (ad->valid_event_count < THRESHOLD_VALID_EVENT_COUNT) {
		__set_result_info(result, result_count, ad, 0);
		return;
	}

	int ratio = (double) area_sum * 100 / (double) IMAGE_RESOLUTION;
	_D("area_sum [%d], ratio [%d]", area_sum, ratio);

	char* msg = g_strdup_printf("Motion Detected! %d%% %d zones", ratio, result_count);
	__send_telegram_message(msg, ad);
	free(msg);

	ad->valid_event_count = 0;
	__set_result_info(result, result_count, ad, 1);
}

static void _start_camera(void)
{
	if (resource_camera_start_preview() == -1) {
		_E("Failed to start camera preview");
	}
}

static void _stop_camera(void)
{
	if (resource_camera_stop_preview() == -1) {
		_E("Failed to stop camera preview");
	}
}

static bool service_app_create(void *data)
{
	app_data *ad = (app_data *)data;

	char* shared_data_path = app_get_shared_data_path();
	if (shared_data_path == NULL) {
		_E("Failed to get shared data path");
		goto ERROR;
	}
	ad->temp_image_filename = g_strconcat(shared_data_path, "tmp.jpg", NULL);
	ad->latest_image_filename = g_strconcat(shared_data_path, "latest.jpg", NULL);
	free(shared_data_path);

	_D("%s", ad->temp_image_filename);
	_D("%s", ad->latest_image_filename);

	controller_image_initialize();

	pthread_mutex_init(&ad->mutex, NULL);

	if (controller_mv_set_movement_detection_event_cb(__mv_detection_event_cb, data) == -1) {
		_E("Failed to set movement detection event callback");
		goto ERROR;
	}

	if (resource_camera_init(__preview_image_buffer_created_cb, ad) == -1) {
		_E("Failed to init camera");
		goto ERROR;
	}

	if (resource_camera_start_preview() == -1) {
		_E("Failed to start camera preview");
		goto ERROR;
	}

	return true;

ERROR:
	resource_camera_close();
	controller_mv_unset_movement_detection_event_cb();
	controller_image_finalize();

	pthread_mutex_destroy(&ad->mutex);
	return false;
}

static void service_app_terminate(void *data)
{
	app_data *ad = (app_data *)data;
	Ecore_Thread *thread_id = NULL;
	unsigned char *buffer = NULL;
	unsigned char *encoded_image_buffer = NULL;
	char *info = NULL;
	gchar *temp_image_filename;
	gchar *latest_image_filename;
	_D("App Terminated - enter");

	resource_camera_close();
	controller_mv_unset_movement_detection_event_cb();

	pthread_mutex_lock(&ad->mutex);
	thread_id = ad->image_writter_thread;
	ad->image_writter_thread = NULL;
	pthread_mutex_unlock(&ad->mutex);

	if (thread_id)
		ecore_thread_wait(thread_id, 3.0); // wait for 3 second

	if(ad->telegram_thread)
		ecore_thread_wait(ad->telegram_thread, 3.0); // wait for 3 second

	ad->telegram_thread = NULL;

	free(ad->telegram_message);
	free(ad->telegram_image_buffer);

	controller_image_finalize();

	pthread_mutex_lock(&ad->mutex);
	buffer = ad->latest_image_buffer;
	ad->latest_image_buffer = NULL;
	encoded_image_buffer = ad->latest_encoded_image_buffer;
	ad->latest_encoded_image_buffer = NULL;
	info  = ad->latest_image_info;
	ad->latest_image_info = NULL;
	temp_image_filename = ad->temp_image_filename;
	ad->temp_image_filename = NULL;
	latest_image_filename = ad->latest_image_filename;
	ad->latest_image_filename = NULL;
	pthread_mutex_unlock(&ad->mutex);
	free(buffer);
	free(encoded_image_buffer);
	free(info);
	g_free(temp_image_filename);
	g_free(latest_image_filename);

	pthread_mutex_destroy(&ad->mutex);
	free(ad);
	_D("App Terminated - leave");
}

static void service_app_control(app_control_h app_control, void *data)
{
	/* APP_CONTROL */
	int ret = 0;
	char *command = NULL;

	_D("App control");
	ret = app_control_get_extra_data(app_control, "command", &command);
	if (ret != APP_CONTROL_ERROR_NONE) {
		_D("Failed to app_control_get_extra_data() From command key [0x%x]", ret);
	} else {
		_D("command = [%s]", command);
		if (!strncmp("send", command, sizeof("send"))) {
			__send_telegram_message("", data);
		} else if (!strncmp("on", command, sizeof("on"))) {
			_start_camera();
		} else if (!strncmp("off", command, sizeof("off"))) {
			_stop_camera();
		}
		free(command);
	}
}

int main(int argc, char* argv[])
{
	app_data *ad = NULL;
	int ret = 0;
	service_app_lifecycle_callback_s event_callback;

	ad = calloc(1, sizeof(app_data));
	retv_if(!ad, -1);

	event_callback.create = service_app_create;
	event_callback.terminate = service_app_terminate;
	event_callback.app_control = service_app_control;

	ret = service_app_main(argc, argv, &event_callback, ad);

	return ret;
}
