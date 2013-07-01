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
#include "wutils.h"
#include <string.h>

gchar *get_random_string (size_t len, gboolean readable)
{
    gchar *out;

    out = g_malloc (len + 1);

    if (readable) {
        gchar readable_chars[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        size_t i;

        for (i = 0; i < len; i++)
            out[i] = readable_chars[rand() % strlen (readable_chars)];
    } else {
        if (!RAND_pseudo_bytes ((unsigned char *)out, len))
            return out;
    }

    *(out + len) = '\0';

    return out;
}

gboolean get_md5_sum (const gchar *buf, size_t len, gchar **md5str, gchar **md5b)
{
    unsigned char digest[16];
    size_t i;
    gchar *out;

    if (!md5b && !md5str)
        return TRUE;

    MD5 ((const unsigned char *)buf, len, digest);

  
    if (md5b)
        *md5b = get_base64 ((const gchar *)digest, 16);
    if (md5str) {
        out = g_malloc (33);
        for (i = 0; i < 16; ++i)
            sprintf(&out[i*2], "%02x", (unsigned int)digest[i]);
        *md5str = out;
    }
    return TRUE;
}

gchar *get_base64 (const gchar *buf, size_t len)
{
    int ret;
    gchar *res;
    BIO *bmem, *b64;
    BUF_MEM *bptr;
    
    b64 = BIO_new (BIO_f_base64 ());
    bmem = BIO_new (BIO_s_mem ());
    b64 = BIO_push (b64, bmem);
    BIO_write (b64, buf, len);
    ret = BIO_flush (b64);
    if (ret != 1) {
        BIO_free_all (b64);
        return NULL;
    }
    BIO_get_mem_ptr (b64, &bptr);
    res = g_malloc (bptr->length);
    memcpy (res, bptr->data, bptr->length);
    res[bptr->length - 1] = '\0';
    BIO_free_all (b64);

    return res;
}

// removes leading and trailing double quotes from str
gchar *str_remove_quotes (gchar *str)
{
    gchar *start;
    size_t len;

    for (start = str; *start && *start == 0x22; start++);
    g_memmove (str, start, strlen (start) + 1);

    len = strlen (str);
    while (len--) {
        if (str[len] == 0x22)
            str[len] = '\0';
        else
            break;
    }

    return str;
}

// 2 * SHA_DIGEST_LENGTH + 1
void sha1_to_hexstr (gchar *out, const uint8_t *sha1)
{
    int i;
    static const char hex[] = "0123456789abcdef";

    for (i=0; i<20; ++i) {
        const unsigned int val = *sha1++;
        *out++ = hex[val >> 4];
        *out++ = hex[val & 0xf];
    }

    *out = '\0';
}

void hexstr_to_sha1 (uint8_t *out, const char *in)
{
    int i;
    static const char hex[] = "0123456789abcdef";

    for (i=0; i<20; ++i) {
        const int hi = strchr (hex, g_ascii_tolower (*in++)) - hex;
        const int lo = strchr (hex, g_ascii_tolower (*in++)) - hex;
        *out++ = (uint8_t)((hi<<4) | lo);
    }
}

void escape_sha1 (char *out, const uint8_t *sha1)
{
    const uint8_t * in = sha1;
    const uint8_t * end = in + SHA_DIGEST_LENGTH;

    while (in != end)
        if (g_ascii_isalnum (*in))
            *out++ = (char) *in++;
        else
            out += g_snprintf (out, 4, "%%%02x", (unsigned int)*in++);

    *out = '\0';
}

