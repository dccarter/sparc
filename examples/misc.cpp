//
// Created by dc on 11/29/16.
//

#include "sparc.h"

using namespace sparc;

int main(int argc, char *argv[]) {

    $enter(argc, argv);
    config::domain(SPARC_DOMAIN_NAME);
    config::workers(4);

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

    return $exit();
}