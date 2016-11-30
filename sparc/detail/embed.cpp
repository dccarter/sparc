//
// Created by dc on 11/25/16.
//

#include <signal.h>
#include <sparc.h>
#include <netdb.h>
#include <sys/stat.h>

#include "kore.h"
#include "app.h"
#if !defined(KORE_NO_HTTP)
#include "http.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif
volatile sig_atomic_t   sig_recv;
struct listener_head    listeners;
u_int8_t                nlisteners;
pid_t                   kore_pid = -1;
u_int16_t               cpu_count = 1;
int                     foreground = 0;
int                     kore_debug = 0;
u_int8_t                worker_count = 0;
int                     skip_chroot = 0;
char                    *chroot_path = NULL;
int                     skip_runas = 0;
char                    *runas_user = NULL;
u_int32_t               kore_socket_backlog = 5000;
char                    *kore_pidfile = (char *) KORE_PIDFILE_DEFAULT;
char                    *kore_tls_cipher_list = (char *) KORE_DEFAULT_CIPHER_LIST;
extern char             *__progname;

static void     usage(void);
static void     version(void);
static void     kore_server_start(void);
static void     kore_write_kore_pid(void);
static void     kore_server_sslstart(void);

int
kore_server_bind(const char *ip, const char *port, const char *ccb)
{
    struct listener        *l;
    int            on, r;
    struct addrinfo        hints, *results;

    kore_debug("kore_server_bind(%s, %s)", ip, port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    r = getaddrinfo(ip, port, &hints, &results);
    if (r != 0)
        fatal("getaddrinfo(%s): %s", ip, gai_strerror(r));

    l = (listener *) kore_malloc(sizeof(struct listener));
    l->type = KORE_TYPE_LISTENER;
    l->addrtype = (u_int8_t) results->ai_family;

    if (l->addrtype != AF_INET && l->addrtype != AF_INET6)
        fatal("getaddrinfo(): unknown address family %d", l->addrtype);

    if ((l->fd = socket(results->ai_family, SOCK_STREAM, 0)) == -1) {
        kore_free(l);
        freeaddrinfo(results);
        kore_debug("socket(): %s", errno_s);
        printf("failed to create socket: %s\n", errno_s);
        return (KORE_RESULT_ERROR);
    }

    if (!kore_connection_nonblock(l->fd, 1)) {
        kore_free(l);
        freeaddrinfo(results);
        printf("failed to make socket non blocking: %s\n", errno_s);
        return (KORE_RESULT_ERROR);
    }

    on = 1;
    if (setsockopt(l->fd, SOL_SOCKET,
                   SO_REUSEADDR, (const char *)&on, sizeof(on)) == -1) {
        close(l->fd);
        kore_free(l);
        freeaddrinfo(results);
        kore_debug("setsockopt(): %s", errno_s);
        printf("failed to set SO_REUSEADDR: %s\n", errno_s);
        return (KORE_RESULT_ERROR);
    }

    if (bind(l->fd, results->ai_addr, results->ai_addrlen) == -1) {
        close(l->fd);
        kore_free(l);
        freeaddrinfo(results);
        kore_debug("bind(): %s", errno_s);
        printf("failed to bind to %s port %s: %s\n", ip, port, errno_s);
        return (KORE_RESULT_ERROR);
    }

    freeaddrinfo(results);

    if (listen(l->fd, kore_socket_backlog) == -1) {
        close(l->fd);
        kore_free(l);
        kore_debug("listen(): %s", errno_s);
        printf("failed to listen on socket: %s\n", errno_s);
        return (KORE_RESULT_ERROR);
    }

    if (ccb != NULL) {
        *(void **)&(l->connect) = kore_module_getsym(ccb);
        if (l->connect == NULL) {
            printf("no such callback: '%s'\n", ccb);
            close(l->fd);
            kore_free(l);
            return (KORE_RESULT_ERROR);
        }
    } else {
        l->connect = NULL;
    }

    nlisteners++;
    LIST_INSERT_HEAD(&listeners, l, list);

    if (foreground) {
#if !defined(KORE_NO_TLS)
        kore_log(LOG_NOTICE, "running on https://%s:%s", ip, port);
#else
        kore_log(LOG_NOTICE, "running on http://%s:%s", ip, port);
#endif
    }

    return (KORE_RESULT_OK);
}

void
kore_listener_cleanup(void)
{
    struct listener        *l;

    while (!LIST_EMPTY(&listeners)) {
        l = LIST_FIRST(&listeners);
        LIST_REMOVE(l, list);
        close(l->fd);
        kore_free(l);
    }
}

void
kore_signal(int sig)
{
    sig_recv = sig;
}

static void
kore_server_sslstart(void)
{
#if !defined(KORE_NO_TLS)
    kore_debug("kore_server_sslstart()");

    SSL_library_init();
    SSL_load_error_strings();
#endif
}

static void
kore_server_start(void)
{
    u_int32_t    tmp;
    int        quit;

    if (foreground == 0 && daemon(1, 1) == -1)
        fatal("cannot daemon(): %s", errno_s);

    kore_pid = getpid();
    if (!foreground)
        kore_write_kore_pid();

    kore_log(LOG_NOTICE, "%s is starting up", __progname);
#if defined(KORE_USE_PGSQL)
    kore_log(LOG_NOTICE, "pgsql built-in enabled");
#endif
#if defined(KORE_USE_TASKS)
    kore_log(LOG_NOTICE, "tasks built-in enabled");
#endif
#if defined(KORE_USE_JSONRPC)
    kore_log(LOG_NOTICE, "jsonrpc built-in enabled");
#endif

    kore_platform_proctitle("kore [parent]");
    kore_msg_init();
    kore_worker_init();

    /* Set worker_max_connections for kore_connection_init(). */
    tmp = worker_max_connections;
    worker_max_connections = worker_count;

    net_init();
    kore_connection_init();
    kore_platform_event_init();
    kore_msg_parent_init();

    quit = 0;
    worker_max_connections = tmp;
    while (quit != 1) {
        if (sig_recv != 0) {
            switch (sig_recv) {
                case SIGHUP:
#if !defined(KORE_SINGLE_BINARY)
                    kore_worker_dispatch_signal(sig_recv);
                    kore_module_reload(0);
#endif
                    break;
                case SIGINT:
                case SIGQUIT:
                case SIGTERM:
                    quit = 1;
                    kore_worker_dispatch_signal(sig_recv);
                    continue;
                default:
                    kore_log(LOG_NOTICE,
                             "no action taken for signal %d", sig_recv);
                    break;
            }

            sig_recv = 0;
        }

        kore_worker_wait(0);
        kore_platform_event_wait(100);
        kore_connection_prune(KORE_CONNECTION_PRUNE_DISCONNECT);
    }

    kore_platform_event_cleanup();
    kore_connection_cleanup();
    kore_domain_cleanup();
    net_cleanup();
}

static void
kore_write_kore_pid(void)
{
    FILE        *fp;

    if ((fp = fopen(kore_pidfile, "w+")) == NULL) {
        printf("warning: couldn't write pid to %s (%s)\n",
               kore_pidfile, errno_s);
    } else {
        fprintf(fp, "%d\n", kore_pid);
        fclose(fp);
    }
}

#ifdef __cplusplus
}
#endif

