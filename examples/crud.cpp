//
// Created by dc on 11/29/16.
//

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
    config::debug($OFF);


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

