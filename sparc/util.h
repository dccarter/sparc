//
// Created by dc on 11/19/16.
//

#ifndef SPARC_SYS_H
#define SPARC_SYS_H

#include <sys/types.h>
#include <sys/queue.h>
#include <openssl/md5.h>

#include <ctime>
#include <cassert>
#include <cstdio>
#include <string>
#include <functional>

enum TU {
    NANOSECONDS,
    MICROSECONDS,
    MILLISECONDS,
    SECONDS,
    MINUTES
};

#ifdef __cplusplus
extern "C" {
#endif
    struct kore_buf;
#ifdef __cplusplus
}
#endif

namespace sparc {

    typedef const char *cc_string;
    typedef char *c_string;

#ifndef SPARCX_DT_FORMAT
    static cc_string DT_FORMAT = "%d-%b-%Y %H:%M:%S %z";
#else
    static cc_string DT_FORMAT = SPARCX_DT_FORMAT
#endif

    struct datetime {
        /**
         *
         * @param units
         * @return
         */
        static inline unsigned long now(const TU units = TU::MILLISECONDS) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            switch (units) {
                case TU::MILLISECONDS :
                    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
                case TU::MICROSECONDS :
                    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
                case TU::NANOSECONDS :
                    return ts.tv_sec * 1000000000 + ts.tv_nsec;
                case TU::SECONDS:
                    return ts.tv_sec;
                case TU::MINUTES:
                    return ts.tv_sec / 60;
                default:
                    assert("Unreachable code");
                    return 0;
            }
        }

        /**
         *
         * @param t
         * @param fmt
         * @param buf
         * @return
         */
        static inline cc_string format(time_t t, cc_string fmt = NULL, c_string buf = NULL) {
            // Choose the format
            cc_string fmt_str = fmt == NULL ? DT_FORMAT : fmt;
            c_string buffer;
            struct tm *stm;

            if (t == 0)
                t = ::time(NULL);

            if (buf == NULL) {
                static char static_buffer[80];
                buffer = static_buffer;
            } else {
                buffer = buf;
            }

            stm = localtime(&t);
            if (strftime(buffer, 78, fmt_str, stm) == 0)
                return NULL;
            return buffer;
        }

        /**
         *
         * @param fmt
         * @param buf
         * @return
         */
        static inline cc_string str(cc_string fmt = NULL, c_string buf = NULL) {
            return format(::time(NULL), fmt, buf);
        }
    };

    static inline cc_string md5Hash(cc_string data, c_string out, size_t len) {
        uint8_t RAW[MD5_DIGEST_LENGTH];
        static char TMP[(MD5_DIGEST_LENGTH*2)+1];
        c_string wb = out? out : TMP;
        size_t i = 0;
        int rc = 0;
        MD5((const uint8_t*)data, len, RAW);
        for(;i < sizeof(RAW); i++)
            rc += sprintf(wb+rc, "%02x", RAW[i]);
        return wb;
    }

#define println(fmt, ...)   printf(fmt "\n", ##__VA_ARGS__ )

    class buffer {
    public:
        buffer();

        buffer(const buffer&) = delete;

        buffer(buffer&&);

        buffer& operator=(const buffer&&) = delete;

        buffer& operator=(buffer&&);

        buffer(size_t);

        void append(const void *, size_t);

        void appendf(const char *, ...);

        void bytes(const uint8_t *, size_t);

        void appendv(const char *, va_list args);
        cc_string toString();

        const size_t offset() const;

        const size_t size() const;

        void foreach(std::function<bool(u_int8_t *)>);

        buffer slice(const size_t, const size_t);

        const u_int8_t *raw() const;

        void reset();

        ~buffer();

    private:
        kore_buf *buf_;
    };

    class memory {
    public:
        static void *malloc(const size_t);

        static void *calloc(size_t, size_t);

        static void *realloc(void *, size_t);

        static void free(void *);

        static c_string strdup(cc_string);

        static cc_string strndup(cc_string, const size_t);
    };

    struct auto_obj {
        auto_obj();
        auto_obj(const auto_obj&);

        virtual auto_obj&operator=(const auto_obj& obj);
        virtual ~auto_obj();
        virtual void destroy(){};
        void debug();
    protected:
        int     *ref;
    };
#define auto_obj_dctor(type) \
    virtual ~ type() { if (ref && *ref == 1)  destroy(); } \
    virtual void destroy() override

    struct autostr : auto_obj {
        autostr(cc_string);
        autostr(autostr&);
        autostr();
        autostr&operator=(const autostr&);
        bool operator==(const autostr&);
        bool operator!=(const autostr&);
        bool compare(const autostr&, bool ignoreCase = true);
        operator c_string();
        size_t      length;
        auto_obj_dctor(autostr);
    private:
        c_string    str;
    };

#define OVERLOAD_MEMOPERATORS() \
    static void operator delete(void *ptr) {\
        memory::free(ptr); \
    }\
    static void* operator new(size_t sz) {\
        return memory::calloc(1, sz); \
    }

#define POCPROP(type, name) \
    private:\
        type    name ##_ ; \
    public:\
        const type&  name () const {  \
            return name## _; \
        } \
        void name ( const type& v ) { \
            name## _ = v; \
        }

#define map_find(m, entry, b) \
    { auto _kv = m.find(entry); if (_kv != m.end()) b = &_kv->second; else b = NULL;}

#define cstr(str) str.c_str()

    void cprint(int c, cc_string fmt, ...);

#define cprintln(c, fmt, ...) sparc::cprint ( c, fmt "\n" , __VA_ARGS__ )

    void $log(int, cc_string, ...);

#define $error(fmt, ...)    sparc::$log( LOG_ERR , fmt , ## __VA_ARGS__ )
#define $warn(fmt, ...)     sparc::$log( LOG_WARNING , fmt , ## __VA_ARGS__ )
#define $debug(fmt, ...)    sparc::$log( LOG_DEBUG , fmt , ## __VA_ARGS__ )
#define $info(fmt, ...)     sparc::$log( LOG_INFO , fmt , ## __VA_ARGS__ )
#define $notice(fmt, ...)   sparc::$log( LOG_NOTICE , fmt , ## __VA_ARGS__ )

}
#endif //SPARC_SYS_H
