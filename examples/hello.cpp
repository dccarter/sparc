//
// Created by dc on 11/29/16.
//

#include "sparc.h"

using namespace sparc;

int main(int argc, char *argv[])
{
    $enter(argc, argv);

    get("/hello", $(req, res) {
        res.body("Hello World");
        return OK;
    });

    return $exit();
}
