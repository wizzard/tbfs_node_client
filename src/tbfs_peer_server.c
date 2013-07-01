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
#include "tbfs_peer_server.h"
#include "tbfs_peer_client.h"

struct _PeerServer {
    Application *app;
    struct evconnlistener *listener;
};

#define PSRV_LOG "psrv"
static void tbfs_peer_server_on_connection_cb (G_GNUC_UNUSED struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *address, G_GNUC_UNUSED int socklen, void *ctx);

PeerServer *tbfs_peer_server_create (Application *app)
{
    PeerServer *server;
    struct sockaddr_in sin;


    server = g_new0 (PeerServer, 1);
    server->app = app;

    memset (&sin, 0, sizeof (sin));
    sin.sin_family = AF_INET;
    inet_aton (conf_get_string (application_get_conf (app), "peer_server.listen"), &sin.sin_addr);
    sin.sin_port = g_htons (conf_get_int (application_get_conf (app), "peer_server.port"));

    server->listener = evconnlistener_new_bind (application_get_evbase (app),
        tbfs_peer_server_on_connection_cb, server, 
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_CLOSE_ON_EXEC | LEV_OPT_REUSEABLE | LEV_OPT_DEFERRED_ACCEPT,
        conf_get_int (application_get_conf (app), "peer_server.backlog"),
        (struct sockaddr*)&sin, sizeof (sin)
    );

    if (!server->listener) {
        LOG_err (PSRV_LOG, "Failed to start peer server on %s:%d !", 
            conf_get_string (application_get_conf (app), "peer_server.listen"),
            conf_get_int (application_get_conf (app), "peer_server.port")
        );
        g_free (server);
        return NULL;
    }

    LOG_msg (PSRV_LOG, "Peer server is listening on: %s:%i",
        conf_get_string (application_get_conf (app), "peer_server.listen"),
        conf_get_int (application_get_conf (app), "peer_server.port")
    );

    return server;
}

void tbfs_peer_server_destroy (PeerServer *server)
{
    evconnlistener_free (server->listener);
    g_free (server);
}

static void tbfs_peer_server_on_connection_cb (G_GNUC_UNUSED struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *address, G_GNUC_UNUSED int socklen, void *ctx)
{
    PeerServer *server = (PeerServer *) ctx;
    PeerClient *client;

    LOG_msg (PSRV_LOG, "Incoming connection from: %s:%i",
        inet_ntoa (((struct sockaddr_in *)address)->sin_addr), 
        g_ntohs (((struct sockaddr_in *)address)->sin_port)
    );

    client = tbfs_peer_client_create (server->app, fd);
    if (!client) {
        close (fd);
        return;
    }

}
