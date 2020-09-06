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

#ifndef __APP_EXIF_H__
#define __APP_EXIF_H__

/* CAUTION
 * This function is only for adding exif with user comment
 * to JPEG which has No existing exif data !!!
 */
int exif_write_jpg_file_with_comment(const char *output_file,
		const unsigned char *jpg_data, unsigned int jpg_size,
		unsigned int jpg_width, unsigned int jpg_height,
		const char *comment, unsigned int comment_len);

#endif /* __APP_EXIF_H__ */
