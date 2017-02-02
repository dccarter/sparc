//
// Created by dc on 11/29/16.
//

#include "sparc.h"

using namespace sparc;

int main(int argc, char *argv[]) {

    $enter(argc, argv);
    config::domain(SPARC_DOMAIN_NAME);
    config::workers(4);
    config::debug($OFF);

    get("/json/:a/:b", "application/json", $(req, res) {
        res.body().appendf("{\"a\":\"%s\",\" b\":\"%s\"}", req.param("a"), req.param("b"));
        return OK;
    });

    get("/misc/style", $(req, res) {
        res << "Hello World" << "\n";
        res << "you are " << 5 << " years old, so " << 9.7 << " points";
        return OK;
    });

    webSocket("/websock",

        $message(ws, data, size, op) {
            ws->broadcast(data, size, op);
            $info("received message: op=%hhu, size=%lu", op, size);
        },

        $connect(ws) {
            $info("Websocket connected");
        },

        $disconnect(ws) {
            $info("WebSocket disconnected");
        });

    return $start($onload(){

        $0 {
            // Only schedule timer on core 0
            $timeout($tm(tid) {

                $info("timer expired...");
            }, 2000, TIMER_ONESHOT);
        }
        $els $1 {
            $debug("worker 1 started");
        };
    });
}
