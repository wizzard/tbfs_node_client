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
#ifndef _TBFS_TORRENT_H_
#define _TBFS_TORRENT_H_

#include "global.h"
#include "tbfs_peer.h"
#include "tbfs_bitfield.h"


Torrent *tbfs_torrent_create (Application *app, const gchar *info_hash, guint32 total_pieces);
void tbfs_torrent_destroy (Torrent *torrent);

//void torrent_add_peer (Torrent *torrent, Peer *peer);
//void torrent_remove_peer (Torrent *torrent, Peer *peer);
void tbfs_torrent_add_peer_addr (Torrent *torrent, const gchar *peer_id, guint32 addr, guint16 port);
void tbfs_torrent_add_piece (Torrent *torrent, guint32 piece_id);

Bitfield *tbfs_torrent_get_bitfield_pieces_have (Torrent *torrent);
Bitfield *tbfs_torrent_get_bitfield_pieces_want (Torrent *torrent);
void tbfs_torrent_peers_updated (Torrent *torrent);

const gchar *tbfs_torrent_get_info_hash (Torrent *torrent);
void tbfs_torrent_info_print (Torrent *torrent, struct evbuffer *buf, PrintFormat *print_format);
#endif
