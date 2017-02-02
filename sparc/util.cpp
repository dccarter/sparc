//
// Created by dc on 11/22/16.
//

#include <sys/param.h>
#include "util.h"
#include "kore.h"

namespace sparc {

    buffer::buffer()
        : buf_(NULL)
    {
    }

    buffer::buffer(size_t isz)
        : buf_(kore_buf_alloc(isz))
    {}

    buffer::buffer(buffer &&b)
        : buf_(b.buf_)
    {
        b.buf_ = NULL;
    }

    buffer& buffer::operator=(buffer &&b)
    {
        if (&b != this) {
            buf_ = b.buf_;
            b.buf_ = NULL;
        }
        return *this;
    }

    buffer::~buffer() {
        if (buf_) {
            kore_buf_free(buf_);
            buf_ = NULL;
        }
    }

    void buffer::reset() {
        buf_->offset = 0;
    }

    void buffer::append(const void *data, size_t len) {
        kore_buf_append(buf_, data, len);
    }

    void buffer::appendf(const char *fmt, ...) {
        va_list        args;
        va_start(args, fmt);
        kore_buf_appendv(buf_, fmt, args);
        va_end(args);
    }

    void buffer::bytes(const uint8_t *data, size_t len) {
        c_string buf = kore_buf_reserve(buf_, (len*3));
        size_t i=0;
        int nwr = 0;
        for (; i < len; i++) {
            nwr += sprintf((buf+nwr), "%02x", data[i]);
        }
        buf_->offset += nwr;
    }

    void buffer::appendv(const char *fmt, va_list args) {
        kore_buf_appendv(buf_, fmt, args);
    }

    const size_t buffer::offset() const {
        return buf_->offset;
    }

    const size_t buffer::size() const {
        return buf_->length;
    }

    void buffer::foreach(std::function<bool(u_int8_t *)> h) {
        unsigned int i;
        for (i=0; i < buf_->offset; i++) {
            if (!h(&buf_->data[i])) break;
        }
    }

    buffer buffer::slice(const size_t from, const size_t count) {
        size_t      a;
        unsigned    i;
        buffer      b;
        kore_buf    *buf;

        if (from > buf_->offset || count == 0) return b;
        a = MIN(count, buf_->offset - from);
        buf = kore_buf_alloc(a+2);
        for (i = 0; i < a; i++) {
            kore_buf_append(buf, buf_->data+from, a);
        }

        b.buf_ = buf;
        return b;
    }

    const u_int8_t* buffer::raw() const {
        return buf_->data;
    }

    cc_string buffer::toString() {
        if (buf_ == NULL) {
            $error("cannot stringify null buffer");
            fatal("cannot stringify null buffer");
            return NULL;
        }

        return kore_buf_stringify(buf_, NULL);
    }

    auto_obj::auto_obj(const auto_obj &obj)
            : ref(obj.ref)
    {
        if (ref) *ref += 1;
    }

    auto_obj& auto_obj::operator=(const auto_obj &obj) {
        if (this != &obj) {
            this->~auto_obj();
            ref = obj.ref;
            if (ref)
                *ref += 1;
        }

        return *this;
    }

    auto_obj::auto_obj()
            : ref(new int)
    {
        *ref = 1;
    }

    auto_obj::~auto_obj() {
        if (ref) {
            if (*ref == 1)
                delete ref;
            else
                *ref -= 1;
            ref = NULL;
        }
    }

    void auto_obj::debug() {
        kore_debug("ref: %p %d", ref, ref? *ref:0);
    }


    autostr::autostr()
            : autostr(NULL)
    {}

    autostr::autostr(cc_string s)
            : str(s? kore_strdup(s): NULL),
              length(s? strlen(s) :0)
    {
    }

    autostr::autostr(autostr &as)
            : auto_obj(as)
    {
        str = as.str;
        length = as.length;
    }

    autostr& autostr::operator=(const autostr &as)
    {
        auto_obj::operator=(as);
        str = as.str;
        length = as.length;
        return *this;
    }

    autostr::operator c_string() {
        return str;
    }

    void autostr::destroy() {
        if (str) {
            memory::free(str);
            str = NULL;
        }
    }

    bool autostr::operator==(const autostr &s) {
        return compare(s);
    }

    bool autostr::operator!=(const autostr &s) {
        return !compare(s);
    }

    bool autostr::compare(const autostr& s, bool ignoreCase) {
        if (str == s.str) return true;
        if (!str || !s.str) return false;
        if (ignoreCase)
            return !strcmp(str, s.str);
        else
            return !strcasecmp(str, s.str);
    }

    void cprint(int c, cc_string fmt, ...) {
        va_list     args;
        va_start(args, fmt);
        kore_color_vprintf(c, fmt, args);
        va_end(args);
    }

    void $log(int prio, cc_string fmt, ...) {
        char        buf[2048];
        va_list     args;
        va_start(args, fmt);
        (void)vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        kore_logs(prio, buf);
    }

    void memory::free(void *ptr) {
        kore_free(ptr);
    }

    void* memory::malloc(const size_t sz) {
        return kore_malloc(sz);
    }

    void* memory::calloc(size_t memb, size_t len) {
        return kore_calloc(memb, len);
    }

    void* memory::realloc(void *ptr, size_t sz) {
        return kore_realloc(ptr, sz);
    }

    c_string memory::strdup(cc_string str) {
        return kore_strdup(str);
    }

    cc_string memory::strndup(cc_string str, const size_t len) {
        return kore_strndup(str, len);
    }
}
