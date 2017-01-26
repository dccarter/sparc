//
// Created by dc on 1/14/17.
//

#ifndef SPARC_DB_PGSQL_H
#define SPARC_DB_PGSQL_H

#include "sparc.h"

#ifdef __cplusplus
extern  "C" {
#endif
struct kore_pgsql;
#ifdef __cplusplus
};
#endif

namespace sparc {

    namespace db {

        class DbManager {
        public:
            DbManager(size_t ndatabases);
            size_t add(Context *db);
            void remove(size_t index);
            Context* get(size_t index);
            bool init();
        private:
            Context      **databases_;
            size_t       ndatabses_;
        };

        class PgSqlResult;

        class PgSqlRow: public virtual Row {
        public:
            PgSqlRow(PgSqlResult *result, size_t idx);
            virtual cc_string value(size_t idx) const;
            virtual cc_string value(cc_string idx) const;
        private:
            struct kore_pgsql       *sql_;
        };

        class PgSqlResult: public virtual result_t {
        public:
            PgSqlResult(PgSqlResult&& result);
            PgSqlResult(struct kore_pgsql *sql);

            virtual Row* at(size_t index) const {
                if (index < nrows_) return &rows_[index];
                return nullptr;
            }

            virtual size_t count() const {
                return nrows_;
            }
            virtual bool isSuccess() const {
                return rows_ != nullptr;
            }

            virtual Json* toJson();

            virtual ~PgSqlResult();

            //PgSqlResult&&operator=()
        private:
            friend class PgSqlRow;
            struct  kore_pgsql      *sql_;
            size_t                  nrows_;
            PgSqlRow                *rows_;
            Json                    *json_;
        };
    }
}
#endif //SPARC_DB_PGSQL_H
