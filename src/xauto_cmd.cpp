#include "xauto_cmd.h"
#include <pluginsdk/bridgemain.h>
#include <pluginsdk/_plugins.h>


size_t get_debugger_pid(msgpack::sbuffer& response_buffer) {
    XAutoPidResponse resp_obj = {GetCurrentProcessId()};
    msgpack::pack(response_buffer, resp_obj);
    return resp_obj.pid;
}

std::string get_compat_v(msgpack::sbuffer& response_buffer) {
    XAutoCompatVersionResponse resp_obj = {XAUTO_COMPAT_VERSION};
    msgpack::pack(response_buffer, resp_obj);
    return resp_obj.version;
}

void dbg_eval(msgpack::object root, msgpack::sbuffer& response_buffer) {
    bool success; 
    std::string cmd;

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::STR) {
        XAutoErrorResponse resp_obj = {"XERROR_BAD_EVAL", "Invalid or missing eval string"};
        msgpack::pack(response_buffer, resp_obj);
        return;
    }

    root.via.array.ptr[1].convert(cmd);
    size_t res = DbgEval(cmd.c_str(), &success);
    XAutoDbgEvalResponse resp_obj = {res, success};
    msgpack::pack(response_buffer, resp_obj);
}

void dbg_cmd_exec_direct(msgpack::object root, msgpack::sbuffer& response_buffer) {
    std::string cmd;

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::STR) {
        XAutoErrorResponse resp_obj = {"XERROR_BAD_EVAL", "Invalid or missing command string"};
        msgpack::pack(response_buffer, resp_obj);
        return;
    }

    root.via.array.ptr[1].convert(cmd);
    XAutoDbgCmdExecDirectResponse resp_obj = {DbgCmdExecDirect(cmd.c_str())};
    msgpack::pack(response_buffer, resp_obj);
}

void dbg_is_running(msgpack::sbuffer& response_buffer) {
    XAutoDbgIsRunningResponse resp_obj = {DbgIsRunning()};
    msgpack::pack(response_buffer, resp_obj);
}

void dbg_is_debugging(msgpack::sbuffer& response_buffer) {
    XAutoDbgIsDebuggingResponse resp_obj = {DbgIsDebugging()};
    msgpack::pack(response_buffer, resp_obj);
}
