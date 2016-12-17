//
// Created by dc on 11/24/16.
//

#include "app.h"
#include "kore.h"
#include "http.h"
#include "sparc.h"
#include "websocket.h"

#ifdef __cplusplus
extern "C" {
#endif
struct kore_wscbs wscbs = {
        on_websocket_connect,
        on_websocket_message,
        on_websocket_disconnect
};
#ifdef __cplusplus
};
#endif

namespace sparc {
    namespace detail {

        App::App()
            : router_{},
              bgTimerManager_{},
              sessionManager_(sessionTimeout),
              sfRouter_(2048)
        {
            TAILQ_INIT(&after_);
            TAILQ_INIT(&before_);
        }

        int App::handle(http_request *raw) {
            HttpRequest     request(raw);
            HttpResponse    response(raw);
            int status      = OK, ret = $CONTINUE;
            RouteHandler    *handler;
            static_file     *sf = NULL;
            c_string       tok;

            kore_debug("handle request(%s:%p)", raw->path, raw);

            tok = strchr(raw->path, '.');
            if (tok) {
                if (strchr(tok, '/')) {
                    tok = NULL;
                }
            }
            if (tok) {
                kore_debug("assuming static file %s", raw->path);
                sf = sfRouter_.find(raw->path);
                if (sf == NULL) {
                    kore_debug("no handler for static request (%s:%p)", raw->path, raw);
                    response.end(NOT_FOUND);
                    goto app_handle_exit;
                }

                // set content type header
                response.header("Content-Type", sf->content_type);
                status = sfRouter_.applyHeaders(request, response, sf);
                if (status == NOT_MODIFIED)
                    response.end(NOT_MODIFIED);
                else
                    response.end(OK, sf->ptr, sf->len);
                status = $ABORT;
                goto app_handle_exit;

            } else {

                handler = request.resolveRoute(&router_);
                kore_debug("route handler for request(%s:%p) %p", raw->path, raw, handler);
                if (handler == NULL) {
                    kore_debug("no handler for request (%s:%p)", raw->path, raw);
                    response.end(NOT_FOUND);
                    goto app_handle_exit;
                }

                kore_debug("calling fidecs request(%s:%p)", raw->path, raw);
                status = callFidecs(&before_, handler->prefix, request, response);
                if (status == $ABORT)
                    goto app_handle_exit;

                status = callRequestHandler(handler, request, response);
                if (status == $ABORT)
                    goto app_handle_exit;

                ResponseTransformer *rt = handler->transformer;
                if (rt) {
                    size_t hint;
                    hint = rt->sizeHint(response.body().offset());
                    buffer b(hint);
                    status = rt->transform(response, b);
                    if (status == $ABORT)
                        goto app_handle_exit;
                    response.swapBody(b);
                }
                if (handler->contentType) {
                    response.header("Content-Type", handler->contentType);
                }
                response.status(status);
                ret = callFidecs(&after_, handler->prefix, request, response);
                response.status(ret);
            }
        app_handle_exit:
            kore_debug("handled request(%s:%p) status=%d", raw->path, raw, status);
            if (status != _SKIP_FLUSH)
                response.flush();

            return KORE_RESULT_OK;
        }

        int App::callFidecs(Fidecs *fidecs, cc_string prefix,
                                  HttpRequest& req, HttpResponse& resp) {
            Fidec *fc;
            int   status    = $CONTINUE;
            size_t plen;

            kore_debug("%s(%p:%p)", __func__, fidecs, prefix);

            plen = strlen(prefix);
            TAILQ_FOREACH(fc, fidecs, link) {
                if (fc->match != NULL) {
                    size_t len = strlen(fc->match);
                    kore_debug("matching fidec(%p:%s:%d) to (%s:%d)",
                                fc, fc->match, plen,  prefix, len);
                    // if a route starts with Fidec's prefix then it's a match
                    len = len > plen ? 0 : (size_t) !strncasecmp(fc->match, prefix, len);
                    if (!len) continue;
                }

                try {
                    kore_debug("calling fidec(%p)", fc->h);
                    status = fc->h(req, resp);
                    if (status == $ABORT || resp.ended()) {
                        kore_debug("fidec(%p) aborted pipeline, status = %d", status);
                        break;
                    }
                } catch (HaltException& hex) {
                    status = hex.status() < OK? INTERNAL_ERROR : hex.status();
                    if (hex.what()) {
                        kore_debug("halt(%d): ", status, hex.what());
                        resp.end(status, hex.what());
                    } else {
                        resp.end(status);
                    }
                    status = $ABORT;
                } catch (std::exception &ex) {
                    kore_debug("fidec(%p) unhandled exception: %s", fc->h, ex.what());
                    status = INTERNAL_ERROR;
                    break;
                }
            }

            return status;
        }

        int App::callRequestHandler(RouteHandler *hh, HttpRequest &req, HttpResponse &res) {
            int status = OK;
            try {
                if (hh->h) {
                    status = hh->h(req, res);
                }
            } catch (HaltException& hex) {
                status = hex.status() < OK? INTERNAL_ERROR : hex.status();
                if (hex.what()) {
                    kore_debug("halt(%d): ", status, hex.what());
                    res.end(status, hex.what());
                } else {
                    res.end(status);
                }
                status = $ABORT;
            } catch (std::exception& ex) {
                kore_debug("calling handler(%d:%s) failed", hh->m, hh->route);
                status = $ABORT;
            }
            return status;
        }

        void App::addFidec(bool bf, Fidec *fidec) {
            if (bf) {
                kore_debug("before handler(%p)", fidec);
                TAILQ_INSERT_TAIL(&before_, fidec, link);
            } else {
                kore_debug("after handler(%p)", fidec);
                TAILQ_INSERT_TAIL(&after_, fidec, link);
            }
        }

