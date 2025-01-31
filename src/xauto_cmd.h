#pragma once
#include <msgpack.hpp>
#include <cstdint>


constexpr const char* XAUTO_COMPAT_VERSION = "bitter_oyster"; // TODO: externalize


class XAutoErrorResponse {
public:
    std::string success;
    std::string error_string;
    MSGPACK_DEFINE(success, error_string);
};

void get_debugger_pid(msgpack::sbuffer& response_buffer);
void get_compat_v(msgpack::sbuffer& response_buffer);
void get_debugger_version(msgpack::sbuffer& response_buffer);
void dbg_eval(msgpack::object root, msgpack::sbuffer& response_buffer);
void dbg_cmd_exec_direct(msgpack::object root, msgpack::sbuffer& response_buffer);
void dbg_is_running(msgpack::sbuffer& response_buffer);
void dbg_is_debugging(msgpack::sbuffer& response_buffer);
void dbg_is_elevated(msgpack::sbuffer& response_buffer);
void dbg_memmap(msgpack::sbuffer& response_buffer);
void dbg_get_bitness(msgpack::sbuffer& response_buffer);
void gui_refresh_views(msgpack::sbuffer& response_buffer);

typedef std::tuple<size_t, size_t, uint32_t, uint16_t, size_t, uint32_t, uint32_t, uint32_t, std::string> MemPageTup;
void dbg_memmap(msgpack::sbuffer& response_buffer);
void dbg_read_memory(msgpack::object root, msgpack::sbuffer& response_buffer);
void dbg_write_memory(msgpack::object root, msgpack::sbuffer& response_buffer);
void dbg_read_regs(msgpack::sbuffer& response_buffer);