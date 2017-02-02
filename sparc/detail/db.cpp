//
// Created by dc on 1/14/17.
//
#include "sparc.h"
#include "db.h"
#include "pgsql.h"
#include "kore.h"
#include "app.h"

#define SPARC_DEFAULT_NUM_DATABASES (5)

#ifdef __cplusplus
extern "C" {
#endif

static int pgsql_async_clean_up(struct http_request *req);
static int pgsql_async_init(struct http_request *req);
static int pgsql_async_query(struct http_request *req);
static int pgsql_async_wait(struct http_request *req);
static int pgsql_async_read(struct http_request *req);
static int pgsql_async_error(struct http_request *req);
static int pgsql_async_done(struct http_request *req);

typedef enum dbasync_states {
    DB_ASYNC_STATE_INIT = 0,
    DB_ASYNC_STATE_QUERY,
    DB_ASYNC_STATE_WAIT,
    DB_ASYNC_STATE_READ,
    DB_ASYNC_STATE_ERROR,
    DB_ASYNC_STATE_DONE,
} dbaysnc_states_t;

static struct http_state pgsql_async_states[] = {
    {"DB_ASYNC_STATE_INIT", pgsql_async_init},
    {"DB_ASYNC_STATE_QUERY", pgsql_async_query},
    {"DB_ASYNC_STATE_WAIT", pgsql_async_wait},
    {"DB_ASYNC_STATE_READ", pgsql_async_read},
    {"DB_ASYNC_STATE_ERROR", pgsql_async_error},
    {"DB_ASYNC_STATE_DONE", pgsql_async_done},
};
#define PGSQL_ASYNC_STATES_SIZE sizeof(pgsql_async_states) / sizeof(struct http_state)

#ifdef __cplusplus
}
#endif

namespace sparc {
    namespace db {

        DbManager::DbManager(size_t ndatabases)
                : ndatabses_(ndatabases <= 0 ? SPARC_DEFAULT_NUM_DATABASES : ndatabases) {
            databases_ = (Context **) kore_calloc(sizeof(Context *), ndatabses_);
        }

        bool DbManager::init() {
            for (size_t i = 0; i < ndatabses_; i++) {
                if (databases_[i] != nullptr) {
                    kore_debug("initializing database at index %u", i);
                    if (databases_[i]->init() == 0) {
                        kore_log(LOG_ERR, "initializing database failed");
                        return false;
                    }
                }
            }
            return true;
        }

        size_t DbManager::add(Context *db) {
            size_t eid = 0;
            for (size_t i = 0; i < ndatabses_; i++) {
                if (databases_[i] == nullptr && eid == 0) {
                    eid = i + 1;
                }
                if (databases_[i] == db) {
                    return i + 1;
                }
            }
            if (eid) {
                databases_[eid - 1] = db;
            }
            return eid;
        }

        Context *DbManager::get(size_t index) {
            size_t eid = index - 1;
            return (eid < ndatabses_) ? databases_[eid] : nullptr;
        }

        void DbManager::remove(size_t index) {
            size_t eid = index - 1;
            if (eid < ndatabses_) {
                databases_[eid] = nullptr;
            }
        }
        Sql::Sql(Request &req)
            : req_(&req),
              res_(nullptr),
              sql_(32),
              result_(nullptr)
        {}
        Sql::Sql()
           : req_(nullptr),
             res_(nullptr),
             sql_(32),
             result_(nullptr)
        {}

        Sql::Sql(Sql &&sql)
            : res_(sql.res_),
              req_(sql.req_),
              sql_(std::move(sql.sql_)),
              result_(sql.result_)
        {
            sql.result_ = nullptr;
            sql.res_ = nullptr;
            sql.req_ = nullptr;
        }

        Sql::~Sql() {
            if (result_) {
                delete result_;
                result_ = nullptr;
            }
        }

        Sql &Sql::operator()(cc_string fmt, ...) {
            va_list args;
            sql_.append(" ", 1);
            va_start(args, fmt);
            sql_.appendv(fmt, args);
            va_end(args);
            return *this;
        }