        HttpSessionManager& App::sessionManager() {
            return sessionManager_;
        }

        BgTimerManager& App::timerManager() {
            return bgTimerManager_;
        }

        StaticFilesRouter& App::staticFiles() {
            return sfRouter_;
        }

        HaltException::HaltException(int status, cc_string msg)
            : std::runtime_error(msg),
              status_(status)
        {}

        InitializetionException::InitializetionException(int exitCode, cc_string msg)
            : std::runtime_error(msg)
        {}
    }

    void __route(const Method m, cc_string route, cc_string type,
                 handler hh, ResponseTransformer *trans) {

        if (route && hh) {
            detail::Route *r;
            detail::Router &router = detail::App::app()->router();
            r = router.add(m, route, hh, type, trans);
            if (r == NULL) {
                throw detail::InitializetionException(
                        KORE_RESULT_ERROR, "Adding route failed, ensure there are not conflicts");
            }
        } else {
            throw detail::InitializetionException(
                    KORE_RESULT_ERROR, "Adding route failed: invalid route add parameters");
        }
    }

    void __fidec(bool b, cc_string match, handler hh) {
        if (hh) {
            detail::Fidec *fidec = new detail::Fidec;
            fidec->match = match ? kore_strdup(match) : NULL;
            fidec->h = hh;
            detail::App::app()->addFidec(b, fidec);
        } else {
            throw detail::InitializetionException(
                    KORE_RESULT_ERROR, "Adding Codec/Filter failed: invalid parameters");
        }
    }

    void __halt(u_int16_t status, cc_string message) {
        throw detail::HaltException(status, message);
    }


    void $timeout(timerid_t::timedout th, u_int64_t tout, int flags ) {
        if (detail::App::initialized()) {
            detail::BgTimerManager& mgr = detail::App::app()->timerManager();
            mgr.schedule(th, tout, flags);
        } else {
            fatal("Scheduling timer while not initialized prohibited");
        }
    }

    void webSocket(cc_string path, WsOnMessage onMessage) {
        webSocket(path, onMessage, NULL, NULL);
    }

    void webSocket(cc_string route, WsOnMessage onMessage, WsOnConnect onConnect, WsOnDisconnect onDisconnect) {
        if (route && onMessage) {
            detail::Route *r;
            detail::Router &router = detail::App::app()->router();

            detail::WebSocketHandler *h = (detail::WebSocketHandler *) kore_malloc(sizeof(detail::WebSocketHandler));
            h->onConnect = onConnect;
            h->onDisconnect = onDisconnect;
            h->onMessage = onMessage;

            r = router.add(GET, route, $(req, res) {
                detail::HttpRequest& hreq = (detail::HttpRequest&) req;
                if (hreq.handler()->data[0]) {
                    hreq.raw()->owner->data[0] = hreq.handler()->data[0];
                    kore_websocket_handshake(hreq.raw(), &wscbs);
                    return _SKIP_FLUSH;
                }
                return $ABORT;
            }, NULL, NULL, h);

            if (r == NULL) {
                delete h;
                throw detail::InitializetionException(
                        KORE_RESULT_ERROR, "Adding route failed, ensure there are not conflicts");
            }
        } else {
            throw detail::InitializetionException(
                    KORE_RESULT_ERROR, "Adding route failed: invalid route add parameters");
        }
    }

    namespace staticFiles {
        void location(cc_string loc) {
            if (detail::App::initialized()) {
                char    cdir[128];
                char    dir[256],  buf[256];
                char    *resolved;
                int     nsize;

                cdir[0] = '/';
                if (getcwd(cdir, sizeof(cdir)) == NULL || cdir[0] != '/') {
                    $error("setting location failed: %s", errno_s);
                    return;
                }

                // this set the directory relative to assets in working diretory
                nsize = snprintf(&dir[0], sizeof(dir), "%s/assets%s", cdir, loc);
                if (nsize < 0) {
                    $error("setting static file location failed: %s", errno_s);
                    return;;
                }
                resolved = realpath(dir, buf);
                if (resolved == NULL || strncmp(resolved, cdir, strlen(cdir))) {
                    $error("setting static files location failed, bad directory '%s': %s", dir, errno_s);
                    kore_debug("cdir=%s, dir=%s, resolved=%s, loc=%s", cdir, dir, resolved, loc);
                    return;
                }
                detail::App::app()->staticFiles().init(resolved);
            } else {
                fatal("Static files setting before initialization prohibited");
            }
        }
        void externalLocation(cc_string loc) {
            if (detail::App::initialized()) {
                char buf[256], *resolved;
                resolved = realpath(loc, buf);
                if (resolved == NULL) {
                    $error("setting static files location failed, bad directory '%s': %s", loc, errno_s);
                    return;
                }
                detail::App::app()->staticFiles().init(resolved);
            } else {
                fatal("Static files setting before initialization prohibited");
            }
        }

        void expireTime(const u_int64_t expire) {
            if (detail::App::initialized()) {
                detail::App::app()->staticFiles().exipires(expire);
            } else {
                fatal("Static files setting before initialization prohibited");
            }
        }
        void header(cc_string name, cc_string value) {
            if (detail::App::initialized()) {
                detail::App::app()->staticFiles().header(name,  value);
            } else {
                fatal("Static files setting before initialization prohibited");
            }
        }
    }
}

sparc::detail::App
        *sparc::detail::App::SPARC = NULL;

void
kore_onload()
{
    // flush timers that were cached
    sparc::detail::App::app()->timerManager().flush();
}

int
sparcxx(http_request *req)
{
    sparc::detail::App *app = sparc::detail::App::app();
    kore_debug("routing request (%p)", req);
    if (app == NULL)
        return (KORE_RESULT_ERROR);
    return app->handle(req);
}