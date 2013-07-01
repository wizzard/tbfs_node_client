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
#include "tbfs_peer_mng.h"
#include "tbfs_peer.h"
#include "tbfs_bitfield.h"

/*{{{ struct */
struct _PeerMng {
    Application *app;
    Torrent *torrent;

    GHashTable *h_peer_addrs;
    gint peer_count;

    GQueue *q_pieces_wanted;

    struct event *ev_timer;

    time_t peers_last_checked;
    gboolean peers_being_checked;

};

typedef struct {
    GHashTable *h_peers;
} PeerAddr;

typedef void (*peer_func) (Peer *peer, gpointer data1, gpointer data2);

#define PMNG_LOG "pmng"

static void tbfs_peer_mng_addr_destroy (PeerAddr *addr);
static void tbfs_peer_mng_on_timer_cb (evutil_socket_t fd, short events, void *arg);
static void tbfs_peer_mng_peer_foreach (PeerMng *mng, peer_func func, gpointer data1, gpointer data2);
/*}}}*/

/*{{{ create / destroy*/
PeerMng *tbfs_peer_mng_create (Application *app, Torrent *torrent)
{
    PeerMng *mng;
    struct timeval tv;

    mng = g_new0 (PeerMng, 1);
    mng->app = app;
    mng->torrent = torrent;
    mng->h_peer_addrs = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)tbfs_peer_mng_addr_destroy);
    mng->peer_count = 0;
    mng->q_pieces_wanted = g_queue_new ();

    mng->peers_last_checked = 0;
    mng->peers_being_checked = FALSE;

    mng->ev_timer = evtimer_new (application_get_evbase (app), tbfs_peer_mng_on_timer_cb, mng);

    // start timer
    tv.tv_sec = conf_get_int (application_get_conf (mng->app), "peer_client.check_sec");
    tv.tv_usec = 0;
    event_add (mng->ev_timer, &tv);

    return mng;
}

void tbfs_peer_mng_destroy (PeerMng *mng)
{
    event_free (mng->ev_timer);
    g_queue_free (mng->q_pieces_wanted);
    g_hash_table_destroy (mng->h_peer_addrs);
    g_free (mng);
}
/*}}}*/

Application *tbfs_peer_mng_get_app (PeerMng *mng)
{
    return mng->app;
}

/*{{{ Peers */
static void tbfs_peer_mng_addr_destroy (PeerAddr *addr)
{
    g_hash_table_destroy (addr->h_peers);
    g_free (addr);
}

void tbfs_peer_mng_peer_add (PeerMng *mng, const gchar *peer_id, guint32 addr, guint16 port)
{
    PeerAddr *p_addr;
    Peer *existing_peer;

    p_addr = g_hash_table_lookup (mng->h_peer_addrs, GUINT_TO_POINTER (addr));
    if (!p_addr) {
        p_addr = g_new0 (PeerAddr, 1);
        p_addr->h_peers = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)tbfs_peer_destroy);
        g_hash_table_insert (mng->h_peer_addrs, GUINT_TO_POINTER (addr), p_addr);
    }

    existing_peer = g_hash_table_lookup (p_addr->h_peers, GUINT_TO_POINTER (port));
    if (existing_peer) {
        LOG_debug (PMNG_LOG, "Peer already exist: %u:%d", addr, port);
    } else {
        Peer *peer;
        peer = tbfs_peer_create (mng, peer_id, addr, port);
        g_hash_table_insert (p_addr->h_peers, GUINT_TO_POINTER (port), peer);
        mng->peer_count++;
    }
}

