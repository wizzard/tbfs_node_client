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
#include "tbfs_peer_client.h"
#include "tbfs_mng.h"
#include "tbfs_torrent.h"
#include "tbfs_bitfield.h"

/*{{{ structs */
typedef enum {
    PCS_Connecting = 0,
    PCS_ReadingHandshake = 1,
    PCS_ReadingPeerID = 2,
    PCS_Ready = 3,
    PCS_Piece = 4,
} PeerClientState;

typedef enum {
    PCRS_NewPacket = 0,
    PCRS_Continue = 1,
} PeerClientReadingState;

struct _PeerClient {
    Application *app;
    Peer *peer;

    struct bufferevent *bev;

    PeerClientState state;
    PeerClientReadingState read_state;
    
    gchar hs_info_hash[2 * SHA_DIGEST_LENGTH + 1];
    gchar hs_peer_id[PEER_ID_LENGTH + 1];
};

// Peer wire protocol
typedef enum {
    PMT_Choke = 0,
    PMT_Unchoke = 1,
    PMT_Interested = 2,
    PMT_Request = 6,

    PMT_KeepAlive = 80,
    PMT_Error,
} PeerMsgType;

#define PCLI_LOG "pcli"
static void tbfs_peer_client_on_write_cb (struct bufferevent *bev, void *ctx);
static void tbfs_peer_client_on_read_cb (struct bufferevent *bev, void *ctx);
static void tbfs_peer_client_on_event_cb (struct bufferevent *bev, short what, void *ctx);
/*}}}*/

/*{{{ create / destroy */
PeerClient *tbfs_peer_client_create (Application *app, evutil_socket_t fd)
{
    PeerClient *client;

    client = g_new0 (PeerClient, 1);
    client->app = app;
    client->peer = NULL;
    client->state = PCS_Connecting;
    client->read_state = PCRS_NewPacket;

    client->bev = bufferevent_socket_new (application_get_evbase (app), 
        fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_UNLOCK_CALLBACKS);

    if (!client->bev) {
        LOG_err (PCLI_LOG, "Failed to create PeerClient bufferevent !");
        tbfs_peer_client_destroy (client);
        return NULL;
    }

    bufferevent_setcb (client->bev,
        tbfs_peer_client_on_read_cb,
        tbfs_peer_client_on_write_cb,
        tbfs_peer_client_on_event_cb,
        client
    );

    bufferevent_enable (client->bev, EV_READ|EV_WRITE);

    LOG_debug (PCLI_LOG, "[pc: %p] PeerClient created !", client);

    if (fd > 0)
        client->state = PCS_ReadingHandshake;


    return client;
}

PeerClient *tbfs_peer_client_create_with_addr (Application *app, struct sockaddr_in *sin)
{
    PeerClient *client;

    client = tbfs_peer_client_create (app, -1);
    if (!client) {
        return NULL;
    }

    LOG_debug (PCLI_LOG, "[pc: %p] PeerClient connecting to %s:%d", client, inet_ntoa (sin->sin_addr), g_ntohs (sin->sin_port));
    if (bufferevent_socket_connect (client->bev,
        (struct sockaddr *)sin, sizeof (struct sockaddr_in)
        ) < 0) 
    {
        LOG_err (PCLI_LOG, "[pc: %p] Failed to create PeerClient bufferevent !", client);
        tbfs_peer_client_destroy (client);
        return NULL;
    }

    return client;
}

void tbfs_peer_client_destroy (PeerClient *client)
{

    LOG_debug (PCLI_LOG, "[pc: %p] PeerClient destroying !", client);
    if (client->bev)
        bufferevent_free (client->bev);
    g_free (client);
}
/*}}}*/

void tbfs_peer_client_set_peer (PeerClient *client, Peer *peer)
{
    client->peer = peer;
    strncpy (client->hs_info_hash, tbfs_peer_get_info_hash (peer), 2 * SHA_DIGEST_LENGTH);
}

/*{{{ Peer wire protocol */

/*{{{ parsers */
static gboolean tbfs_peer_client_handshake_parse (PeerClient *client, struct evbuffer *inbuf)
{
    ssize_t pstrlen = 0;
    gchar *pstr;
    guint8 reserved[8] = {0};
    uint8_t sha1[SHA_DIGEST_LENGTH];

    if (evbuffer_remove (inbuf, &pstrlen, 1) != 1) {
        LOG_err (PCLI_LOG, "Failed to read handshake !");
        return FALSE;
    }

    pstr = g_new0 (gchar, pstrlen);
    if (evbuffer_remove (inbuf, pstr, pstrlen) != pstrlen) {
        LOG_err (PCLI_LOG, "Failed to read handshake !");
        g_free (pstr);
        return FALSE;
    }
    g_free (pstr);

    if (evbuffer_remove (inbuf, reserved, 8) != 8) {
        LOG_err (PCLI_LOG, "Failed to read handshake !");
        return FALSE;
    }
    
    // info_hash
    if (evbuffer_remove (inbuf, sha1, SHA_DIGEST_LENGTH) != SHA_DIGEST_LENGTH) {
        LOG_err (PCLI_LOG, "Failed to read InfoHash handshake !");
        return FALSE;
    }
    sha1_to_hexstr (client->hs_info_hash, sha1);

    return TRUE;
}

