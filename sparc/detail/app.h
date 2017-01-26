//
// Created by dc on 11/24/16.
//

#ifndef SPARC_APP_H
#define SPARC_APP_H

#include <functional>
#include <sparc.h>

#include "static_file.h"
#include "bgtimer.h"
#include "router.h"
#include "session.h"
#include "request.h"
#include "response.h"
#include "router.h"
#include "db.h"

#ifdef  __cplusplus
extern "C" {
#endif

void kore_onload(void);
int sparcxx(http_request*);

#ifdef  __cplusplus
}
#endif

namespace sparc {

    namespace detail {

        struct Fidec {
            TAILQ_ENTRY(Fidec)  link;
            handler             h;
            c_string      match;

            OVERLOAD_MEMOPERATORS();
        };
        typedef  TAILQ_HEAD(,Fidec) Fidecs;

        struct Configuration {
            c_string      bindIp;
            c_string      bindPort;
            c_string      accessLog;
            int64_t             sessionTimeout;
        };

        class App : public Configuration {
        public:
            int handle(http_request*);
            Router& router() {
                return router_;
            }

            void addFidec(bool, Fidec*);
            static App* app() { return SPARC; }
            static bool init();
            static bool initialized() { return SPARC != NULL; }
            HttpSessionManager& sessionManager();
            BgTimerManager&     timerManager();
            db::DbManager&      dbManager();
            StaticFilesRouter&  staticFiles();
            int run();
        private:
            int     callFidecs(Fidecs*, cc_string, HttpRequest&, HttpResponse&);
            int     callRequestHandler(RouteHandler*, HttpRequest&, HttpResponse&);
            int     startAsyncStateMachine();
            bool    initConfig();
        private:
            App();
            Router              router_;
            HttpSessionManager  sessionManager_;
            db::DbManager       dbManager_;
            Fidecs              before_;
            Fidecs              after_;
            BgTimerManager      bgTimerManager_;
            StaticFilesRouter   sfRouter_;
            static App          *SPARC;
        };

        class HaltException : public std::runtime_error {
        public:
            HaltException(int status, cc_string);
            const int status() const { return status_; }

        private:
            int                 status_;
        };

        class InitializetionException : public std::runtime_error {
        public:
            InitializetionException(int exitCode, cc_string msg);
            const int exitCode() const { return exitCode_; }
        private:
            int             exitCode_;
        };
    }
}
#endif //SPARC_APP_H
