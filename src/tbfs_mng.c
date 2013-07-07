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
#include "tbfs_mng.h"
#include "tbfs_torrent.h"
#include "tbfs_tracker_client.h"

/*{{{ struct*/
struct _TBFSMng {
    Application *app;

    struct event *ev_timer;
    time_t tracker_last_checked;
    gboolean tracker_being_checked;

    GHashTable *h_torrent_data;
};

typedef struct {
    Torrent *torrent;

    time_t last_checked;
    gboolean being_checked;
} TorrentData;

#define MNG_LOG "mng"

static void tbfs_mng_on_timer_cb (evutil_socket_t fd, short events, void *arg);
static void tbfs_mng_torrent_data_destroy (TorrentData *tdata);
/*}}}*/

/*{{{ create / destroy */
TBFSMng *tbfs_mng_create (Application *app)
{
    TBFSMng *mng;
    struct timeval tv;

    mng = g_new0 (TBFSMng, 1);
    mng->app = app;
    mng->h_torrent_data = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) tbfs_mng_torrent_data_destroy);

    mng->tracker_last_checked = 0;
    mng->tracker_being_checked = FALSE;

    mng->ev_timer = evtimer_new (application_get_evbase (app), tbfs_mng_on_timer_cb, mng);

    // start timer
    tv.tv_sec = conf_get_int (application_get_conf (mng->app), "tracker.check_sec");
    tv.tv_usec = 0;
    event_add (mng->ev_timer, &tv);

    return mng;
}

void tbfs_mng_destroy (TBFSMng *mng)
{
    event_free (mng->ev_timer);
    g_hash_table_destroy (mng->h_torrent_data);
    g_free (mng);
}

static TorrentData *tbfs_mng_torrent_data_create (Torrent *torrent)
{
    TorrentData *tdata;

    tdata = g_new0 (TorrentData, 1);
    tdata->torrent = torrent;
    tdata->last_checked = 0;
    tdata->being_checked = FALSE;

    return tdata;
}

static void tbfs_mng_torrent_data_destroy (TorrentData *tdata)
{
    tbfs_torrent_destroy (tdata->torrent);
    g_free (tdata);
}
/*}}}*/

/*{{{ get / set */
Torrent *tbfs_mng_torrent_get (TBFSMng *mng, const gchar *info_hash)
{
    TorrentData *tdata;

    tdata = g_hash_table_lookup (mng->h_torrent_data, info_hash);
    if (tdata)
        return tdata->torrent;
    else
        return NULL;
}
/*}}}*/

/*{{{ on_timer_cb */

// Tracker cb function
static void tbfs_mng_on_torrent_checked_cb (gboolean status, TBFSMng *mng, gchar *info_hash, GList *l_peer_addrs)
{
    TorrentData *tdata;
    GList *l;

    if (!status) {
        LOG_err (MNG_LOG, "[t: %s] Failed to check torrent !", info_hash);
        return;
    }

    tdata = g_hash_table_lookup (mng->h_torrent_data, info_hash);
    if (!tdata || !tdata->torrent) {
        LOG_err (MNG_LOG, "[t: %s] Torrent does not exist !", info_hash);
        return;
    }

    LOG_debug (MNG_LOG, "[t: %s] Torrent is checked !", info_hash);

    for (l = g_list_first (l_peer_addrs); l; l = g_list_next (l)) {
        PeerAddr *addr = (PeerAddr *) l->data;
        
        // add peer
        tbfs_torrent_add_peer_addr (tdata->torrent, 
            conf_get_string (application_get_conf (mng->app), "peer.default_id"),
            addr->addr4, addr->port
        );
    }

    tbfs_torrent_peers_updated (tdata->torrent);

    tdata->being_checked = FALSE;
    tdata->last_checked = time (NULL);
}

static void tbfs_mng_foreach_torrent (G_GNUC_UNUSED gpointer key, gpointer value, gpointer user_data)
{
    TBFSMng *mng = (TBFSMng *) user_data;
    TorrentData *tdata = (TorrentData *) value;
    time_t now = time (NULL);

    if (!tdata->being_checked && now >= tdata->last_checked &&
        now - tdata->last_checked >= conf_get_int (application_get_conf (mng->app), "tracker.torrent_check_sec")) {

        tdata->being_checked = TRUE;
    
        LOG_debug (MNG_LOG, "[t: %s] Checking torrent..", tbfs_torrent_get_info_hash (tdata->torrent));

        // XXX:
        tbfs_tracker_client_send_request (application_get_tracker_client (mng->app), 
            tbfs_torrent_get_info_hash (tdata->torrent), TE_started, 
            (TrackerClient_on_request_done_cb)tbfs_mng_on_torrent_checked_cb, mng
        );
    }
}

static void tbfs_mng_on_timer_cb (G_GNUC_UNUSED evutil_socket_t fd, G_GNUC_UNUSED short events, void *arg)
{
    TBFSMng *mng = (TBFSMng *) arg;
    struct timeval tv;
    time_t now = time (NULL);

    // tracker
    if (!mng->tracker_being_checked && g_hash_table_size (mng->h_torrent_data) &&
        now - mng->tracker_last_checked >= conf_get_int (application_get_conf (mng->app), "tracker.check_sec")) 
    {
        mng->tracker_being_checked = TRUE;

        LOG_debug (MNG_LOG, "Checking tracker..");
        g_hash_table_foreach (mng->h_torrent_data, tbfs_mng_foreach_torrent, mng);
        mng->tracker_being_checked = FALSE;
        mng->tracker_last_checked = time (NULL);
    }

    // re-start timer
    tv.tv_sec = conf_get_int (application_get_conf (mng->app), "tracker.check_sec");
    tv.tv_usec = 0;
    event_add (mng->ev_timer, &tv);
}
/*}}}*/

/*{{{ torrent registration */
// adds and new torrent
Torrent *tbfs_mng_torrent_register (TBFSMng *mng, const gchar *info_hash, guint32 total_pieces)
{
    TorrentData *tdata;
    Torrent *torrent;

    // search for existing torrent
    tdata = g_hash_table_lookup (mng->h_torrent_data, info_hash);
    if (tdata) {
        //XXX:
        LOG_msg (MNG_LOG, "[t: %s] Torrent already exist !", info_hash);
        return NULL;
    }

    torrent = tbfs_torrent_create (mng->app, info_hash, total_pieces);
    if (!torrent) {
        LOG_err (MNG_LOG, "[t: %s] Failed to create torrent !", info_hash);
        return NULL;
    }

    tdata = tbfs_mng_torrent_data_create (torrent);

    g_hash_table_insert (mng->h_torrent_data, (gchar *)tbfs_torrent_get_info_hash (torrent), tdata);

    LOG_debug (MNG_LOG, "[t: %s] Registering torrent", info_hash);

    return torrent;
}
/*}}}*/
