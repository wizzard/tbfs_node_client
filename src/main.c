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
#include "global.h"
#include "tbfs_peer_server.h"
#include "tbfs_cmd_server.h"
#include "tbfs_mng.h"
#include "tbfs_tracker_client.h"
#include "tbfs_storage_mng.h"

/*{{{ structs */
struct _Application {
    ConfData *conf;
    gchar *conf_path;

    struct event_base *evbase;
    struct evdns_base *dns_base;
    PeerServer *peer_server;
    CmdServer *cmd_server;
    TBFSMng *mng;
    TrackerClient *tracker_client;
    StorageMng *storage_mng;

    GHashTable *h_torrents;

    struct event *sigint_ev;
    struct event *sigpipe_ev;
    struct event *sigusr1_ev;
};

#define APP_LOG "main"
/*}}}*/

/*{{{ Application */
ConfData *application_get_conf (Application *app)
{
    return app->conf;
}

struct event_base *application_get_evbase (Application *app)
{
    return app->evbase;
}

struct evdns_base *application_get_dnsbase (Application *app)
{
    return app->dns_base;
}

TBFSMng *application_get_mng (Application *app)
{
    return app->mng;
}

TrackerClient *application_get_tracker_client (Application *app)
{
    return app->tracker_client;
}

static void application_destroy (Application *app)
{
    if (app->peer_server)
        tbfs_peer_server_destroy (app->peer_server);
    if (app->cmd_server)
        tbfs_cmd_server_destroy (app->cmd_server);
    if (app->mng)
        tbfs_mng_destroy (app->mng);
    if (app->tracker_client)
        tbfs_tracker_client_destroy (app->tracker_client);
    if (app->storage_mng)
        tbfs_storage_mng_destroy (app->storage_mng);
    if (app->sigint_ev)
        event_free (app->sigint_ev);
    if (app->sigpipe_ev)
        event_free (app->sigpipe_ev);
    if (app->sigusr1_ev)
        event_free (app->sigusr1_ev);
    if (app->dns_base)
        evdns_base_free (app->dns_base, 0);
    if (app->evbase)
        event_base_free (app->evbase);
    if (app->conf)
        conf_destroy (app->conf);
    if (app->conf_path)
        g_free (app->conf_path);
    g_free (app);
}
/*}}}*/

/*{{{ signal handlers */
/* This structure mirrors the one found in /usr/include/asm/ucontext.h */
typedef struct _sig_ucontext {
    unsigned long     uc_flags;
    struct ucontext   *uc_link;
    stack_t           uc_stack;
    struct sigcontext uc_mcontext;
    sigset_t          uc_sigmask;
} sig_ucontext_t;

static void sigsegv_cb (int sig_num, siginfo_t *info, void * ucontext)
{
    void *array[50];
    void *caller_address;
    char **messages;
    int size, i;
    sig_ucontext_t *uc;
    FILE *f;
    
    g_fprintf (stderr, "Got segmentation fault !\n");

    uc = (sig_ucontext_t *)ucontext;

    /* Get the address at the time the signal was raised from the EIP (x86) */
#ifdef __i386__
    caller_address = (void *) uc->uc_mcontext.eip;   
#else
    caller_address = (void *) uc->uc_mcontext.rip;   
#endif

    f = stderr;

    fprintf (f, "signal %d (%s), address is %p from %p\n", sig_num, strsignal (sig_num), info->si_addr, (void *)caller_address);

    size = backtrace (array, 50);

    /* overwrite sigaction with caller's address */
    array[1] = caller_address;

    messages = backtrace_symbols (array, size);

    /* skip first stack frame (points here) */
    for (i = 1; i < size && messages != NULL; ++i) {
        fprintf (f, "[bt]: (%d) %s\n", i, messages[i]);
    }

    fflush (f);

    free (messages);

    LOG_err (APP_LOG, "signal %d (%s), address is %p from %p\n", sig_num, strsignal (sig_num), info->si_addr, (void *)caller_address);
}

// ignore SIGPIPE
static void sigpipe_cb (G_GNUC_UNUSED evutil_socket_t sig, G_GNUC_UNUSED short events, G_GNUC_UNUSED void *user_data)
{
    LOG_msg (APP_LOG, "Got SIGPIPE");
}

// XXX: re-read config or do some useful work here
static void sigusr1_cb (G_GNUC_UNUSED evutil_socket_t sig, G_GNUC_UNUSED short events, G_GNUC_UNUSED void *user_data)
{
    LOG_err (APP_LOG, "Got SIGUSR1");
}

