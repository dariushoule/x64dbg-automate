#include "xauto_cmd.h"
#include <pluginsdk/bridgemain.h>
#include <pluginsdk/_plugins.h>


void get_debugger_pid(msgpack::sbuffer& response_buffer) {
    msgpack::pack(response_buffer, GetCurrentProcessId());
}

void get_compat_v(msgpack::sbuffer& response_buffer) {
    msgpack::pack(response_buffer, std::string(XAUTO_COMPAT_VERSION));
}

void get_debugger_version(msgpack::sbuffer& response_buffer) {
    msgpack::pack(response_buffer, BridgeGetDbgVersion());
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
    std::tuple<size_t, bool> out_tup(res, success);
    msgpack::pack(response_buffer, out_tup);
}

void dbg_cmd_exec_direct(msgpack::object root, msgpack::sbuffer& response_buffer) {
    std::string cmd;

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::STR) {
        XAutoErrorResponse resp_obj = {"XERROR_BAD_EVAL", "Invalid or missing command string"};
        msgpack::pack(response_buffer, resp_obj);
        return;
    }

    root.via.array.ptr[1].convert(cmd);
    msgpack::pack(response_buffer, DbgCmdExecDirect(cmd.c_str()));
}

void dbg_is_running(msgpack::sbuffer& response_buffer) {
    msgpack::pack(response_buffer, DbgIsRunning());
}

void dbg_is_debugging(msgpack::sbuffer& response_buffer) {
    msgpack::pack(response_buffer, DbgIsDebugging());
}

void dbg_is_elevated(msgpack::sbuffer& response_buffer) {
    msgpack::pack(response_buffer, BridgeIsProcessElevated());
}

void dbg_get_bitness(msgpack::sbuffer& response_buffer) {
    msgpack::pack(response_buffer, sizeof(void*) == 8 ? 64 : 32);
}

void dbg_memmap(msgpack::sbuffer& response_buffer) {
    MEMMAP mm;
    DbgMemMap(&mm);
    std::vector<MemPageTup> memmap_entry_vec;
    for (size_t i = 0; i < mm.count; i++) {
        auto bi = mm.page[i].mbi;
        memmap_entry_vec.push_back(MemPageTup(
            (size_t)bi.BaseAddress,
            (size_t)bi.AllocationBase,
            bi.AllocationProtect,
            #if defined (_WIN64)
            bi.PartitionId,
            #else
            0,
            #endif
            bi.RegionSize,
            bi.State,
            bi.Protect,
            bi.Type,
            std::string(mm.page[i].info)
        ));
    }
    msgpack::pack(response_buffer, memmap_entry_vec);
    BridgeFree(mm.page);
}

void gui_refresh_views(msgpack::sbuffer& response_buffer) {
    GuiUpdateAllViews();
    msgpack::pack(response_buffer, true);
}

void dbg_read_memory(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t addr;
    size_t size;

    if(root.via.array.size < 3 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER || root.via.array.ptr[2].type != msgpack::type::POSITIVE_INTEGER) {
        XAutoErrorResponse resp_obj = {"XERROR_BAD_READ", "Invalid or missing memory read parameters"};
        msgpack::pack(response_buffer, resp_obj);
        return;
    }

    root.via.array.ptr[1].convert(addr);
    root.via.array.ptr[2].convert(size);

    std::vector<uint8_t> membuf(size);
    if(!DbgMemRead(addr, membuf.data(), size)) {
        XAutoErrorResponse resp_obj = {"XERROR_READ_FAILED", "Memory read failed"};
        msgpack::pack(response_buffer, resp_obj);
        return;
    }

    msgpack::pack(response_buffer, membuf);
}

void dbg_write_memory(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t addr;
    std::vector<uint8_t> data;

    if(root.via.array.size < 3 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER || root.via.array.ptr[2].type != msgpack::type::BIN) {
        XAutoErrorResponse resp_obj = {"XERROR_BAD_WRITE", "Invalid or missing memory write parameters"};
        msgpack::pack(response_buffer, resp_obj);
        return;
    }

    root.via.array.ptr[1].convert(addr);
    root.via.array.ptr[2].convert(data);

    if(!DbgMemWrite(addr, data.data(), data.size())) {
        XAutoErrorResponse resp_obj = {"XERROR_WRITE_FAILED", "Memory write failed"};
        msgpack::pack(response_buffer, resp_obj);
        return;
    }

    msgpack::pack(response_buffer, true);
}

void dbg_read_regs(msgpack::sbuffer& response_buffer) {
    REGDUMP rd;
    DbgGetRegDump(&rd); // WIP
    msgpack::pack(response_buffer, rd);
}