static gboolean tbfs_peer_client_handshake_peerid_parse (PeerClient *client, struct evbuffer *inbuf)
{
    // peer_id
    if (evbuffer_remove (inbuf, client->hs_peer_id, PEER_ID_LENGTH) != PEER_ID_LENGTH) {
        LOG_err (PCLI_LOG, "Failed to read PeerID in handshake !");
        return FALSE;
    }

    return TRUE;
}

static gboolean tbfs_peer_client_request_parse (PeerClient *client, struct evbuffer *inbuf,
    guint32 *idx, guint32 *begin, guint32 *len)
{
    if (evbuffer_remove (inbuf, idx, 4) != 4) {
        LOG_err (PCLI_LOG, "Failed to read Request pecket !");
        return FALSE;
    }
    if (evbuffer_remove (inbuf, begin, 4) != 4) {
        LOG_err (PCLI_LOG, "Failed to read Request pecket !");
        return FALSE;
    }
    if (evbuffer_remove (inbuf, len, 4) != 4) {
        LOG_err (PCLI_LOG, "Failed to read Request pecket !");
        return FALSE;
    }

    return TRUE;
}
/*}}}*/

/*{{{ encoders */
static PeerMsgType tbfs_peer_client_msg_typelen_get (struct evbuffer *inbuf, guint32 *msg_len)
{
    guint8 type;
    guint32 n_msg_len;

    if (evbuffer_remove (inbuf, &n_msg_len, 4) != 4) {
        LOG_err (PCLI_LOG, "Failed to read Peer packet !");
        return PMT_Error;
    }
    *msg_len = g_ntohl (n_msg_len);

    if (*msg_len == 0)
        return PMT_KeepAlive;

    if (evbuffer_remove (inbuf, &type, 1) != 1) {
        LOG_err (PCLI_LOG, "Failed to read Peer packet !");
        return PMT_Error;
    }

    return type;
}

static struct evbuffer *tbfs_peer_client_handshake_pkg_create (PeerClient *client)
{
    struct evbuffer *outbuf;
    const gchar pstr[] = "BitTorrent protocol";
    size_t pstrlen;
    guint8 reserved[8] = {0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    uint8_t sha1[SHA_DIGEST_LENGTH];
    Torrent *torrent;

    pstrlen = strlen (pstr);

    torrent = tbfs_mng_torrent_get (application_get_mng (client->app), client->hs_info_hash);
    if (!torrent) {
        LOG_err (PCLI_LOG, "[pc: %p] Cant find Torrent, info_hash: %s !", client, client->hs_info_hash);
        return NULL;
    }

    hexstr_to_sha1 (sha1, client->hs_info_hash);

    outbuf = evbuffer_new ();
    evbuffer_add (outbuf, &pstrlen, 1);
    evbuffer_add (outbuf, pstr, pstrlen);
    evbuffer_add (outbuf, reserved, 8);
    evbuffer_add (outbuf, sha1, SHA_DIGEST_LENGTH);
    // self peerid
    evbuffer_add (outbuf, 
        conf_get_string (application_get_conf (client->app), "peer.peer_id"),
        SHA_DIGEST_LENGTH
    );

    return outbuf;
}

static struct evbuffer *tbfs_peer_client_choke_pkg_create (PeerClient *client)
{
    struct evbuffer *outbuf;
    guint32 len = 1;
    guint32 n_len;
    guint8 msg = 0;

    n_len = g_htonl (len);

    outbuf = evbuffer_new ();
    evbuffer_add (outbuf, &n_len, 4);
    evbuffer_add (outbuf, &msg, 1);
    return outbuf;
}

static struct evbuffer *tbfs_peer_client_unchoke_pkg_create (PeerClient *client)
{
    struct evbuffer *outbuf;
    guint32 len = 1;
    guint32 n_len;
    guint8 msg = 1;

    n_len = g_htonl (len);

    outbuf = evbuffer_new ();
    evbuffer_add (outbuf, &n_len, 4);
    evbuffer_add (outbuf, &msg, 1);
    return outbuf;
}

static struct evbuffer *tbfs_peer_client_interested_pkg_create (PeerClient *client)
{
    struct evbuffer *outbuf;
    guint32 len = 1;
    guint32 n_len;
    guint8 msg = 2;

