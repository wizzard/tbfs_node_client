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
#include "tbfs_torrent.h"
#include "tbfs_peer_mng.h"

/*{{{ structs */
struct _Torrent {
    Application *app;
    gchar *info_hash;

    guint32 total_pieces;
    Bitfield *bf_pieces_want;
    Bitfield *bf_pieces_have;

    PeerMng *pmng;
};
/*}}}*/

/*{{{ create / destroy */
Torrent *tbfs_torrent_create (Application *app, const gchar *info_hash, guint32 total_pieces)
{
    Torrent *torrent;

    torrent = g_new0 (Torrent, 1);
    torrent->app = app;
    torrent->info_hash = g_strdup (info_hash);
    torrent->bf_pieces_want = tbfs_bitfield_create (total_pieces);
    torrent->bf_pieces_have = tbfs_bitfield_create (total_pieces);

    torrent->pmng = tbfs_peer_mng_create (app, torrent);

    return torrent;
}

void tbfs_torrent_destroy (Torrent *torrent)
{
    tbfs_peer_mng_destroy (torrent->pmng);
    tbfs_bitfield_destroy (torrent->bf_pieces_want);
    tbfs_bitfield_destroy (torrent->bf_pieces_have);
    g_free (torrent->info_hash);
    g_free (torrent);
}
/*}}}*/

const gchar *tbfs_torrent_get_info_hash (Torrent *torrent)
{
    return torrent->info_hash;
}

void tbfs_torrent_add_piece (Torrent *torrent, guint32 piece_id)
{
    tbfs_bitfield_set_bit (torrent->bf_pieces_want, piece_id);

    // notify peer manager that a new piece is added
    // XXX: check that his is a new ?
    tbfs_peer_mng_torrent_piece_added (torrent->pmng, piece_id);
}


Bitfield *tbfs_torrent_get_bitfield_pieces_have (Torrent *torrent)
{
    return torrent->bf_pieces_have;
}

Bitfield *tbfs_torrent_get_bitfield_pieces_want (Torrent *torrent)
{
    return torrent->bf_pieces_want;
}


void tbfs_torrent_add_peer_addr (Torrent *torrent, const gchar *peer_id, guint32 addr, guint16 port)
{
    tbfs_peer_mng_peer_add (torrent->pmng, peer_id, addr, port);
}

void tbfs_torrent_peers_updated (Torrent *torrent)
{
    tbfs_peer_mng_peers_updated (torrent->pmng);
}

void tbfs_torrent_info_print (Torrent *torrent, struct evbuffer *buf, PrintFormat *print_format)
{
    evbuffer_add_printf (buf, "info_hash: %s\n", torrent->info_hash);
    evbuffer_add_printf (buf, "peers: %d\n", tbfs_peer_mng_peer_count (torrent->pmng));
    tbfs_peer_mng_info_print (torrent->pmng, buf, print_format);
}
