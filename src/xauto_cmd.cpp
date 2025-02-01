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
    DbgGetRegDumpEx(&rd, sizeof(rd));

    std::array<uint8_t, 80> reg_area;
    std::copy_n((uint8_t*)&rd.regcontext.RegisterArea[0], 80, reg_area.begin()); 
    std::array<uint8_t, 16 * 16> xmm_regs;
    std::copy_n((uint8_t*)&rd.regcontext.XmmRegisters[0], 16 * 16, xmm_regs.begin()); 
    std::array<uint8_t, 16 * 32> ymm_regs;
    std::copy_n((uint8_t*)&rd.regcontext.YmmRegisters[0], 16 * 32, ymm_regs.begin()); 

    FpuRegsArr fpu_regs;
    for (size_t i = 0; i < 8; i++) {
        fpu_regs[i] = FpuRegsTup(
            std::array<uint8_t, 10>(),
            rd.x87FPURegisters[i].st_value,
            rd.x87FPURegisters[i].tag
        );
        std::copy_n((uint8_t*)&rd.x87FPURegisters[i].data[0], 10, std::get<0>(fpu_regs[i]).begin()); 
    }

    std::array<uint8_t, 128> lastError;
    std::copy_n((uint8_t*)&rd.lastError.name[0], 128, lastError.begin());
    std::array<uint8_t, 128> lastStatus;
    std::copy_n((uint8_t*)&rd.lastStatus.name[0], 128, lastStatus.begin());

    #ifdef _WIN64
    std::tuple<
        size_t, 
        CtxTup64, 
        FlagsTup, 
        FpuRegsArr, 
        MmxArr,
        MxcsrFieldsTup,
        x87StatusWordFieldsTup,
        x87ControlWordFieldsTup,
        std::tuple<uint32_t, std::array<uint8_t, 128>>,
        std::tuple<uint32_t, std::array<uint8_t, 128>>
    > regdump(
        64,
        CtxTup64(
            rd.regcontext.cax, rd.regcontext.cbx, rd.regcontext.ccx, rd.regcontext.cdx, rd.regcontext.cbp, rd.regcontext.csp, rd.regcontext.csi, rd.regcontext.cdi,
            rd.regcontext.r8, rd.regcontext.r9, rd.regcontext.r10, rd.regcontext.r11, rd.regcontext.r12, rd.regcontext.r13, rd.regcontext.r14, rd.regcontext.r15,
            rd.regcontext.cip,
            rd.regcontext.eflags,
            rd.regcontext.cs, rd.regcontext.ds, rd.regcontext.es, rd.regcontext.fs, rd.regcontext.gs, rd.regcontext.ss,
            rd.regcontext.dr0, rd.regcontext.dr1, rd.regcontext.dr2, rd.regcontext.dr3, rd.regcontext.dr6, rd.regcontext.dr7,
            reg_area,
            x87fpuTup(
                rd.regcontext.x87fpu.ControlWord,
                rd.regcontext.x87fpu.StatusWord,
                rd.regcontext.x87fpu.TagWord,
                rd.regcontext.x87fpu.ErrorOffset,
                rd.regcontext.x87fpu.ErrorSelector,
                rd.regcontext.x87fpu.DataOffset,
                rd.regcontext.x87fpu.DataSelector,
                rd.regcontext.x87fpu.Cr0NpxState
            ),
            rd.regcontext.MxCsr,
            xmm_regs,
            ymm_regs
        ),
        FlagsTup(rd.flags.c, rd.flags.p, rd.flags.a, rd.flags.z, rd.flags.s, rd.flags.t, rd.flags.i, rd.flags.d, rd.flags.o),
        fpu_regs,
        MmxArr {rd.mmx[0], rd.mmx[1], rd.mmx[2], rd.mmx[3], rd.mmx[4], rd.mmx[5], rd.mmx[6], rd.mmx[7]},
        MxcsrFieldsTup(
            rd.MxCsrFields.FZ, rd.MxCsrFields.PM, rd.MxCsrFields.UM, rd.MxCsrFields.OM,
            rd.MxCsrFields.ZM, rd.MxCsrFields.IM, rd.MxCsrFields.DM, rd.MxCsrFields.DAZ,
            rd.MxCsrFields.PE, rd.MxCsrFields.UE, rd.MxCsrFields.OE, rd.MxCsrFields.ZE,
            rd.MxCsrFields.DE, rd.MxCsrFields.IE, rd.MxCsrFields.RC
        ),
        x87StatusWordFieldsTup(
            rd.x87StatusWordFields.B, rd.x87StatusWordFields.C3, rd.x87StatusWordFields.C2, rd.x87StatusWordFields.C1, rd.x87StatusWordFields.C0,
            rd.x87StatusWordFields.ES, rd.x87StatusWordFields.SF, rd.x87StatusWordFields.P, rd.x87StatusWordFields.U, rd.x87StatusWordFields.O, 
            rd.x87StatusWordFields.Z, rd.x87StatusWordFields.D, rd.x87StatusWordFields.I, rd.x87StatusWordFields.TOP),
        x87ControlWordFieldsTup(
            rd.x87ControlWordFields.IC, rd.x87ControlWordFields.IEM, rd.x87ControlWordFields.PM, rd.x87ControlWordFields.UM, rd.x87ControlWordFields.OM, 
            rd.x87ControlWordFields.ZM, rd.x87ControlWordFields.DM, rd.x87ControlWordFields.IM, rd.x87ControlWordFields.RC, rd.x87ControlWordFields.PC),
        std::tuple<uint32_t, std::array<uint8_t, 128>>(rd.lastError.code, lastError),
        std::tuple<uint32_t, std::array<uint8_t, 128>>(rd.lastStatus.code, lastStatus)
    );
    #else
    std::tuple<
        size_t, 
        CtxTup64, 
        FlagsTup, 
        FpuRegsArr, 
        MmxArr,
        MxcsrFieldsTup,
        x87StatusWordFieldsTup,
        x87ControlWordFieldsTup,
        std::tuple<uint32_t, std::array<uint8_t, 128>>,
        std::tuple<uint32_t, std::array<uint8_t, 128>>
    > regdump(
        32,
        CtxTup32(
            rd.regcontext.cax, rd.regcontext.cbx, rd.regcontext.ccx, rd.regcontext.cdx, rd.regcontext.cbp, rd.regcontext.csp, rd.regcontext.csi, rd.regcontext.cdi,
            rd.regcontext.cip,
            rd.regcontext.eflags,
            rd.regcontext.cs, rd.regcontext.ds, rd.regcontext.es, rd.regcontext.fs, rd.regcontext.gs, rd.regcontext.ss,
            rd.regcontext.dr0, rd.regcontext.dr1, rd.regcontext.dr2, rd.regcontext.dr3, rd.regcontext.dr6, rd.regcontext.dr7,
            reg_area,
            x87fpuTup(
                rd.regcontext.x87fpu.ControlWord,
                rd.regcontext.x87fpu.StatusWord,
                rd.regcontext.x87fpu.TagWord,
                rd.regcontext.x87fpu.ErrorOffset,
                rd.regcontext.x87fpu.ErrorSelector,
                rd.regcontext.x87fpu.DataOffset,
                rd.regcontext.x87fpu.DataSelector,
                rd.regcontext.x87fpu.Cr0NpxState
            ),
            rd.regcontext.MxCsr,
            xmm_regs,
            ymm_regs
        ),
        FlagsTup(rd.flags.c, rd.flags.p, rd.flags.a, rd.flags.z, rd.flags.s, rd.flags.t, rd.flags.i, rd.flags.d, rd.flags.o),
        fpu_regs,
        MmxArr {rd.mmx[0], rd.mmx[1], rd.mmx[2], rd.mmx[3], rd.mmx[4], rd.mmx[5], rd.mmx[6], rd.mmx[7]},
        MxcsrFieldsTup(
            rd.MxCsrFields.FZ, rd.MxCsrFields.PM, rd.MxCsrFields.UM, rd.MxCsrFields.OM,
            rd.MxCsrFields.ZM, rd.MxCsrFields.IM, rd.MxCsrFields.DM, rd.MxCsrFields.DAZ,
            rd.MxCsrFields.PE, rd.MxCsrFields.UE, rd.MxCsrFields.OE, rd.MxCsrFields.ZE,
            rd.MxCsrFields.DE, rd.MxCsrFields.IE, rd.MxCsrFields.RC
        ),
        x87StatusWordFieldsTup(
            rd.x87StatusWordFields.B, rd.x87StatusWordFields.C3, rd.x87StatusWordFields.C2, rd.x87StatusWordFields.C1, rd.x87StatusWordFields.C0,
            rd.x87StatusWordFields.ES, rd.x87StatusWordFields.SF, rd.x87StatusWordFields.P, rd.x87StatusWordFields.U, rd.x87StatusWordFields.O, 
            rd.x87StatusWordFields.Z, rd.x87StatusWordFields.D, rd.x87StatusWordFields.I, rd.x87StatusWordFields.TOP),
        x87ControlWordFieldsTup(
            rd.x87ControlWordFields.IC, rd.x87ControlWordFields.IEM, rd.x87ControlWordFields.PM, rd.x87ControlWordFields.UM, rd.x87ControlWordFields.OM, 
            rd.x87ControlWordFields.ZM, rd.x87ControlWordFields.DM, rd.x87ControlWordFields.IM, rd.x87ControlWordFields.RC, rd.x87ControlWordFields.PC),
        std::tuple<uint32_t, std::array<uint8_t, 128>>(rd.lastError.code, lastError),
        std::tuple<uint32_t, std::array<uint8_t, 128>>(rd.lastStatus.code, lastStatus)
    );
    #endif

    msgpack::pack(response_buffer, regdump);
}