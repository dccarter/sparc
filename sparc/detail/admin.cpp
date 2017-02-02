//
// Created by dc on 1/29/17.
//

#include "app.h"

namespace sparc {
    namespace admin {
        Json *routes() {
            return detail::App::app()->router().toJson();
        }
    }
}
