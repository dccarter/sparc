//
// Created by dc on 1/14/17.
//

#include "sparc.h"
#include "db.h"
#include "pgsql.h"
#include "kore.h"

#define SPARC_DEFAULT_NUM_DATABASES (5)

namespace sparc {
    namespace db {

        DbManager::DbManager(size_t ndatabases)
            : ndatabses_(ndatabases<=0? SPARC_DEFAULT_NUM_DATABASES: ndatabases)
        {
            databases_ = (Context **) kore_calloc(sizeof(Context*), ndatabses_);
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
                    eid = i+1;
                }
                if (databases_[i] == db) {
                    return i+1;
                }
            }
            if (eid) {
                databases_[eid-1] = db;
            }
            return eid;
        }

        Context* DbManager::get(size_t index) {
            size_t eid = index-1;
            return (eid < ndatabses_)? databases_[eid]: nullptr;
        }

        void DbManager::remove(size_t index) {
            size_t eid = index-1;
            if (eid < ndatabses_) {
                databases_[eid] = nullptr;
            }
        }

        Sql::Sql()
            : req_(nullptr),
              res_(nullptr),
              sql_(32),
              result_(nullptr)
        {}
        Sql::Sql(const Request &req, Response &res)
            : req_(&req),
              res_(&res),
              sql_(32),
              result_(nullptr)
        {}

        Sql::~Sql() {
            if (result_) {
                delete  result_;
                result_ = nullptr;
            }
        }

        Sql& Sql::operator()(cc_string fmt, ...) {
            va_list        args;
            sql_.append(" ", 1);
            va_start(args, fmt);
            sql_.appendv(fmt, args);
            va_end(args);
            return *this;
        }

        void Sql::setResult(result_t *result) {
            if (result_) delete  result_;
            result_ = result;
        }

        void Sql::debug() {
            kore_debug(sql());
        }

        Row& result_t::row_iterator_t::operator*() const {
            return *(results_->at(index_));
        }

        PgSqlRow::PgSqlRow(PgSqlResult *result, size_t idx)
            : Row(result, idx),
              sql_(result->sql_)
        {}

        cc_string PgSqlRow::value(size_t idx) const {
            return kore_pgsql_getvalue(sql_, index_, idx);
        }

        cc_string PgSqlRow::value(cc_string fname) const {
            return kore_pgsql_getvalue_by_name(sql_, index_, fname);
        }

        PgSqlResult::PgSqlResult(struct kore_pgsql *sql)
            : sql_(sql),
              rows_(nullptr),
              nrows_(0),
              json_(nullptr)
        {
            do {
                if (sql_ != NULL) {
                    size_t i;
                    nrows_ = (size_t) kore_pgsql_ntuples(sql_);
                    rows_ = (PgSqlRow *) memory::calloc(sizeof(PgSqlRow), nrows_+1);
                    if (rows_ == NULL) break;

                    for (i=0; i <= nrows_; i++) {
                        PgSqlRow row(this, i);
                        memcpy(&rows_[i], &row, sizeof(row));
                    }
                }
            } while (0);
        }

        PgSqlResult::PgSqlResult(PgSqlResult &&result)
            : sql_(result.sql_),
              nrows_(result.nrows_),
              rows_(result.rows_),
              json_(result.json_)
        {
            $debug("MOVED...");
            result.json_ = nullptr;
            result.sql_ = nullptr;
            result.nrows_ = 0;
            result.rows_ = nullptr;
        }

        Json* PgSqlResult::toJson() {
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

                if (sql_) {
                    kore_pgsql_cleanup(sql_);
                    kore_free(sql_);
                }
                sql_ = NULL;
            }
        }

        PgSqlContext::PgSqlContext(cc_string name, cc_string connString)
            : name_(kore_strdup(name)),
              connString_(kore_strdup(connString))
        {
        }

        PgSqlContext::~PgSqlContext() {
            if (name_) {
                kore_free(name_);
                name_ = NULL;
            }
        }

        int PgSqlContext::init() {
            if (name_ && connString_) {
                return kore_pgsql_register(name_, connString_);
            }
            return 0;
        }

        Result PgSqlContext::exec(Sql &sql) {
            struct kore_pgsql   *pgsql =
                    (struct kore_pgsql*) kore_malloc(sizeof(struct kore_pgsql));
            if (!kore_pgsql_query_init(pgsql, NULL, name_, KORE_PGSQL_SYNC)) {
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

        int PgSqlContext::exec(Sql&& sql, OnAsync onAsync) {
            return $ABORT;
        }
    }
}
