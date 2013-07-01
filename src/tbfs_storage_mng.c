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
#include "tbfs_storage_mng.h"

/*{{{ structs*/
struct _StorageMng {
    Application *app;
};

#define PCLI_LOG "pcli"
/*}}}*/

/*{{{ create / destroy */
StorageMng *tbfs_storage_mng_create (Application *app)
{
    StorageMng *mng;

    mng = g_new0 (StorageMng, 1);
    mng->app = app;

    return mng;
}

void tbfs_storage_mng_destroy (StorageMng *mng)
{
    g_free (mng);
}
/*}}}*/


void tbfs_storage_add_buf (StorageMng *mng, const gchar *info_hash, guint32 piece_idx, guint32 offset, struct evbuffer *in_buf)
{
}

void tbfs_storage_get_buf (StorageMng *mng, const gchar *info_hash, guint32 piece_idx, guint32 offset)
{
    struct evbuffer *out_buf;

    out_buf = evbuffer_new ();
    return out_buf;
}
