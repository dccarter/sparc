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
        // request authentication
        auth::user_t user = auth::authenticate(req, [&](auth::auth_info_t& info){
            bool result = true;
            if (info.type == auth::AUTH_TYPE_DIGEST) {
                info.password = "bar";
            } else if (info.type == auth::AUTH_TYPE_BASIC) {
                info.password = "bar";
                info.salt = "bar";
            } else {
                result = false;
            }
            return result;
        }, auth::AUTH_TYPE_DIGEST);

        if(!user) {
            if (auth::authorize(req, res, user, "sparc", auth::AUTH_TYPE_DIGEST)) {
                $halt(UNAUTHORIZED, "");
            }
            return $ABORT;
        } else {
            req.session()->attribute("user", user.getUsername());
            return $CONTINUE;
        }
    });

    before("/hello", $(req, res) {
        res.header("Foo", "Set by second before filter");
        return $CONTINUE;
    });

    get("/hello", $(req, res) {
        res.body().appendf("You are an authorized user: %s", req.session("user"));
        return OK;
    });

    after("/hello", $(req, res) {
        res.header("Sparc", "Set by the after filter (FIDEC)");
        return OK;
    });

    return $exit();
}
