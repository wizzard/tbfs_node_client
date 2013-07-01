/*
 * Copyright (C) 2012-2013 Paul Ionkin <paul.ionkin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#ifndef _W_UTILS_H_
#define _W_UTILS_H_

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <openssl/engine.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/md5.h>

typedef enum _LogLevel LogLevel;
LogLevel log_level;

enum _LogLevel {
    LOG_err = 0,
    LOG_msg = 1,
    LOG_debug = 2,
};

void logger_log_msg (const gchar *file, gint line, const gchar *func,
        LogLevel level, const gchar *subsystem,
        const gchar *format, ...);

void logger_set_syslog (gboolean use);
void logger_destroy (void);

#define LOG_debug(subsystem, x...) \
G_STMT_START { \
    logger_log_msg (__FILE__, __LINE__, __func__, LOG_debug, subsystem, x); \
} G_STMT_END

#define LOG_msg(subsystem, x...) \
G_STMT_START { \
    logger_log_msg (__FILE__, __LINE__, __func__, LOG_msg, subsystem, x); \
} G_STMT_END

#define LOG_err(subsystem, x...) \
G_STMT_START { \
    logger_log_msg (__FILE__, __LINE__, __func__, LOG_err, subsystem, x); \
} G_STMT_END

// Conf
typedef struct _ConfData ConfData;

ConfData *conf_create ();
void conf_destroy ();

gboolean conf_parse_file (ConfData *conf, const gchar *filename);

const gchar *conf_get_string (ConfData *conf, const gchar *path);
void conf_set_string (ConfData *conf, const gchar *full_path, const gchar *val);

gint32 conf_get_int (ConfData *conf, const gchar *path);
void conf_set_int (ConfData *conf, const gchar *full_path, gint32 val);

guint32 conf_get_uint (ConfData *conf, const gchar *path);
void conf_set_uint (ConfData *conf, const gchar *full_path, guint32 val);

gboolean conf_get_boolean (ConfData *conf, const gchar *path);
void conf_set_boolean (ConfData *conf, const gchar *full_path, gboolean val);

GList *conf_get_list (ConfData *conf, const gchar *path);
void conf_list_set_string (ConfData *conf, const gchar *full_path, const gchar *val);

void conf_print (ConfData *conf);

typedef void (*ConfNodeChangeCB) (const gchar *path, gpointer user_data);
gboolean conf_set_node_change_cb (ConfData *conf, const gchar *path, ConfNodeChangeCB change_cb, gpointer user_data);

// libevent utils
gboolean uri_is_https (const struct evhttp_uri *uri);
gint uri_get_port (const struct evhttp_uri *uri);
const gchar *http_find_header (const struct evkeyvalq *headers, const gchar *key);

// string_utils
gchar *get_random_string (size_t len, gboolean readable);
gboolean get_md5_sum (const gchar *buf, size_t len, gchar **md5str, gchar **md5b);
gchar *get_base64 (const gchar *buf, size_t len);
gchar *str_remove_quotes (gchar *str);

void sha1_to_hexstr (gchar *out, const uint8_t *sha1);
void hexstr_to_sha1 (uint8_t *out, const char *in);
void escape_sha1 (char * out, const uint8_t *sha1);

// file utils
// remove directory tree
int utils_del_tree (const gchar *path);
gboolean dir_exists_and_writable (const gchar *path);

// sys utils
int wutils_daemonize (void);

// range
typedef struct _WRange WRange;
WRange *wrange_create (void);
void wrange_destroy (WRange *range);
void wrange_add (WRange *range, guint64 start, guint64 end);
gboolean wrange_contain (WRange *range, guint64 start, guint64 end);
gint wrange_count (WRange *range);
guint64 wrange_length (WRange *range);
void wrange_print (WRange *range);

// get min / max of integer types
#define type_bits(t) ((t) (sizeof(t) * (CHAR_BIT)))
#define int_max(t) ((t) (~ ((t) (((t) 1) << ((t)type_bits(t) - 1)))))
#define int_min(t) ((t) (( (t) 1) << ((t)type_bits(t) - 1)))

#endif
