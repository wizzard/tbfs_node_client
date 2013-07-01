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

gboolean uri_is_https (const struct evhttp_uri *uri)
{
    const char *scheme;

    if (!uri)
        return FALSE;

    scheme = evhttp_uri_get_scheme (uri);
    if (!scheme)
        return FALSE;

    if (!strncasecmp ("https", scheme, 5))
        return TRUE;
    
    return FALSE;
}


gint uri_get_port (const struct evhttp_uri *uri)
{
    gint port;

    port = evhttp_uri_get_port (uri);

    // if no port is specified, libevent returns -1
    if (port == -1) {
        if (uri_is_https (uri))
            port = 443;
        else
            port = 80;
    }

    return port;
}

const gchar *http_find_header (const struct evkeyvalq *headers, const gchar *key)
{
    if (!headers || !key)
        return NULL;

    return evhttp_find_header (headers, key);
}