namespace sparc {

    namespace detail {
        bool App::init() {
            if (SPARC == NULL) {
                kore_mem_init();
                SPARC = new App;

                kore_pid    = getpid();

                nlisteners  = 0;
                LIST_INIT(&listeners);

#if !defined(KORE_NO_HTTP)
                // not actually using kore's auth and validation
                kore_auth_init();
                kore_validator_init();
#endif
                kore_domain_init();
                kore_module_init();
                kore_server_sslstart();

                // following kore's single binary architecture
                kore_module_load(NULL, NULL);

                // load default or initial configuration
                return SPARC->initConfig();
            }

            return false;
        }

        int App::run() {
            $notice("Server starting on %s:%s", config::ip(), config::port());
            // bind server address and port to server

            if (!kore_server_bind(config::ip(), config::port(), NULL)) {
                $error("Starting embedded server failed: %s", errno_s);
                return errno;
            }

            kore_platform_init();

#if !defined(KORE_NO_HTTP)
            kore_accesslog_init();
            if (http_body_disk_offload > 0) {
                if (mkdir(http_body_disk_path, 0700) != -1 && errno != EEXIST) {
                    $error("error creating directory http_body_disk_path '%s': %s",
                                http_body_disk_path, errno_s);
                    return EXIT_FAILURE;
                }
            }
#endif
            sig_recv = 0;
            signal(SIGHUP, kore_signal);
            signal(SIGQUIT, kore_signal);
            signal(SIGTERM, kore_signal);

            if (foreground)
                signal(SIGINT, kore_signal);
            else
                signal(SIGINT, SIG_IGN);

            kore_server_start();

            kore_log(LOG_NOTICE, "server shutting down");
            kore_worker_shutdown();

            if (!foreground)
                unlink(kore_pidfile);

            kore_listener_cleanup();
            kore_log(LOG_NOTICE, "goodbye");

            return (0);
        }

