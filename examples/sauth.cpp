//
// Created by dc on 11/29/16.
//

#include "sparc.h"
#include <unordered_map>
#include <cstring>

using namespace sparc;

int main(int argc, char *argv[])
{
    $enter(argc, argv);
    config::debug($ON);

    std::unordered_map<std::string, std::string> passbox_;
    passbox_.insert({"foo", "bar"});
    passbox_.insert({"admin", "admin"});

    before($(req, res) {
        cc_string user = req.queryParam("user");
        cc_string password = req.queryParam("password");
        if (user && password) {
            std::string *pbpass;
            map_find(passbox_, user, pbpass);
            if (pbpass && !strcmp(password, pbpass->c_str())) {
                return $CONTINUE;
            }
        }

        $halt(UNAUTHORIZED, "Gotcha, you are not a registered user :-)");
        return $ABORT;
    });

    before("/hello", $(req, res) {
        res.header("Foo", "Set by second before filter");
        return $CONTINUE;
    });

    get("/hello", $(req, res) {
        res.body().appendf("You are an authorized user: %s", req.queryParam("user"));
        return OK;
    });

    after("/hello", $(req, res) {
        res.header("Sparc", "Set by the after filter (FIDEC)");
        return OK;
    });

    return $exit();
}