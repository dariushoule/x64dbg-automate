#pragma once
#include <msgpack.hpp>


constexpr const char* XAUTO_COMPAT_VERSION = "bitter_oyster"; // TODO: externalize


class XAutoErrorResponse {
public:
    std::string success;
    std::string error_string;
    MSGPACK_DEFINE(success, error_string);
};

class XAutoPidResponse {
public:
    size_t pid;
    MSGPACK_DEFINE(pid);
};
size_t get_debugger_pid(msgpack::sbuffer& response_buffer);

class XAutoCompatVersionResponse {
public:
    std::string version;
    MSGPACK_DEFINE(version);
};
std::string get_compat_v(msgpack::sbuffer& response_buffer);

class XAutoDbgEvalResponse {
public:
    size_t response;
    bool success;
    MSGPACK_DEFINE(response, success);
};
void dbg_eval(msgpack::object root, msgpack::sbuffer& response_buffer);

class XAutoDbgCmdExecDirectResponse {
public:
    bool success;
    MSGPACK_DEFINE(success);
};
void dbg_cmd_exec_direct(msgpack::object root, msgpack::sbuffer& response_buffer);

class XAutoDbgIsRunningResponse {
public:
    bool is_running;
    MSGPACK_DEFINE(is_running);
};
void dbg_is_running(msgpack::sbuffer& response_buffer);

class XAutoDbgIsDebuggingResponse {
public:
    bool is_debugging;
    MSGPACK_DEFINE(is_debugging);
};
void dbg_is_debugging(msgpack::sbuffer& response_buffer);