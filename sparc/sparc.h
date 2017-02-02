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

#ifdef __cplusplus
extern "C" {
#endif
struct pgsql_db;
#ifdef __cplusplus
}
#endif

namespace sparc {

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

    namespace staticFiles {
        void location(cc_string);
        void externalLocation(cc_string);
        void expireTime(const u_int64_t);
        void header(cc_string, cc_string);
    }

    int __id();
#define $n(n)       if (n == __id())
#define $els        else
#define $0          $n(0)
#define $1          $n(1)
#define $2          $n(2)
#define $3          $n(3)
#define $4          $n(4)
#define $5          $n(5)
#define $6          $n(6)
#define $7          $n(7)

#define $message(w, d, s, o) [&]( WebSocket* w , void* d , size_t s , u_int8_t o )
#define $connect(w) [&]( WebSocket* w )
#define $disconnect(w) [&]( WebSocket* w )

    void webSocket(cc_string, WsOnMessage);
    void webSocket(cc_string, WsOnMessage, WsOnConnect, WsOnDisconnect);

#define $CONTINUE   1
#define $ABORT      0
#define $SKIP_FLUSH 2
#define $ASYNC      3
#define $ON         1
#define $OFF        0

    void __halt(u_int16_t, cc_string);
    #define $halt(s, m) { __halt((s), (m)); return $ABORT; }

#define TIMER_ONESHOT       (0x01)
    void $timeout(timerid_t::timedout, u_int64_t, int flags = 0);
    #define $tm( tid )  [&]( timerid tid )

#define    $async(status) ({if(status != $ASYNC) return $ABORT; return $ASYNC; })

    namespace detail {
        class App;
    }

    namespace db {

        struct result_t;
        typedef struct  result_t *Result;
        class Sql;

#define $db(sql, res) [&]( sparc::db::Sql& sql , sparc::db::result_t& res )
        using OnAsync = std::function<int( Sql& sql, result_t& res)>;
        Result query(int dbid, Sql&);
        int    query(int dbid, Sql&, OnAsync);

        class Sql {
        public:
            Sql();
            Sql(Request& req);
            Sql(Sql&& sql);

            Sql& operator()(cc_string fmt, ...);
            Request& req() {
                return *req_;
            }

            Response& resp() {
                return *res_;
            }

            cc_string sql() {
                return sql_.toString();
            }

            void async(Request *req, Response *resp) {
                req_ = req;
                res_ = resp;
            }

            ~Sql();

            void debug();

            OVERLOAD_MEMOPERATORS();

        private:
            friend Result query(int dbid, Sql&);
            friend int    query(int dbid, Sql&, OnAsync);
            void setResult(result_t *result);
        private:
            Request         *req_;
            Response        *res_;
            result_t        *result_;
            buffer          sql_;
        };

        struct Row {
            Row(const Row &row)
                : result_(row.result_),
                  index_(row.index_)
            {}

            Row()
                : result_(nullptr),
                  index_(INT32_MAX)
            {}

            Row(result_t *result, size_t idx)
                    : result_(result),
                      index_(idx)
            {}

            virtual cc_string value(size_t idx) const {
                return NULL;
            };

            cc_string operator[](size_t idx) const {
                return value(idx);
            }
            virtual cc_string value(cc_string idx) const {
                return NULL;
            };

            cc_string operator[](cc_string col) const {
                return value(col);
            }

        protected:
            size_t      index_;
            result_t    *result_;
        };

        struct result_t {
            result_t() {}
            result_t(result_t&) = delete;

            struct row_iterator_t {

                row_iterator_t(const result_t* res, int index)
                    : index_(index),
                      results_(res)
                {}

                bool operator!=(const row_iterator_t& other) const {
                    return index_ != other.index_;
                }

                Row& operator*() const;

                const row_iterator_t& operator++() {
                    index_++;
                    return *this;
                }

                const row_iterator_t& operator++(int) {
                    index_++;
                    return *this;
                }

            private:
                const result_t       *results_;
                int                  index_;
            };

            typedef row_iterator_t  iterator;
            typedef const row_iterator_t  const_iterator;

            virtual iterator begin() { return iterator(this, 0); };

            virtual const_iterator begin() const { return iterator(this, 0); };

            virtual const_iterator cbegin() const { return iterator(this, 0); };

            virtual iterator end() { return iterator(this, count()); };

            virtual const_iterator end() const { return iterator(this, count()); };

            virtual const_iterator cend() const { return iterator(this, count()); };

            virtual Row* at(size_t index) const { return  NULL; };

            Row* operator[](size_t index) const {
                return at(index);
            }

            virtual size_t count() const { return 0; }

            virtual bool isSuccess() const { return false; };

            operator bool () const {
                return isSuccess() && count() > 0;
            }

            virtual Json* toJson() { return NULL; };

            virtual ~result_t() {}

            OVERLOAD_MEMOPERATORS();
        };

        class Context {
        public:
            virtual int init() = 0;
        protected:
            friend Result query(int, Sql&);
            friend int    query(int, Sql&, OnAsync);
            virtual Result exec(Sql& sql) = 0;
            virtual int exec(Sql&& sql, OnAsync onAsync) = 0;
        };

        class PgSqlContext : public virtual Context {
        public:
            virtual int init();
            PgSqlContext(cc_string name, cc_string connString);
            virtual Result exec(Sql& sql);
            virtual int exec(Sql&& sql, OnAsync onAsync);

            virtual ~PgSqlContext();
        private:
            c_string        name_;
            c_string        connString_;
            struct pgsql_db *db_;
        };
    }

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

        int64_t   sessionTimeout(int64_t v = -INT64_MAX);
        int db(db::Context *db);
    }

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

    void $enter(int argc, char *argv[]);
    int $exit();
    using OnLoad = std::function<void(void)>;
#define $onload()   []( void )
    int $start(OnLoad onl);

    namespace admin {
        /**
         * retrive all the installed route handlers in json format
         * @return a json object containing all the installed routes
         */
        Json *routes();
    }

    namespace auth {
        enum auth_type {
            AUTH_TYPE_ANY,
            AUTH_TYPE_BASIC,
            AUTH_TYPE_DIGEST,
            AUTH_TYPE_TOKEN
        };

        struct auth_info_t {
            auth_info_t(auth_type t, cc_string uname)
                : type(t),
                  username(uname),
                  password(NULL),
                  salt(NULL)
            {}
            auth_type     type;
            cc_string     username;
            cc_string     password;
            cc_string     salt;
        };

        using AuthInfoCb = std::function<bool(auth_info_t&)>;

        struct user_t : public virtual auto_obj {
            operator bool() const {
                return authenticated;
            }

            cc_string getUsername() const {
                return username;
            }

            cc_string getPassword() const {
                return password;
            }

            auto_obj_dctor(user_t);
        protected:

            friend user_t authenticate(Request &, AuthInfoCb, auth_type);
            user_t(c_string user, c_string pwd)
                : auto_obj(),
                  username(user),
                  password(pwd),
                  authenticated(false)
            { }

            c_string    username;
            c_string    password;
            bool        authenticated;
        };

        user_t authenticate(Request &req,
                             AuthInfoCb getInfo,
                             auth_type t = AUTH_TYPE_ANY);

        bool authorize(Request& req,
                       Response& resp,
                       user_t& user,
                       cc_string realm,
                       auth_type type = AUTH_TYPE_ANY);

    }
}

#endif //SPARC_SPARC_H_H
