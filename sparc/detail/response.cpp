//
// Created by dc on 11/24/16.
//

#include "response.h"
#include "http.h"
#include "kore.h"

namespace sparc {
    namespace detail {

        HttpResponse::HttpResponse(http_request *req)
            : req_(req),
              status_(OK),
              body_(32),
              ended_(false)
        {
            TAILQ_INIT(&cookies_);
        }

        HttpResponse::~HttpResponse() {
            // remove all cookies
            http_cookie *c, *tmp;
            for ( c = TAILQ_FIRST(&cookies_); c != NULL; c = tmp) {
                tmp = TAILQ_NEXT(c, link);
                eraseCookie(c);
            }
        }

        buffer& HttpResponse::body() {
            return body_;
        }

        void HttpResponse::body(cc_string str) {
            body_.reset();
            body_.append(str, strlen(str));
        }

        cc_string HttpResponse::raw() {
            return body_.toString();
        }

        void HttpResponse::cookie(cc_string name,
                                  cc_string value,
                                  cc_string path,
                                  cc_string domain,
                                  int64_t expire, bool secure)
        {
            http_cookie *i, *c = NULL;
            TAILQ_FOREACH(i, &cookies_, link) {
                if (!strcmp(i->name, name)) {
                    c = i;
                    break;
                }
            }

            if (!c) {
                // adding new cookie
                c = (http_cookie *) kore_calloc(1, sizeof(http_cookie));
                c->name = kore_strdup(name);
                c->domain = domain? kore_strdup(domain) : NULL;
                c->path   = path? kore_strdup(path) : NULL;
                TAILQ_INSERT_TAIL(&cookies_, c, link);
            }

            c->value = kore_strdup(value);
            c->expiryTime = expire;
            c->secure = secure;
        }

        void HttpResponse::header(cc_string header, cc_string value) {
            http_response_header(req_, header, value);
        }

        void HttpResponse::removeCookie(cc_string name) {
            http_cookie *i, *c = NULL;
            TAILQ_FOREACH(i, &cookies_, link) {
                if (!strcmp(i->name, name)) {
                    c = i;
                    break;
                }
            }

            if (c) {
                eraseCookie(c);
            }
        }

        void HttpResponse::eraseCookie(http_cookie *c) {
            if (c) {
                TAILQ_REMOVE(&cookies_, c, link);

                if (c->domain) kore_free(c->domain);
                if (c->path) kore_free(c->path);
                if (c->name) kore_free(c->name);
                if (c->value) kore_free(c->value);

                kore_free(c);
            }
        }

        cc_string HttpResponse::header(cc_string name) {
            http_header *hh;
            TAILQ_FOREACH(hh, &(req_->resp_headers), list) {
                if (!strcasecmp(hh->header, name)) return hh->value;
            }
            return NULL;
        }

        cc_string HttpResponse::type() {
            return header("Content-Type");
        }

        void HttpResponse::type(cc_string type) {
            http_header *i, *hh = NULL;
            TAILQ_FOREACH(i, &(req_->resp_headers), list) {
                if (!strcasecmp(i->header, "Content-Type")) {
                    hh = i;
                    kore_free(i->value);
                    hh->value = kore_strdup(type);
                    break;
                }
            }

            if (!hh) {
                header("Content-Type", type);
            }
        }

        void HttpResponse::redirect(cc_string location) {
            redirect(location, FOUND);
        }

        void HttpResponse::redirect(cc_string location, const int status) {
            status_ = status;
            kore_debug("redirecting to '%s' with status code %d",
                       (char *) location, status);
            header("Location", location);
            header("Connection", "close");
            body_.reset();
        }

        void HttpResponse::flushCookies() {
            kore_buf  cbuf;
            http_cookie *cookie, *tmp;

            kore_buf_init(&cbuf, 128);
            for (cookie = TAILQ_FIRST(&cookies_); cookie != NULL; cookie = tmp) {
                tmp = TAILQ_NEXT(cookie, link);
                TAILQ_REMOVE(&cookies_, cookie, link);

                kore_buf_appendf(&cbuf, "%s=%s; ", cookie->name, cookie->value);
                kore_free(cookie->name);
                kore_free(cookie->value);

                if (cookie->domain) {
                    kore_buf_appendf(&cbuf, "Domain=%s; ", cookie->domain);
                    kore_free(cookie->domain);
                }

                if (cookie->path) {
                    kore_buf_appendf(&cbuf, "Domain=%s; ", cookie->path);
                    kore_free(cookie->path);
                }

                if (cookie->secure)
                    kore_buf_append(&cbuf, "Secure; ", sizeof("Secure; ")-1);

                if (cookie->expiryTime)
                    kore_buf_appendf(&cbuf, "Max-Age=%d", cookie->expiryTime);

                header("Set-Cookie", kore_buf_stringify(&cbuf, NULL));
                // deallocate memory for the cookie

                kore_free(cookie);
            }
            kore_buf_free(&cbuf);
        }

        void HttpResponse::end(int status, void *data, size_t len) {
            if (!ended_) {
                status_ = status;
                flushCookies();
                if (!data) {
                    data = (void *) body_.raw();
                    len  = body_.offset();
                }
                http_response(req_, status, data, len);
                ended_ = true;
                body_.reset();
            }
        }
    }
}