        void Sql::setResult(result_t *result) {
            if (result_) delete result_;
            result_ = result;
        }

        void Sql::debug() {
            kore_debug(sql());
        }

        Row &result_t::row_iterator_t::operator*() const {
            return *(results_->at(index_));
        }

        PgSqlRow::PgSqlRow(PgSqlResult *result, size_t idx)
                : Row(result, idx),
                  sql_(result->sql_) {}

        cc_string PgSqlRow::value(size_t idx) const {
            return kore_pgsql_getvalue(sql_, index_, idx);
        }

        cc_string PgSqlRow::value(cc_string fname) const {
            return kore_pgsql_getvalue_by_name(sql_, index_, fname);
        }

        PgSqlResult::PgSqlResult(struct kore_pgsql *sql, bool sqlGive)
                : sql_(nullptr),
                  rows_(nullptr),
                  nrows_(0),
                  json_(nullptr),
                  sqlOwner_(sqlGive) {
            setup(sql, sqlGive);
        }

        PgSqlResult::PgSqlResult()
                : PgSqlResult(nullptr, false) {}

        int PgSqlResult::setup(struct kore_pgsql *sql, bool sqlGive) {
            int status = 1;

            do {
                if (sql_ != NULL) {
                    kore_debug("Pgsql result already setup");
                    status = 0;
                    break;
                }

                sql_ = sql;
                sqlOwner_ = sqlGive;

                size_t i;
                nrows_ = (size_t) kore_pgsql_ntuples(sql_);
                rows_ = (PgSqlRow *) memory::calloc(sizeof(PgSqlRow), nrows_ + 1);
                if (rows_ == NULL) {
                    $error("unable to allocate memory for results row");
                    status = 0;
                    break;
                };

                for (i = 0; i <= nrows_; i++) {
                    PgSqlRow row(this, i);
                    memcpy(&rows_[i], &row, sizeof(row));
                }
            } while (0);

            return status;
        }

        PgSqlResult::PgSqlResult(PgSqlResult &&result)
                : sql_(result.sql_),
                  nrows_(result.nrows_),
                  rows_(result.rows_),
                  json_(result.json_),
                  sqlOwner_(result.sqlOwner_) {
            $debug("MOVED...");
            result.json_ = nullptr;
            result.sql_ = nullptr;
            result.nrows_ = 0;
            result.rows_ = nullptr;
        }

        Json *PgSqlResult::toJson() {
            if (json_ == nullptr && sql_ != nullptr) {
                int nfields = kore_pgsql_nfields(sql_);
                json_ = Json::create();
                JsonObject entries = JsonObject(json_).arr("results");
                for (size_t i = 0; i < nrows_; i++) {
                    JsonObject entry = entries.add();
                    for (int j = 0; j < nfields; j++) {
                        entry.set(
                                kore_pgsql_fname(sql_, j),
                                kore_pgsql_getvalue(sql_, i, j));
                    }
                }
            }
            return json_;
        }

        PgSqlResult::~PgSqlResult() {
            if (rows_ != NULL) {
                kore_debug("deleting result: %p", this);
                if (json_) {
                    delete json_;
                    json_ = NULL;
                }

                memory::free(rows_);
                rows_ = NULL;

                if (sql_ && sqlOwner_) {
                    kore_pgsql_cleanup(sql_);
                    kore_free(sql_);
                }
                sql_ = NULL;
            }
        }

        struct PgsqlAsync {
            PgsqlAsync(Sql &&query,
                       struct pgsql_db *database,
                       OnAsync cb,
                       detail::RouteHandler *routeHandler)
                : sql(std::move(query)),
                  db(database),
                  handler(cb),
                  status(HTTP_STATUS_INTERNAL_ERROR),
                  rh(routeHandler)
            {}

            Sql sql;
            struct kore_pgsql pgsql;
            struct pgsql_db *db;
            OnAsync handler;
            int status;
            detail::RouteHandler *rh;

            OVERLOAD_MEMOPERATORS();
        };

