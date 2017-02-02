//
// Created by dc on 11/24/16.
//

#include <openssl/md5.h>
#include <openssl/rand.h>

#include "uthash.h"
#include "session.h"
#include "kore.h"

#ifdef __cplusplus
extern "C" {
#endif

struct kv_attribute {
    TAILQ_ENTRY(kv_attribute)    link;
    char                *key;
    char                *value;
};

struct http_session {
    UT_hash_handle                  hh;
    char                            *id;
    int64_t                         expiryTime;
    void                            *data;
    TAILQ_HEAD(,kv_attribute)       attributes;
};

static inline kv_attribute*
session_find_attribute(http_session *session, const char* name) {
    kv_attribute *attr;
    TAILQ_FOREACH(attr, &session->attributes, link) {
        if (!strcmp(attr->key, name)) return attr;
    }
    return NULL;
}

#ifdef __cplusplus
};
#endif

namespace sparc {
    namespace detail {

#ifndef SPARC_SESSION_TIMEOUT
#define SPARC_SESSION_TIMEOUT   3600
#endif

        HttpSession::HttpSession(HttpSessionManager *mgr)
            : mgr_(mgr),
              session_(NULL)
        {}

        HttpSession::~HttpSession() {
            if (session_) {
                kv_attribute *attr, *tmp;
                if (session_->id) kore_free(session_->id);

                // clean-up attributes
                for (attr = TAILQ_FIRST(&session_->attributes); attr != NULL; attr = tmp) {
                    tmp = TAILQ_NEXT(attr, link);
                    TAILQ_REMOVE(&session_->attributes, attr, link);
                    if (attr->key) kore_free(attr->key);
                    if (attr->value) kore_free(attr->value);
                    kore_free(attr);
                }

                kore_free(session_);
                session_ = NULL;
            }
        }

        cc_string HttpSession::attribute(cc_string name) {
            kv_attribute *attr;
            TAILQ_FOREACH(attr, &session_->attributes, link) {
                if (!strcmp(attr->key, name)) return attr->value;
            }
            return NULL;
        }

        void HttpSession::attribute(cc_string name, cc_string value) {
            addAttribute(name, kore_strdup(value));
        }
        void HttpSession::addAttribute(cc_string name, c_string value) {
            kv_attribute *attr;
            attr = session_find_attribute(session_, name);
            if (attr == NULL) {
                attr = (kv_attribute *) kore_calloc(1, sizeof(kv_attribute));
                attr->key = kore_strdup(name);
                TAILQ_INSERT_TAIL(&session_->attributes, attr, link);
            }
            attr->value = value;
        }

        cc_string HttpSession::id() {
            return session_->id;
        }

        void HttpSession::eachAttribute(KVIterator it) {
            if (it) {
                kv_attribute *attr;
                TAILQ_FOREACH(attr, &session_->attributes, link) {
                    if (!it(attr->key, attr->value)) break;
                }
            }
        }

        c_string HttpSession::removeAttribute(cc_string name) {
            kv_attribute *attr;
            c_string value = NULL;

            attr = session_find_attribute(session_, name);
            if (attr) {
                value = attr->value;
                TAILQ_REMOVE(&session_->attributes, attr, link);
                kore_free(attr->key);
                kore_free(attr);
                attr = NULL;
            }

            return value;
        }

        long HttpSession::exipryTime() {
            return session_->expiryTime;
        }

        bool HttpSession::isNew() {
            return new_;
        }

        HttpSessionManager::HttpSessionManager(int64_t *timeout)
            : timeout_(timeout),
              sessions_(NULL)
        {
            if (!RAND_bytes(SESSIONS_KEY, sizeof(SESSIONS_KEY))) {
                $error("Unable to allocate sessions key");
            }
        }

        HttpSessionManager::~HttpSessionManager() {
            http_session *raw, *tmp;
            HASH_ITER(hh, sessions_, raw, tmp) {
                discard(raw);
            }
            sessions_ = NULL;
        }

        cc_string HttpSessionManager::ipToSessionId(cc_string ip) const {
            cc_string dstr = NULL;
            buffer tmp(64);
            cc_string sessionId;
            tmp.append(SESSIONS_KEY, sizeof(SESSIONS_KEY));
            tmp.append(ip, strlen(ip));
            dstr = tmp.toString();
            sessionId = md5Hash(dstr, NULL, tmp.offset()-1);
            return (char *) sessionId;
        }

        HttpSession* HttpSessionManager::find(cc_string sessionId) {
            http_session *raw;
            HASH_FIND_STR(sessions_, sessionId, raw);
            if (raw) {
                // session has expired, delete immediately
                if (raw->expiryTime > 0 && ((uint64_t)raw->expiryTime < datetime::now())) {
                    kore_debug("session expired... %ld %lu", raw->expiryTime, datetime::now());
                    discard(raw);
                } else {
                    HttpSession *session = (HttpSession *) raw->data;
                    session->session_->expiryTime =
                            *timeout_ < 0 ? *timeout_ : (int64_t) (*timeout_ + datetime::now());
                    return session;
                }
            }
            kore_debug("session not found...");
            return NULL;
        }

        void HttpSessionManager::discard(http_session *raw) {
            if (raw) {
                HttpSession *session = (HttpSession *)raw->data;
                HASH_DEL(sessions_, raw);
                delete  session;
            }
        }

        HttpSession* HttpSessionManager::create(cc_string ip, bool create) {
            HttpSession *session;
            cc_string sessionId = ipToSessionId(ip);
            session = find(sessionId);
            if (session == NULL && create) {
                session = new HttpSession(this);
                session->session_ = (http_session *) kore_calloc(1, sizeof(http_session));
                session->new_ = true;
                session->session_->expiryTime =
                        *timeout_ < 0 ? *timeout_ : (int64_t) (*timeout_ + datetime::now());
                session->session_->data = session;
                session->session_->id   = kore_strdup(sessionId);
                TAILQ_INIT(&session->session_->attributes);
                // add to sessions
                HASH_ADD_STR(sessions_, id, session->session_);
                kore_debug("created a new session..., %s:%ld",
                           session->session_->id, session->session_->expiryTime);
            } else if (session) {
                // this is not a new session
                session->new_ = false;
                kore_debug("using a cached session..., %s",
                           sessionId);
            }

            return session;
        }
    }
}
