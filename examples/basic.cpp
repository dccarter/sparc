//
// Created by dc on 11/29/16.
//

#include "sparc.h"

using namespace sparc;

int main(int argc, char *argv[]) {

    $enter(argc, argv);
    config::domain(SPARC_DOMAIN_NAME);

    get("/hello", $(req, res) {
        res << "Hello world";
        return OK;
    });

    post("/hello", $(req, res) {
        res << "Hello World: " << req.body();
        return OK;
    });

    get("/private", $(req, res){
        res << "Go away!!!";
        return UNAUTHORIZED;
    });

    get("/users/:name", $(req, res) {
        res << "Selected user: " << req.param("name");
        return OK;
    });

    get("/news/:section", $(req, res) {
        res.type("text/html");
        res << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><news>"
            << req.param("section")
            << "</news>";
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