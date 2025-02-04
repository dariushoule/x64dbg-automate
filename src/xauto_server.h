#pragma once
#include <cstdint>
#include <zmq.hpp>
#include <msgpack.hpp>


constexpr const char* XAUTO_REQ_DEBUGGER_PID = "XAUTO_REQ_DEBUGGER_PID";
constexpr const char* XAUTO_REQ_COMPAT_VERSION = "XAUTO_REQ_COMPAT_VERSION";
constexpr const char* XAUTO_REQ_DEBUGGER_VERSION = "XAUTO_REQ_DEBUGGER_VERSION";
constexpr const char* XAUTO_REQ_QUIT = "XAUTO_REQ_QUIT";
constexpr const char* XAUTO_REQ_DBG_EVAL = "XAUTO_REQ_DBG_EVAL";
constexpr const char* XAUTO_REQ_DBG_CMD_EXEC_DIRECT = "XAUTO_REQ_DBG_CMD_EXEC_DIRECT";
constexpr const char* XAUTO_REQ_DBG_IS_RUNNING = "XAUTO_REQ_DBG_IS_RUNNING";
constexpr const char* XAUTO_REQ_DBG_IS_DEBUGGING = "XAUTO_REQ_DBG_IS_DEBUGGING";
constexpr const char* XAUTO_REQ_DBG_IS_ELEVATED = "XAUTO_REQ_DBG_IS_ELEVATED";
constexpr const char* XAUTO_REQ_DBG_MEMMAP = "XAUTO_REQ_DBG_MEMMAP";
constexpr const char* XAUTO_REQ_DBG_GET_BITNESS = "XAUTO_REQ_DBG_GET_BITNESS";
constexpr const char* XAUTO_REQ_GUI_REFRESH_VIEWS = "XAUTO_REQ_GUI_REFRESH_VIEWS";
constexpr const char* XAUTO_REQ_DBG_READ_MEMORY = "XAUTO_REQ_DBG_READ_MEMORY";
constexpr const char* XAUTO_REQ_DBG_WRITE_MEMORY = "XAUTO_REQ_DBG_WRITE_MEMORY";
constexpr const char* XAUTO_REQ_DBG_READ_REGISTERS = "XAUTO_REQ_DBG_READ_REGISTERS";
constexpr const char* XAUTO_REQ_DBG_READ_SETTING_SZ = "XAUTO_REQ_DBG_READ_SETTING_SZ";
constexpr const char* XAUTO_REQ_DBG_WRITE_SETTING_SZ = "XAUTO_REQ_DBG_WRITE_SETTING_SZ";
constexpr const char* XAUTO_REQ_DBG_READ_SETTING_UINT = "XAUTO_REQ_DBG_READ_SETTING_UINT";
constexpr const char* XAUTO_REQ_DBG_WRITE_SETTING_UINT = "XAUTO_REQ_DBG_WRITE_SETTING_UINT";
constexpr const char* XAUTO_REQ_DBG_IS_VALID_READ_PTR = "XAUTO_REQ_DBG_IS_VALID_READ_PTR";
constexpr const char* XAUTO_REQ_DISASSEMBLE = "XAUTO_REQ_DISASSEMBLE";
constexpr const char* XAUTO_REQ_ASSEMBLE = "XAUTO_REQ_ASSEMBLE";
constexpr const char* XAUTO_REQ_GET_BREAKPOINTS = "XAUTO_REQ_GET_BREAKPOINTS";
constexpr const char* XAUTO_REQ_GET_LABEL = "XAUTO_REQ_GET_LABEL";
constexpr const char* XAUTO_REQ_GET_COMMENT = "XAUTO_REQ_GET_COMMENT";
constexpr const char* XAUTO_REQ_GET_SYMBOL = "XAUTO_REQ_GET_SYMBOL";


#define SESS_REQ_REP_PORT 41600 + xauto_session_id
#define SESS_PUB_SUB_PORT 51600 + xauto_session_id


class XAutoServer {
    public:
    XAutoServer();
    zmq::socket_t pub_sock;
    HANDLE hMutex;

    private:
    uint16_t xauto_session_id = 0;
    zmq::context_t context;

    void xauto_srv_req_rep_thread();
    void acquire_session();
    int _dispatch_cmd(msgpack::object root, msgpack::sbuffer& response_buffer);
};