        bool App::initConfig() {
            // setup minimal default configuration
            config::ip("127.0.0.1");
            config::port("1080");
            config::domain("127.0.0.1");
            config::workers(1);
            config::setForeground($ON);
            config::skipRunAs($ON);
            config::skipChroot($ON);

            if (primary_dom) {
                int status = kore_module_handler_new("/",
                                         config::domain(),
                                        "sparcxx",
                                        NULL, HANDLER_TYPE_DYNAMIC);

                if (status != KORE_RESULT_OK) {
                    $error("App::config - registering root handler with kore failed\n");
                    return false;
                }
            } else {
                $error("App::config - there is no registered domain");
            }
        }
    }

    void $enter(int argc, char *argv[]) {
        kore_debug("entering service context argc=%d, argv[0]=%d", argc, argv[0]);
        if (!detail::App::init()) {
            fatal("initializing service failed");
        }
    }

    int $exit() {
        try {
            return  detail::App::app()->run();
        } catch (std::exception& ex) {
            $warn("unhandled exception: %s", ex.what());
            return EXIT_FAILURE;
        }
    }

    namespace config {

        static inline detail::App *app() {
            return detail::App::app();
        }

        static inline int64_t configureInt64(int64_t *ptr, int64_t v) {
            if (v >= 0)
                *ptr = v;
            return *ptr;
        }

        static inline cc_string configString(c_string *ptr, cc_string v) {
            if (v) {
                if (*ptr) kore_free(*ptr);
                *ptr = kore_strdup(v);
            }
            return *ptr;
        }

        cc_string ip(cc_string ip) {
            return configString(&app()->bindIp, ip);
        }

        cc_string port(cc_string port) {
            return configString(&app()->bindPort, port);
        }

        cc_string domain(cc_string domainName) {
            if (domainName != NULL) {
                if (!primary_dom)
                    kore_domain_new(domainName);
                else {
                    if (primary_dom->domain)
                        kore_free(primary_dom->domain);
                    primary_dom->domain = kore_strdup(domainName);
                }
            }
            return primary_dom? primary_dom->domain : NULL;
        }

        cc_string user(cc_string user) {
            return configString(&runas_user, user);
        }

        cc_string pidFile(cc_string pf) {
            return configString(&kore_pidfile, pf);
        }

        cc_string accessLog(cc_string acessLog) {
            return configString(&app()->accessLog, acessLog);
        }

        cc_string httpBodyDiskPath(cc_string path) {
            return configString(&http_body_disk_path, path);
        }

        cc_string chroot(c_string path) {
            return configString(&chroot_path, path);
        }

        u_int16_t maxHttpHeader(int16_t v) {
            if (v >= 0) http_header_max = (u_int16_t) v;
            return http_header_max;
        }

        u_int64_t maxHttpBody(int64_t v) {
            if (v >= 0) http_body_max = (u_int64_t) v;
            return http_body_max;
        }

        u_int64_t httpBodyDiskOffload(int64_t v) {
            if (v >= 0) http_body_disk_offload = (u_int64_t) v;
            return http_body_disk_offload;
        }

        u_int64_t httpHsts(int64_t v) {
            if (v >= 0) http_hsts_enable = (u_int64_t) v;
            return http_hsts_enable;
        }

        u_int16_t httpKeepAliveTime(int32_t v) {
            if (v >= 0) http_keepalive_time = (u_int16_t) v;
            return http_keepalive_time;
        }

        u_int32_t httpRequestLimit(int32_t v) {
            if (v >= 0) http_request_limit = (u_int32_t) v;
            return http_request_limit;
        }

        u_int64_t webSocketMaxFrame(int64_t v) {
            if (v >= 0) kore_websocket_maxframe = (u_int64_t) v;
            return kore_websocket_maxframe;
        }

        u_int64_t webSocketTimeout(int64_t v) {
            if (v >= 0) kore_websocket_timeout = (u_int64_t) v;
            return kore_websocket_timeout;
        }

        u_int8_t workers(int8_t v) {
            if (v >= 0) worker_count = (u_int8_t) v;
            return worker_count;
        }

        u_int32_t workerMaxConnections(int32_t v) {
            if (v >= 0) worker_max_connections = (u_int32_t) v;
            return worker_max_connections;
        }

        u_int32_t rlimitNoFiles(int32_t v) {
            if (v >= 0) worker_rlimit_nofiles = (u_int32_t) v;
            return worker_rlimit_nofiles;
        }

