#pragma once
#include <cstdint>
#include <mutex>
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
constexpr const char* XAUTO_REQ_GET_LOG = "XAUTO_REQ_GET_LOG";

// Local extension: native analysis lookups
constexpr const char* XAUTO_REQ_XREF_COUNT_AT = "XAUTO_REQ_XREF_COUNT_AT";
constexpr const char* XAUTO_REQ_XREF_TYPE_AT = "XAUTO_REQ_XREF_TYPE_AT";
constexpr const char* XAUTO_REQ_XREF_GET = "XAUTO_REQ_XREF_GET";
constexpr const char* XAUTO_REQ_XREF_ADD = "XAUTO_REQ_XREF_ADD";
constexpr const char* XAUTO_REQ_FUNCTION_TYPE_AT = "XAUTO_REQ_FUNCTION_TYPE_AT";
constexpr const char* XAUTO_REQ_MODULE_PATH = "XAUTO_REQ_MODULE_PATH";
constexpr const char* XAUTO_REQ_ANALYSE_RANGE = "XAUTO_REQ_ANALYSE_RANGE";
constexpr const char* XAUTO_REQ_SCRIPT_LOAD = "XAUTO_REQ_SCRIPT_LOAD";
constexpr const char* XAUTO_REQ_SCRIPT_RUN = "XAUTO_REQ_SCRIPT_RUN";
constexpr const char* XAUTO_REQ_SCRIPT_CMDEXEC = "XAUTO_REQ_SCRIPT_CMDEXEC";
constexpr const char* XAUTO_REQ_DBG_CMD_EXEC = "XAUTO_REQ_DBG_CMD_EXEC";

class XAutoServer {
    public:
    zmq::socket_t pub_socket;
    zmq::socket_t rep_socket;
    uint16_t sess_req_rep_port = 0;
    uint16_t sess_pub_sub_port = 0;
    std::string bind_address = "localhost";

    XAutoServer();
    void release_session();

    void pub_send(msgpack::sbuffer& buf) {
        std::lock_guard<std::mutex> lock(pub_mutex);
        pub_socket.send(zmq::buffer(buf.data(), buf.size()), zmq::send_flags::none);
    }

    private:
    std::mutex pub_mutex;
    size_t session_pid = 0;
    zmq::context_t context;

    void xauto_srv_req_rep_thread();
    bool acquire_session();
    int _dispatch_cmd(msgpack::object root, msgpack::sbuffer& response_buffer);
};
