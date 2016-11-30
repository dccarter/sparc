//
// Created by dc on 11/26/16.
//

#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "kore.h"
#include "http.h"
#include "static_file.h"

namespace sparc {
    namespace detail {

        StaticFilesRouter::StaticFilesRouter(u_int64_t minMapped)
            : base_dir(NULL),
              files_(NULL),
              minimumMapped_(minMapped),
              exipires_(-1)
        {
            TAILQ_INIT(&headers_);
        }

        void StaticFilesRouter::header(cc_string name, cc_string value) {
            http_header *i, *hh = NULL;
            TAILQ_FOREACH(i, &headers_, list) {
                if (!strcasecmp(name, i->header)) {
                    hh = i;
                    kore_free(hh->value);
                    break;
                }
            }
            if (hh == NULL) {
                hh = (http_header *) kore_calloc(1, sizeof(http_header));
                hh->header = strdup(name);
                TAILQ_INSERT_TAIL(&headers_, hh, list);
            }
            hh->value = strdup(value);
        }

        StaticFilesRouter::~StaticFilesRouter() {
            static_file *sf, *tmp;
            http_header *hh, *htmp;
            HASH_ITER(hh, files_, sf, tmp) {
                HASH_DELETE(hh, files_, sf);
                delete sf;
            }
            // delete headers
            for (hh = TAILQ_FIRST(&headers_); hh != NULL; hh = htmp) {
                htmp = TAILQ_NEXT(hh, list);
                TAILQ_REMOVE(&headers_, hh, list);
                if (hh->header) kore_free(hh->header);
                if (hh->value) kore_free(hh->value);
                kore_free(hh);
            }
        }

        int StaticFilesRouter::applyHeaders(Request& req, Response &resp, static_file *sf) {
            http_header *hh;
            cc_string   sh = NULL;
            cc_string   dot;

            // avoid cache control .html
            dot = strchr(sf->path, '.');
            if (dot && strcasecmp(".html", dot)) {
                // cache control headers
                sh = req.header("If-Modified-Since");
                if (sh != NULL) {
                    time_t if_mod = kore_date_to_time(sh);
                    if (if_mod >= sf->last_mod)
                        return NOT_MODIFIED;
                }

                resp.header("Last-Modified", kore_time_to_date(sf->last_mod));
                // add cache control header
                if (exipires_ > 0) {
                    char buf[128];
                    snprintf(&buf[0], sizeof(buf), "public, max-age=%ld", exipires_);
                    resp.header("Cache-Control", buf);
                }
            }

            TAILQ_FOREACH(hh, &headers_, list) {
                resp.header(hh->header, hh->value);
            }

            return OK;
        }

        bool StaticFilesRouter::init(cc_string base) {
            c_string tmp_base;
            size_t base_len;

            // if already initialized
            if (base_dir) {
                $error("static files already initialized to %s", base_dir);
                return false;
            }

            tmp_base = kore_strdup(base ? base : SPARC_STATIC_FILES_BASE_DIR);
            base_len = strlen(tmp_base);
            if (tmp_base[base_len - 1] == '/') tmp_base[base_len - 1] = '\0';

            struct stat st;
            if (stat(tmp_base, &st) != 0 || !S_ISDIR(st.st_mode)) {
                $error("Static files: %s is not a valid base directory", tmp_base);
                kore_debug("%s", errno_s);
                kore_free(tmp_base);
                return false;
            }
            base_dir = tmp_base;
            return true;
        }

        static_file* StaticFilesRouter::find(cc_string path) {
            static_file *sf = NULL;
            cc_string sf_path;
            char full_path[256];
            sf_path = !strcmp(path, "/")? "index.html" : path;

            snprintf(&full_path[0], sizeof(full_path), "%s%s", base_dir, sf_path);
            struct stat st;
            if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) {
                kore_debug("request resource file %s does not exist", path);
                return NULL;
            }

            HASH_FIND_STR(files_, sf_path, sf);
            if (sf == NULL) {

                sf = loadFile(full_path, &st);
                if (sf == NULL)
                    return NULL;
                kore_debug("static resource %s loaded", full_path);
                sf->path = kore_strdup(path);

                HASH_ADD_STR(files_, path, sf);
            } else if(sf->last_mod != (time_t) st.st_mtim.tv_sec) {
                kore_debug("static file (%s) modified, reloading from disk", sf->path);
                if (!reloadFile(full_path, sf, &st)) {
                    kore_debug("reloading static file failure: %s", errno_s);
                    // remove file from cached content
                    HASH_DELETE(hh, files_, sf);
                    delete  sf;
                    return NULL;
                }
            }
            sf->last_access = time(NULL);
            return sf;
        }

