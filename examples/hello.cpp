//
// Created by dc on 11/29/16.
//

#include "sparc.h"

using namespace sparc;

int main(int argc, char *argv[])
{
    $enter(argc, argv);
    config::debug($OFF);

    before("/hello", $(req, res) {
        res.redirect("/");
        return $ABORT;
    });

    get("/", "application/json", $(req, res) {
        Json *c = Json::create();
        JsonObject json = c;
        json.set("user", "carter");
        json.arr("numers", 1, 2, 4, 8, 9, 10);
        json.arr("numers1", 1, 2, 4, 8, 9, 10);
        json.arr("numers2", 1, 2, 4, 8, 9, 10);
        json.arr("numers3", 1, 2, 4, 8, 9, 10);
        json.arr("numers4", 1, 2, 4, 8, 9, 10);
        json.arr("numers5", 1, 2, 4, 8, 9, 10);
        json.arr("numers6", 1, 2, 4, 8, 9, 10);
        json.arr("numers7", 1, 2, 4, 8, 9, 10);
        json.arr("numers8", 1, 2, 4, 8, 9, 10);
        json.arr("numers9", 1, 2, 4, 8, 9, 10);
        json.arr("numers10", 1, 2, 4, 8, 9, 10);
        json.arr("numers11", 1, 2, 4, 8, 9, 10);
        json.arr("numers12", 1, 2, 4, 8, 9, 10, "this is meant to be a very long sting of characters in a json array"
                "this is meant to be a very long sting of characters in a json array"
                "this is meant to be a very long sting of characters in a json array"
                "this is meant to be a very long sting of characters in a json array"
                "this is meant to be a very long sting of characters in a json array"
                "this is meant to be a very long sting of characters in a json array"
                "this is meant to be a very long sting of characters in a json array"
                "this is meant to be a very long sting of characters in a json array");
        res << c;
        delete c;
        return OK;
    });

    get("/hello1/:a/:b", $(req, res) {
        return OK;
    });

    get("/hello1/one/two/:b/:b/:c/:d/:e/:f/:g/:h", $(req, res) {
        return OK;
    });
    get("/hello2/:a/:b", $(req, res) {
        return OK;
    });
    get("/hello3/:a/:b", $(req, res) {
        return OK;
    });
    get("/hello4/:a/:b", $(req, res) {
        return OK;
    });
    get("/hello5/:a/:b", $(req, res) {
        return OK;
    });
    get("/hello1/one/:a", $(req, res) {
        return NO_CONTENT;
    });

    get("/admin/routes", "application/json", $(req, res) {
        res << admin::routes();
        return OK;
    });

    return $exit();
}
