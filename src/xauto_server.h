#pragma once
#include <cstdint>
#include <zmq.hpp>
#include <msgpack.hpp>


constexpr const char* XAUTO_REQ_DEBUGGER_PID = "XAUTO_REQ_DEBUGGER_PID";
constexpr const char* XAUTO_REQ_COMPAT_VERSION = "XAUTO_REQ_COMPAT_VERSION";
constexpr const char* XAUTO_REQ_DBG_EVENT = "XAUTO_REQ_DBG_EVENT";
constexpr const char* XAUTO_REQ_QUIT = "XAUTO_REQ_QUIT";
constexpr const char* XAUTO_REQ_DBG_EVAL = "XAUTO_REQ_DBG_EVAL";
constexpr const char* XAUTO_REQ_DBG_CMD_EXEC_DIRECT = "XAUTO_REQ_DBG_CMD_EXEC_DIRECT";
constexpr const char* XAUTO_REQ_DBG_IS_RUNNING = "XAUTO_REQ_DBG_IS_RUNNING";
constexpr const char* XAUTO_REQ_DBG_IS_DEBUGGING = "XAUTO_REQ_DBG_IS_DEBUGGING";


#define SESS_PORT 41600 + xauto_session_id


namespace XAuto {
    class XAutoServer {
        private:
        uint16_t xauto_session_id = 0;

        void xauto_srv_thread();
        void acquire_session();
        int _dispatch_cmd(msgpack::object root, msgpack::sbuffer& response_buffer);

        public:
        XAutoServer();
    };
}