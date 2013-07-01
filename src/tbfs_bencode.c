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
#include "tbfs_bencode.h"

/*{{{ structs*/
struct _BValue {
    BType type;
    gint32 strlen;

    union {
        gchar *str;
        gint64 integer;
        GList *list;
        GHashTable *h_dict;
    } value;
};

#define B_LOG "benc"
/*}}}*/

/*{{{ declaration */
static gchar *bvalue_parse_string (struct evbuffer *in, gint32 *len);
static gint64 bvalue_parse_int (struct evbuffer *in);
static GList *bvalue_parse_list (struct evbuffer *in);
static GHashTable *bvalue_parse_dict (struct evbuffer *in);
/*}}}*/

/*{{{ create / destroy*/
BValue *bvalue_create_from_buff (struct evbuffer *in)
{
    BValue *bval = NULL;
    size_t len;
    gchar c;


    len = evbuffer_get_length (in);

    if (!len) {
        return NULL;
    }

    evbuffer_copyout (in, &c, 1);

    if (g_ascii_isdigit (c)) {
        gchar *str;
        gint32 strlen;
        str = bvalue_parse_string (in, &strlen);
        
        bval = g_new0 (BValue, 1);
        bval->type = BT_STRING;
        bval->value.str = str;
        bval->strlen = strlen;
    } else if (c == 'i') {
        gint64 i;
        i = bvalue_parse_int (in);

        bval = g_new0 (BValue, 1);
        bval->type = BT_INT;
        bval->value.integer = i;
    } else if (c == 'l') {
        GList *l;

        l = bvalue_parse_list (in);

        bval = g_new0 (BValue, 1);
        bval->type = BT_LIST;
        bval->value.list = l;
    } else if (c == 'd') {
        GHashTable *h;

        h = bvalue_parse_dict (in);

        bval = g_new0 (BValue, 1);
        bval->type = BT_DICT;
        bval->value.h_dict = h;

    } else {
        bval = NULL;
    }

    return bval;
}

void bvalue_destroy (BValue *bval)
{
    switch (bval->type) {
        case BT_INT:
            break;
        case BT_STRING:
            g_free (bval->value.str);
            break;
        case BT_LIST: {
            GList *l;
            for (l = g_list_first (bval->value.list); l; l = g_list_next (l)) {
                BValue *tmp = (BValue *) l->data;
                bvalue_destroy (tmp);
            }
            g_list_free (bval->value.list);
            break;
        }
        case BT_DICT:
            g_hash_table_destroy (bval->value.h_dict);
            break;
        default:
            break;
    }

    g_free (bval);
}
/*}}}*/

/*{{{ types */
BType bvalue_get_type (BValue *bval)
{
    return bval->type;
}

gboolean bvalue_is_string (BValue *bval)
{
    return (bval->type == BT_STRING);
}

gboolean bvalue_is_int (BValue *bval)
{
    return (bval->type == BT_INT);
}

gboolean bvalue_is_list (BValue *bval)
{
    return (bval->type == BT_LIST);
}

gboolean bvalue_is_dict (BValue *bval)
{
    return (bval->type == BT_DICT);
}

/*}}}*/

/*{{{ string */
static gchar *bvalue_parse_string (struct evbuffer *in, gint32 *len)
{
    gchar *str;
    gchar *clen;
    struct evbuffer_ptr pos;
    gchar tmp[1];

    pos = evbuffer_search (in, ":", 1, NULL);
    clen = g_new0 (gchar, pos.pos + 1);
    evbuffer_remove (in, clen, pos.pos);
    clen[pos.pos] = '\0';

    *len = atoi (clen);
    g_free (clen);
    
    // remove ":"
    evbuffer_remove (in, tmp, 1);

    str = g_new0 (gchar, *len + 1);
    evbuffer_remove (in, str, *len);
    str[*len] = '\0';

    return str;
}

// it could be binary !
gchar *bvalue_get_string (BValue *bval)
{
    return bval->value.str;
}

guint8 *bvalue_get_binary_string (BValue *bval, gint32 *len)
{
    *len = bval->strlen;
    return (guint8 *)bval->value.str;
}

/*}}}*/

/*{{{ int*/
static gint64 bvalue_parse_int (struct evbuffer *in)
{
    gchar tmp[1];
    struct evbuffer_ptr pos;
    gchar *str;
    guint64 i;
    
    // remove "i"
    evbuffer_remove (in, tmp, 1);

    pos = evbuffer_search (in, "e", 1, NULL);
    str = g_new0 (gchar, pos.pos + 1);

    evbuffer_remove (in, str, pos.pos);
    str[pos.pos] = '\0';

    i = atoll (str);
    g_free (str);

    // remove "e"
    evbuffer_remove (in, tmp, 1);
    
    return i;
}

gint64 bvalue_get_int (BValue *bval)
{
    return bval->value.integer;
}

/*}}}*/

