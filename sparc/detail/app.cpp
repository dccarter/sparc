//
// Created by dc on 11/24/16.
//

#include "app.h"
#include "http.h"
#include "websocket.h"
#include "kore.h"
#include "pgsql.h"
#include "version.h"

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

    Request::~Request() {}
    Response::~Response() {}

    namespace detail {

        App::App()
            : router_{},
              bgTimerManager_{},
              sessionManager_(&sessionTimeout),
              sfRouter_(2048),
              dbManager_(0)
        {
            TAILQ_INIT(&after_);
            TAILQ_INIT(&before_);
        }

        int App::nextState(struct http_request *raw) {

            if (raw->data[0]) {
                HttpRequest *request = (HttpRequest *) raw->data[0];
                struct http_state *states = request->asyncStates;
                if (states) {
                    return http_state_run(states, request->nAsyncStates, raw);
                }
            }
            $warn("error transitioning to next state, request to found");
            return (HTTP_STATE_COMPLETE);
        }

        void App::asyncError(struct http_request *raw) {
            if (raw->data[0] != NULL) {
                Request *req = (HttpRequest *) raw->data[0];
                delete  req;
                raw->data[0] = NULL;
            }

            if (raw->data[0] != NULL) {
                Response *resp = (HttpResponse *) raw->data[1];
                delete  resp;
                raw->data[1] = NULL;
            }
        }

        int App::handle(http_request *raw) {
            HttpRequest     *req = new HttpRequest(raw), &request = *req;
            HttpResponse    *resp = new HttpResponse(raw), &response = *resp;
            int status      = OK, ret = $CONTINUE, flash = 0;
            RouteHandler    *handler;
            static_file     *sf = NULL;
            c_string       tok;

            kore_debug("handle request(%s:%p)", raw->path, raw);
            // append server header
            resp->header("server", SPARC_SERVER_VERSION);

            tok = strchr(raw->path, '.');
            if (tok && strchr(tok, '/')) {
                tok = NULL;
            }

            if (tok) {

                flash = 1;
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
                if (status == $ABORT) {
                    flash = 1;
                    goto app_handle_exit;
                }

                status = callRequestHandler(handler, request, response);
                if (status == $ASYNC) {
                    struct http_state *states = request.asyncStates;
                    if (states) {
                        raw->data[0] = req;
                        raw->data[1] = resp;
                        return http_state_run(states, request.nAsyncStates, raw);
                    }
                    $warn("unable to complete asynchronous request, no states provided");
                    response.status(HTTP_STATUS_INTERNAL_ERROR);
                    // if request data was already set free the data
                    if (request.asyncCleanup_ && raw->hdlr_extra) {
                        kore_debug("cleaning up request data");
                        request.asyncCleanup_(raw->hdlr_extra);
                        raw->hdlr_extra = NULL;
                    }
                }
                else {
                    return completeRequest(req, resp, handler, status);
                }
            }
        app_handle_exit:
            kore_debug("handled request(%s:%p) status=%d", raw->path, raw, status);
            if (flash)
                response.flush();

            delete req;
            delete resp;
            return KORE_RESULT_OK;
        }

        int App::completeRequest(Request *req, Response *resp, RouteHandler *rh, int status) {

            int ret = $CONTINUE;
            ResponseTransformer *rt = rh->transformer;
            HttpResponse& response = (HttpResponse&) *resp;
            HttpRequest& request = (HttpRequest&) *req;

            if (rt) {
                size_t hint;
                hint = rt->sizeHint(response.body().offset());
                buffer b(hint);
                status = rt->transform(response, b);
                if (status == $ABORT)
                    goto app_handle_exit;
                response.swapBody(b);
            }

            if (rh->contentType) {
                response.header("Content-Type", rh->contentType);
            }
            response.status(status);
            ret = callFidecs(&after_, rh->prefix, request, response);
            if (ret >= HTTP_STATUS_OK)
                response.status(ret);
            else {
                switch (ret) {
                    case $CONTINUE:
                    case $SKIP_FLUSH:
                        break;
                    case $ASYNC:
                        $warn("fidecs returned $ASYNC which is not allowed in a FIDEC");
                    case $ABORT:
                        response.status(HTTP_STATUS_INTERNAL_ERROR);
                        break;
                    default:
                        $warn("unknown return code from fidec %d", ret);
                        response.status(HTTP_STATUS_INTERNAL_ERROR);
                        break;
                }
            }

        app_handle_exit:
            kore_debug("handled request(%s:%p) status=%d",
                       request.raw()->path, request.raw(), status);
            if (ret != $SKIP_FLUSH)
                response.flush();

            delete req;
            delete resp;
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

        db::DbManager& App::dbManager() {
            return dbManager_;
        }

        StaticFilesRouter& App::staticFiles() {
            return sfRouter_;
        }

        HaltException::HaltException(int status, cc_string msg)
            : std::runtime_error(msg),
              status_(status)
        {}

        InitializetionException::InitializetionException(int exitCode, cc_string msg)
            : std::runtime_error(msg),
              exitCode_(exitCode)
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

    int __id() {
        return worker->id;
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
               const  detail::HttpRequest& hreq = (detail::HttpRequest&) req;
                if (hreq.handler()->data[0]) {
                    hreq.raw()->owner->data[0] = hreq.handler()->data[0];
                    kore_websocket_handshake(hreq.raw(), &wscbs);
                    return $SKIP_FLUSH;
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

    namespace db {

        Result query(int dbid, Sql& sql) {
            detail::App *app = detail::App::app();
            if (app) {
                Context *ctx = app->dbManager().get(dbid);
                if (ctx) {
                    Result res = ctx->exec(sql);
                    sql.setResult(res);
                    return res;
                }
            }
            kore_debug("database with index %d was not found", dbid);
            return nullptr;
        }

        int    query(int dbid, Sql& sql, OnAsync onAsync) {
            detail::App *app = detail::App::app();
            if (app) {
                Context *ctx = app->dbManager().get(dbid);
                if (ctx)
                    return ctx->exec(std::move(sql), onAsync);
            }
            kore_debug("database with index %d was not found", dbid);
            return 0;
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
    sparc::detail::App::app()->dbManager().init();
    // invoke registered onload
    sparc::detail::App::app()->onLoad();
}

int
sparcxx(http_request *req)
{
    sparc::detail::App *app = sparc::detail::App::app();
    kore_debug("routing request (%p)", req);
    if (app == NULL)
        return (KORE_RESULT_ERROR);
    if (req->hdlr_extra == NULL)
        return app->handle(req);
    else
        return app->nextState(req);
}
