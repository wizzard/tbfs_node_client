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
#include "tbfs_storage_torrent.h"

/*{{{ structs*/
struct _StorageMng {
    Application *app;
    GHashTable *h_storage_torrents;
};

#define SMNG_LOG "smng"
/*}}}*/

/*{{{ create / destroy */
StorageMng *tbfs_storage_mng_create (Application *app)
{
    StorageMng *mng;

    mng = g_new0 (StorageMng, 1);
    mng->app = app;
    mng->h_storage_torrents = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) tbfs_storage_torrent_destroy);

    return mng;
}

void tbfs_storage_mng_destroy (StorageMng *mng)
{
    g_hash_table_destroy (mng->h_storage_torrents);
    g_free (mng);
}
/*}}}*/

Application *tbfs_storage_mng_get_app (StorageMng *mng)
{
    return mng->app;
}

StorageTorrent *tbfs_storage_get_storage_torrent (StorageMng *mng, const gchar *info_hash)
{
    StorageTorrent *storage;

    storage = g_hash_table_lookup (mng->h_storage_torrents, info_hash);
    if (!storage) {
        storage = tbfs_storage_torrent_create (mng, info_hash);
        if (!storage) {
            LOG_err (SMNG_LOG, "Failed to create storage torrent %s", info_hash);
            return NULL;
        }

        g_hash_table_insert (mng->h_storage_torrents, (gpointer)tbfs_storage_torrent_get_info_hash (storage), storage);
    }

    return storage;
}

void tbfs_storage_add_buf (StorageMng *mng, const gchar *info_hash, guint32 piece_idx, guint32 offset, struct evbuffer *in_buf)
{
    StorageTorrent *storage;

    storage = tbfs_storage_get_storage_torrent (mng, info_hash);
    if (!storage) {
        LOG_err (SMNG_LOG, "Failed to get storage torrent %s", info_hash);
        return;
    }

    tbfs_storage_torrent_piece_write_block_buf (storage, piece_idx, offset, in_buf);
}

struct evbuffer *tbfs_storage_get_buf (StorageMng *mng, const gchar *info_hash, guint32 piece_idx, guint32 offset, guint32 length)
{
    StorageTorrent *storage;

    storage = tbfs_storage_get_storage_torrent (mng, info_hash);
    if (!storage) {
        LOG_err (SMNG_LOG, "Failed to get storage torrent %s", info_hash);
        return NULL;
    }

    return tbfs_storage_torrent_piece_read_block_buf (storage, piece_idx, offset, length);
}