// launch "pieces" requests to peers
void tbfs_peer_mng_peers_updated (PeerMng *mng)
{
    Bitfield *bf_want;

    bf_want = tbfs_torrent_get_bitfield_pieces_want (mng->torrent);
    if (!tbfs_bitfield_get_set_bits (bf_want)) {
        LOG_debug (PMNG_LOG, "[%s] No pieces wanted !", tbfs_torrent_get_info_hash (mng->torrent));
        return;
    }

    if (!mng->peer_count) {
        LOG_debug (PMNG_LOG, "[%s] No peers !", tbfs_torrent_get_info_hash (mng->torrent));
        return;
    }

    tbfs_peer_mng_peer_foreach (mng, (peer_func)tbfs_peer_on_pieces_request_cb, NULL, NULL);
}

gint tbfs_peer_mng_peer_count (PeerMng *mng)
{
    return mng->peer_count;
}
/*}}}*/

/*{{{ Peers Foreach */
typedef struct {
    peer_func func;
    gpointer data1;
    gpointer data2;
} PeerMngForeachData;

static void tbfs_peer_mng_peer_foreach_port (G_GNUC_UNUSED gpointer key, gpointer value, gpointer user_data)
{
    PeerMngForeachData *data = (PeerMngForeachData *) user_data;
    Peer *peer = (Peer *) value;

    data->func (peer, data->data1, data->data2);
}

static void tbfs_peer_mng_peer_foreach_addr (G_GNUC_UNUSED gpointer key, gpointer value, gpointer user_data)
{
    PeerMngForeachData *data = (PeerMngForeachData *) user_data;
    PeerAddr *p_addr = (PeerAddr *) value;

    g_hash_table_foreach (p_addr->h_peers, tbfs_peer_mng_peer_foreach_port, data);
}

static void tbfs_peer_mng_peer_foreach (PeerMng *mng, peer_func func, gpointer data1, gpointer data2)
{
    PeerMngForeachData *data;
    data = g_new0 (PeerMngForeachData, 1);
    data->func = func;
    data->data1 = data1;
    data->data2 = data2;

    g_hash_table_foreach (mng->h_peer_addrs, tbfs_peer_mng_peer_foreach_addr, data);

    g_free (data);
}
/*}}}*/

/*{{{ on_timer_cb */
static void tbfs_peer_mng_on_timer_cb (G_GNUC_UNUSED evutil_socket_t fd, G_GNUC_UNUSED short events, void *arg)
{
    PeerMng *mng = (PeerMng *) arg;
    struct timeval tv;
    time_t now = time (NULL);

    LOG_debug (PMNG_LOG, "[%s] On timer", tbfs_torrent_get_info_hash (mng->torrent));

    // peers
    if (!mng->peers_being_checked && 
        now - mng->peers_last_checked >= conf_get_int (application_get_conf (mng->app), "peer_client.check_sec")) 
    {
        //mng->peers_last_checked = now;
        mng->peers_being_checked = TRUE;
        LOG_debug (PMNG_LOG, "[%s] Checking peers..", tbfs_torrent_get_info_hash (mng->torrent));
    }

    // re-start timer
    tv.tv_sec = conf_get_int (application_get_conf (mng->app), "peer_client.check_sec");
    tv.tv_usec = 0;
    event_add (mng->ev_timer, &tv);
}
/*}}}*/

/*{{{ */
void tbfs_peer_mng_torrent_piece_added (PeerMng *mng, guint32 piece_id)
{
    if (g_queue_find (mng->q_pieces_wanted, GUINT_TO_POINTER (piece_id))) {
        LOG_debug (PMNG_LOG, "[%s] Piece %u already in queue !", tbfs_torrent_get_info_hash (mng->torrent), piece_id);
        return;
    }

    g_queue_push_head (mng->q_pieces_wanted, GUINT_TO_POINTER (piece_id));
}
/*}}}*/

/*{{{ print*/
void tbfs_peer_mng_info_print (PeerMng *mng, struct evbuffer *buf, PrintFormat *print_format)
{
    tbfs_peer_mng_peer_foreach (mng, (peer_func)tbfs_peer_on_info_print_cb, buf, print_format);
}
/*}}}*/
