//
// Created by dc on 11/24/16.
//

#include <sys/param.h>
#include "kore.h"
#include "http.h"
#include "app.h"

namespace sparc {
    namespace detail {

        HttpRequest::HttpRequest(http_request *req)
            : req_(req),
              params_(NULL),
              tokenized_(NULL),
              ip_(NULL),
              session_(NULL),
              cookieStr_(NULL),
              cookies_(NULL),
              hanadler_(NULL),
              bodyLoaded_(false),
              body_(NULL),
              json_(NULL),
              argsLoaded_(false)
        {}

        HttpRequest::~HttpRequest() {
            if (ip_)
                kore_free(ip_);
            if (tokenized_)
                kore_free(tokenized_);
            if (cookies_)
                kore_free(cookies_);
            if (cookieStr_)
                kore_free(cookieStr_);
            if (params_)
                kore_free(params_);
            if (json_) {
                delete json_;
                json_ = NULL;
            }
            if (body_) {
                kore_buf_free(body_);
                body_ = NULL;
            }
            hanadler_ = NULL;
            session_ = NULL;
            params_ = NULL;
            cookies_ = NULL;
            cookieStr_ = NULL;
            tokenized_ = NULL;
        }

        cc_string HttpRequest::body() {
            ssize_t  ret  = 0;
            char     data[BUFSIZ];

            if (!bodyLoaded_ && body_ == NULL) {
                bodyLoaded_ = true;

                body_ = kore_buf_alloc(MIN(req_->content_length, BUFSIZ));
                for (;;) {
                    ret = http_body_read(req_, data, sizeof(data));
                    if (ret == -1) {
                        kore_buf_free(body_);
                        body_ = NULL;
                        $error("reading request body failed: %s", errno_s);
                        return NULL;
                    }

                    if (ret == 0)
                        break;
                    kore_buf_append(body_, data, ret);
                }
            }

            return body_? kore_buf_stringify(body_, NULL) : NULL;
        }

        size_t HttpRequest::contentLength() {
            return req_->content_length;
        }

        cc_string HttpRequest::contentType() {
            return header("Content-Type");
        }

        cc_string HttpRequest::contextPath() {
            return uri();
        }

        void HttpRequest::eachHeader(cc_string name, KVIterator it) {
            if (it) {
                http_header *h;
                TAILQ_FOREACH(h, &(req_->req_headers), list) {
                    if (name != NULL && !strcasecmp(name, h->header))
                        if (!it(h->header, h->value)) break;
                }
            }
        }

        cc_string HttpRequest::header(cc_string name) {
            char *h = NULL;
            http_request_header(req_, name, &h);
            return h;
        }

        void HttpRequest::eachCookie(cc_string name, KVIterator it) {
            if (it) {
                char *k;
                int j = 0;
                if (!cookies_) parseCookies();

                while ((k = cookies_[j]) != NULL) {
                    if (name != NULL && !strcmp(name, k))
                        if (!it(k, cookies_[j+1])) break;

                    j+=2;
                }
            }
        }

        cc_string HttpRequest::cookie(cc_string name) {
            char *k;
            int j = 0;
            if (!cookies_) parseCookies();

            while ((k = cookies_[j]) != NULL) {
                if (!strcmp(name, k))
                    return cookies_[j+1];

                j += 2;
            }
            return NULL;
        }

        cc_string HttpRequest::ip() {
            if (ip_ == NULL) {
                char ipstr[INET6_ADDRSTRLEN];
                if (req_->owner->addrtype == AF_INET6)
                    inet_ntop(AF_INET6, &req_->owner->addr.ipv6.sin6_addr, ipstr, INET6_ADDRSTRLEN);
                else
                    inet_ntop(AF_INET, &req_->owner->addr.ipv4.sin_addr, ipstr, INET6_ADDRSTRLEN);

                ip_ = kore_strdup(ipstr);
            }
            return ip_;
        }

        cc_string HttpRequest::param(cc_string name) {
            int index;
            for(index = 0; index < hanadler_->nparams; index++) {
                kore_debug("handler(%s) %d %d", name, index, hanadler_->nparams);
                if (!strcmp(hanadler_->params[index], name)) {
                    return params_[index];
                }
            }
            return NULL;
        }

