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
#include "tbfs_tracker_client.h"
#include "tbfs_bencode.h"
#include "tbfs_mng.h"

struct _TrackerClient {
    Application *app;

    struct evhttp_connection *evcon;
};

#define TCLI_LOG "tcli"

static void tbfs_tracker_client_on_close_cb (struct evhttp_connection *evcon, void *ctx);
static void tbfs_tracker_client_on_send_request_cb (struct evhttp_request *req, void *ctx);

TrackerClient *tbfs_tracker_client_create (Application *app)
{
    TrackerClient *client;
    struct bufferevent *bev;

    client = g_new0 (TrackerClient, 1);
    client->app = app;

    bev = bufferevent_socket_new (
        application_get_evbase (app),
        -1,
		BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_UNLOCK_CALLBACKS
    );

    if (!bev) {
        LOG_err (TCLI_LOG, "Failed to create bufferevent !");
        g_free (client);
        return NULL;
    }

    client->evcon = evhttp_connection_base_bufferevent_new (
        application_get_evbase (app),
        application_get_dnsbase (app),
        bev,
        conf_get_string (application_get_conf (app), "tracker.host"),
        conf_get_int (application_get_conf (app), "tracker.port")
    );

    if (!client->evcon) {
        LOG_err (TCLI_LOG, "Failed to create evhttp_connection !");
        g_free (client);
        return NULL;
    }

    evhttp_connection_set_timeout (client->evcon, conf_get_int (application_get_conf (app), "tracker.timeout"));
    evhttp_connection_set_retries (client->evcon, conf_get_int (application_get_conf (app), "tracker.retries"));

    evhttp_connection_set_closecb (client->evcon, tbfs_tracker_client_on_close_cb, client);

    return client;
}

void tbfs_tracker_client_destroy (TrackerClient *client)
{
    if (client->evcon)
        evhttp_connection_free (client->evcon);
    g_free (client);
}

static void tbfs_tracker_client_on_close_cb (struct evhttp_connection *evcon, void *ctx)
{
    TrackerClient *client = (TrackerClient *) ctx;

    LOG_msg (TCLI_LOG, "[evcon: %p client: %p] Connection closed", evcon, client);
}

typedef struct {
    TrackerClient *client;
    gchar *info_hash;

    TrackerClient_on_request_done_cb on_request_done_cb;
    gpointer ctx;
} TrackerRequestData;

static void tr_data_destroy (TrackerRequestData *tr_data)
{
    g_free (tr_data->info_hash);
    g_free (tr_data);
}

//http://10.0.0.211:6969/announce?info_hash=0w%baNH%058M%3ae%eaL%3cTu%15Np%933&peer_id=-TR2770-pmk1smsjhy7t&port=55735&uploaded=0&downloaded=0&left=0&numwant=80&key=2b565f82&compact=1&supportcrypto=1&event=started
void tbfs_tracker_client_send_request (TrackerClient *client, const gchar *info_hash, TrackerEvent event_type, 
    TrackerClient_on_request_done_cb on_request_done_cb, gpointer ctx)
{
    struct evhttp_request *req;
    int res;
    gchar *uri = NULL;
    gchar s_event[10];
    gchar escaped_info_hash[SHA_DIGEST_LENGTH*3 + 1];
    TrackerRequestData *tr_data;
    uint8_t sha1[SHA_DIGEST_LENGTH];

    tr_data = g_new0 (TrackerRequestData, 1);
    tr_data->client = client;
    tr_data->info_hash = g_strdup (info_hash);
    tr_data->on_request_done_cb = on_request_done_cb;
    tr_data->ctx = ctx;

    req = evhttp_request_new (tbfs_tracker_client_on_send_request_cb, tr_data);
    if (!req) {
        LOG_err (TCLI_LOG, "Failed to create HTTP request object !");
        tr_data->on_request_done_cb (FALSE, tr_data->ctx, tr_data->info_hash, NULL);
        tr_data_destroy (tr_data);
        return;
    }

    if (event_type == TE_started) {
        strcpy (s_event, "started");
    } else if (event_type == TE_completed) {
        strcpy (s_event, "completed");
    } else if (event_type == TE_stopped) {
        strcpy (s_event, "stopped");
    } else {
        LOG_err (TCLI_LOG, "Unknown tracker event type: %d !", event_type);
        tr_data->on_request_done_cb (FALSE, tr_data->ctx, tr_data->info_hash, NULL);
        tr_data_destroy (tr_data);
        return;
    }

    hexstr_to_sha1 (sha1, info_hash);
    escape_sha1 (escaped_info_hash, sha1);

    uri = g_strdup_printf ("/announce?info_hash=%s&peer_id=%s&port=%d&uploaded=0&downloaded=0&left=0&numwant=80&event=%s&compact=1",
        escaped_info_hash,
        conf_get_string (application_get_conf (client->app), "peer.peer_id"),
        conf_get_int (application_get_conf (client->app), "peer_server.port"),
        s_event
    );

    LOG_debug (TCLI_LOG, "Sending request to tracker: %s", uri);

    res = evhttp_make_request (client->evcon, req, EVHTTP_REQ_GET, uri);
    g_free (uri);

    if (res < 0) {
        LOG_err (TCLI_LOG, "Failed execute HTTP request !");
        tr_data->on_request_done_cb (FALSE, tr_data->ctx, tr_data->info_hash, NULL);
        tr_data_destroy (tr_data);
        return;
    }
}

