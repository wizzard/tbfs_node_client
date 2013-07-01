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
#include "tbfs_cmd_server.h"
#include "tbfs_mng.h"

/*{{{ structs */
struct _CmdServer {
    Application *app;
    struct evhttp *httpd;
};

#define CSRV_LOG "csrv"
static void tbfs_cmd_server_on_http_gen_cb (struct evhttp_request *req, G_GNUC_UNUSED void *ctx);
static void tbfs_cmd_server_on_add_torrent_cb (struct evhttp_request *req, void *ctx);
static void tbfs_cmd_server_on_info_torrent_cb (struct evhttp_request *req, void *ctx);
/*}}}*/

/*{{{ create / destroy */
CmdServer *tbfs_cmd_server_create (Application *app)
{
    CmdServer *server;

    server = g_new0 (CmdServer, 1);
    server->app = app;

    server->httpd = evhttp_new (application_get_evbase (app));
    if (evhttp_bind_socket (server->httpd, 
        conf_get_string (application_get_conf (app), "cmd_server.listen"), 
        conf_get_int (application_get_conf (app), "cmd_server.port")) == -1) 
    {
        LOG_err (CSRV_LOG, "Failed to bind command server to: %s:%d", 
            conf_get_string (application_get_conf (app), "cmd_server.listen"),
            conf_get_int (application_get_conf (app), "cmd_server.port")
        );
        return NULL;
    }
    evhttp_set_cb (server->httpd, "/cmd_torrent_add", tbfs_cmd_server_on_add_torrent_cb, server);
    evhttp_set_cb (server->httpd, "/cmd_torrent_info", tbfs_cmd_server_on_info_torrent_cb, server);
    evhttp_set_gencb (server->httpd, tbfs_cmd_server_on_http_gen_cb, server);

    LOG_msg (CSRV_LOG, "Command server is listening on: %s:%i",
        conf_get_string (application_get_conf (app), "cmd_server.listen"),
        conf_get_int (application_get_conf (app), "cmd_server.port")
    );

    return server;
}

void tbfs_cmd_server_destroy (CmdServer *server)
{
    evhttp_free (server->httpd);
    g_free (server);
}
/*}}}*/

/*{{{ HTTP gen cb*/
static void tbfs_cmd_server_on_http_gen_cb (struct evhttp_request *req, G_GNUC_UNUSED void *ctx)
{
    evhttp_send_reply (req, HTTP_NOCONTENT, "Not Found", NULL);
}

/*}}}*/

/*{{{ on_add_torrent_cb */
// Add torrent and piece
// x.x.x.x/cmd_torrent_add?info_hash=xxxx&total_pieces=n&piece=n
static void tbfs_cmd_server_on_add_torrent_cb (struct evhttp_request *req, void *ctx)
{
    CmdServer *server = (CmdServer *) ctx;
    struct evbuffer *evb = NULL;
    const gchar *query;
    struct evkeyvalq q_params;
    const gchar *info_hash;
    const gchar *s_piece;
    const gchar *s_total_pieces;
    Torrent *torrent;

    LOG_debug (CSRV_LOG, "[%s:%d] URL: %s", req->remote_host, req->remote_port, req->uri);

    query = evhttp_uri_get_query (evhttp_request_get_evhttp_uri (req));
    if (!query) {
        evhttp_send_reply (req, HTTP_NOCONTENT, "Not found", NULL);
        return;
    }

    TAILQ_INIT (&q_params);
    evhttp_parse_query_str (query, &q_params);

    info_hash = http_find_header (&q_params, "info_hash");
    if (!info_hash) {
        LOG_err (CSRV_LOG, "Required \"info_hash\" parameter not found !");
        evhttp_send_reply (req, HTTP_NOCONTENT, "Not Found", NULL);
        evhttp_clear_headers (&q_params);
        return;
    }

    s_total_pieces = http_find_header (&q_params, "total_pieces");
    if (!s_total_pieces) {
        LOG_err (CSRV_LOG, "Required \"total_pieces\" parameter not found !");
        evhttp_send_reply (req, HTTP_NOCONTENT, "Not Found", NULL);
        evhttp_clear_headers (&q_params);
        return;
    }
    
    s_piece = http_find_header (&q_params, "piece");
    if (!s_piece) {
        LOG_err (CSRV_LOG, "Required \"piece\" parameter not found !");
        evhttp_send_reply (req, HTTP_NOCONTENT, "Not Found", NULL);
        evhttp_clear_headers (&q_params);
        return;
    }

    torrent = tbfs_mng_torrent_get (application_get_mng (server->app), info_hash);
    if (!torrent) {
        torrent = tbfs_mng_torrent_register (application_get_mng (server->app), info_hash, evutil_strtoll (s_total_pieces, NULL, 10));
    }
    if (!torrent) {
        LOG_err (CSRV_LOG, "Failed to register torrent !");
        evhttp_send_reply (req, HTTP_NOCONTENT, "Not Found", NULL);
        evhttp_clear_headers (&q_params);
        return;
    }
    
    // add a piece
    tbfs_torrent_add_piece (torrent, evutil_strtoll (s_piece, NULL, 10));

    evb = evbuffer_new ();
    evhttp_send_reply (req, HTTP_OK, "OK", evb);
    evbuffer_free (evb);
    
    evhttp_clear_headers (&q_params);
}
/*}}}*/

/*{{{ on_info_torrent_cb*/
// Print torrent info
// x.x.x.x/cmd_torrent_info?info_hash=xxxx
static void tbfs_cmd_server_on_info_torrent_cb (struct evhttp_request *req, void *ctx)
{
    CmdServer *server = (CmdServer *) ctx;
    struct evbuffer *evb = NULL;
    const gchar *query;
    struct evkeyvalq q_params;
    const gchar *info_hash;
    Torrent *torrent;
    PrintFormat *print_format;

    LOG_debug (CSRV_LOG, "[%s:%d] URL: %s", req->remote_host, req->remote_port, req->uri);

    query = evhttp_uri_get_query (evhttp_request_get_evhttp_uri (req));
    if (!query) {
        evhttp_send_reply (req, HTTP_NOCONTENT, "Not found", NULL);
        return;
    }

    TAILQ_INIT (&q_params);
    evhttp_parse_query_str (query, &q_params);

    info_hash = http_find_header (&q_params, "info_hash");

    if (!info_hash) {
        LOG_err (CSRV_LOG, "Required params not found !");
        evhttp_send_reply (req, HTTP_NOCONTENT, "Not Found", NULL);
        evhttp_clear_headers (&q_params);
        return;
    }
    
    torrent = tbfs_mng_torrent_get (application_get_mng (server->app), info_hash);
    if (!torrent) {
        LOG_err (CSRV_LOG, "Failed to get torrent !");
        evhttp_send_reply (req, HTTP_NOCONTENT, "Not Found", NULL);
        evhttp_clear_headers (&q_params);
        return;
    }

    evb = evbuffer_new ();
    tbfs_torrent_info_print (torrent, evb, print_format);
    evhttp_send_reply (req, HTTP_OK, "OK", evb);
    evbuffer_free (evb);
    
    evhttp_clear_headers (&q_params);
}
/*}}}*/
