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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libexif/exif-loader.h>
#include <libexif/exif-utils.h>
#include <libexif/exif-data.h>
#include "log.h"

#define ASCII_COMMENT_HEADER "ASCII\0\0\0"
// #define CHECK_EXIF_BEFOR_CREATE

static int check_exif_from_data(const unsigned char *img, unsigned int size)
{
#ifdef CHECK_EXIF_BEFOR_CREATE
	ExifLoader *loader = NULL;
	unsigned char ret = 0;
	const unsigned char *buf = NULL;
	unsigned int b_size = 0;

	retv_if(!img, -1);
	retv_if(size == 0, -1);

	loader = exif_loader_new();
	retv_if(!loader, -1);

	ret = exif_loader_write(loader, img, size);
	if (ret) {
		_E("failed to loader write");
		exif_loader_unref(loader);
		return -1;
	}

	exif_loader_get_buf(loader, &buf, &b_size);
	if (buf) {
		_E("image has already exif data [%u]", b_size);
		exif_loader_unref(loader);
		return -1;
	}

	exif_loader_unref(loader);
#endif /* CHECK_EXIF_BEFOR_CREATE */

	return 0;
}

static ExifEntry *
add_tag_by_init(ExifData *exif, ExifIfd ifd, ExifTag tag)
{
	ExifEntry *entry = NULL;

	retv_if(!exif, NULL);

	entry = exif_content_get_entry(exif->ifd[ifd], tag);
	if (!entry) {
		entry = exif_entry_new();
		retv_if(!entry, NULL);

		exif_entry_initialize(entry, tag);

		entry->tag = tag;
		exif_content_add_entry(exif->ifd[ifd], entry);

		exif_entry_unref(entry);
	}
	return entry;
}

static ExifEntry *
add_tag_by_malloc(ExifData *exif, ExifIfd ifd, ExifTag tag, unsigned int size)
{
	ExifEntry *entry = NULL;
	void *buffer = NULL;
	ExifMem *mem = NULL;

	retv_if(!exif, NULL);
	retv_if(size == 0, NULL);

	mem = exif_mem_new_default();
	retv_if(!mem, NULL);

	entry = exif_entry_new_mem(mem);
	if (!entry) {
		_E("failed to exif_entry_new_mem()");
		exif_mem_unref(mem);
		return NULL;
	}

	buffer = exif_mem_alloc(mem, size);
	if (!buffer) {
		_E("failed to exif_mem_alloc()");
		exif_mem_unref(mem);
		exif_entry_unref(entry);
		return NULL;
	}

	entry->data = buffer;
	entry->size = size;
	entry->tag = tag;
	entry->components = size;
	entry->format = EXIF_FORMAT_UNDEFINED;
	exif_content_add_entry (exif->ifd[ifd], entry);

	exif_mem_unref(mem);
	exif_entry_unref(entry);

	return entry;
}

static ExifEntry *
add_tag_user_comment(ExifData *exif, const char *comment, unsigned int length)
{
	ExifEntry *entry = NULL;
	size_t header_size = 0;

	retv_if(!exif, NULL);
	retv_if(!comment, NULL);
	retv_if(length == 0, NULL);

	header_size = sizeof(ASCII_COMMENT_HEADER) -1;

	entry = add_tag_by_malloc(exif, EXIF_IFD_EXIF, EXIF_TAG_USER_COMMENT,
				header_size + length);

	retv_if(!entry, NULL);

	memcpy(entry->data, ASCII_COMMENT_HEADER, header_size);
	memcpy(entry->data + header_size, comment, length);

	return entry;
}