/*{{{ list*/
static GList *bvalue_parse_list (struct evbuffer *in)
{
    gchar tmp[1];
    gchar c;
    BValue *value;
    GList *l = NULL;
    GList *l_tmp;
    size_t len;
    
    // remove "l"
    evbuffer_remove (in, tmp, 1);

    evbuffer_copyout (in, &c, 1);

    len = evbuffer_get_length (in);

    while (c != 'e' && len > 1) {
        value = bvalue_create_from_buff (in);
        
        if (!value) {
            // free already created list
            for (l_tmp = g_list_first (l); l_tmp; l_tmp = g_list_next (l_tmp)) {
                BValue *val = (BValue *) l_tmp->data;
                bvalue_destroy (val);
            }
            g_list_free (l);
            return NULL;
        }

        l = g_list_append (l, value);
        len = evbuffer_get_length (in);
        evbuffer_copyout (in, &c, 1);
    }
    
    // remove "e"
    evbuffer_remove (in, tmp, 1);
    
    return l;
}

GList *bvalue_get_list (BValue *bval)
{
    if (!bvalue_is_list (bval)) {
        LOG_err (B_LOG, "BValue is not a list !");
        return NULL;
    }

    return bval->value.list;
}
/*}}}*/

/*{{{ dict */

static void bvalue_dict_item_free (gpointer data)
{
    BValue *value = (BValue *) data;
    bvalue_destroy (value);
}

static GHashTable *bvalue_parse_dict (struct evbuffer *in)
{
    GHashTable *h_dict = NULL;
    gchar tmp[1];
    gchar c;
    BValue *key;
    gchar *key_str;
    BValue *value;
    size_t len;
    
    // remove "d"
    evbuffer_remove (in, tmp, 1);

    evbuffer_copyout (in, &c, 1);

    len = evbuffer_get_length (in);

    h_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, bvalue_dict_item_free);

    while (c != 'e' && len > 1) {
        key = bvalue_create_from_buff (in);
        if (!bvalue_is_string (key))  {
            LOG_err (B_LOG, "Failed parsing Bencode Dict !");
            //XXX
            return NULL;
        }
        key_str = bvalue_get_string (key);
        if (!key_str) {
            LOG_err (B_LOG, "Failed parsing Bencode Dict !");
            //XXX
            return NULL;
        }

        value = bvalue_create_from_buff (in);

        g_hash_table_insert (h_dict, g_strdup (key_str), value);

        bvalue_destroy (key);


        len = evbuffer_get_length (in);
        evbuffer_copyout (in, &c, 1);
    }
    // remove "e"
    evbuffer_remove (in, tmp, 1);
    
    return h_dict;
}


BValue *bvalue_dict_get_value (BValue *bval, const gchar *key)
{
    return (BValue *)g_hash_table_lookup (bval->value.h_dict, key);
}

/*}}}*/

/*{{{ misc*/
typedef struct {
    GString *str;
    int tabs;
} DictHelper;

static void bvalue_get_dict_string (gpointer key, gpointer value, gpointer data)
{
    gchar *tab_s;
    DictHelper *hlp = (DictHelper *) data;

    if (hlp->tabs) {
        int i;
        tab_s = g_new0 (gchar, hlp->tabs + 1);
        for (i = 0; i < hlp->tabs; i++) 
            tab_s[i] = '\t';
        tab_s[hlp->tabs] = '\0';
    } else {
        tab_s = g_strdup ("");
    }

    g_string_append_printf (hlp->str, "%s%s", tab_s, (gchar *)key);
    g_free (tab_s);

    bvalue_print_string ((BValue *)value, hlp->str, hlp->tabs);
}

void bvalue_print_string (BValue *bval, GString *str, int tabs)
{
    GList *l;
    gchar *tab_s;

    if (tabs) {
        int i;
        tab_s = g_new0 (gchar, tabs + 1);
        for (i = 0; i < tabs; i++) 
            tab_s[i] = '\t';
        tab_s[tabs] = '\0';
    } else {
        tab_s = g_strdup ("");
    }

    switch (bval->type) {
        case BT_STRING:
            g_string_append_printf (str, "%sSTR: %s\n", tab_s, bval->value.str);
            break;
        case BT_INT:
            g_string_append_printf (str, "%sINT: %ld\n", tab_s, bval->value.integer);
            break;
        case BT_LIST:
            g_string_append_printf (str, "%sLIST: %d\n", tab_s, g_list_length (bval->value.list));
            tabs++;
            for (l = g_list_first (bval->value.list); l; l = g_list_next (l)) {
                BValue *tmp = (BValue *) l->data;
                bvalue_print_string (tmp, str, tabs);
            }
            break;
        case BT_DICT: {
            DictHelper *hlp;

            g_string_append_printf (str, "%sDICT: %d\n", tab_s, g_hash_table_size (bval->value.h_dict));
            
            hlp = g_new0 (DictHelper, 1);
            hlp->str = str;
            hlp->tabs = tabs + 1;
            g_hash_table_foreach (bval->value.h_dict, bvalue_get_dict_string, hlp);
            g_free (hlp);
            break;
        }
        default:
            break;
    }
    
    g_free (tab_s);
}
/*}}}*/
