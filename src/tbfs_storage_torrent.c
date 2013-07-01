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
#include "tbfs_storage_torrent.h"

/*{{{ struct */
struct _StorageTorrent {
    StorageMng *mng;
    Application *app;

    gchar *info_hash;
    gchar *dir_path;

    GHashTable *h_pieces;
};

typedef struct {
    guint32 piece_idx;
    WRange *r_blocks;
    gchar *fname;
    int fd;
} StoragePiece;

#define ST_LOG "storage"

static void tbfs_storage_piece_destroy (StoragePiece *piece);
/*}}}*/

/*{{{ create / destroy */
StorageTorrent *tbfs_storage_torrent_create (StorageMng *mng, const gchar *info_hash)
{
    StorageTorrent *storage;

    storage = g_new0 (StorageTorrent, 1);
    storage->mng = mng;
    storage->app = tbfs_storage_mng_get_app (mng);
    storage->info_hash = g_strdup (info_hash);
    storage->dir_path = g_build_path ("/", 
        conf_get_string (application_get_conf (storage->app), "storage.dir"),
        info_hash, NULL
    );

    // check if dir exists
    if (!dir_exists_and_writable (storage->dir_path)) {
        // try to create a new
        if (g_mkdir (storage->dir_path, 0700) != 0 || !dir_exists_and_writable (storage->dir_path)) {
            LOG_err (ST_LOG, "Failed to create %s dir !", storage->dir_path);
            tbfs_storage_torrent_destroy (storage);
            return NULL;
        }
    }

    storage->h_pieces = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) tbfs_storage_piece_destroy);

    return storage;
}

void tbfs_storage_torrent_destroy (StorageTorrent *storage)
{
    if (storage->h_pieces)
        g_hash_table_destroy (storage->h_pieces);
    if (storage->dir_path)
        g_free (storage->dir_path);
    g_free (storage->info_hash);
    g_free (storage);
}
/*}}}*/

/*{{{ StoragePiece*/
static StoragePiece *tbfs_storage_piece_create (StorageTorrent *storage, guint32 piece_idx)
{
    StoragePiece *piece;

    piece = g_new0 (StoragePiece, 1);
    piece->piece_idx = piece_idx;
    piece->r_blocks = wrange_create ();
    piece->fname = g_strdup_printf ("%s/%u", storage->dir_path, piece_idx);

    piece->fd = open (piece->fname, O_CLOEXEC | O_CREAT | O_NOATIME, S_IRWXU);
    if (piece->fd < 0) {
        LOG_err (ST_LOG, "Failed to open storage file %s (%s) !", piece->fname, strerror (errno));
        tbfs_storage_piece_destroy (piece);
        return NULL;
    }

    return piece;
}

static void tbfs_storage_piece_destroy (StoragePiece *piece)
{
    if (piece->fd >= 0)
        close (piece->fd);
    g_free (piece->fname);
    wrange_destroy (piece->r_blocks);
    g_free (piece);
}

static StoragePiece *tbfs_storage_piece_get (StorageTorrent *storage, guint32 piece_idx)
{
    StoragePiece *piece;

    piece = g_hash_table_lookup (storage->h_pieces, GUINT_TO_POINTER (piece_idx));
    if (!piece) {
        piece = tbfs_storage_piece_create (storage, piece_idx);
        if (!piece)
            return NULL;
        g_hash_table_insert (storage->h_pieces, GUINT_TO_POINTER (piece_idx), piece);
    }

    return piece;
}
/*}}}*/

/*{{{ get / set */
const gchar *tbfs_storage_torrent_get_info_hash (StorageTorrent *storage)
{
    return storage->info_hash;
}
/*}}}*/

/*{{{ piece_read_block_buf */
struct evbuffer *tbfs_storage_torrent_piece_read_block_buf (StorageTorrent *storage, guint32 piece_idx, guint32 offset, guint32 length)
{
    struct evbuffer *out_buf;
    struct evbuffer_file_segment *seg;
    StoragePiece *piece;
    
    piece = tbfs_storage_piece_get (storage, piece_idx);
    if (!piece)
        return NULL;

    seg = evbuffer_file_segment_new (piece->fd, offset, length, EVBUF_FS_DISABLE_SENDFILE);
    if (!seg) {
        LOG_err (ST_LOG, "Failed to read data of %u bytes from file %s !", length, piece->fname);
		return NULL;
    }

    out_buf = evbuffer_new ();
    if (evbuffer_add_file_segment (out_buf, seg, 0, length) < 0) {
        LOG_err (ST_LOG, "Failed to read data of %u bytes from file %s !", length, piece->fname);
        if (seg)
            evbuffer_file_segment_free(seg);
        evbuffer_free (out_buf);
		return NULL;
    }

    evbuffer_file_segment_free (seg);

    return out_buf;
}
/*}}}*/

/*{{{ piece_write_block_buf */
void tbfs_storage_torrent_piece_write_block_buf (StorageTorrent *storage, guint32 piece_idx, guint32 offset, struct evbuffer *in_buf)
{
    StoragePiece *piece;
    ssize_t len;
    ssize_t wbytes;

    piece = tbfs_storage_piece_get (storage, piece_idx);
    if (!piece)
        return;

    len = evbuffer_get_length (in_buf);

    wbytes = pwrite (piece->fd, evbuffer_pullup (in_buf, len), len, offset);

    if (wbytes != len) {
        LOG_err (ST_LOG, "Failed to write data of %u bytes to file %s !", len, piece->fname);
        return;
    }

    wrange_add (piece->r_blocks, offset, offset + len);
}
/*}}}*/
