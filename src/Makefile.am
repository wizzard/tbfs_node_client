AM_CFLAGS=-DSYSCONFDIR=\""$(sysconfdir)/@PACKAGE@/"\"
bin_PROGRAMS = tbfs_node_client
tbfs_node_client_SOURCES = log.c
tbfs_node_client_SOURCES += conf.c
tbfs_node_client_SOURCES += libevent_utils.c
tbfs_node_client_SOURCES += sys_utils.c
tbfs_node_client_SOURCES += string_utils.c
tbfs_node_client_SOURCES += tbfs_bitfield.c
tbfs_node_client_SOURCES += tbfs_bencode.c
tbfs_node_client_SOURCES += tbfs_torrent.c
tbfs_node_client_SOURCES += tbfs_cmd_server.c
tbfs_node_client_SOURCES += tbfs_peer.c
tbfs_node_client_SOURCES += tbfs_peer_mng.c
tbfs_node_client_SOURCES += tbfs_peer_server.c
tbfs_node_client_SOURCES += tbfs_peer_client.c
tbfs_node_client_SOURCES += tbfs_mng.c
tbfs_node_client_SOURCES += tbfs_tracker_client.c
tbfs_node_client_SOURCES += tbfs_storage_mng.c
tbfs_node_client_SOURCES += tbfs_storage_torrent.c
tbfs_node_client_SOURCES += main.c

tbfs_node_client_CFLAGS = $(AM_CFLAGS) $(DEPS_CFLAGS) $(LIBEVENT_OPENSSL_CFLAGS) $(SSL_CFLAGS)
tbfs_node_client_LDADD = $(AM_LDADD) $(DEPS_LIBS) $(LIBEVENT_OPENSSL_LIBS) $(SSL_LIBS)
