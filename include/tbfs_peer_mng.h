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
#ifndef _TBFS_PEER_MNG_H_
#define _TBFS_PEER_MNG_H_

#include "global.h"
#include "tbfs_torrent.h"


PeerMng *tbfs_peer_mng_create (Application *app, Torrent *torrent);
void tbfs_peer_mng_destroy (PeerMng *mng);

Application *tbfs_peer_mng_get_app (PeerMng *mng);

void tbfs_peer_mng_peer_add (PeerMng *mng, const gchar *peer_id, guint32 addr, guint16 port);
void tbfs_peer_mng_peers_updated (PeerMng *mng);
gint tbfs_peer_mng_peer_count (PeerMng *mng);

void tbfs_peer_mng_torrent_piece_added (PeerMng *mng, guint32 piece_id);

void tbfs_peer_mng_info_print (PeerMng *mng, struct evbuffer *buf, PrintFormat *print_format);

#endif
