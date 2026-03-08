/* See LICENSE.dwm file for copyright and license details. */
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

void die(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

void *ecalloc(size_t nmemb, size_t size) {
	void *p;

	if (!(p = calloc(nmemb, size)))
		die("calloc:");
	return p;
}

int32_t fd_set_nonblock(int32_t fd) {
	int32_t flags = fcntl(fd, F_GETFL);
	if (flags < 0) {
		perror("fcntl(F_GETFL):");
		return -1;
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		perror("fcntl(F_SETFL):");
		return -1;
	}

	return 0;
}

int32_t regex_match(const char *pattern, const char *str) {
	int32_t errnum;
	PCRE2_SIZE erroffset;

	if (!pattern || !str) {
		return 0;
	}

	pcre2_code *re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
								   PCRE2_UTF, // 启用 UTF-8 支持
								   &errnum, &erroffset, NULL);
	if (!re) {
		PCRE2_UCHAR errbuf[256];
		pcre2_get_error_message(errnum, errbuf, sizeof(errbuf));
		fprintf(stderr, "PCRE2 error: %s at offset %zu\n", errbuf, erroffset);
		return 0;
	}

	pcre2_match_data *match_data =
		pcre2_match_data_create_from_pattern(re, NULL);
	int32_t ret =
		pcre2_match(re, (PCRE2_SPTR)str, strlen(str), 0, 0, match_data, NULL);

	pcre2_match_data_free(match_data);
	pcre2_code_free(re);
	return ret >= 0;
}

void wl_list_append(struct wl_list *list, struct wl_list *object) {
	wl_list_insert(list->prev, object);
}

uint32_t get_now_in_ms(void) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	return timespec_to_ms(&now);
}

uint32_t timespec_to_ms(struct timespec *ts) {
	return (uint32_t)ts->tv_sec * 1000 + (uint32_t)ts->tv_nsec / 1000000;
}

char *join_strings(char *arr[], const char *sep) {
	if (!arr || !arr[0]) {
		char *empty = malloc(1);
		if (empty)
			empty[0] = '\0';
		return empty;
	}

	size_t total_len = 0;
	int count = 0;
	for (int i = 0; arr[i] != NULL; i++) {
		total_len += strlen(arr[i]);
		count++;
	}
	if (count > 0) {
		total_len += strlen(sep) * (count - 1);
	}

	char *result = malloc(total_len + 1);
	if (!result)
		return NULL;

	result[0] = '\0';
	for (int i = 0; arr[i] != NULL; i++) {
		if (i > 0)
			strcat(result, sep);
		strcat(result, arr[i]);
	}
	return result;
}

char *join_strings_with_suffix(char *arr[], const char *suffix,
							   const char *sep) {
	if (!arr || !arr[0]) {
		char *empty = malloc(1);
		if (empty)
			empty[0] = '\0';
		return empty;
	}

	size_t total_len = 0;
	int count = 0;
	for (int i = 0; arr[i] != NULL; i++) {
		total_len += strlen(arr[i]) + strlen(suffix);
		count++;
	}
	if (count > 0) {
		total_len += strlen(sep) * (count - 1);
	}

	char *result = malloc(total_len + 1);
	if (!result)
		return NULL;

	result[0] = '\0';
	for (int i = 0; arr[i] != NULL; i++) {
		if (i > 0)
			strcat(result, sep);
		strcat(result, arr[i]);
		strcat(result, suffix);
	}
	return result;
}

char *string_printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);
	if (len < 0)
		return NULL;

	char *str = malloc(len + 1);
	if (!str)
		return NULL;

	va_start(args, fmt);
	vsnprintf(str, len + 1, fmt, args);
	va_end(args);
	return str;
}
