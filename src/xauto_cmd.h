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


typedef std::tuple<uint16_t, uint16_t, uint16_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t> x87fpuTup;
typedef std::tuple<
    size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, // gp regs
    size_t, size_t, // ip, flags
    uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, // segs
    size_t, size_t, size_t, size_t, size_t, size_t, // dregs
    std::array<uint8_t, 80>,
    x87fpuTup,
    uint32_t,
    std::array<uint8_t, 16 * 16>,
    std::array<uint8_t, 16 * 32>
> CtxTup64;
typedef std::tuple<
    size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, // gp regs
    size_t, size_t, // ip, flags
    uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, // segs
    size_t, size_t, size_t, size_t, size_t, size_t, // dregs
    std::array<uint8_t, 80>,
    x87fpuTup,
    uint32_t,
    std::array<uint8_t, 8 * 16>,
    std::array<uint8_t, 8 * 32>
> CtxTup32;
typedef std::tuple<bool, bool, bool, bool, bool, bool, bool, bool, bool> FlagsTup;
typedef std::tuple<std::array<uint8_t, 10>, size_t, size_t> FpuRegsTup;
typedef std::array<FpuRegsTup, 8> FpuRegsArr;
typedef std::array<uint64_t, 8> MmxArr;
typedef std::tuple<bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, uint16_t> MxcsrFieldsTup;
typedef std::tuple<bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, uint16_t> x87StatusWordFieldsTup;
typedef std::tuple<bool, bool, bool, bool, bool, bool, bool, bool, uint16_t, uint16_t> x87ControlWordFieldsTup;
void dbg_read_regs(msgpack::sbuffer& response_buffer);
void dbg_read_setting_sz(msgpack::object root, msgpack::sbuffer& response_buffer);
void dbg_write_setting_sz(msgpack::object root, msgpack::sbuffer& response_buffer);
void dbg_read_setting_uint(msgpack::object root, msgpack::sbuffer& response_buffer);
void dbg_write_setting_uint(msgpack::object root, msgpack::sbuffer& response_buffer);