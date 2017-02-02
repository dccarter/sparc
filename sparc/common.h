//
// Created by dc on 11/22/16.
//

#ifndef SPARC_COMMON_H
#define SPARC_COMMON_H

#include <sys/queue.h>

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif
    struct http_cookie;
    struct JsonNode;
#ifdef __cplusplus
}
#endif

namespace sparc {

    enum Status {
        CONTINUE            = 100,
        SWITCHING_PROTOCOLS = 101,
        OK                  = 200,
        CREATED             = 201,
        ACCEPTED            = 202,
        NON_AUTHORITATIVE   = 203,
        NO_CONTENT          = 204,
        RESET_CONTENT       = 205,
        PARTIAL_CONTENT     = 206,
        MULTIPLE_CHOICES    = 300,
        MOVED_PERMANENTLY   = 301,
        FOUND               = 302,
        SEE_OTHER           = 303,
        NOT_MODIFIED        = 304,
        USE_PROXY           = 305,
        TEMPORARY_REDIRECT  = 307,
        BAD_REQUEST         = 400,
        UNAUTHORIZED        = 401,
        PAYMENT_REQUIRED    = 402,
        FORBIDDEN           = 403,
        NOT_FOUND           = 404,
        METHOD_NOT_ALLOWED  = 405,
        NOT_ACCEPTABLE      = 406,
        PROXY_AUTH_REQUIRED = 407,
        REQUEST_TIMEOUT     = 408,
        CONFLICT            = 409,
        GONE                = 410,
        LENGTH_REQUIRED     = 411,
        PRECONDITION_FAILED = 412,
        REQUEST_ENTITY_TOO_LARGE    = 413,
        REQUEST_URI_TOO_LARGE       = 414,
        UNSUPPORTED_MEDIA_TYPE      = 415,
        REQUEST_RANGE_INVALID       = 416,
        EXPECTATION_FAILED          = 417,
        INTERNAL_ERROR              = 500,
        NOT_IMPLEMENTED             = 501,
        BAD_GATEWAY                 = 502,
        SERVICE_UNAVAILABLE         = 503,
        GATEWAY_TIMEOUT             = 504,
        BAD_VERSION                 = 505
    };

    enum Method {
        GET, POST, PUT, DELETE, HEAD, UPDATE
    };

    typedef enum {
        J_NULL,
        J_BOOL,
        J_STRING,
        J_NUMBER,
        J_ARRAY,
        J_OBJECT,
    } JsonType;

    class Json;

    struct JsonValue {
        JsonValue(JsonNode*);
        JsonValue(Json*);
        using JsonEach = std::function<bool(JsonValue&)>;
        virtual void                each(JsonEach);
        virtual JsonValue          operator[](int);
        virtual JsonValue          operator[](cc_string);
        virtual bool                operator==(const JsonValue&);
        virtual bool                operator!=(const JsonValue&);
        virtual double              n(cc_string p = NULL);
        virtual cc_string           s(cc_string p = NULL);
        virtual bool                b(cc_string p = NULL);
        virtual JsonValue          o(cc_string p = NULL);
        virtual JsonValue          a(cc_string p = NULL);
        virtual bool               isNull() const;
        virtual JsonType           type() const;

        inline operator double()  {
            return n(NULL);
        }

        inline operator cc_string() {
            return s(NULL);
        }

        inline operator bool() {
            return b();
        }

        JsonNode                    *node;
    };

    struct JsonObject : public JsonValue {
        JsonObject(JsonNode*, JsonType);
        JsonObject(JsonValue);
        JsonObject(Json*);
        JsonObject push(){}
        JsonObject add();
        JsonObject add(cc_string);
        JsonObject addb(bool);
        JsonObject add(double);
        JsonObject set(cc_string, cc_string);
        JsonObject setb(cc_string, bool);
        JsonObject set(cc_string, double);
        JsonObject set(cc_string);

        template <typename Arg, typename... Args>
        void       push(Arg arg, Args... args) {
            if (type_ == J_ARRAY) {
                if (typeid(arg) == typeid(bool))
                    addb(arg);
                else
                    add(std::forward<Arg>(arg));
                push(args...);
            }
        };

        template <typename... Args>
        JsonObject arr(cc_string name, Args... args) {
            if (type_ == J_OBJECT) {
                JsonObject j = mkarr(name);
                if (j.type_ == J_ARRAY) {
                    j.push(args...);
                    return j;
                }
            }
            return JsonObject(NULL);
        }

        inline operator JsonValue() {
            return JsonValue(node);
        }

    private:
        JsonObject  mkarr(cc_string);
        JsonType    type_;
    };

    static inline cc_string JsonType_str(JsonType t) {
        switch (t) {
            case J_BOOL:
                return "boolean";
            case J_STRING:
                return "string";
            case J_NUMBER:
                return "number";
            case J_ARRAY:
                return "array";
            case J_OBJECT:
                return "object";
            default:
                return "undefined";
        }
    }

