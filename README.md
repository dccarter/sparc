[![Build Status](https://travis-ci.org/dccarter/sparc.svg?branch=master)](https://travis-ci.org/dccarter/sparc)

# `$.Sparc(c++1y); // lightweight web framework `

Sparc C++1y is Linux lightweight [Spark](http://sparkjava.com/) web framework  for designing web applications/API
in morden C++. The framework uses the blazing fast components of [kore](https://kore.io/).

####*This project is still under heavy development*

### Getting Started
 
 Follow the following steps to build the project including examples. *The idea is to add a custom tool on top of CMake that
 will be used to manage sparc projects. For now follow the [examples](examples/CMakeLists.txt) to add build custom projects*

```sh
> git clone https://github.com/dccarter/sparc.git
> cd sparc/
> mkdir build
> cd build
> cmake ../
> make
> cd examples
> make install
```
The following is a basic Hello World application in sparc

```C++
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

```
This will create a simple REST API that returns '*hello world*' and can be viewed at [localhost](http://127.0.0.1:1080/hello)


### Examples
The following examples try to match those provided by sparcjava where possible

###[`basic`](examples/basic.cpp)`

```C++
#include "sparc.h"

using namespace sparc;

int main(int argc, char *argv[]) {

    $enter(argc, argv);

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
```

#####[`Simple CRUD`](examples/crud.cpp)
Example shows how to implement other other REST methods
```C++
#include <unordered_map>
#include "sparc.h"

struct Book {
    std::string author;
    std::string title;
};

using namespace sparc;

#define map_ite(a, m) for (auto a : m )

int main(int argc, char *argv[])
{
    // the most elegant solution for you will be to use std map here
    $enter(argc, argv);


    static int    UID = 0;
    std::unordered_map<int, Book> books_;

    post("/books", $(req, res) {
        cc_string author = req.queryParam("author");
        cc_string title = req.queryParam("title");
        if (!author || !title) {
            return BAD_REQUEST;
        }
        Book b;
        b.author = author;
        b.title  = title;
        books_[UID++] = b;
        return CREATED;
    });

    get("/books/:id", $(req, res) {
        std::string s = req.param("id");
        Book *b = NULL;
        map_find(books_, std::stoi(s), b);
        if (b) {
            res.body().appendf("Title: %s, Author: %s", cstr(b->title), cstr(b->author));
            return OK;
        } else {
            res.body("Book not found");
            return NOT_FOUND;
        }
    });


    put("/books/:id", $(req, res) {
        std::string id = req.param("id");
        Book *b = NULL;
        map_find(books_, std::stoi(id), b);
        if (b != NULL) {
            cc_string author = req.queryParam("author");
            cc_string title  = req.queryParam("title");
            if (author) b->author = author;
            if (title)  b->title  = title;

            res.body().appendf("Book with id '%s' updated", cstr(id));
            return OK;
        } else {
            res.body("Book not found");
            return NOT_FOUND;
        }
    });

    del("/books/:id", $(req, res) {
        std::string id = req.param("id");
        Book *b = NULL;
        auto it = books_.find(std::stoi(id));
        if (it != books_.end()) {
            books_.erase(it);
            res.body().appendf("Book with id '%s' deleted", cstr(id));
            return OK;
        } else {
            res.body("Book not found");
            return NOT_FOUND;
        }
    });

    get("/books", $(req, res) {
        map_ite(b, books_) {
            res.body().appendf("%d ", b.first);
        }
        return OK;
    });

    return $exit();
}
```

#####[`suath`](/examples/sauth.cpp)
Examples shows the usage of filters or codecs
```C++
#include "sparc.h"
#include <unordered_map>
#include <cstring>

using namespace sparc;

int main(int argc, char *argv[])
{
    $enter(argc, argv);

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
```

#####[`static files`](examples/serve.cpp)
Example shows how to serve static files. Note that the in `staticFiles::location(serve_PUBLIC)`, the macro `serve_PUBLIC` is defined in the CMake build script. The call `staticFiles::location` will try to set the serving directory relative to the directory `${WORKING_DIR}/assets`. For absolute directories use `staticFiles::externalLocation`
```C++
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
```
For this example, view the served page @ [localhost](http://127.0.0.1:1080)


### More to come
- documentation
- more examples
- more unit tests
- websocket API
- TLS is already built into kore, need to be enabled and tested
