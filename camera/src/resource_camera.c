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

#include <stdlib.h>
#include <Ecore.h>
#include <camera.h>

#include "log.h"
#include "controller.h"
#include "resource_camera.h"

struct __camera_data {
	camera_h cam_handle;

	void *captured_file;
	unsigned int image_size;

	preview_image_buffer_created_cb preview_image_buffer_created_cb;
	void *preview_image_buffer_created_cb_data;

	capture_completed_cb capture_completed_cb;
	void *capture_completed_cb_data;

	bool is_af_enabled;
};

static struct __camera_data *g_camera_data = NULL;

static const char * __cam_err_to_str(camera_error_e err)
{
	const char *err_str;

	switch (err) {
	case CAMERA_ERROR_NONE:
		err_str = "CAMERA_ERROR_NONE";
		break;
	case CAMERA_ERROR_INVALID_PARAMETER:
		err_str = "CAMERA_ERROR_INVALID_PARAMETER";
		break;
	case CAMERA_ERROR_INVALID_STATE:
		err_str = "CAMERA_ERROR_INVALID_STATE";
		break;
	case CAMERA_ERROR_OUT_OF_MEMORY:
		err_str = "CAMERA_ERROR_OUT_OF_MEMORY";
		break;
	case CAMERA_ERROR_DEVICE:
		err_str = "CAMERA_ERROR_DEVICE";
		break;
	case CAMERA_ERROR_INVALID_OPERATION:
		err_str = "CAMERA_ERROR_INVALID_OPERATION";
		break;
	case CAMERA_ERROR_SECURITY_RESTRICTED:
		err_str = "CAMERA_ERROR_SECURITY_RESTRICTED";
		break;
	case CAMERA_ERROR_DEVICE_BUSY:
		err_str = "CAMERA_ERROR_DEVICE_BUSY";
		break;
	case CAMERA_ERROR_DEVICE_NOT_FOUND:
		err_str = "CAMERA_ERROR_DEVICE_NOT_FOUND";
		break;
	case CAMERA_ERROR_ESD:
		err_str = "CAMERA_ERROR_ESD";
		break;
	case CAMERA_ERROR_PERMISSION_DENIED:
		err_str = "CAMERA_ERROR_PERMISSION_DENIED";
		break;
	case CAMERA_ERROR_NOT_SUPPORTED:
		err_str = "CAMERA_ERROR_NOT_SUPPORTED";
		break;
	case CAMERA_ERROR_RESOURCE_CONFLICT:
		err_str = "CAMERA_ERROR_RESOURCE_CONFLICT";
		break;
	case CAMERA_ERROR_SERVICE_DISCONNECTED:
		err_str = "CAMERA_ERROR_SERVICE_DISCONNECTED";
		break;
	default:
		err_str = "Unknown";
		break;
	}
	return err_str;
}

static void __print_camera_state(camera_state_e previous, camera_state_e current, bool by_policy, void *user_data)
{
	switch (current) {
	case CAMERA_STATE_CREATED:
		_D("Camera state: CAMERA_STATE_CREATED");
		break;

	case CAMERA_STATE_PREVIEW:
		_D("Camera state: CAMERA_STATE_PREVIEW");
		break;

	case CAMERA_STATE_CAPTURING:
		_D("Camera state: CAMERA_STATE_CAPTURING");
		break;

	case CAMERA_STATE_CAPTURED:
		_D("Camera state: CAMERA_STATE_CAPTURED");
		break;

	default:
		_D("Camera state: CAMERA_STATE_NONE");
		break;
	}
}

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

static image_buffer_data_s *__make_preview_image_buffer_data(camera_preview_data_s *frame)
{
	unsigned char *buffer = NULL;
	unsigned int buffer_size = 0;

	image_buffer_data_s *image_buffer = malloc(sizeof(image_buffer_data_s));
	if (image_buffer == NULL) {
		_E("Failed to allocate image buffer data");
		goto ERROR;
	}

	switch (frame->num_of_planes) {
	case 1:
		buffer_size = frame->data.single_plane.size;
		buffer = malloc(buffer_size);
		if (!buffer) {
			_E("failed to malloc buffer");
			goto ERROR;
		}
		memcpy(buffer, frame->data.single_plane.yuv, buffer_size);
		break;
	case 2:
		{
			unsigned char *buffer2 = NULL;
			buffer_size =
				frame->data.double_plane.y_size + frame->data.double_plane.uv_size;
			buffer = malloc(buffer_size);
			if (!buffer) {
				_E("failed to malloc buffer");
				goto ERROR;
			}
			buffer2 = buffer + frame->data.double_plane.y_size;
			memcpy(buffer,
				frame->data.double_plane.y, frame->data.double_plane.y_size);
			memcpy(buffer2,
				frame->data.double_plane.uv, frame->data.double_plane.uv_size);
		}
		break;
	case 3:
		{
			unsigned char *buffer2 = NULL;
			unsigned char *buffer3 = NULL;
			buffer_size = frame->data.triple_plane.y_size
						+ frame->data.triple_plane.u_size
						+ frame->data.triple_plane.v_size;
			buffer = malloc(buffer_size);
			if (!buffer) {
				_E("failed to malloc buffer");
				goto ERROR;
			}
			buffer2 = buffer + frame->data.triple_plane.y_size;
			buffer3 = buffer2 + frame->data.triple_plane.u_size;
			memcpy(buffer,
				frame->data.triple_plane.y, frame->data.triple_plane.y_size);
			memcpy(buffer2,
				frame->data.triple_plane.u, frame->data.triple_plane.u_size);
			memcpy(buffer3,
				frame->data.triple_plane.v, frame->data.triple_plane.v_size);
		}
		break;
	default:
		_E("unhandled num of planes : %d", frame->num_of_planes);
		goto ERROR;
		break;
	}

	image_buffer->image_width = frame->width;
	image_buffer->image_height = frame->height;
	image_buffer->buffer_size = buffer_size;
	image_buffer->buffer = buffer;
	image_buffer->format = frame->format;

	return image_buffer;

ERROR:
	free(buffer);
	free(image_buffer);

	return NULL;
}

