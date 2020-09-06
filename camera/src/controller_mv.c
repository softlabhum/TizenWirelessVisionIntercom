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
#include <stdlib.h>
#include <mv_common.h>
#include <mv_surveillance.h>
#include "controller.h"
#include "controller_mv.h"
#include "log.h"

#define VIDEO_STREAM_ID 0
#define THRESHOLD_SIZE_REGION 100

struct __mv_data {
	mv_surveillance_event_trigger_h mv_trigger_handle;
	movement_detected_cb movement_detected_cb;
	void *movement_detected_cb_data;
};

static struct __mv_data *mv_data = NULL;

static const char *__mv_err_to_str(mv_error_e err)
{
	const char *err_str;
	switch (err) {
	case MEDIA_VISION_ERROR_NONE:
		err_str = "MEDIA_VISION_ERROR_NONE";
		break;
	case MEDIA_VISION_ERROR_NOT_SUPPORTED:
		err_str = "MEDIA_VISION_ERROR_NOT_SUPPORTED";
		break;
	case MEDIA_VISION_ERROR_MSG_TOO_LONG:
		err_str = "MEDIA_VISION_ERROR_MSG_TOO_LONG";
		break;
	case MEDIA_VISION_ERROR_NO_DATA:
		err_str = "MEDIA_VISION_ERROR_NO_DATA";
		break;
	case MEDIA_VISION_ERROR_KEY_NOT_AVAILABLE:
		err_str = "MEDIA_VISION_ERROR_KEY_NOT_AVAILABLE";
		break;
	case MEDIA_VISION_ERROR_OUT_OF_MEMORY:
		err_str = "MEDIA_VISION_ERROR_OUT_OF_MEMORY";
		break;
	case MEDIA_VISION_ERROR_INVALID_PARAMETER:
		err_str = "MEDIA_VISION_ERROR_INVALID_PARAMETER";
		break;
	case MEDIA_VISION_ERROR_INVALID_OPERATION:
		err_str = "MEDIA_VISION_ERROR_INVALID_OPERATION";
		break;
	case MEDIA_VISION_ERROR_PERMISSION_DENIED:
		err_str = "MEDIA_VISION_ERROR_PERMISSION_DENIED";
		break;
	case MEDIA_VISION_ERROR_NOT_SUPPORTED_FORMAT:
		err_str = "MEDIA_VISION_ERROR_NOT_SUPPORTED_FORMAT";
		break;
	case MEDIA_VISION_ERROR_INTERNAL:
		err_str = "MEDIA_VISION_ERROR_INTERNAL";
		break;
	case MEDIA_VISION_ERROR_INVALID_DATA:
		err_str = "MEDIA_VISION_ERROR_INVALID_DATA";
		break;
	case MEDIA_VISION_ERROR_INVALID_PATH:
		err_str = "MEDIA_VISION_ERROR_INVALID_PATH";
		break;
	default:
		err_str = "Unknown Error";
		break;
	}

	return err_str;
}

static void __movement_detected_event_cb(mv_surveillance_event_trigger_h trigger, mv_source_h source, int video_stream_id, mv_surveillance_result_h event_result, void *data)
{
	int ret = 0;
	int result[MV_RESULT_LENGTH_MAX] = {0, };
	int result_count = 0;
	int result_index = 0;
	int valid_area_sum = 0;
	int i;
	size_t move_regions_num = 0;
	mv_rectangle_s *regions = NULL;

	ret_if(!trigger);
	ret_if(!event_result);
	ret_if(!mv_data);

	ret = mv_surveillance_get_result_value(event_result, MV_SURVEILLANCE_MOVEMENT_NUMBER_OF_REGIONS, &move_regions_num);
	retm_if(ret, "failed to mv_surveillance_get_result_value for %s - [%s]", MV_SURVEILLANCE_MOVEMENT_NUMBER_OF_REGIONS, __mv_err_to_str(ret));

	regions = malloc(sizeof(mv_rectangle_s) * move_regions_num);
	ret = mv_surveillance_get_result_value(event_result, MV_SURVEILLANCE_MOVEMENT_REGIONS, regions);
	retm_if(ret, "failed to mv_surveillance_get_result_value for %s - [%s]", MV_SURVEILLANCE_MOVEMENT_REGIONS, __mv_err_to_str(ret));

	for (i = 0; i < move_regions_num; i++) {
		// _D("region[%u] - position[%d x %d], witdh[%d], height[%d]", i, regions[i].point.x, regions[i].point.y, regions[i].width, regions[i].height);
		// _D("region[%u] - area[%d]", i, regions[i].width * regions[i].height);

		if (regions[i].width * regions[i].height < THRESHOLD_SIZE_REGION || result_count >= MV_RESULT_COUNT_MAX)
			continue;

		result[result_index] = regions[i].point.x * 99 / IMAGE_WIDTH;
		result[result_index + 1] = regions[i].point.y * 99 / IMAGE_HEIGHT;
		result[result_index + 2] = regions[i].width * 99 / IMAGE_WIDTH;
		result[result_index + 3] = regions[i].height * 99 / IMAGE_HEIGHT;

		result_count++;
		result_index = result_count * 4;

		valid_area_sum += regions[i].width * regions[i].height;
	}
	free(regions);

	mv_data->movement_detected_cb(valid_area_sum, result, result_count, mv_data->movement_detected_cb_data);
}