        void  *StaticFilesRouter::getFileContents(cc_string path, struct stat* st, int& fd, u_int32_t& flags) {
            void        *addr;

            fd = open(path, O_RDONLY);
            if (fd < 0) {
                $warn("opening resource %s failed", path);
                return NULL;
            }

            if ((size_t) st->st_size >= minimumMapped_) {
                size_t total = (size_t) st->st_size;
                int page_sz = getpagesize();
                total += page_sz-(total % page_sz);
                addr = mmap(NULL, total, PROT_READ, MAP_SHARED , fd, 0);
                if (addr == MAP_FAILED) {
                    $warn("mapping static (%s) of size %d failed: %s", path, st->st_size, errno_s);
                    close(fd);
                    return NULL;
                }
                flags = STATIC_FILE_MAPPED;
            } else {
                // read file
                size_t total = (size_t) st->st_size + ((st->st_size >> 8)*64);
                addr = malloc(total);
                if (addr) {
                    size_t nread = 0, toread = (size_t) st->st_size;
                    ssize_t cread = 0;
                    do {
                        cread = read(fd, (char*)addr + nread, toread - nread);
                        if (cread < 0) {
                            $warn("reading file %s failed: %s", path, errno_s);
                            free(addr);
                            return NULL;
                        }
                        nread += cread;
                    } while(nread < toread);
                } else {
                    $warn("mapping static (%s) of size %d failed: %s", path, st->st_size, errno_s);
                    close(fd);
                    return NULL;
                }
                flags = STATIC_FILE_CACHED;
            }
            return addr;
        }

        bool StaticFilesRouter::reloadFile(cc_string path, static_file *sf, struct stat *st) {
            void        *addr;
            int         fd;
            u_int32_t   flag;

            if (sf->ptr) {
                if (sf->flags & STATIC_FILE_CACHED)
                    free(sf->ptr);
                else
                    munmap(sf->ptr, sf->len);
                sf->ptr = NULL;
            }
            // try re-loading the file
            if (sf->fd > 0)
                close(sf->fd);

            addr = getFileContents(path, st, fd, flag);
            if (addr) {
                kore_debug("static file updated: %s:%d:%d", path, fd, flag);
                sf->ptr     =  addr;
                sf->fd      = fd;
                sf->flags   = flag;
                sf->len     =  (size_t) st->st_size;
                sf->last_mod = (time_t) st->st_mtim.tv_sec;
                return true;
            }

            return false;
        }

        static_file* StaticFilesRouter::loadFile(cc_string path, struct stat *st) {
            void        *addr;
            int         fd;
            static_file *sf;
            u_int32_t   flag;
            addr = getFileContents(path, st, fd, flag);
            if (addr) {
                sf = new static_file;
                sf->fd = fd;
                sf->flags = flag;
                sf->len = (size_t) st->st_size;
                sf->ptr = addr;
                sf->content_type = kore_strdup(getContentType(path));
                sf->last_mod = (time_t) st->st_mtim.tv_sec;
                return sf;
            } else {
                kore_debug("loading static file %s failed", path);
                return NULL;
            }
        }

        cc_string StaticFilesRouter::getContentType(cc_string path) {
            cc_string    dot;
            dot = strchr(path, '.');
            if (dot) {
                dot++;
                if (!strcasecmp(dot, "html"))
                    return "text/html";
                else if (!strcasecmp(dot, "json"))
                    return "application/json";
                else if (!strcasecmp(dot, "pdf"))
                    return "application/pdf";
                else if (!strcasecmp(dot, "png"))
                    return "image/png";
                else if (!strcasecmp(dot, "jpeg"))
                    return "image/jpeg";
                else if (!strcasecmp(dot, "jpg"))
                    return "image/jpg";
                else if (!strcasecmp(dot, "gif"))
                    return "image/gif";
                else if (!strcasecmp(dot, "bmp"))
                    return "image/bmp";
                else if (!strcasecmp(dot, "css"))
                    return "text/css";
                else if (!strcasecmp(dot, "js"))
                    return "text/javascript";
            }
            return "text/plain";
        }

        static_file::~static_file() {
            // release resources
            if (ptr != NULL) {
                if (flags & STATIC_FILE_MAPPED)
                    munmap(ptr, len);
                else
                    free(ptr);
                ptr = NULL;
            }

            if (fd && fd > 0) {
                close(fd);
                fd = -1;
            }

            if (content_type) {
                kore_free(content_type);
                content_type = NULL;
            }

            if (path) {
                kore_free(path);
                path = NULL;
            }
        }
    }
}