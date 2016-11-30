//
// Created by dc on 11/24/16.
//

#ifndef SPARC_SESSION_H
#define SPARC_SESSION_H

#include "common.h"


#ifdef __cplusplus
extern "C" {
#endif
    struct http_session;
#ifdef __cplusplus
}
#endif

namespace sparc {
    namespace detail {

        class HttpSessionManager;

        class HttpSession : public Session {
        public:
            virtual void attribute(cc_string, cc_string) override ;
            virtual void attributeSet(cc_string name, c_string value) override {
                addAttribute(name, value);
            }
            virtual cc_string attribute(cc_string) override ;
            virtual c_string removeAttribute(cc_string) override ;
            virtual void eachAttribute(KVIterator) override;
            virtual cc_string id() override;
            virtual bool isNew() override;
            virtual int64_t exipryTime() override;
            virtual ~HttpSession();

            OVERLOAD_MEMOPERATORS();
        private:
            void addAttribute(cc_string, c_string);
            friend class HttpSessionManager;
            HttpSession(HttpSessionManager*);
            bool                        new_;
            http_session                *session_;
            HttpSessionManager          *mgr_;
        };

        class HttpSessionManager {
        public:
            HttpSession *find(cc_string sessionId);
            HttpSession *create(cc_string sessionId);
            ~HttpSessionManager();

            OVERLOAD_MEMOPERATORS();
        private:
            friend class App;
            HttpSessionManager(int64_t);
            void discard(http_session*);
            http_session    *sessions_;
            int64_t         timeout_;
        };
    }
}
#endif //SPARC_SESSION_H