        void HttpRequest::eachParam(KVIterator it) {
            if (it) {
                int i;
                for (i = 0; i < hanadler_->nparams; i++) {
                    if (!it(hanadler_->params[i], params_[i])) break;
                }
            }
        }

        int HttpRequest::port() {
            if (req_->owner->addrtype == AF_INET6)
                return req_->owner->addr.ipv6.sin6_port;
            else
                return req_->owner->addr.ipv4.sin_port;
        }

        cc_string HttpRequest::protocol() {
            return "HTTP/1.1";
        }

        cc_string HttpRequest::queryParam(cc_string name) {
            http_arg *qp;
            if (!argsLoaded_) {
                http_populate_get(req_);
                argsLoaded_ = true;
            }

            TAILQ_FOREACH(qp, &req_->arguments, list) {
                if (!strcmp(name, qp->name)) return qp->s_value;
            }

            return NULL;
        }

        void HttpRequest::eachQueryParam(cc_string name, KVIterator it) {
            if (it) {
                http_arg *qp = NULL;
                TAILQ_FOREACH(qp, &req_->arguments, list) {
                    if (name != NULL && !strcmp(name, qp->name))
                        if (!it(qp->name, qp->s_value)) break;
                }
            }
        }

        Method HttpRequest::requestMethod() {
            return (Method) req_->method;
        }

        cc_string HttpRequest::scheme() {
#ifdef KORE_NO_TLS
            return "http";
#else
            return "https";
#endif
        }

        Session* HttpRequest::session(bool create) {
            if (!session_) {
                if (create)
                    session_ = App::app()->sessionManager().create(ip());
                else
                    session_ = App::app()->sessionManager().find(ip());
            }
            return session_;
        }

        cc_string HttpRequest::uri() {
            return req_->path;
        }

        cc_string HttpRequest::uril() {
            return req_->path;
        }

        cc_string HttpRequest::userAgent() {
            return req_->agent;
        }

        RouteHandler* HttpRequest::resolveRoute(Router *r) {
            if (hanadler_ == NULL && tokenized_ == NULL) {
                char *tokens[16], **tmp;
                size_t ntokens = sizeof (tokens) / sizeof(char *);
                hanadler_ = r->find(requestMethod(),
                                    req_->path, &tokenized_, tokens, &ntokens, &tmp);
                kore_debug("resolve token: %s:%d:%p", req_->path, ntokens, hanadler_);
                if (hanadler_ != NULL) {
                    unsigned i;
                    if (ntokens) {
                        params_ = (c_string *) kore_calloc(ntokens, sizeof(char *));
                        for (i = 0; i < ntokens; i++)
                            params_[i] = tmp[i];
                    }
                }
            }

            return hanadler_;
        }

        void HttpRequest::parseCookies() {
            cc_string cookieHeader;

            if (cookieStr_) return;
            cookieHeader = header("Cookie");
            if (cookieHeader) {
                char *tokens[32], *tok, c;
                int ntoks, i, j;
                cookieStr_ = kore_strdup(cookieHeader);
                ntoks = kore_split_string(cookieStr_, ";", tokens, 32);
                if (ntoks) {
                    cookies_ = (char **) kore_calloc(ntoks*2+1, sizeof(char *));
                    for (i = 0; i < ntoks; i++) {
                        tok = tokens[i];
                        for (tok; isspace(*tok); tok++);
                        cookies_[j] = tok;

                        // strsep should work well here
                        tok = strchr(tokens[i], '=');
                        if (tok) {
                            *tok = '\0';
                            cookies_[j+1] = ++tok;
                        } else
                            // in this case there was no key provided
                            cookies_[j+1] = NULL;
                        j += 2;
                    }
                    cookies_[j] = NULL;
                }
            }
        }

        Json* HttpRequest::json() {
            if (json_ == NULL) {
                cc_string rawBody = body();
                if (rawBody) {
                    json_ = Json::decode(rawBody);
                }
            }

            return json_;
        }
    }
}
