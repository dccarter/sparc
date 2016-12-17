//
// Created by dc on 12/1/16.
//

#ifndef SPARC_WEBSOCKET_H
#define SPARC_WEBSOCKET_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

void on_websocket_connect(struct connection*);
void on_websocket_disconnect(struct connection*);
void on_websocket_message(struct connection*, u_int8_t, void*, size_t);

struct connection;

#ifdef __cplusplus
};
#endif

namespace sparc {
    namespace detail {
        struct WebSocketHandler {
            WsOnConnect         onConnect;
            WsOnDisconnect      onDisconnect;
            WsOnMessage         onMessage;
        };

        struct WebSocketImpl : public WebSocket {
        public:
            WebSocketImpl(struct connection*, WebSocketHandler*);
            virtual void send(void*, size_t, u_int8_t op=WS_OP_TEXT) override;
            virtual void send(cc_string, u_int8_t op=WS_OP_TEXT) override;
            virtual void broadcast(void*, size_t, u_int8_t op=WS_OP_TEXT) override;
            virtual void broadcast(cc_string, u_int8_t op=WS_OP_TEXT) override;
            virtual void globalBroadcast(void*, size_t, u_int8_t op=WS_OP_TEXT) override;
            virtual void globalBroadcast(cc_string, u_int8_t op=WS_OP_TEXT) override;
            virtual void close() override;
            WebSocketHandler        *handler;
            struct  connection      *conn;
            bool                    markClose;

            OVERLOAD_MEMOPERATORS();
            virtual ~WebSocketImpl() { close(); }
        };
    }
};
#endif //SPARC_WEBSOCKET_H
