//
// Created by dc on 11/26/16.
//

#include "kore.h"
#include "bgtimer.h"

#ifdef __cplusplus
#endif

static void handle_timeout(void *context, u_int64_t);

#ifdef __cplusplus
#endif

namespace sparc {
    namespace detail {

        struct background_timer : public  timerid_t {
            kore_timer              *timer;
            timedout                handler;
            bool                    canceled;

            background_timer(timedout th)
                : handler(th), canceled(false)
            {}

            virtual void cancel() override {
                if (!canceled) {
                    canceled    = true;
                    timer->arg  = NULL;

                    // if not in place it will be removed soon
                    if (timer->flags & KORE_TIMER_INPLACE)
                        kore_timer_remove(timer);
                    else
                        timer->flags |= KORE_TIMER_ONESHOT;
                    handler = NULL;
                }
            }

            void fire() {
                if (!canceled && handler) {
                    handler(this);
                }
            }

            virtual ~background_timer() {
                cancel();
            }

            OVERLOAD_MEMOPERATORS();
        };

        BgTimerManager::BgTimerManager()
            : cache_(true)
        {
            TAILQ_INIT(&waitlist_);
        }

        BgTimerManager::~BgTimerManager() {
            if (!TAILQ_EMPTY(&waitlist_)) {
                kore_timer *t, *tmp;
                for (t = TAILQ_FIRST(&waitlist_); t != NULL; t = tmp) {
                    tmp = TAILQ_NEXT(t, list);
                    TAILQ_REMOVE(&waitlist_, t, list);

                    // free the timers
                    if (t->arg) {
                        background_timer *bgtimer = (background_timer *) t->arg;
                        // as we know this won't free the allocated timer memory
                        delete bgtimer;
                    }
                    kore_free(t);
                }
            }
        }

        void BgTimerManager::schedule(timerid_t::timedout hh, u_int64_t tout, int flags) {
            if (hh && tout > 10) {
                background_timer *bgtimer = new background_timer(hh);
                kore_timer *timer = kore_timer_new(handle_timeout, tout, bgtimer, flags);
                bgtimer->timer = timer;
                if (cache_) {
                    TAILQ_INSERT_TAIL(&waitlist_, timer, list);
                } else {
                    kore_timer_set(timer);
                }
            } else
                $error("unsupported parameters while scheduling timer");
        }

        void BgTimerManager::flush() {
            cache_ = false;
            if (!TAILQ_EMPTY(&waitlist_)) {
                kore_timer *t, *tmp;
                // transfer wait list to kore timer management
                for (t = TAILQ_FIRST(&waitlist_); t != NULL; t = tmp) {
                    tmp = TAILQ_NEXT(t, list);
                    TAILQ_REMOVE(&waitlist_, t, list);

                    kore_timer_set(t);
                }
            }
        }
    }
}

static void
handle_timeout(void *context, u_int64_t curr)
{
    if (context) {
        sparc::detail::background_timer *bgtimer = (sparc::detail::background_timer *) context;
        bgtimer->fire();
        if (bgtimer->timer == NULL || bgtimer->timer->flags & KORE_TIMER_ONESHOT || bgtimer->canceled) {
            $debug("deallocating timer memory");
            delete bgtimer;
        }
    } else {
        $error("invalid timer context");
    }
}
