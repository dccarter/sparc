//
// Created by dc on 11/24/16.
//

#ifndef SPARC_RESPONSE_H
#define SPARC_RESPONSE_H

#include <exception>
#include <cstring>

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct http_request;
struct connection;

struct http_cookie {
    TAILQ_ENTRY(http_cookie)  link;
    char            *name;
    char            *value;
    char            *path;
    char            *domain;
    int64_t         expiryTime;
    bool            secure;
};

#ifdef __cplusplus
}
#endif

namespace sparc {
    namespace detail {

        class HttpResponse : public Response {
        public:
            HttpResponse(http_request *);

            virtual ~HttpResponse();

            virtual buffer &body();

            virtual cc_string raw() override;

            virtual cc_string type() override;

            virtual void body(cc_string) override;

            virtual void header(cc_string, cc_string) override;

            virtual void type(cc_string) override;

            virtual void redirect(cc_string) override;

            virtual void redirect(cc_string, const int) override;

            virtual void cookie(cc_string name, cc_string value) override {
                cookie(name, value, NULL, NULL, -1, false);
            }

            virtual void cookie(cc_string name, cc_string value, int64_t expire) override {
                cookie(name, value, NULL, NULL, expire, false);
            }

            virtual void cookie(cc_string name, cc_string value, int64_t expire, bool secure) override {
                cookie(name, value, NULL, NULL, expire, secure);
            }

            virtual void cookie(cc_string name,
                                cc_string value,
                                cc_string path,
                                int64_t expire,
                                bool secure) {
                cookie(name, value, path, NULL, expire, secure);
            }

            virtual void cookie(cc_string, cc_string,
                                cc_string, cc_string, int64_t, bool) override;

            virtual void removeCookie(cc_string) override;

            virtual int  status(int s) override  {
                if (s) {
                    if (s > OK) status_ = s;
                }
                return s;
            }

            void swapBody(buffer& b) {
                body_ = b;
            }

            int status() const {
                return status_;
            }

            void end(int status) {
                end(status, NULL, 0);
            }

            void end(int status, cc_string b) {
                if (b)
                    end(status, (void *)b, strlen(b));
                else
                    end(status, NULL, 0);
            }

            void end(int, void*, size_t);

            void flush() {
                end(status_, NULL);
            }

            const bool ended() const {
                return ended_;
            }

            virtual Response&operator<<(const cc_string str) override {
                body_.append(str, strlen(str));
                return *this;
            }

            virtual Response&operator<<(const double real) override  {
                body_.appendf("%.4f", real);
                return *this;
            }

            virtual Response&operator<<(const int num) override  {
                body_.appendf("%d", num);
                return *this;
            }

            OVERLOAD_MEMOPERATORS()

        private:
            void flushCookies();

            cc_string header(cc_string);

            void eraseCookie(http_cookie *);

            buffer body_;
            http_request *req_;
            int status_;
            bool ended_;
            TAILQ_HEAD(, http_cookie) cookies_;
        };
    }
}
#endif //SPARC_RESPONSE_H
