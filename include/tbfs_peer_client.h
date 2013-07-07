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
#ifndef _TBFS_PEER_CLIENT_H_
#define _TBFS_PEER_CLIENT_H_

#include "global.h"
#include "tbfs_peer_server.h"

typedef struct _PeerClient PeerClient;

PeerClient *tbfs_peer_client_create (Application *app, evutil_socket_t fd);
PeerClient *tbfs_peer_client_create_with_addr (Application *app, struct sockaddr_in *sin);
void tbfs_peer_client_destroy (PeerClient *client);

void tbfs_peer_client_set_peer (PeerClient *client, Peer *peer);

void tbfs_peer_client_connect_get_piece (PeerClient *client);

#endif

