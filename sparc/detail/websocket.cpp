//
// Created by dc on 12/1/16.
//

#include "websocket.h"
#include "kore.h"

namespace sparc {
    namespace detail{
        WebSocketImpl::WebSocketImpl(struct connection *c, WebSocketHandler *h)
            : conn(c),
              handler(h),
              markClose(false)
        {}

        void WebSocketImpl::send(cc_string str, u_int8_t op) {
            if (conn) {
                kore_websocket_send(conn, op, str, strlen(str));
            }
        }

        void WebSocketImpl::send(void *data, size_t len, u_int8_t op) {
            if (conn) {
                kore_websocket_send(conn, op, data, len);
            }
        }

        void WebSocketImpl::broadcast(cc_string str, u_int8_t op) {
            if (conn) {
                kore_websocket_broadcast(conn, op, str, strlen(str), WS_BROADCAST_LOCAL);
            }
        }
        void WebSocketImpl::broadcast(void *data, size_t len, u_int8_t op) {
            if (conn) {
                kore_websocket_broadcast(conn, op, data, len, WS_BROADCAST_LOCAL);
            }
        }

        void WebSocketImpl::globalBroadcast(cc_string str, u_int8_t op) {
            if (conn) {
                kore_websocket_broadcast(conn, op, str, strlen(str), WS_BROADCAST_GLOBAL);
            }
        }

        void WebSocketImpl::globalBroadcast(void *data, size_t len, u_int8_t op) {
            if (conn) {
                kore_websocket_broadcast(conn, op, data, len, WS_BROADCAST_GLOBAL);
            }if (conn) {
                kore_connection_disconnect(conn);
            }
        }

        void WebSocketImpl::close() {
            if (!markClose) {
                conn = NULL;
                handler = NULL;
                markClose = true;
            }
        }
    }
}

void on_websocket_connect(struct connection *c) {
    if (c->data[0]) {
        sparc::detail::WebSocketHandler *handler = (sparc::detail::WebSocketHandler *) c->data[0];
        sparc::detail::WebSocketImpl *ws = new sparc::detail::WebSocketImpl(c, handler);
        if (handler->onConnect)
            handler->onConnect(ws);
        c->data[0]  = ws;
    } else {
        kore_connection_disconnect(c);
    }
}

void on_websocket_disconnect(struct connection *c) {
    if (c->data[0]) {
        sparc::detail::WebSocketImpl *ws  = (sparc::detail::WebSocketImpl*) c->data[0];
        if (!ws->markClose && ws->handler && ws->handler->onDisconnect)
            ws->handler->onDisconnect(ws);
        delete ws;
    }
}
void on_websocket_message(struct connection *c, u_int8_t op, void *data, size_t len) {
    if (c->data[0]) {
        sparc::detail::WebSocketImpl *ws = (sparc::detail::WebSocketImpl *) c->data[0];
        if (ws->handler && ws->handler->onMessage)
            ws->handler->onMessage(ws, data, len, op);
    }
}