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

#include <curl/curl.h>
#include <glib.h>
#include "log.h"

#define REQ_CON_TIMEOUT 5L
#define REQ_TIMEOUT 7L

//https://api.telegram.org/bot{YOUR_BOT_TOKEN}/sendMessage?text={TEXT_MSG}&chat_id={CHATROOM_ID}
//https://api.telegram.org/bot{YOUR_BOT_TOKEN}/sendPhoto?chat_id={CHATROOM_ID}

#define TELEGRAM_BOT_INFO "1193973544:AAEAJOr_-i_Yq2mVQnVbs4_OhPLPwFg1sJY"
#define TELEGRAM_CHATROOM_INFO "1061311253"

#define TELEGRAM_API_HOST_URL "https://api.telegram.org/bot"
#define TELEGRAM_BOT_SEND_MSG_URL "/sendMessage?text=[동작감지] 센서가 작동하고있습니다."
#define TELEGRAM_BOT_SEND_PHOTO_URL "/sendPhoto"

static size_t _response_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	size_t res_size = 0;

	res_size = size*nmemb;

	if (res_size > 0)
		_I("CURL response : %s", ptr);
	/* What should we do here, if response body has negative message? */

	return res_size;
}

static int __curl_debug(CURL *handle, curl_infotype type,
	char *data, size_t size, void *userptr)
{
	const char *prefix = NULL;
	char *message = NULL;

	switch (type) {
	case CURLINFO_END:
		return 0;
	case CURLINFO_TEXT:
		_D("== text Info: %s", data);
		return 0;
	case CURLINFO_HEADER_OUT:
		prefix = "=> Send header:";
		break;
	case CURLINFO_DATA_OUT:
		prefix = "=> Send data:";
		break;
	case CURLINFO_SSL_DATA_OUT:
		prefix = "=> Send SSL data:";
		break;
	case CURLINFO_HEADER_IN:
		prefix = "<= Recv header:";
		break;
	case CURLINFO_DATA_IN:
		prefix = "<= Recv data:";
		break;
	case CURLINFO_SSL_DATA_IN:
		prefix = "<= Recv SSL data:";
		break;
	}
	message = g_strndup(data, size);
	_D("%s %s", prefix, message);
	g_free(message);
	return 0;
}

int controller_telegram_send_message(const char* msg)
{
	int ret = 0;
	CURL *curl = NULL;
	CURLcode response = CURLE_OK;

	curl = curl_easy_init();

	if (!curl) {
		_E("fail to init curl");
		return -1;
	}

	char* url_with_msg = g_strdup_printf("%s%s%s%s&chat_id=%s",
		TELEGRAM_API_HOST_URL,
		TELEGRAM_BOT_INFO,
		TELEGRAM_BOT_SEND_MSG_URL,
		msg,
		TELEGRAM_CHATROOM_INFO);

	_D("Send TEXT Url: [%s]", url_with_msg);

	curl_easy_setopt(curl, CURLOPT_URL, url_with_msg);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _response_callback);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, REQ_CON_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, REQ_TIMEOUT);

	response = curl_easy_perform(curl);

	if (response != CURLE_OK) {
		_E("curl_easy_perform() failed: %s",
			curl_easy_strerror(response));
		/* What should we do here, if response is kind of errors? */
		ret = -1;
	}

	g_free(url_with_msg);
	curl_easy_cleanup(curl);

	return ret;
}

int controller_telegram_send_image(const unsigned char* image_buffer, unsigned int buffer_size)
{
	int ret = 0;
	CURL *curl = NULL;
	CURLcode response = CURLE_OK;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;

	curl = curl_easy_init();

	if (!image_buffer) {
		_E("NULL image_buffer");
		return -1;
	}

	if (!curl) {
		_E("fail to init curl");
		return -1;
	}

	char* url_with_msg = g_strdup_printf("%s%s%s?chat_id=%s",
		TELEGRAM_API_HOST_URL,
		TELEGRAM_BOT_INFO,
		TELEGRAM_BOT_SEND_PHOTO_URL,
		TELEGRAM_CHATROOM_INFO);

	_D("Send PHOTO Url: [%s]", url_with_msg);

	curl_formadd(&formpost, &lastptr,
		CURLFORM_COPYNAME, "content-type:",
		CURLFORM_COPYCONTENTS, "multipart/form-data",
		CURLFORM_END);

	curl_formadd(&formpost, &lastptr,
		CURLFORM_COPYNAME, "photo",
		CURLFORM_BUFFER, "motion.jpg",
		CURLFORM_BUFFERPTR, image_buffer,
		CURLFORM_BUFFERLENGTH, buffer_size,
		CURLFORM_END);

	curl_easy_setopt(curl, CURLOPT_URL, url_with_msg);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _response_callback);

	/* if CURLOPT_VERBOSE is enabled, __curl_debug() function will be called */
	// curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, __curl_debug);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, REQ_CON_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, REQ_TIMEOUT);

	response = curl_easy_perform(curl);

	if (response != CURLE_OK) {
		_E("curl_easy_perform() failed: %s",
			curl_easy_strerror(response));
		ret = -1;
	}

	curl_easy_cleanup(curl);
	curl_formfree(formpost);
	g_free(url_with_msg);

	return ret;
}
