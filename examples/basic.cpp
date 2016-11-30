//
// Created by dc on 11/29/16.
//

#include "sparc.h"

using namespace sparc;

int main(int argc, char *argv[]) {

    $enter(argc, argv);
    config::debug($ON);

    get("/hello", $(req, res) {
        res.body("Hello world");
        return OK;
    });

    post("/hello", $(req, res) {
        res.body().appendf("Hello World: %s", req.body());
        return OK;
    });

    get("/private", $(req, res){
        res.body("Go away!!!");
        return UNAUTHORIZED;
    });

    get("/users/:name", $(req, res) {
        res.body().appendf("Selected user: %s", req.param("name"));
        return OK;
    });

    get("/news/:section", $(req, res) {
        res.type("text/html");
        res.body().appendf("<?xml version=\"1.0\" encoding=\"UTF-8\"?><news>%s</news>",
                           req.param("section"));
        return OK;
    });

    get("/protected", $(req, res) {
        $halt(FORBIDDEN, "I don't think so!!!");
    });

    get("/redirect", $(req, res) {
        res.redirect("/news/world");
        return OK;
    });

    get("/", $(req, res) {
        res.body("root");
        return OK;
    });

    return $exit();
}