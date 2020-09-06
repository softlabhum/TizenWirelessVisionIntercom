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
#include <stdio.h>
#include <tizen.h>
#include <image_util.h>
#include "log.h"
#include "exif.h"

static image_util_encode_h encode_h = NULL;
static image_util_decode_h decode_h = NULL;

#define IMAGE_COLORSPACE IMAGE_UTIL_COLORSPACE_I420

void controller_image_initialize(void)
{
	int error_code = image_util_encode_create(IMAGE_UTIL_JPEG, &encode_h);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_create [%s]", get_error_message(error_code));
	}

	error_code = image_util_decode_create(&decode_h);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_decode_create [%s]", get_error_message(error_code));
	}
}

void controller_image_finalize(void)
{
	int error_code = image_util_encode_destroy(encode_h);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_destroy [%s]", get_error_message(error_code));
	}

	error_code = image_util_decode_destroy(decode_h);
    if (error_code != IMAGE_UTIL_ERROR_NONE) {
        _E("image_util_decode_destroy [%s]", get_error_message(error_code));
    }
}

int controller_image_save_image_file(const char *path,
	unsigned int width, unsigned int height, const unsigned char *buffer,
	unsigned char** encoded, unsigned long long* encoded_size, const char *comment, unsigned int comment_len)
{
	int error_code = image_util_encode_set_resolution(encode_h, width, height);

	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_set_resolution [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_encode_set_colorspace(encode_h, IMAGE_COLORSPACE);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_set_colorspace [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_encode_set_quality(encode_h, 90);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_set_quality [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_encode_set_input_buffer(encode_h, buffer);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_set_input_buffer [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_encode_set_output_buffer(encode_h, encoded);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_set_output_buffer [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_encode_run(encode_h, encoded_size);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_run [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = exif_write_jpg_file_with_comment(path,
			*encoded, (unsigned int)*encoded_size, width, height, comment, comment_len);

	return error_code;
}

int controller_image_read_image_file(const char *path,
	unsigned long *width, unsigned long *height, unsigned char *buffer, unsigned long long *size)
{
	int error_code = image_util_decode_set_input_path(decode_h, path);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_decode_set_input_path [%s] [%s]", path, get_error_message(error_code));
		return -1;
	}

	error_code = image_util_decode_set_output_buffer(decode_h, &buffer);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_decode_set_output_buffer [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_decode_set_colorspace(decode_h, IMAGE_UTIL_COLORSPACE_RGBA8888);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_decode_set_colorspace [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_decode_set_jpeg_downscale(decode_h, IMAGE_UTIL_DOWNSCALE_1_1);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_decode_set_jpeg_downscale [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_decode_run(decode_h, width, height, size);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_decode_run [%s]", get_error_message(error_code));
		return -1;
	}

	return error_code;
}