        PgSqlContext::PgSqlContext(cc_string name, cc_string connString)
                : name_(kore_strdup(name)),
                  connString_(kore_strdup(connString)),
                  db_(NULL) {
        }

        PgSqlContext::~PgSqlContext() {
            if (name_) {
                kore_free(name_);
                name_ = NULL;
            }

            // remove database
            if (db_) {
                kore_pgsql_unregister(db_);
                db_ = NULL;
            }
        }

        int PgSqlContext::init() {
            if (db_ == NULL, name_ && connString_) {
                // reqgister database and get handler
                db_ = kore_pgsql_register(name_, connString_);
                if (db_) return 1;
            }
            return 0;
        }

        Result PgSqlContext::exec(Sql &sql) {
            struct kore_pgsql *pgsql =
                    (struct kore_pgsql *) kore_malloc(sizeof(struct kore_pgsql));
            if (!kore_pgsql_query_init(pgsql, NULL, db_, KORE_PGSQL_SYNC)) {
                kore_pgsql_logerror(pgsql);
                kore_free(pgsql);
                return nullptr;
            }

            if (!kore_pgsql_query(pgsql, sql.sql())) {
                kore_pgsql_logerror(pgsql);
                kore_free(pgsql);
                return nullptr;
            }

            return new PgSqlResult(pgsql);
        }

        int PgSqlContext::exec(Sql &&sql, OnAsync onAsync) {
            PgsqlAsync *async;
            detail::HttpRequest *request;

            request = (detail::HttpRequest *) &sql.req();
            async = new PgsqlAsync(std::move(sql), db_, onAsync, request->handler());
            if (async == NULL) {
                $error("allocating memory for pgsql async state failed.");
                return $ABORT;
            }
            request->raw()->hdlr_extra = async;
            request->raw()->free_hdlr_extra = pgsql_async_clean_up;
            request->async(pgsql_async_states, PGSQL_ASYNC_STATES_SIZE,
            [](void *data){
                PgsqlAsync *pgasync = (PgsqlAsync *) data;
                delete pgasync;
            });
            return $ASYNC;
        }
    }
}

int
pgsql_async_clean_up(struct http_request *req) {

    int ret = (KORE_RESULT_ERROR);

    kore_debug("force cleanup pgsql query %p", req);
    if (req->hdlr_extra) {
        sparc::db::PgsqlAsync *async = (sparc::db::PgsqlAsync *) req->hdlr_extra;
        // reset hdlr_extra to null as it wont be needed anymore
        req->hdlr_extra = NULL;
        req->free_hdlr_extra = NULL;
        // cleanup the sql
        kore_pgsql_cleanup(&async->pgsql);
        delete async;
        ret = (KORE_RESULT_OK);
    }
    sparc::detail::App::app()->asyncError(req);
    return ret;
}

int
pgsql_async_init(struct http_request *req) {

    kore_debug("pgsql_async_init: %p->%p", req, req->hdlr_extra);
    // we expect handler to have been set
    if (req->hdlr_extra == NULL) {
        $error("initializing async pgsql query failed, unexpected call");
        http_response(req, 500, NULL, 0);
        return (HTTP_STATE_COMPLETE);
    } else {
        sparc::db::PgsqlAsync *async = (sparc::db::PgsqlAsync *) req->hdlr_extra;
        if (!kore_pgsql_query_init(&async->pgsql, req, async->db, KORE_PGSQL_ASYNC)) {
            // if we are still initializing, try again later
            if (async->pgsql.state == KORE_PGSQL_STATE_INIT) {
                req->fsm_state = DB_ASYNC_STATE_INIT;
                return HTTP_STATE_RETRY;
            }

            kore_pgsql_logerror(&async->pgsql);
            req->fsm_state = DB_ASYNC_STATE_ERROR;
        } else {
            req->fsm_state = DB_ASYNC_STATE_QUERY;
        }
        return (HTTP_STATE_CONTINUE);
    }
}

