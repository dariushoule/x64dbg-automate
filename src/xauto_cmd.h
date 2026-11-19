#pragma once
#include <msgpack.hpp>
#include <cstdint>


constexpr const char* XAUTO_COMPAT_VERSION = "ghost_fungus_2026_11"; // TODO: externalize


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
    std::array<uint8_t, 64 * 32>
> CtxTup64;
typedef std::tuple<
    size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t, // gp regs
    size_t, size_t, // ip, flags
    uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, // segs
    size_t, size_t, size_t, size_t, size_t, size_t, // dregs
    std::array<uint8_t, 80>,
    x87fpuTup,
    uint32_t,
    std::array<uint8_t, 64 * 8>
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
void dbg_is_valid_read_ptr(msgpack::object root, msgpack::sbuffer& response_buffer);

typedef std::tuple<std::string, size_t, size_t, size_t, size_t, size_t> DisasmArgTup;
typedef std::tuple<std::string, std::string, size_t, size_t, size_t, std::array<DisasmArgTup, 3>> DisasmTup;
void disassemble_at(msgpack::object root, msgpack::sbuffer& response_buffer);
void assemble_at(msgpack::object root, msgpack::sbuffer& response_buffer);

typedef std::tuple<
    size_t, size_t, bool, bool, bool, std::string, std::string, 
    uint16_t, uint8_t, uint8_t, size_t, bool, bool, std::string, 
    std::string, std::string, std::string, std::string> BpxTup;
void get_breakpoints(msgpack::object root, msgpack::sbuffer& response_buffer);
void get_label_at(msgpack::object root, msgpack::sbuffer& response_buffer);
void get_comment_at(msgpack::object root, msgpack::sbuffer& response_buffer);
void get_symbol_at(msgpack::object root, msgpack::sbuffer& response_buffer);
void get_log(msgpack::object root, msgpack::sbuffer& response_buffer);

// Local extension: native analysis lookups (no disassembly scan required).
// These wrap DbgGetXrefCountAt / DbgGetXrefTypeAt / DbgGetFunctionTypeAt and
// DbgFunctions()->ModPathFromAddr/Name.  All take one argument in root[1]:
// an address (integer) or, for module_path, an address or module name (string).
void xref_count_at(msgpack::object root, msgpack::sbuffer& response_buffer);
void xref_type_at(msgpack::object root, msgpack::sbuffer& response_buffer);
void xref_get(msgpack::object root, msgpack::sbuffer& response_buffer);
void xref_add(msgpack::object root, msgpack::sbuffer& response_buffer);
void function_type_at(msgpack::object root, msgpack::sbuffer& response_buffer);
void module_path(msgpack::object root, msgpack::sbuffer& response_buffer);

// Local extension: queue-aware command execution and x64dbg's own analysis.
// x64dbg's analyse/analxrefs commands ignore their arguments and work on the
// GUI disassembly selection, so analyse_range sets/restores the selection and
// waits for a sentinel command (xauto_barrier) to run after them.
void analyse_range(msgpack::object root, msgpack::sbuffer& response_buffer);
void script_load(msgpack::object root, msgpack::sbuffer& response_buffer);
void script_run(msgpack::object root, msgpack::sbuffer& response_buffer);
void script_cmd_exec(msgpack::object root, msgpack::sbuffer& response_buffer);
void dbg_cmd_exec(msgpack::object root, msgpack::sbuffer& response_buffer);
bool cb_xauto_barrier(int argc, char** argv);

std::wstring get_session_filename(size_t session_pid);
