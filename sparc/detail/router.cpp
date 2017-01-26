//
// Created by dc on 11/22/16.
//

#include "kore.h"
#include "sparc.h"
#include "router.h"

namespace sparc {
    namespace detail {
        RouteHandler::~RouteHandler() {
            if (route) {
                kore_free(route);
                route = NULL;
            }

            if (params) {
                kore_free(params);
                params = NULL;
            }

            if (contentType) {
                kore_free(contentType);
                contentType = NULL;
            }

            if (transformer) {
                delete transformer;
                transformer = NULL;
            }
            if (data[0]) {
                kore_free(data[0]);
                data[0] = NULL;
            }

            nparams = 0;
            h = nullptr;
        }

        Router::Node* Router::Node::get(const char c) {
            Node *n = NULL, *ret = NULL;
            TAILQ_FOREACH(n, &children, link) {
                if (n->tag == c) {
                    ret = n;
                    break;
                }
            }
            return n;
        }

        Router::Node* Router::Node::add(const char c) {
            Node *n = NULL;
            n = new Node;
            n->tag      = c;
            n->route    = NULL;
            TAILQ_INIT(&n->children);
            TAILQ_INSERT_TAIL(&children, n, link);
            return n;
        }

        Router::Node::~Node() {
            Node *n, *tmp;
            for (n = TAILQ_FIRST(&children); n != NULL; n = tmp) {
                tmp = TAILQ_NEXT(n, link);
                TAILQ_REMOVE(&children, n, link);
                // delete the node
                delete n;
            }

            if (route != NULL) {
                delete route;
                route = NULL;
            }
        }

        Route::Route()
            : methods(0),
              prefix(NULL)
        {
            TAILQ_INIT(&handlers);
        }

        /**
         * The notion is that the route owns the memory
         * on its members
         */
        Route::~Route() {
            RouteHandler *h, *tmp;
            for (h = TAILQ_FIRST(&handlers); h != NULL; h = tmp) {
                tmp = TAILQ_NEXT(h, link);
                TAILQ_REMOVE(&handlers, h, link);
                delete h;
            }
        }

        RouteHandler* Route::find(Method m, const int ntoks) {
            // fast check
            if (methods & (1<<m)) {
                RouteHandler *rh;
                TAILQ_FOREACH(rh, &handlers, link) {
                    if (rh->m == m && ntoks == rh->nparams) {
                        return rh;
                    }
                }
            }
            return NULL;
        }

        RouteHandler* Route::add(Method m, c_string *toks, const int ntoks) {
            RouteHandler *rh;
            rh = new RouteHandler;
            rh->nparams = (u_int8_t) ntoks;
            rh->params  = toks;
            rh->m       = m;
            rh->prefix  = prefix;
            rh->h       = NULL;
            methods |= (1 << m);
            TAILQ_INSERT_TAIL(&handlers, rh, link);
            return rh;
        }

        Router::Node* Router::add(Node *node, cc_string prefix, size_t len) {
            char         c;
            unsigned     i = 0;
            Node         *n;

            if (node->tag != prefix[i]) {
                return NULL;
            }

            i++;
            while (i < len) {
                c = prefix[i];
                n = node->get(c);
                if (n == NULL) {
                    depth++;
                    n = node->add(c);
                }
                node = n;
                i++;
            }

            return node;
        }

        Router::Router()
            : depth(1)
        {
            root_ = new Node;
            root_->tag = '/';
            root_->route = NULL;
            TAILQ_INIT(&root_->children);
        }

        Router::~Router() {
            if (root_)
                // this should initiate deleting the entire trie
                delete root_;

            root_ = NULL;
        }

        Route* Router::add(Method m,
                           cc_string route,
                           handler hh,
                           cc_string contentType,
                           ResponseTransformer* t,
                           void * data
        )
        {
            char         *tokens[8], *tok;
            char         *rpath;
            int          ntoks, i = 0;

            rpath = kore_strdup(route);
            ntoks = kore_split_string(rpath, ":", tokens, sizeof(tokens));

            if (ntoks) {
                char  **rtoks = NULL, *prefix;
                Route *r;
                Node* n;
                RouteHandler *h;

                prefix = tokens[0];
                n = add(root_, prefix, strlen(prefix));
                if (n == NULL) goto free_rpath;
                if (n->route == NULL) {
                    r = new Route;
                    r->prefix   = prefix;
                    n->route = r;
                } else {
                    r = n->route;
                }

                // if route handler is already in place, reject
                if (r->find(m, ntoks-1)) goto free_rpath;

                if (ntoks > 1) {
                    rtoks = (char **) kore_calloc(ntoks, sizeof(char **));
                    // add route, compile parameters
                    for (i = 1; i < ntoks; i++) {
                        tok = strchr(tokens[i], '/');
                        if (tok) *tok = '\0';
                        rtoks[i - 1] = tokens[i];
                    }
                    i--;
                    rtoks[i] = NULL;
                }

                h = r->add(m, rtoks, i);
                h->route        = rpath;
                h->contentType  = contentType? kore_strdup(contentType) : NULL;
                h->h            = hh? std::move(hh) : nullptr;
                h->transformer  = t? std::move(t) : nullptr;
                h->data[0]      = data;
                return r;
            }

        free_rpath:
            kore_free(rpath);
            return NULL;
        }

        RouteHandler* Router::find(Method m,
                                   cc_string route,
                                   c_string *tokenized,
                                   c_string* tokens,
                                   size_t *ntoks,
                                   c_string **params) {
            char            c;
            unsigned int    i, itok;
            size_t          rlen, tlen;
            Node            *node = root_, *n;

            *tokenized = kore_strdup(route);
            *ntoks = (size_t) kore_split_string(*tokenized, "/", tokens, *ntoks);

            i = 0, itok = 0, rlen = strlen(route);
            tlen = *ntoks? strlen(tokens[0]) : 0;
            kore_debug("%s: %d", route, *ntoks);
            while (i < rlen) {
                c = route[i];
                node = i == 0 ? root_ : node->get(c);
                if (node == NULL)
                    goto exit_free_tokenized;
                if (c == '/' || tlen == 0) {
                    RouteHandler *rh;
                    if (i && c == '/') {
                        itok++;
                        if (itok >= *ntoks) {
                            break;
                        }
                        tlen = strlen(tokens[itok]);
                    } else if (i == rlen-1)
                        itok++;

                    if (node->route) {
                        int rtoks = (*ntoks==0)? 0: (int)(*ntoks-itok);
                        rh = node->route->find(m, rtoks);
                        // we found our handler
                        if (rh) {
                            // prepare parameters
                            *ntoks = (size_t) rtoks;
                            if (*ntoks)
                                *params = &tokens[itok];
                            return rh;
                        }
                    }
                }

                i++;
                tlen -= tlen ? 1 : 0;
            }

        exit_free_tokenized:
            if (*tokenized) kore_free(*tokenized);
            *tokenized = NULL;
            return NULL;
        }

        RouteHandler* Router::remove(Method m, cc_string c) {
            // TODO implement remove
            return NULL;
        }
    }
}
