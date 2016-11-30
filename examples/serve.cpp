//
// Created by dc on 11/29/16.
//

#include "sparc.h"

using namespace sparc;

int main(int argc, char *argv[])
{
    $enter(argc, argv);
    config::debug($OFF);
    config::ip("192.168.214.143");
    config::domain("192.168.214.143");

    // serve files at public directory
    staticFiles::location(serve_PUBLIC);
    staticFiles::header("Static-Header", "Static-Value");

    get("/hello", $(req, res) {
        res.body("Hello from dynamic");
        return OK;
    });

    return $exit();
}