static bool __camera_attr_supported_af_mode_cb(camera_attr_af_mode_e mode, void *user_data)
{
	struct __camera_data *camera_data = user_data;

	_I("Auto-Focus Mode [%d]", mode);

	if (mode != CAMERA_ATTR_AF_NONE)
		camera_data->is_af_enabled = true;

	return true;
}

static void __capturing_cb(camera_image_data_s *image, camera_image_data_s *postview, camera_image_data_s *thumbnail, void *user_data)
{
	struct __camera_data *camera_data = user_data;
	if (image == NULL) {
		_E("Image is NULL");
		return;
	}

	free(camera_data->captured_file);
	camera_data->captured_file = malloc(image->size);
	if (camera_data->captured_file == NULL)
		return;

	_D("Now is on Capturing: Image size[%d x %d] size[%d]", image->width, image->height, image->size);

	memcpy(camera_data->captured_file, image->data, image->size);
	camera_data->image_size = image->size;

	return;
}

static void __completed_cb(void *user_data)
{
	int ret = 0;
	struct __camera_data *camera_data = user_data;

	if (camera_data->capture_completed_cb)
		camera_data->capture_completed_cb(camera_data->captured_file, camera_data->image_size, camera_data->capture_completed_cb_data);

	camera_data->capture_completed_cb = NULL;
	free(camera_data->captured_file);
	camera_data->captured_file = NULL;

	if (!camera_data->cam_handle) {
		_E("Camera is NULL");
		return;
	}

	ret = camera_start_preview(camera_data->cam_handle);
	if (ret != CAMERA_ERROR_NONE) {
		_E("Failed to start preview [%s]", __cam_err_to_str(ret));
		return;
	}

    return;
}

static void __start_capture(void *user_data)
{
	int ret = 0;
	struct __camera_data *camera_data = user_data;

	ret = camera_start_capture(camera_data->cam_handle, __capturing_cb, __completed_cb, camera_data);
	if (ret != CAMERA_ERROR_NONE) {
		_E("Failed to start capturing [%s]", __cam_err_to_str(ret));
		return;
	}
}

static void __camera_preview_cb(camera_preview_data_s *frame, void *user_data)
{
	struct __camera_data *camera_data = user_data;
	static long long int last = 0;
	long long int now = __get_monotonic_ms();

	if (now - last < CAMERA_PREVIEW_INTERVAL_MIN)
		return;

	image_buffer_data_s *image_buffer_data = __make_preview_image_buffer_data(frame);
	if (image_buffer_data == NULL) {
		_E("Failed to create mv source");
		return;
	}
	image_buffer_data->user_data = camera_data->preview_image_buffer_created_cb_data;

	ecore_main_loop_thread_safe_call_async(camera_data->preview_image_buffer_created_cb, image_buffer_data);
	last = now;
}