    n_len = g_htonl (len);

    outbuf = evbuffer_new ();
    evbuffer_add (outbuf, &n_len, 4);
    evbuffer_add (outbuf, &msg, 1);
    return outbuf;
}

static struct evbuffer *tbfs_peer_client_request_pkg_create (PeerClient *client, guint32 idx, guint32 begin, guint32 length)
{
    struct evbuffer *outbuf;
    guint32 len = sizeof (uint8_t) + 3 * sizeof (uint32_t);
    guint32 n_len;
    guint8 msg = 6;
    guint32 n_idx, n_begin, n_length;

    n_idx = g_htonl (idx);
    n_begin = g_htonl (begin);
    n_length = g_htonl (length);

    n_len = g_htonl (len);

    outbuf = evbuffer_new ();
    evbuffer_add (outbuf, &n_len, 4);
    evbuffer_add (outbuf, &msg, 1);
    evbuffer_add (outbuf, &n_idx, 4);
    evbuffer_add (outbuf, &n_begin, 4);
    evbuffer_add (outbuf, &n_length, 4);

    return outbuf;
}


static struct evbuffer *tbfs_peer_client_bitfield_pkg_create (PeerClient *client)
{
    struct evbuffer *outbuf;
    guint32 len;
    guint32 n_len;
    guint8 msg = 5;
    Torrent *torrent;
    Bitfield *bf;
    const guint8 *bits;

    torrent = tbfs_mng_torrent_get (application_get_mng (client->app), client->hs_info_hash);
    if (!torrent) {
        LOG_err (PCLI_LOG, "[pc: %p] Cant find Torrent, info_hash: %s !", client, client->hs_info_hash);
        return NULL;
    }

    bf = tbfs_torrent_get_bitfield_pieces_have (torrent);
    if (!bf) {
        LOG_err (PCLI_LOG, "[pc: %p] Cant find Torrent's pieces bitfield !", client);
        return NULL;
    }

    len = tbfs_bitfield_get_length (bf) + 1; // + type
    bits = tbfs_bitfield_get_bits (bf);

    n_len = g_htonl (len);