        u_int32_t workerAcceptThreshold(int32_t v) {
            if (v >= 0) worker_accept_threshold = (u_int32_t) v;
            return worker_accept_threshold;
        }

        u_int8_t workerSetAffinity(int8_t v) {
            if (v >= 0) worker_set_affinity = (u_int8_t) v;
            return worker_set_affinity;
        }

        u_int32_t socketBacklog(int32_t v) {
            if (v >= 0) kore_socket_backlog = (u_int32_t) v;
            return kore_socket_backlog;
        }

        u_int8_t  setForeground(int8_t v) {
            if (v >= 0) foreground = (u_int8_t) v;
            return (u_int8_t) foreground;
        }

        u_int8_t  debug(int8_t v) {
            if (v >= 0) kore_debug = (u_int8_t) v;
            return kore_debug;
        }

        u_int8_t  skipRunAs(int8_t v) {
            if (v >= 0) skip_runas = (u_int8_t) v;
            return (u_int8_t) skip_runas;
        }

        u_int8_t  skipChroot(int8_t v) {
            if (v >= 0) skip_chroot = (u_int8_t) v;
            return (u_int8_t) skip_chroot;
        }
    }

#if !defined(KORE_NO_TLS)
    namespace tls {
        cc_string certificateFile(cc_string cert) {
            if (cert != NULL) {
                if (primary_dom == NULL) {
                    cprint(CRED, "certificate files should be setup after initializing domain\n");
                    throw std::runtime_error("invalid call context");
                }

                if (primary_dom->certfile) {
                    cprint(CRED, "TLS - certificate file already set\n");
                    throw std::runtime_error("invalid call context");
                }

                primary_dom->certfile = kore_strdup(cert);
            }

            return primary_dom ? primary_dom->certfile : NULL;
        }

        cc_string certificateKey(cc_string key) {
            if (key) {
                if (primary_dom == NULL) {
                    cprint(CRED, "certificate files should be setup after initializing domain\n");
                    throw tls::TlsConfigurationException("invalid call context");
                }

                if (primary_dom->certkey) {
                    cprint(CRED, "TLS - certificate key already set\n");
                    throw tls::TlsConfigurationException("invalid call context");
                }

                primary_dom->certkey = kore_strdup(key);
            }
        }

        void clientCertificate(cc_string cafile, cc_string crlfile) {
            if (primary_dom == NULL) {
                cprint(CRED, "certificate files should be setup after initializing domain\n");
                throw tls::TlsConfigurationException("invalid call context");
            }

            if (cafile != NULL) {
                if (primary_dom->certkey) {
                    cprint(CRED, "TLS - cafile already set\n");
                    throw tls::TlsConfigurationException("invalid call context");
                }
                primary_dom->cafile = kore_strdup(cafile);
            } else
                cprint(CRED, "TLS - invalid cafile argument (null) \n");

            if (crlfile != NULL) {
                if (primary_dom->certkey) {
                    cprint(CRED, "TLS - cafile already set\n");
                    throw tls::TlsConfigurationException("invalid call context");
                }
                primary_dom->crlfile = kore_strdup(crlfile);
            }
        }

        cc_string cipher(cc_string cipherlist) {
            return configString(&kore_tls_cipher_list, cipherlist);
        }

        void dhparam(cc_string path) {
            if (path) {
                BIO *bio;
                if (tls_dhparam != NULL) {
                    cprint(CRED, "TLS::dhparam - already set\n");
                    throw tls::TlsConfigurationException("invalid call context");
                }

                bio = BIO_new_file(path, "r");
                if (!bio) {
                    cprint(CRED, "TLS::dhparam - % does not exist\n", path);
                    throw tls::TlsConfigurationException("file does not exist");
                }

                tls_dhparam = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
                BIO_free(bio);
                if (!tls_dhparam) {
                    cprint(CRED, "TLS::dhparam - %s\n", errno_s);
                    throw tls::TlsConfigurationException("file does not exist");
                }
            }
        }

        int version(int v) {
            if (v >= 0) {
                switch (v) {
                    case KORE_TLS_VERSION_1_0:
                    case KORE_TLS_VERSION_1_2:
                    case KORE_TLS_VERSION_BOTH:
                        tls_version = v;
                        break;
                    default:
                        cprint(CRED, "Unsupported TLS version %d\n", v);
                        throw tls::TlsConfigurationException("unsupported TLS version");
                }
            }
            return tls_version;
        }
    }
#endif

}
