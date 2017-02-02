//
// Created by dc on 11/29/16.
//

#include <openssl/md5.h>
#include <cstring>
#include "sparc.h"

using namespace sparc;

int main(int argc, char *argv[])
{
    $enter(argc, argv);

    // serve files at public directory
    staticFiles::location(serve_PUBLIC);
    staticFiles::header("Static-Header", "Static-Value");

    get("/hello", $(req, res) {
        res.body("Hello from dynamic");
        return OK;
    });

    return $exit();
}