int resource_camera_init(preview_image_buffer_created_cb preview_image_buffer_created_cb, void *user_data)
{
	int ret = CAMERA_ERROR_NONE;

	if (preview_image_buffer_created_cb == NULL)
		return -1;

	g_camera_data = malloc(sizeof(struct __camera_data));
	if (g_camera_data == NULL) {
		_E("Failed to allocate Camera data");
		return -1;
	}
	memset(g_camera_data, 0, sizeof(struct __camera_data));

	ret = camera_create(CAMERA_DEVICE_CAMERA0, &(g_camera_data->cam_handle));
	if (ret != CAMERA_ERROR_NONE) {
		_E("Failed to create camera [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_attr_set_image_quality(g_camera_data->cam_handle, CAMERA_IMAGE_QUALITY);
	if (ret != CAMERA_ERROR_NONE) {
		_E("Failed to set image quality [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_set_preview_resolution(g_camera_data->cam_handle, IMAGE_WIDTH, IMAGE_HEIGHT);
	if (ret != CAMERA_ERROR_NONE) {
		_E("Failed to set preview resolution [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_set_capture_resolution(g_camera_data->cam_handle, IMAGE_WIDTH, IMAGE_HEIGHT);
	if (ret != CAMERA_ERROR_NONE) {
		_E("Failed to set capture resolution [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_set_capture_format(g_camera_data->cam_handle, CAMERA_PIXEL_FORMAT_JPEG);
	if (ret != CAMERA_ERROR_NONE) {
		_E("Failed to set capture format [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_set_state_changed_cb(g_camera_data->cam_handle, __print_camera_state, NULL);
	if (ret != CAMERA_ERROR_NONE) {
		_E("Failed to set state changed callback [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_set_preview_cb(g_camera_data->cam_handle, __camera_preview_cb, g_camera_data);
	if (ret != CAMERA_ERROR_NONE) {
		_E("Failed to set preview callback [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_attr_foreach_supported_af_mode(g_camera_data->cam_handle, __camera_attr_supported_af_mode_cb, g_camera_data);
	if (ret != CAMERA_ERROR_NONE) {
		_E("Failed to set auto focus attribute check callback [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	g_camera_data->preview_image_buffer_created_cb = preview_image_buffer_created_cb;
	g_camera_data->preview_image_buffer_created_cb_data = user_data;

	return 0;

ERROR:
	if (g_camera_data->cam_handle)
		camera_destroy(g_camera_data->cam_handle);

	free(g_camera_data);
	g_camera_data = NULL;
	return -1;
}

int resource_camera_start_preview(void)
{
	camera_state_e state;
	int ret = CAMERA_ERROR_NONE;

	if (g_camera_data == NULL) {
		_I("Camera is not initialized");
		return -1;
	}

	ret = camera_get_state(g_camera_data->cam_handle, &state);
	if (ret != CAMERA_ERROR_NONE) {
		_E("Failed to get camera state [%s]", __cam_err_to_str(ret));
		return -1;
	}

	if (state == CAMERA_STATE_CAPTURING) {
		_D("Camera is now capturing");
		return -1;
	}

	if (state != CAMERA_STATE_PREVIEW) {
		ret = camera_start_preview(g_camera_data->cam_handle);
		if (ret != CAMERA_ERROR_NONE) {
			_E("Failed to start preview [%s]", __cam_err_to_str(ret));
			return -1;
		}
	}

	return 0;
}

int resource_camera_capture(capture_completed_cb capture_completed_cb, void *user_data)
{
	camera_state_e state;
	int ret = CAMERA_ERROR_NONE;

	if (g_camera_data == NULL) {
		_I("Camera is not initialized");
		return -1;
	}

	ret = camera_get_state(g_camera_data->cam_handle, &state);
	if (ret != CAMERA_ERROR_NONE) {
		_E("Failed to get camera state [%s]", __cam_err_to_str(ret));
		return -1;
	}

	if (state == CAMERA_STATE_CAPTURING) {
		_D("Camera is now capturing");
		return -1;
	}

	if (state != CAMERA_STATE_PREVIEW) {
		_I("Preview is not started [%d]", state);
		ret = camera_start_preview(g_camera_data->cam_handle);
		if (ret != CAMERA_ERROR_NONE) {
			_E("Failed to start preview [%s]", __cam_err_to_str(ret));
			return -1;
		}
	}

	__start_capture(g_camera_data);

	g_camera_data->capture_completed_cb = capture_completed_cb;
	g_camera_data->capture_completed_cb_data = user_data;

	return 0;
}


int resource_camera_stop_preview(void)
{
	camera_state_e state;
	int ret = CAMERA_ERROR_NONE;

	if (g_camera_data == NULL) {
		_I("Camera is not initialized");
		return -1;
	}

	ret = camera_get_state(g_camera_data->cam_handle, &state);
	if (ret != CAMERA_ERROR_NONE) {
		_E("Failed to get camera state [%s]", __cam_err_to_str(ret));
		return -1;
	}

	if (state == CAMERA_STATE_CAPTURING) {
		_D("Camera is now capturing");
		return -1;
	}

	if (state == CAMERA_STATE_PREVIEW) {
		ret = camera_stop_preview(g_camera_data->cam_handle);
		if (ret != CAMERA_ERROR_NONE) {
			_E("Failed to stop preview [%s]", __cam_err_to_str(ret));
			return -1;
		}
	}

	return 0;
}


void resource_camera_close(void)
{
	if (g_camera_data == NULL)
		return;

	camera_unset_preview_cb(g_camera_data->cam_handle);
	camera_stop_preview(g_camera_data->cam_handle);

	camera_destroy(g_camera_data->cam_handle);
	g_camera_data->cam_handle = NULL;

	free(g_camera_data->captured_file);
	g_camera_data->captured_file = NULL;

	free(g_camera_data);
	g_camera_data = NULL;
}
