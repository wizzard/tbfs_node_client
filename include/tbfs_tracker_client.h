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
#ifndef _TBFS_TRACKER_CLIENT_H_
#define _TBFS_TRACKER_CLIENT_H_

#include "global.h"

typedef enum {
    TE_started = 0,
    TE_stopped = 1,
    TE_completed = 2,
} TrackerEvent;

typedef struct {
    guint32 addr4;
    guint16 port;
} PeerAddr;

typedef void (*TrackerClient_on_request_done_cb) (gboolean status, gpointer ctx, gchar *info_hash, GList *l_peer_addrs);

TrackerClient *tbfs_tracker_client_create (Application *app);
void tbfs_tracker_client_destroy (TrackerClient *client);

void tbfs_tracker_client_send_request (TrackerClient *client, const gchar *info_hash, TrackerEvent event_type, 
    TrackerClient_on_request_done_cb on_request_done_cb, gpointer ctx);
#endif
