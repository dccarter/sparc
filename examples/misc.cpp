//
// Created by dc on 11/29/16.
//

#include <syslog.h>
#include "sparc.h"

using namespace sparc;

int main(int argc, char *argv[]) {

    $enter(argc, argv);

    get("/json/:a/:b", "application/json", $(req, res) {
        res.body().appendf("{\"a\":\"%s\",\" b\":\"%s\"}", req.param("a"), req.param("b"));
        return OK;
    });

    return $exit();
}