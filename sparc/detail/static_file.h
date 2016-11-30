//
// Created by dc on 11/26/16.
//

#ifndef SPARC_STATIC_FILE_H
#define SPARC_STATIC_FILE_H

#include "common.h"
#include "uthash.h"

#ifndef SPARC_STATIC_FILES_BASE_DIR
#define SPARC_STATIC_FILES_BASE_DIR "/www"
#endif

#ifndef __cplusplus
#endif
struct http_header;
#ifndef __cplusplus
#endif

namespace sparc {
    namespace detail {

#define STATIC_FILE_MAPPED  0x01
#define STATIC_FILE_CACHED  0x02

        struct static_file {
            UT_hash_handle      hh;
            char                *path;
            c_string            content_type;
            void                *ptr;
            size_t              len;
            u_int32_t           flags;
            time_t              last_mod;
            time_t              last_access;
            // for mapped files
            int                 fd;

            ~static_file();

            OVERLOAD_MEMOPERATORS();
        };

        class StaticFilesRouter {
        public:
            StaticFilesRouter(u_int64_t minMapped);
            bool init(cc_string);
            static_file *find(cc_string);
            void header(cc_string, cc_string);
            int applyHeaders(Request &req, Response& resp, static_file *);
            void exipires(int64_t expires = -1) {
                if (expires) exipires_ = expires;
            }
            static cc_string getContentType(cc_string);
            ~StaticFilesRouter();
        private:
            static_file *loadFile(cc_string, struct stat*);
            bool  reloadFile(cc_string, static_file *sf, struct stat*);
            void  *getFileContents(cc_string, struct stat*, int&, u_int32_t& flags);
            c_string            base_dir;
            u_int64_t           minimumMapped_{2048};
            static_file         *files_{NULL};
            int64_t             exipires_;
            TAILQ_HEAD(,http_header)    headers_;
        };
    }
}
#endif //SPARC_STATIC_FILE_H
