//
// Created by dc on 11/22/16.
//

#ifndef SPARC_SPARC_H_H
#define SPARC_SPARC_H_H

#include <sys/types.h>
#include <sys/syslog.h>
#include <stdint.h>

#include <functional>
#include <exception>

#include "common.h"

#define FOLD

namespace sparc {

#if defined(FOLD) || defined(ROUTING_APIS)

    #define $(rq, rs)   [&]( Request& rq, Response& rs )
    #define $_(o, f)    std::bind( &f, o , std::placeholders::_1, std::placeholders::_2)

    void __route(const Method, cc_string, cc_string, handler, ResponseTransformer*);

    static inline void get(cc_string r, handler h) {
        __route(Method::GET, r, NULL, h, NULL);
    }

    static inline void get(cc_string r, cc_string c, handler h) {
        __route(Method::GET, r, c, h, NULL);
    }

    static inline void get(cc_string r, cc_string c, handler h, ResponseTransformer *t) {
        __route(Method::GET, r, c, h, t);
    }

    static inline void post(cc_string r, handler h) {
        __route(Method::POST, r, NULL, h, NULL);
    }

    static inline void post(cc_string r, cc_string c, handler h) {
        __route(Method::POST, r, c, h, NULL);
    }

    static inline void post(cc_string r, cc_string c, handler h, ResponseTransformer *t) {
        __route(Method::POST, r, c, h, t);
    }

    static inline void put(cc_string r, handler h) {
        __route(Method::PUT, r, NULL, h, NULL);
    }

    static inline void put(cc_string r, cc_string c, handler h) {
        __route(Method::PUT, r, c, h, NULL);
    }

    static inline void put(cc_string r, cc_string c, handler h, ResponseTransformer *t) {
        __route(Method::PUT, r, c, h, t);
    }

    static inline void del(cc_string r, handler h) {
        __route(Method::DELETE, r, NULL, h, NULL);
    }

    static inline void del(cc_string r, cc_string c, handler h) {
        __route(Method::DELETE, r, c, h, NULL);
    }

    static inline void del(cc_string r, cc_string c, handler h, ResponseTransformer *t) {
        __route(Method::DELETE, r, c, h, t);
    }

    static inline void update(cc_string r, handler h) {
        __route(Method::UPDATE, r, NULL, h, NULL);
    }

    static inline void update(cc_string r, cc_string c, handler h) {
        __route(Method::UPDATE, r, c, h, NULL);
    }

    static inline void update(cc_string r, cc_string c, handler h, ResponseTransformer *t) {
        __route(Method::UPDATE, r, c, h, t);
    }

    static inline void head(cc_string r, handler h) {
        __route(Method::HEAD, r, NULL, h, NULL);
    }

    static inline void head(cc_string r, cc_string c, handler h) {
        __route(Method::HEAD, r, c, h, NULL);
    }

    static inline void head(cc_string r, cc_string c, handler h, ResponseTransformer *t) {
        __route(Method::HEAD, r, c, h, t);
    }

#endif

#if defined(FOLD) || defined(FILTER_CODECS)
    void __fidec(bool, cc_string, handler);

    static inline void before(handler h) {
        __fidec(true, NULL, h);
    }

    static inline void before(cc_string p, handler h) {
        __fidec(true, p, h);
    }

    static inline void after(handler h) {
        __fidec(false, NULL, h);
    }

    static inline void after(cc_string p, handler h) {
        __fidec(false, p, h);
    }
#endif

#if defined(FOLD) || defined(STATIC_FILES)

    namespace staticFiles {
        void location(cc_string);
        void externalLocation(cc_string);
        void expireTime(const u_int64_t);
        void header(cc_string, cc_string);
    }

#endif

#if defined(FOLD) || defined(WEBSOCKET)
#define $message(w, d, s, o) [&]( WebSocket* w , void* d , size_t s , u_int8_t o )
#define $connect(w) [&]( WebSocket* w )
#define $disconnect(w) [&]( WebSocket* w )

    void webSocket(cc_string, WsOnMessage);
    void webSocket(cc_string, WsOnMessage, WsOnConnect, WsOnDisconnect);
#endif

#define $CONTINUE   1
#define $ABORT      0
#define _SKIP_FLUSH 2

#define $ON         1
#define $OFF        0

#if defined(FOLD) || defined(OTHER_APIS)

    void __halt(u_int16_t, cc_string);
    #define $halt(s, m) { __halt((s), (m)); return $ABORT; }

    void $timeout(timerid_t::timedout, u_int64_t, int flags = 0);
    #define $tm( tid )  [&]( timerid tid )

#endif

#if defined(FOLD) || defined(CONFIGURATION_APIS)

    namespace config {
        cc_string ip(cc_string ip = NULL);

        cc_string port(cc_string port = NULL);

        cc_string domain(cc_string domain = NULL);

        cc_string user(cc_string user = NULL);

        cc_string pidFile(cc_string pidFile = NULL);

        cc_string accessLog(cc_string acessLog = NULL);

        cc_string httpBodyDiskPath(cc_string path = NULL);

        cc_string chroot(c_string path = NULL);

        u_int16_t maxHttpHeader(int16_t v = -1);

        u_int64_t maxHttpBody(int64_t v = -1);

        u_int64_t httpBodyDiskOffload(int64_t v = -1);

        u_int64_t httpHsts(int64_t v = -1);

        u_int16_t httpKeepAliveTime(int32_t v = -1);

        u_int32_t httpRequestLimit(int32_t v = -1);

        u_int64_t webSocketMaxFrame(int64_t v = -1);

        u_int64_t webSocketTimeout(int64_t v = -1);

        u_int8_t workers(int8_t v = -1);

        u_int32_t workerMaxConnections(int32_t v = -1);

        u_int32_t rlimitNoFiles(int32_t v = -1);

        u_int32_t workerAcceptThreshold(int32_t v = -1);

        u_int8_t workerSetAffinity(int8_t v = -1);

        u_int32_t socketBacklog(int32_t v = -1);

        u_int8_t  setForeground(int8_t v = -1);

        u_int8_t  debug(int8_t v = -1);

        u_int8_t  skipRunAs(int8_t v = -1);

        u_int8_t  skipChroot(int8_t v = -1);
    }

#if !defined(KORE_NO_TLS)
    namespace tls {

        class TlsConfigurationException : public std::runtime_error {
        public:
            TlsConfigurationException(cc_string msg)
                : std::runtime_error(msg)
            {}
        };

        cc_string certificateFile(cc_string cert = NULL);
        cc_string certificateKey(cc_string key = NULL);
        cc_string cipher(cc_string cipherlist = NULL);
        void clientCertificate(cc_string cafile, cc_string crlfile);
        void dhparam(cc_string param);
        int             version(int v = -1);
    }
#endif

    void $enter(int argc, char *argv[]);
    int $exit();
#endif
}

#endif //SPARC_SPARC_H_H
