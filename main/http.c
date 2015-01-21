#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "ch.h"
#include "chprintf.h"
#include "uip.h"
#include "http.h"
#include "util.h"
#include "ui.h"

static const char *http_seps = " \r\n";

static const char http_resp_ok[] = \
								   "HTTP/1.1 200\r\n" \
								   "\r\n";

static const char http_resp_serverfault[] = \
									"HTTP/1.1 500\r\n" \
									"\r\n";

static const char http_resp_notimp[] = \
									"HTTP/1.1 501\r\n" \
									"\r\n";

static const char http_resp_redir[] = \
									 "HTTP/1.1 301\r\n" \
									 "Location: /\r\n" \
									 "\r\n";

static const char http_resp_badreq[] = \
									  "HTTP/1.1 4˝\r\n" \
									  "\r\n";

struct http_file {
	char path[10];
	const uint8_t *data;
	const unsigned int *len;
	void (*get_handler)(const struct http_file *);
	void (*post_handler)(const struct http_file *);
};

void *memmem(const void *l, size_t l_len, const void *s, size_t s_len)
{
	register char *cur, *last;
	const char *cl = (const char *)l;
	const char *cs = (const char *)s;

	/* we need something to compare */
	if (l_len == 0 || s_len == 0)
		return NULL;

	/* "s" must be smaller or equal to "l" */
	if (l_len < s_len)
		return NULL;

	/* special case where s_len == 1 */
	if (s_len == 1)
		return memchr(l, (int)*cs, l_len);

	/* the last position where its possible to find "s" in "l" */
	last = (char *)cl + l_len - s_len;

	for (cur = (char *)cl; cur <= last; cur++)
		if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
			return cur;

	return NULL;
}

static void http_handle_static(const struct http_file *file)
{
	uip_conn->appstate.remaining = *file->len;
	uip_conn->appstate.buff = file->data;
}

static void http_handle_status(const struct http_file *file)
{
	char sbuf[128];
	int len;

	len = chsnprintf(sbuf, ARRAY_SIZE(sbuf),
			"HTTP/1.1 200\r\n"
			"Content-Type: application/json\r\n"
			"\r\n"
			"{"
			"\"temp_board\": %d,"
			"\"temp_oven\": %d,"
			"\"oven_time\": %d,"
			"\"is_running\": %s"
			"}"
			);
	uip_send(sbuf, len);
}

static void http_handle_profile_get(const struct http_file *file)
{
	char sbuf[10];
	int len = 0;
	//int i;

	uip_send(sbuf, len);
}

static void http_handle_profile_post(const struct http_file *file)
{
	char buf[128];
	char *uptr;
	//char *tok;
	//int eints[2];
	int datalen;
	//unsigned int i;
	//unsigned int k;
	
	uptr = memmem(uip_appdata, uip_datalen(), "\r\n\r\n", 4);

	if (uptr == NULL) { /* Probably invalid data */
		uip_send(http_resp_badreq, sizeof(http_resp_badreq));
		return;
	}

	uptr += 4;

	/* Account for the terminating \0 */
	datalen = uip_datalen() - (uptr - (char *)uip_appdata) + 1;
	/* Three characters are minimum needed to encode at least one entry
	 * We also cannot handle strings longer than our work buffer
	 * */
	if (datalen <= 4 || datalen > (int)sizeof(buf)) {
		uip_send(http_resp_badreq, sizeof(http_resp_badreq));
		return;
	}

	memcpy(buf, uptr, datalen);
	buf[datalen] = '\0';

	//tok = strtok(buf, " \n");

	uip_send(http_resp_ok, sizeof(http_resp_notimp));
}

static void http_handle_start(const struct http_file *file)
{
	if (TRUE)
	{
		uip_send(http_resp_serverfault, sizeof(http_resp_serverfault));
		return;
	}
	//uip_send(http_resp_ok, sizeof(http_resp_ok));
}

static void http_handle_stop(const struct http_file *file)
{
	if (TRUE)
	{
		uip_send(http_resp_serverfault, sizeof(http_resp_serverfault));
		return;
	}
	//uip_send(http_resp_ok, sizeof(http_resp_ok));
}

//static const struct http_file http_files[] = {
//	{ "/", ui_min_html, &ui_min_html_len, http_handle_static, NULL },
//	{ "/ui.css", ui_min_css, &ui_min_css_len, http_handle_static, NULL },
//	{ "/ui.js", ui_min_js, &ui_min_js_len, http_handle_static, NULL },
//	{ "/status", NULL, NULL, http_handle_status, NULL },
//	{ "/profile", NULL, NULL, http_handle_profile_get, http_handle_profile_post },
//	{ "/start", NULL, NULL, NULL, http_handle_start },
//	{ "/stop", NULL, NULL, NULL, http_handle_stop },
//};

static const struct http_file http_files[] = {
	{ "/", ui_min_html, &ui_min_html_len, http_handle_static, NULL },
	{ "/ui.css", ui_min_css, &ui_min_css_len, http_handle_static, NULL },
	{ "/ui.js", ui_min_js, &ui_min_js_len, http_handle_static, NULL },
	{ "/status", NULL, NULL, NULL, NULL },
	{ "/profile", NULL, NULL, NULL, NULL },
	{ "/start", NULL, NULL, NULL, NULL },
	{ "/stop", NULL, NULL, NULL, NULL },
};

static void http_appcall_newdata()
{
	char buf[20];
	char *method;
	char *url;
	unsigned int i = 0;
	bool isget = false;

	strncpy(buf, uip_appdata, ARRAY_SIZE(buf));

	buf[ARRAY_SIZE(buf) - 1] = 0;

	method = strtok(buf, http_seps);
	url = strtok(NULL, http_seps);

	/* Get method */
	if (strcmp(method, "GET") == 0)
	{
		isget = true;
	}
	else if (strcmp(method, "POST") != 0)
	{
		uip_send(http_resp_notimp, ARRAY_SIZE(http_resp_notimp));
		return;
	}

	/* Select file */
	for (i = 0; i < ARRAY_SIZE(http_files); i++)
	{
		if (strcmp(url, http_files[i].path) == 0)
		{
			if (isget && http_files[i].get_handler)
				http_files[i].get_handler(&http_files[i]);
			else if (http_files[i].post_handler)
				http_files[i].post_handler(&http_files[i]);
			else
				uip_send(http_resp_notimp, ARRAY_SIZE(http_resp_notimp));
			return;
		}
	}

	/* Not found */
	uip_send(http_resp_redir, ARRAY_SIZE(http_resp_redir));
}

void http_appcall()
{
	int i;

	if (uip_newdata())
	{
		http_appcall_newdata();
	}
	else if (uip_acked() || uip_poll())
	{
		/* If we still have data to send, continue, if not, then close */
		if (uip_conn->appstate.remaining == 0)
		{
			uip_close();
			return;
		}
		i = uip_conn->appstate.remaining > uip_mss()
			? uip_mss() : uip_conn->appstate.remaining;
		uip_send(uip_conn->appstate.buff, i);
		uip_conn->appstate.remaining -= i;
		uip_conn->appstate.buff += i;
	}
}