// terminate application, freeing all used memory
static void sigint_cb (G_GNUC_UNUSED evutil_socket_t sig, G_GNUC_UNUSED short events, void *user_data)
{
    Application *app = (Application *) user_data;

    LOG_err (APP_LOG, "Got SIGINT");

    // terminate after running all active events 
    event_base_loopexit (app->evbase, NULL);
}
/*}}}*/

int main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
    Application *app;
    GOptionContext *context;
    GError *error = NULL;
    gchar **s_config = NULL;
    gchar **s_peer_id = NULL;
    gchar **s_storage = NULL;
    gboolean foreground = FALSE;
    gchar conf_str[1023];
    gboolean verbose = FALSE;
    gboolean version = FALSE;
    struct sigaction sigact;
    
    app = g_new0 (Application, 1);
    app->conf_path = g_build_filename (SYSCONFDIR, "tbfs_node_client.conf", NULL);
    g_snprintf (conf_str, sizeof (conf_str), "Path to configuration file. Default: %s", app->conf_path);

    GOptionEntry entries[] = {
        { "peer-id", 'p', 0, G_OPTION_ARG_STRING_ARRAY, &s_peer_id, "Set PeerID.", NULL },
        { "storage-dir", 's', 0, G_OPTION_ARG_FILENAME_ARRAY, &s_storage, "Set torrent storage directory", NULL},
        { "config", 'c', 0, G_OPTION_ARG_FILENAME_ARRAY, &s_config, conf_str, NULL},
        { "foreground", 'f', 0, G_OPTION_ARG_NONE, &foreground, "Flag. Do not daemonize process.", NULL },
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Verbose output.", NULL },
        { "version", 'V', 0, G_OPTION_ARG_NONE, &version, "Show application version and exit.", NULL },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };


    // parse command line arguments
    context = g_option_context_new ("[-p peer_id]");
    g_option_context_add_main_entries (context, entries, NULL);
    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_fprintf (stderr, "Failed to parse command line options: %s\n", error->message);
        application_destroy (app);
        g_option_context_free (context);
        return -1;
    }
    g_option_context_free (context);

    // user provided alternative config path
    if (s_config && g_strv_length (s_config) > 0) {
        g_free (app->conf_path);
        app->conf_path = g_strdup (s_config[0]);
        g_strfreev (s_config);
    }

    app->conf = conf_create ();
    if (access (app->conf_path, R_OK) == 0) {
        LOG_debug (APP_LOG, "Using config file: %s", app->conf_path);
        if (!conf_parse_file (app->conf, app->conf_path)) {
            LOG_err (APP_LOG, "Failed to parse configuration file: %s", app->conf_path);
            application_destroy (app);
            return -1;
        }

    // default values
    } else {
        // XXX: update !
        conf_set_int (app->conf, "peer_server.port", 55736);
        conf_set_string (app->conf, "peer_server.listen", "0.0.0.0");
        conf_set_int (app->conf, "peer_server.backlog", 128);
        conf_set_string (app->conf, "cmd_server.listen", "0.0.0.0");
        conf_set_int (app->conf, "cmd_server.port", 8000);
        conf_set_int (app->conf, "log.level", LOG_msg);
        conf_set_string (app->conf, "tracker.host", "10.0.0.197");
        conf_set_int (app->conf, "tracker.port", 6969);
        conf_set_int (app->conf, "tracker.timeout", 30);
        conf_set_int (app->conf, "tracker.retries", 2);
        conf_set_int (app->conf, "tracker.check_sec", 1);
        conf_set_int (app->conf, "tracker.torrent_check_sec", 10);
        //conf_set_string (app->conf, "tracker.announce_url", "http://127.0.0.1:6969/announce");
        conf_set_string (app->conf, "tracker.announce_url", "http://10.0.0.197:6969/announce");
        conf_set_boolean (app->conf, "app.foreground", FALSE);
        conf_set_string (app->conf, "peer.default_id", "xxxxxxxxxxxxxxxxxxxx");
        conf_set_int (app->conf, "peer_client.check_sec", 10);
        conf_set_string (app->conf, "storage.dir", "storage/");
    }

    if (verbose)
        conf_set_int (app->conf, "log.level", LOG_debug);

    log_level = conf_get_int (app->conf, "log.level");

    // check if --version is specified
    if (version) {
            g_fprintf (stdout, "%s v%s\n", PACKAGE_NAME, VERSION);
            g_fprintf (stdout, "Copyright (C) 2013 Paul Ionkin <paul.ionkin@gmail.com>\n");
            g_fprintf (stdout, "Libraries:\n");
            g_fprintf (stdout, " GLib: %d.%d.%d   libevent: %s  glibc: %s\n", 
                    GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION, 
                    LIBEVENT_VERSION,
                    gnu_get_libc_version ()
            );
        return 0;
    }

    // set peer_id from command line
    if (s_peer_id && g_strv_length (s_peer_id) >= 1) {
        LOG_debug (APP_LOG, "Peer ID: %s", s_peer_id[0]);
        conf_set_string (app->conf, "peer.peer_id", s_peer_id[0]);
        g_strfreev (s_peer_id);
    }
    // set storage dir from command line
    if (s_storage && g_strv_length (s_storage) >= 1) {
        conf_set_string (app->conf, "storage.dir", s_storage[0]);
        g_strfreev (s_storage);
    }

    // check if path is a directory and try to access it
    if (!dir_exists_and_writable (conf_get_string (app->conf, "storage.dir"))) {
        LOG_err (APP_LOG, "Storage directory %s does not exist! Please check directory permissions!", 
            conf_get_string (app->conf, "storage.dir"));
        application_destroy (app);
        return -1;
    }

    // set foreground
    if (foreground)
        conf_set_boolean (app->conf, "app.foreground", foreground);

    // check that peer_id is set
    if (!conf_get_string (app->conf, "peer.peer_id")) {
        LOG_err (APP_LOG, "Peer ID is not provided, please run  %s -p peer_id !", argv[0]);
        application_destroy (app);
        return -1;
    }

    app->evbase = event_base_new ();
    if (!app->evbase) {
        LOG_err (APP_LOG, "Failed to create event base !");
        application_destroy (app);
        return -1;
    }

    app->dns_base = evdns_base_new (app->evbase, 1);
    if (!app->dns_base) {
        LOG_err (APP_LOG, "Failed to create DNS base !");
        application_destroy (app);
        return -1;
    }

