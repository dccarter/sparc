//
// Created by dc on 11/27/16.
//

#ifndef SPARC_JSON_H
#define SPARC_JSON_H

#include "ccan_json.h"
#include "common.h"

namespace sparc {
    namespace detail {

        struct CcanJson : public Json {
        public:
            CcanJson(JsonNode *);

            virtual ~CcanJson();

            virtual c_string encode(size_t &, char *space) override;

            JsonNode *root_;

            OVERLOAD_MEMOPERATORS();
        };
    }
}
#endif //SPARC_JSON_H
