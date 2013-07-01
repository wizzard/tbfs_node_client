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
#ifndef _TBFS_PEER_H_
#define _TBFS_PEER_H_

#include "global.h"
#include "tbfs_peer_mng.h"

typedef struct _Peer Peer;

Peer *tbfs_peer_create (PeerMng *mng, const gchar *peer_id, guint32 addr, guint16 port);
void tbfs_peer_destroy (Peer *peer);

const gchar *tbfs_peer_get_id (Peer *peer);

void tbfs_peer_on_pieces_request_cb (Peer *peer);
void tbfs_peer_on_info_print_cb (Peer *peer, struct evbuffer *buf, PrintFormat *print_format);

#endif