int
pgsql_async_query(struct http_request *req) {

    sparc::db::PgsqlAsync *async = (sparc::db::PgsqlAsync *) req->hdlr_extra;

    // transition to DB wait after issuing the queury
    req->fsm_state = DB_ASYNC_STATE_WAIT;

    if (!kore_pgsql_query(&async->pgsql, async->sql.sql())) {

        return (HTTP_STATE_CONTINUE);
    }

    return (HTTP_STATE_RETRY);
}

int
pgsql_async_wait(struct http_request *req) {

    sparc::db::PgsqlAsync *async = (sparc::db::PgsqlAsync *) req->hdlr_extra;

    kore_debug("pgsql_async_wait, state=%d", async->pgsql.state);

    switch (async->pgsql.state) {
        case KORE_PGSQL_STATE_WAIT:
            return (HTTP_STATE_RETRY);
        case KORE_PGSQL_STATE_COMPLETE:
            req->fsm_state = DB_ASYNC_STATE_DONE;
            break;
        case KORE_PGSQL_STATE_ERROR:
            req->fsm_state = DB_ASYNC_STATE_ERROR;
            async->status = HTTP_STATUS_INTERNAL_ERROR;
            kore_debug("pgsql async error now... %p", async->pgsql.error);
            kore_pgsql_logerror(&async->pgsql);
            kore_debug("pgsql async error now...");
            break;
        case KORE_PGSQL_STATE_RESULT:
            req->fsm_state = DB_ASYNC_STATE_READ;
            break;

        default:
            kore_pgsql_continue(req, &async->pgsql);
            break;
    }

    return (HTTP_STATE_CONTINUE);
}

int
pgsql_async_read(struct http_request *req) {

    sparc::db::PgsqlAsync *async = (sparc::db::PgsqlAsync *) req->hdlr_extra;

    // create a result making sure not to give away pgsql
    sparc::db::PgSqlResult result(&async->pgsql, false);

    if (async->handler) {
        kore_debug("invoking sql query handler: %p", async->handler);
        try {
            async->sql.async((sparc::detail::HttpRequest *) req->data[0],
                             (sparc::detail::HttpResponse *)req->data[1]);
            async->status = async->handler(async->sql, result);
        } catch(std::exception& ex) {
            $error("Unhandled Async Pgsql handler exception: %s", ex.what());
            async->status = HTTP_STATUS_INTERNAL_ERROR;
        }
    }

    if (async->status != HTTP_STATUS_OK) {
        req->fsm_state = DB_ASYNC_STATE_DONE;
    } else {
        kore_pgsql_continue(req, &async->pgsql);
        req->fsm_state = DB_ASYNC_STATE_WAIT;
    }
    return (HTTP_STATE_CONTINUE);
}

int
pgsql_async_error(struct http_request *req) {

    sparc::db::PgsqlAsync *async = (sparc::db::PgsqlAsync *) req->hdlr_extra;
    // reset hdlr_extra to null as it wont be needed anymore
    req->hdlr_extra = NULL;
    req->free_hdlr_extra = NULL;
    kore_pgsql_cleanup(&async->pgsql);
    // complete the async request
    sparc::detail::App::app()->completeRequest(
               (sparc::Request *)req->data[0],
               (sparc::Response *)req->data[1],
               async->rh,
               async->status);
    // async was allocated using new, use delete so to invoke destructor
    delete async;

    return (HTTP_STATE_COMPLETE);
}

int
pgsql_async_done(struct http_request *req) {

    sparc::db::PgsqlAsync *async = (sparc::db::PgsqlAsync *) req->hdlr_extra;
    // reset hdlr_extra to null as it wont be needed anymore
    req->hdlr_extra = NULL;
    req->free_hdlr_extra = NULL;
    kore_pgsql_cleanup(&async->pgsql);
    // complete the async request
    sparc::detail::App::app()->completeRequest(
            (sparc::Request *)req->data[0],
            (sparc::Response *)req->data[1],
            async->rh,
            async->status);
    // async was allocated using new, use delete so to invoke destructor
    delete async;

    return (HTTP_STATE_COMPLETE);
}
