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
#include "tbfs_peer.h"
#include "tbfs_peer_client.h"

/*{{{ struct */
struct _Peer {
    PeerMng *mng;
    gchar peer_id[PEER_ID_LENGTH + 1];
    PeerClient *client;
    PeerType type;

    struct sockaddr_in sin;

    gboolean request_sent;
};

#define PEER_LOG "peer"

/*}}}*/

/*{{{ create / destroy */
Peer *tbfs_peer_create (PeerMng *mng, const gchar *peer_id, guint32 addr, guint16 port)
{
    Peer *peer;

    peer = g_new0 (Peer, 1);
    peer->mng = mng;
    strncpy (peer->peer_id, peer_id, PEER_ID_LENGTH);

    memset (&peer->sin, 0, sizeof (peer->sin));
    peer->sin.sin_family = AF_INET;
    peer->sin.sin_addr.s_addr = addr;
    peer->sin.sin_port = g_ntohs (port);

    peer->request_sent = FALSE;

    return peer;
}

void tbfs_peer_destroy (Peer *peer)
{
    g_free (peer);
}
/*}}}*/

/*{{{ get / set */
const gchar *tbfs_peer_get_id (Peer *peer)
{
    return peer->peer_id;
}
/*}}}*/

// call from tbfs_peer_mng_peers_updated ()
void tbfs_peer_on_pieces_request_cb (Peer *peer)
{
    // peer is busy
    if (peer->request_sent)
        return;

    if (!peer->client) {
        peer->client = tbfs_peer_client_create_with_addr (tbfs_peer_mng_get_app (peer->mng), &peer->sin);

        if (peer->client) {
            LOG_msg (PEER_LOG, "Peer is unavailable !");
            return;
        }
    }

    tbfs_peer_client_connect_get_piece (peer->client);
}

/*{{{ on_info_print_cb */
void tbfs_peer_on_info_print_cb (Peer *peer, struct evbuffer *buf, PrintFormat *print_format)
{
    /*
    evbuffer_add_printf (buf, "peer {%s %s:%u}\n", 
        peer->peer_id,
        inet_ntoa (peer->addr4),
        peer->port
    );
    */
}
/*}}}*/