/*{{{ signal handlers*/
    // SIGINT
    app->sigint_ev = evsignal_new (app->evbase, SIGINT, sigint_cb, app);
    event_add (app->sigint_ev, NULL);
    // SIGSEGV
    sigact.sa_sigaction = sigsegv_cb;
    sigact.sa_flags = (int)SA_RESETHAND | SA_SIGINFO;
    sigemptyset (&sigact.sa_mask);
    if (sigaction (SIGSEGV, &sigact, (struct sigaction *) NULL) != 0) {
        LOG_err (APP_LOG, "error setting signal handler for %d (%s)\n", SIGSEGV, strsignal(SIGSEGV));
        event_base_loopexit (app->evbase, NULL);
        return 1;
    }
    // SIGABRT
    sigact.sa_sigaction = sigsegv_cb;
    sigact.sa_flags = (int)SA_RESETHAND | SA_SIGINFO;
    sigemptyset (&sigact.sa_mask);
    if (sigaction (SIGABRT, &sigact, (struct sigaction *) NULL) != 0) {
        LOG_err (APP_LOG, "error setting signal handler for %d (%s)\n", SIGABRT, strsignal(SIGABRT));
        event_base_loopexit (app->evbase, NULL);
        return 1;
    }
    // SIGPIPE
    app->sigpipe_ev = evsignal_new (app->evbase, SIGPIPE, sigpipe_cb, app);
    event_add (app->sigpipe_ev, NULL);
    // SIGUSR1
    app->sigusr1_ev = evsignal_new (app->evbase, SIGUSR1, sigusr1_cb, app);
    event_add (app->sigusr1_ev, NULL);
/*}}}*/

    app->mng = tbfs_mng_create (app);
    if (!app->mng) {
        LOG_err (APP_LOG, "Failed to create TBFSMng !");
        application_destroy (app);
        return -1;
    }

    app->storage_mng = tbfs_storage_mng_create (app);
    if (!app->storage_mng) {
        LOG_err (APP_LOG, "Failed to create StorageMng !");
        application_destroy (app);
        return -1;
    }


    app->cmd_server = tbfs_cmd_server_create (app);
    if (!app->cmd_server) {
        LOG_err (APP_LOG, "Failed to create CmdServer !");
        application_destroy (app);
        return -1;
    }

    app->peer_server = tbfs_peer_server_create (app);
    if (!app->peer_server) {
        LOG_err (APP_LOG, "Failed to create PeerServer !");
        application_destroy (app);
        return -1;
    }

    app->tracker_client = tbfs_tracker_client_create (app);
    if (!app->tracker_client) {
        LOG_err (APP_LOG, "Failed to create TrackerClient !");
        application_destroy (app);
        return -1;
    }

    if (!conf_get_boolean (app->conf, "app.foreground"))
        wutils_daemonize ();

    // start the loop
    event_base_dispatch (app->evbase);

    application_destroy (app);

    return 0;
}