static int
create_exif_data(const unsigned char *jpg_data, unsigned int jpg_size,
		unsigned int jpg_width, unsigned int jpg_height,
		const char *user_comment, unsigned int comment_len,
		unsigned char **exif_data, unsigned int *exif_size)
{

	unsigned char *data;
	unsigned int data_len;
	ExifEntry *ee = NULL;
	ExifData *exif = NULL;

	retv_if(!jpg_data, -1);
	retv_if(!exif_data, -1);
	retv_if(!exif_size, -1);

	if (check_exif_from_data(jpg_data, jpg_size))
		return -1;

	exif = exif_data_new();
	retv_if(!exif, -1);

	exif_data_set_option(exif, EXIF_DATA_OPTION_FOLLOW_SPECIFICATION);
	exif_data_set_data_type(exif, EXIF_DATA_TYPE_COMPRESSED);
	exif_data_set_byte_order(exif, EXIF_BYTE_ORDER_INTEL);

	exif_data_fix(exif);

	ee = add_tag_by_init(exif, EXIF_IFD_EXIF, EXIF_TAG_PIXEL_X_DIMENSION);
	exif_set_long(ee->data, EXIF_BYTE_ORDER_INTEL, jpg_width);

	ee = add_tag_by_init(exif, EXIF_IFD_EXIF, EXIF_TAG_PIXEL_Y_DIMENSION);
	exif_set_long(ee->data, EXIF_BYTE_ORDER_INTEL, jpg_height);

	ee = add_tag_user_comment(exif, user_comment, comment_len);

	exif_data_save_data(exif, &data, &data_len);
	if (!data) {
		_E("failed to get exif data");
		exif_data_unref(exif);
		return -1;
	}
	exif_data_unref(exif);

	*exif_data = data;
	*exif_size = data_len;

	return 0;
}

static int
save_jpeg_file(const char *output_file,
		const unsigned char *jpg_data, unsigned int jpg_size)
{
	FILE *fp = NULL;
	retv_if(!output_file, -1);
	retv_if(!jpg_data, -1);
	retv_if(jpg_size <= 2, -1);

	fp = fopen(output_file, "wb");
	retv_if(!fp, -1);

	if (1 != fwrite(jpg_data, jpg_size, 1, fp)) {
		_E("failed to write jpg data");
		fclose(fp);
		return -1;
	}

	fflush(fp);
	fclose(fp);

	_D("file saved [%s]", output_file);

	return 0;
}

static int
save_jpeg_file_with_exif(const char *output_file,
		const unsigned char *jpg_data, unsigned int jpg_size,
		unsigned char *exif_data, unsigned int exif_size)
{
	FILE *fp = NULL;
	const unsigned char exif_header[] = { 0xff, 0xd8, 0xff, 0xe1 }; // FFD8 -SOI Maker, FFE1 - APP1 Maker
	const unsigned short exif_header_length = 4;
	const unsigned short jpg_offset = 2; // SOI maker offset
	unsigned short exif_length = 0;

	((unsigned char *)&exif_length)[0] = (exif_size + 2) >> 8;
	((unsigned char *)&exif_length)[1] = (exif_size + 2) & 0x00ff;

	retv_if(!output_file, -1);
	retv_if(!jpg_data, -1);
	retv_if(!exif_data, -1);
	retv_if(jpg_size <= 2, -1);
	retv_if(exif_size == 0, -1);

	fp = fopen(output_file, "wb");
	retv_if(!fp, -1);

	if (1 != fwrite(exif_header, exif_header_length, 1, fp)) {
		_E("failed to write exif header");
		goto ERROR;
	}

	if (1 != fwrite(&exif_length, sizeof(exif_length), 1, fp)) {
		_E("failed to write exif legth");
		goto ERROR;
	}

	if (1 != fwrite(exif_data, exif_size, 1, fp)) {
		_E("failed to write exif");
		goto ERROR;
	}

	if (1 != fwrite(jpg_data + jpg_offset, jpg_size - jpg_offset, 1, fp)) {
		_E("failed to write jpg data");
		goto ERROR;
	}

	fflush(fp);
	fclose(fp);

	return 0;
ERROR:
	fclose(fp);
	unlink(output_file);
	_E(" failed to save file - [%s]", output_file);
	return -1;
}

int exif_write_jpg_file_with_comment(const char *output_file,
		const unsigned char *jpg_data, unsigned int jpg_size,
		unsigned int jpg_width, unsigned int jpg_height,
		const char *comment, unsigned int comment_len)
{
	int ret = 0;
	unsigned char *exif_data  = NULL;
	unsigned int exif_size = 0;

	if (!comment || (comment_len ==  0)) {
		_W("There is no comment");
		return save_jpeg_file(output_file, jpg_data, jpg_size);
	}

	ret = create_exif_data(jpg_data, jpg_size, jpg_width, jpg_height,
			comment, comment_len, &exif_data, &exif_size);
	if (ret) {
		_E("failed to create_exif_data(), save jpg data only");
		return save_jpeg_file(output_file, jpg_data, jpg_size);
	}

	ret = save_jpeg_file_with_exif(
			output_file, jpg_data, jpg_size, exif_data, exif_size);

	free(exif_data);
	return ret;
}