// handle responce from Tracker
static void tbfs_tracker_client_on_send_request_cb (struct evhttp_request *req, void *ctx)
{
    TrackerRequestData *tr_data = (TrackerRequestData *) ctx;
    struct evbuffer *inbuf;
    BValue *bval, *bpeers;
    guint8 *s_peers;
    gint32 peers_len;
    gint i;
    const uint8_t * walk;
    GList *l_peer_addrs = NULL;

    if (!tr_data || !req) {
        LOG_err (TCLI_LOG, "Failed to get tracker response !");
        tr_data->on_request_done_cb (FALSE, tr_data->ctx, tr_data->info_hash, NULL);
        tr_data_destroy (tr_data);
        return;
    }

    if (evhttp_request_get_response_code (req) != 200) {
        LOG_err (TCLI_LOG, "Got response from tracker: (%d) %s",
            evhttp_request_get_response_code (req), evhttp_request_get_response_code_line (req));
        tr_data->on_request_done_cb (FALSE, tr_data->ctx, tr_data->info_hash, NULL);
        tr_data_destroy (tr_data);
        return;
    }

    inbuf = evhttp_request_get_input_buffer (req);

    LOG_debug (TCLI_LOG, "Got response from tracker (%zd bytes): %s", evbuffer_get_length (inbuf), evbuffer_pullup (inbuf, -1));

    bval = bvalue_create_from_buff (inbuf);
    if (!bval || !bvalue_is_dict (bval)) {
        LOG_err (TCLI_LOG, "Failed to parse Tracker response !");
        tr_data->on_request_done_cb (FALSE, tr_data->ctx, tr_data->info_hash, NULL);
        tr_data_destroy (tr_data);
        return;
    }

    bpeers = bvalue_dict_get_value (bval, "peers");
    if (!bpeers || !bvalue_is_string (bpeers)) {
        LOG_err (TCLI_LOG, "Failed to parse Tracker response !");
        bvalue_destroy (bval);
        tr_data->on_request_done_cb (FALSE, tr_data->ctx, tr_data->info_hash, NULL);
        tr_data_destroy (tr_data);
        return;
    }

    s_peers = bvalue_get_binary_string (bpeers, &peers_len);
    if (peers_len % 6 != 0) {
        LOG_err (TCLI_LOG, "Failed to parse Tracker response !");
        bvalue_destroy (bval);
        tr_data->on_request_done_cb (FALSE, tr_data->ctx, tr_data->info_hash, NULL);
        tr_data_destroy (tr_data);
        return;
    }

    LOG_debug (TCLI_LOG, "Got peers: %d", peers_len / 6);

    walk = s_peers;
    for (i = 0; i < peers_len / 6; i++) {
        PeerAddr *addr;

        addr = g_new0 (PeerAddr, 1);

        memcpy (&addr->addr4, walk, 4); walk += 4;
        memcpy (&addr->port, walk, 2); walk += 2;

        l_peer_addrs = g_list_append (l_peer_addrs, addr);
    }
    
    bvalue_destroy (bval);
    
    tr_data->on_request_done_cb (TRUE, tr_data->ctx, tr_data->info_hash, l_peer_addrs);
    // free the list
    g_list_foreach (l_peer_addrs, (GFunc) g_free, NULL);
    
    tr_data_destroy (tr_data);
}