    class Json {
    public:
        static Json*  decode(cc_string);
        static Json*  create(bool cache = false);
        virtual c_string  encode(size_t&, char *space = NULL) = 0;
        virtual c_string  encode() = 0;
        virtual ~Json() {}
    };

#define $json(j) [&]( JsonValue& j )

    using KVIterator = std::function<bool(cc_string,cc_string)>;
    #define $_kv(k, v) [&]( auto k , auto v )

    using CookieIterator = std::function<bool(http_cookie*)>;
    #define $for(k) [&] ( auto k )

    struct Session {
    public:
        virtual void attribute(cc_string, cc_string) = 0;
        virtual void attributeSet(cc_string, c_string) = 0;
        virtual cc_string attribute(cc_string) = 0;
        virtual c_string removeAttribute(cc_string) = 0;
        virtual void eachAttribute(KVIterator) = 0;
        virtual cc_string id() = 0;
        virtual bool isNew() = 0;
        virtual long exipryTime() = 0;
    };

    using AsyncCleanup = std::function<void(void*)>;

    class Request {
    public:
        virtual cc_string body() = 0;
        virtual size_t contentLength() = 0;
        virtual cc_string contentType() = 0;
        virtual cc_string contextPath() = 0;
        virtual void eachHeader(cc_string, KVIterator) = 0;
        virtual cc_string header(cc_string) = 0;
        virtual void eachCookie(cc_string, KVIterator) = 0;
        virtual cc_string cookie(cc_string) = 0;
        virtual cc_string form(cc_string) = 0;
        virtual cc_string ip() = 0;
        virtual cc_string  param(cc_string) = 0;
        virtual void eachParam(KVIterator) = 0;
        virtual int port() = 0;
        virtual cc_string protocol() = 0;
        virtual cc_string queryParam(cc_string) = 0;
        virtual void eachQueryParam(cc_string, KVIterator) = 0;
        virtual Method requestMethod() = 0;
        virtual cc_string scheme() = 0;
        virtual Session* session(bool create = true) = 0;
        virtual cc_string session(cc_string) = 0;
        virtual cc_string uri() = 0;
        virtual cc_string uril() = 0;
        virtual cc_string userAgent() = 0;
        virtual Json *json() = 0;
        virtual ~Request() = 0;
    };

    class Response {
    public:
        virtual buffer& body() = 0;
        virtual cc_string raw() = 0;
        virtual cc_string type() = 0;

        virtual void body(cc_string) = 0;
        virtual void header(cc_string, cc_string) = 0;
        virtual void type(cc_string) = 0;
        virtual void redirect(cc_string) = 0;
        virtual void redirect(cc_string, const int) = 0;
        virtual void cookie(cc_string, cc_string) = 0;
        virtual void cookie(cc_string, cc_string, int64_t) = 0;
        virtual void cookie(cc_string, cc_string, int64_t, bool) = 0;
        virtual void cookie(cc_string, cc_string,
                            cc_string, int64_t, bool) = 0;
        virtual void cookie(cc_string, cc_string,
                            cc_string, cc_string, int64_t, bool) = 0;
        virtual void removeCookie(cc_string) = 0;
        virtual int  status(int status = 0) = 0;
        virtual Response&operator<<(const cc_string) = 0;
        virtual Response&operator<<(const double) = 0;
        virtual Response&operator<<(const int) = 0;
        virtual Response&operator<<(Json*) = 0;
        virtual ~Response() = 0;
    };

    class ResponseTransformer {
    public:
        virtual size_t sizeHint(size_t size);
        virtual int transform(Response& resp, buffer&) = 0;
        virtual ~ResponseTransformer() {};
    };

    typedef struct timerid_t {
        using timedout = std::function<void(timerid_t *)>;
        virtual void cancel() = 0;
    } *timerid;

    using handler = std::function<int(Request&, Response&)>;

    enum {
        WS_OP_CONT   = 0x00,
        WS_OP_TEXT   = 0x01,
        WS_OP_BINARY = 0x02,
        WS_OP_CLOSE  = 0x08,
        WS_OP_PING   = 0x09,
        WS_OP_PONG   = 0x10
    };

    enum {
        WS_BROADCAST_LOCAL  =   1,
        WS_BROADCAST_GLOBAL
    };

    class WebSocket {
    public:
        virtual void send(void*, size_t, u_int8_t op=WS_OP_BINARY) = 0;
        virtual void send(cc_string, u_int8_t op=WS_OP_TEXT) = 0;
        virtual void broadcast(void*, size_t, u_int8_t op=WS_OP_BINARY) = 0;
        virtual void broadcast(cc_string, u_int8_t op=WS_OP_TEXT) = 0;
        virtual void globalBroadcast(void*, size_t, u_int8_t op=WS_OP_BINARY) = 0;
        virtual void globalBroadcast(cc_string, u_int8_t op=WS_OP_TEXT) = 0;
        virtual void close() = 0;
    };

    using WsOnConnect       = std::function<void(WebSocket*)>;
    using WsOnDisconnect    = std::function<void(WebSocket*)>;
    using WsOnMessage       = std::function<void(WebSocket*, void*, size_t, u_int8_t)>;
}
#endif //SPARC_DETAIL_H