void controller_mv_push_source(mv_source_h source)
{
	int ret = 0;
	ret_if(!source);

	ret = mv_surveillance_push_source(source, VIDEO_STREAM_ID);
	if (ret)
		_E("failed to mv_surveillance_push_source() - [%s]", __mv_err_to_str(ret));

	mv_destroy_source(source);
}

mv_source_h controller_mv_create_source(
		unsigned char *buffer, unsigned int size,
		unsigned int width, unsigned int height, mv_colorspace_e colorspace)
{
	mv_source_h source = NULL;
	int ret = 0;

	retv_if(!buffer, NULL);

	ret = mv_create_source(&source);
	retvm_if(ret, NULL, "failed to mv_create_source - [%s]", __mv_err_to_str(ret));

	ret = mv_source_fill_by_buffer(source, buffer, size, width, height, colorspace);
	if (ret) {
		_E("failed to fill source - %d", ret);

		if (source)
			mv_destroy_source(source);

		return NULL;
	}

	return source;
}

int controller_mv_set_movement_detection_event_cb(movement_detected_cb movement_detected_cb, void *user_data)
{
	int ret = 0;
	mv_engine_config_h engine_cfg = NULL;

	if (movement_detected_cb == NULL)
		return -1;

	if (mv_data != NULL) {
		mv_data->movement_detected_cb = movement_detected_cb;
		mv_data->movement_detected_cb_data = user_data;
		return 0;
	}

	mv_data = malloc(sizeof(struct __mv_data));
	if (mv_data == NULL) {
		_E("Failed to allocate media vision data");
		return -1;
	}
	memset(mv_data, 0, sizeof(struct __mv_data));

	ret = mv_create_engine_config(&engine_cfg);
	if (ret) {
		_E("failed to subsmv_create_engine_configs() - %s", __mv_err_to_str(ret));
		goto ERROR;
	}

	mv_engine_config_set_int_attribute(engine_cfg, MV_SURVEILLANCE_MOVEMENT_DETECTION_THRESHOLD, 50); /* 10 is default value [0 ~ 255] */

	ret = mv_surveillance_event_trigger_create(MV_SURVEILLANCE_EVENT_TYPE_MOVEMENT_DETECTED, &mv_data->mv_trigger_handle);
	if (ret) {
		_E("failed to mv_surveillance_event_trigger_create - [%s]", __mv_err_to_str(ret));
		goto ERROR;
	}

	ret = mv_surveillance_subscribe_event_trigger(mv_data->mv_trigger_handle, VIDEO_STREAM_ID, engine_cfg, __movement_detected_event_cb, mv_data);

	if (ret) {
		_E("failed to subscribe %s - %s", MV_SURVEILLANCE_EVENT_TYPE_MOVEMENT_DETECTED, __mv_err_to_str(ret));
		goto ERROR;
	}

	if (engine_cfg)
		mv_destroy_engine_config(engine_cfg);

	mv_data->movement_detected_cb = movement_detected_cb;
	mv_data->movement_detected_cb_data = user_data;

	return 0;

ERROR:
	if (engine_cfg)
		mv_destroy_engine_config(engine_cfg);

	if (mv_data->mv_trigger_handle)
		mv_surveillance_event_trigger_destroy(mv_data->mv_trigger_handle);

	free(mv_data);
	mv_data = NULL;

	return -1;
}

void controller_mv_unset_movement_detection_event_cb(void)
{
	if (mv_data == NULL)
		return;

	if (mv_data->mv_trigger_handle)
		mv_surveillance_event_trigger_destroy(mv_data->mv_trigger_handle);

	free(mv_data);
	mv_data = NULL;
}
