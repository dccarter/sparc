//
// Created by dc on 1/14/17.
//
#include <iostream>
#include "sparc.h"

using  namespace sparc;

int main(int argc, char *argv[]) {

    $enter(argc, argv);
    config::domain(SPARC_DOMAIN_NAME);
    config::debug($OFF);
    config::workers(1);

    db::PgSqlContext ctx("db", "user=postgres password=8292db dbname=mytest hostaddr=127.0.0.1");
    const int DB_MYTEST = config::db(&ctx);

    get("/pgsql/:id", "application/json", $(req, res) {
        db::Sql sql;
        sql("SELECT * FROM cafe")
           ("WHERE id >= %s", req.param("id"));

        db::Result result = db::query(DB_MYTEST, sql);
        if (result && *result) {
            size_t tmp = 64;
            res << result->toJson()->encode(tmp, "\t");
        }
        return OK;
    });

    get("/row/:id", $(req, res) {
        db::Sql sql;
        sql("SELECT * FROM cafe")
           ("WHERE id = %s", req.param("id"));
        sql.debug();
        auto result = db::query(DB_MYTEST, sql);
        if (result && *result) {
            db::result_t& r1 = *result;;
            for (auto& row: r1) {
                res << "=========== RECORD============\n";
                res << "id: " << row["id"] << "\n";
                res << "category: " << row["category"] << "\n";
                res << "name: " << row["name"] << "\n";
                res << "price: " << row["price"] << "\n";
                res << "last_upade: " << row["last_update"] << "\n";
            }
            return OK;
        }

        return NO_CONTENT;
    });

    get("/async/:id", "application/json", $(req, res) {
        int status = $ASYNC;
        db::Sql sql(req);
        sql("SELECT * from cafe")
           ("WHERE id >= %s", req.param("id"));

        status =
        db::query(DB_MYTEST, sql, $db(q, r) {
            if (r) {
                Response& resp = q.resp();
                size_t tmp = 64;
                q.resp() << r.toJson()->encode(tmp);
                return OK;
            }
            return NO_CONTENT;
        });

        $async(status);
    });

    get("/async/raw/:id", $(req, res) {
        int status = $ASYNC;
        db::Sql sql(req);
        sql("SELECT * from cafe")
           ("WHERE id >= %s", req.param("id"));
        status =
        db::query(DB_MYTEST, sql, $db(sq, r){
            if (r) {
                for (auto& row: r) {
                    sq.resp() << "=========== RECORD============\n";
                    sq.resp() << "id: " << row["id"] << "\n";
                    sq.resp() << "category: " << row["category"] << "\n";
                    sq.resp() << "name: " << row["name"] << "\n";
                    sq.resp() << "price: " << row["price"] << "\n";
                    sq.resp() << "last_upade: " << row["last_update"] << "\n";
                }
                return OK;
            }
            return NO_CONTENT;
        });
        $async(status);
    });

    return $exit();
}
