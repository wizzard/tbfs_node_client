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
#ifndef _TBFS_BENCODE_H_
#define _TBFS_BENCODE_H_

#include "global.h"

typedef struct _BValue BValue;

typedef enum {
    BT_NONE = 0,
    BT_STRING = 1,
    BT_INT = 2,
    BT_LIST = 3,
    BT_DICT = 4,
} BType;

BValue *bvalue_create_from_buff (struct evbuffer *in);
void bvalue_destroy (BValue *bval);

BType bvalue_get_type (BValue *bval);


// string
gboolean bvalue_is_string (BValue *bval);
gchar *bvalue_get_string (BValue *bval);
guint8 *bvalue_get_binary_string (BValue *bval, gint32 *len);

// int
gboolean bvalue_is_int (BValue *bval);
gint64 bvalue_get_int (BValue *bval);

// list
gboolean bvalue_is_list (BValue *bval);
GList *bvalue_get_list (BValue *bval);

// dict
gboolean bvalue_is_dict (BValue *bval);
BValue *bvalue_dict_get_value (BValue *bval, const gchar *key);


void bvalue_print_string (BValue *bval, GString *str, int tabs);

#endif
