//
// Created by dc on 11/22/16.
//

#ifndef SPARC_ROUTER_H
#define SPARC_ROUTER_H

#include <sys/queue.h>
#include "common.h"

namespace sparc {
    namespace detail {

        struct RouteHandler {
            TAILQ_ENTRY(RouteHandler)    link;
            c_string      route;
            c_string      contentType;
            ResponseTransformer *transformer;
            handler             h;
            Method              m;
            c_string        *params;
            cc_string       prefix;
            u_int8_t        nparams;
            void            *data[1];
            ~RouteHandler();
            OVERLOAD_MEMOPERATORS()
        };

        struct Route {
            Route();
            cc_string     prefix;
            u_int32_t           methods;
            TAILQ_HEAD(,RouteHandler)   handlers;
            RouteHandler*   find(Method, const int);
            RouteHandler*   add(Method, c_string*, const int);
            ~Route();
            OVERLOAD_MEMOPERATORS()
        };

        struct Router {
            struct Node {
                TAILQ_ENTRY(Node)   link;
                char                tag;
                Route               *route;
                TAILQ_HEAD(,Node)   children;
                Node*    get(const char c);
                Node*    add(const char c);
                OVERLOAD_MEMOPERATORS()
                ~Node();
            };

            Router();
            ~Router();

            Route* add(Method, cc_string, handler, cc_string, ResponseTransformer*, void *data = NULL);
            RouteHandler* remove(Method, cc_string);
            RouteHandler* find(Method, cc_string, c_string*, c_string*, size_t*, c_string**);

            OVERLOAD_MEMOPERATORS()

            Node* add(Node*, cc_string, size_t);
            Node                    *root_;
            int                     depth;
        };
    }
}
#endif //SPARC_ROUTER_H
