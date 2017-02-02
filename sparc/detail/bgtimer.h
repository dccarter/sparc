//
// Created by dc on 11/26/16.
//

#ifndef SPARC_BGTIMER_H
#define SPARC_BGTIMER_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct kore_timer;

#ifdef __cplusplus
}
#endif

namespace sparc {
    namespace detail {
        class BgTimerManager {
        public:
            BgTimerManager();
            void flush();
            void schedule(timerid_t::timedout, u_int64_t, int flags);
            ~BgTimerManager();
            OVERLOAD_MEMOPERATORS();
        private:
            bool                      cache_;
            // this list will only be valid prior to initialization,
            // after initialization let kore manager it's timers
            TAILQ_HEAD(,kore_timer)   waitlist_;
        };
    }
}
#endif //SPARC_BGTIMER_H
