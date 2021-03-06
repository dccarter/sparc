//
// Created by dc on 11/23/16.
//

#ifndef SPARC_REQUEST_H
#define SPARC_REQUEST_H

#include <kore/http.h>
#include "common.h"
#include "router.h"

#ifdef __cplusplus
extern "C" {
#endif
    struct http_request;
#ifdef __cplusplus
}
#endif

namespace sparc {
    namespace detail {

        class HttpRequest : public Request {
        public:
            HttpRequest(http_request *);

            virtual cc_string body() override;

            virtual size_t contentLength() override;

            virtual cc_string contentType() override;

            virtual cc_string contextPath() override;

            virtual void eachHeader(cc_string, KVIterator) override;

            virtual cc_string header(cc_string) override;

            virtual void eachCookie(cc_string, KVIterator) override;

            virtual cc_string cookie(cc_string) override;

            virtual cc_string form(cc_string) override;

            virtual cc_string ip() override;

            virtual cc_string param(cc_string) override;

            virtual void eachParam(KVIterator) override;

            virtual int port() override;

            virtual cc_string protocol() override;

            virtual cc_string queryParam(cc_string) override;

            virtual void eachQueryParam(cc_string, KVIterator) override;

            virtual Method requestMethod() override;

            virtual cc_string scheme() override;

            virtual Session *session(bool create = true) override;

            virtual cc_string session(cc_string) override;

            virtual cc_string uri() override;

            virtual cc_string uril() override;

            virtual cc_string userAgent() override;

            virtual Json     *json() override;

            RouteHandler *resolveRoute(Router *r);

            RouteHandler *handler() const {
                return hanadler_;
            }

            struct http_request *raw() const {
                return req_;
            }

            void async(struct http_state *states,
                       u_int8_t nstates,
                       AsyncCleanup cleanup = NULL) {
                if (states != NULL && asyncStates == NULL) {
                    asyncStates = states;
                    nAsyncStates = nstates;
                }
                asyncCleanup_ = cleanup;
            }

            virtual ~HttpRequest();

            OVERLOAD_MEMOPERATORS();

        private:
            friend class App;
            void parseCookies();
            struct http_request *req_;
            c_string tokenized_;
            c_string *params_;
            RouteHandler *hanadler_;
            c_string ip_;
            Session *session_;
            c_string cookieStr_;
            c_string *cookies_;
            kore_buf *body_;
            bool      bodyLoaded_;
            bool      argsLoaded_;
            bool      formParsed_;
            Json      *json_;
            // pointer to the asynchrounous sm
            struct http_state   *asyncStates;
            u_int8_t         nAsyncStates;
            AsyncCleanup     asyncCleanup_;
        };
    }
}
#endif //SPARC_REQUEST_H