    outbuf = evbuffer_new ();
    evbuffer_add (outbuf, &n_len, 4);
    evbuffer_add (outbuf, &msg, 1);
    evbuffer_add (outbuf, bits, len - 1); // - type
    return outbuf;
}
/*}}}*/
/*}}}*/

/*{{{ on_read_cb / on_write_cb / on_event_cb */

/*{{{ on_read_cb */
static void tbfs_peer_client_on_read_cb (struct bufferevent *bev, void *ctx)
{
    PeerClient *client = (PeerClient *) ctx;
    struct evbuffer *inbuf;
    size_t inlen;

    inbuf = bufferevent_get_input (bev);
    inlen = evbuffer_get_length (inbuf);

    LOG_debug (PCLI_LOG, "[pc: %p] Incoming data: %zd", client, evbuffer_get_length (inbuf));

    // part of data received
    if (client->read_state == PCRS_Continue) {

    // a new packet received
    } else {

    if (client->state == PCS_ReadingHandshake && inlen) {
        struct evbuffer *outbuf = NULL;
        Torrent *torrent;

        if (!tbfs_peer_client_handshake_parse (client, inbuf)) {
            LOG_err (PCLI_LOG, "[pc: %p] Failed to parse handshake !", client);
            tbfs_peer_client_destroy (client);
            return;
        }
        LOG_debug (PCLI_LOG, "[pc: %p] Handshake is parsed !", client);

        // check if Torrent exists
        torrent = tbfs_mng_torrent_get (application_get_mng (client->app), client->hs_info_hash);
        if (!torrent) {
            LOG_msg (PCLI_LOG, "[pc: %p] Torrent %s does not exist !", client, client->hs_info_hash);
            tbfs_peer_client_destroy (client);
            return;
        }
        
        client->state = PCS_ReadingPeerID;

        // leecher
        if (client->peer) {
        } else {
            outbuf = tbfs_peer_client_handshake_pkg_create (client);
            bufferevent_write_buffer (bev, outbuf);
            LOG_debug (PCLI_LOG, "[pc: %p] Handshake package is sent !", client);
            evbuffer_free (outbuf);
        }

        inlen = evbuffer_get_length (inbuf);
    } 
    
    if (client->state == PCS_ReadingPeerID && inlen) {
        struct evbuffer *outbuf = NULL;
        const gchar *self_id;

        if (!tbfs_peer_client_handshake_peerid_parse (client, inbuf)) {
            LOG_err (PCLI_LOG, "[pc: %p] Failed to parse PeerID !", client);
            tbfs_peer_client_destroy (client);
            return;
        }
        LOG_debug (PCLI_LOG, "[pc: %p] PeerID is parsed !", client);

        // compare with self
        self_id = conf_get_string (application_get_conf (client->app), "peer.peer_id");
        if (!strncmp (self_id, client->hs_peer_id, PEER_ID_LENGTH)) {
            LOG_debug (PCLI_LOG, "[pc: %p] PeerID belongs to us, disconnecting client !", client);
            tbfs_peer_client_destroy (client);
            return;
        }

        client->state = PCS_Ready;

        // leecher
        if (client->peer) {
            outbuf = tbfs_peer_client_interested_pkg_create (client);
            if (!outbuf) {
                LOG_err (PCLI_LOG, "[pc: %p] Failed to create Interested package !", client);
                tbfs_peer_client_destroy (client);
                return;
            }
            bufferevent_write_buffer (bev, outbuf);
            LOG_debug (PCLI_LOG, "[pc: %p] Interested package is sent !", client);
            evbuffer_free (outbuf);

        // seeder
        } else {
            outbuf = tbfs_peer_client_bitfield_pkg_create (client);
            if (!outbuf) {
                LOG_err (PCLI_LOG, "[pc: %p] Failed to create bitfield package !", client);
                tbfs_peer_client_destroy (client);
                return;
            }
            bufferevent_write_buffer (bev, outbuf);
            LOG_debug (PCLI_LOG, "[pc: %p] Bitfield package is sent !", client);
            evbuffer_free (outbuf);
        }

        inlen = evbuffer_get_length (inbuf);
    }

    if (client->state == PCS_Ready && inlen)  {
        PeerMsgType msg_type;
        guint32 msg_len = 0;

        msg_type = tbfs_peer_client_msg_typelen_get (inbuf, &msg_len);
        LOG_debug (PCLI_LOG, "[pc: %p] Got %d type message, len: %u", client, msg_type, msg_len);

        if (msg_type == PMT_Unchoke) {
            struct evbuffer *outbuf = NULL;

            // leecher
            if (client->peer) {
                outbuf = tbfs_peer_client_request_pkg_create (client, 0, 0, 4000);
                bufferevent_write_buffer (bev, outbuf);
                LOG_debug (PCLI_LOG, "[pc: %p] Request package is sent !", client);
                evbuffer_free (outbuf);
                
                client->state = PCS_Piece;


            // seeder
            } else {
                outbuf = tbfs_peer_client_unchoke_pkg_create (client);
                bufferevent_write_buffer (bev, outbuf);
                LOG_debug (PCLI_LOG, "[pc: %p] Unchoke package is sent !", client);
                evbuffer_free (outbuf);
            }
        } else if (msg_type == PMT_Interested) {
        } else if (msg_type == PMT_Request) {
            guint32 idx, begin, len;
            if (!tbfs_peer_client_request_parse (client, inbuf, &idx, &begin, &len)) {
                LOG_err (PCLI_LOG, "[pc: %p] Failed to parse Request package !", client);
                tbfs_peer_client_destroy (client);
                return;
            }
        }


        inlen = evbuffer_get_length (inbuf);
    }

    if (client->state == PCS_Piece && inlen) {
        gchar *data;

        data = evbuffer_pullup (inbuf, -1);

        LOG_debug (PCLI_LOG, "[pc: %p] Incoming Piece data, len: %u", client, inlen);
    }
    
    if (evbuffer_get_length (inbuf) > 0)
        LOG_debug (PCLI_LOG, "[pc: %p] Still left: %zd", client, evbuffer_get_length (inbuf));
    
    } // packet read

    // XXX
    evbuffer_drain (inbuf, -1);
}
/*}}}*/

static void tbfs_peer_client_on_write_cb (struct bufferevent *bev, void *ctx)
{
    PeerClient *client = (PeerClient *) ctx;
    LOG_debug (PCLI_LOG, "[pc: %p] Package is written !", client);
}

/*{{{ on_event_cb */
static void tbfs_peer_client_on_event_cb (struct bufferevent *bev, short what, void *ctx)
{
    PeerClient *client = (PeerClient *) ctx;

    if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        LOG_msg (PCLI_LOG, "[pc: %p] Disconnected !", client);
        tbfs_peer_client_destroy (client);
        return;
    } else if (what & BEV_EVENT_CONNECTED) {
        struct evbuffer *pkg;
        
        LOG_debug (PCLI_LOG, "[pc: %p] Connected to peer, sending HS !", client);
        client->state = PCS_ReadingHandshake;
        pkg = tbfs_peer_client_handshake_pkg_create (client);
        bufferevent_write_buffer (bev, pkg);
        evbuffer_free (pkg);
    }

}
/*}}}*/

/*}}}*/
