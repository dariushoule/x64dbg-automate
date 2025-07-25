#include "xauto_cmd.h"
#include <pluginsdk/bridgemain.h>
#include "pluginmain.h"
#include <TlHelp32.h>
#include <Shlwapi.h>


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

// Carbon copy of the non-exposed methods in BridegeMain.cpp
#define MXCSRFLAG_IE 0x1
#define MXCSRFLAG_DE 0x2
#define MXCSRFLAG_ZE 0x4
#define MXCSRFLAG_OE 0x8
#define MXCSRFLAG_UE 0x10
#define MXCSRFLAG_PE 0x20
#define MXCSRFLAG_DAZ 0x40
#define MXCSRFLAG_IM 0x80
#define MXCSRFLAG_DM 0x100
#define MXCSRFLAG_ZM 0x200
#define MXCSRFLAG_OM 0x400
#define MXCSRFLAG_UM 0x800
#define MXCSRFLAG_PM 0x1000
#define MXCSRFLAG_FZ 0x8000

static void GetMxCsrFields(MXCSRFIELDS* MxCsrFields, DWORD MxCsr)
{
    MxCsrFields->IE = ((MxCsr & MXCSRFLAG_IE) != 0);
    MxCsrFields->DE = ((MxCsr & MXCSRFLAG_DE) != 0);
    MxCsrFields->ZE = ((MxCsr & MXCSRFLAG_ZE) != 0);
    MxCsrFields->OE = ((MxCsr & MXCSRFLAG_OE) != 0);
    MxCsrFields->UE = ((MxCsr & MXCSRFLAG_UE) != 0);
    MxCsrFields->PE = ((MxCsr & MXCSRFLAG_PE) != 0);
    MxCsrFields->DAZ = ((MxCsr & MXCSRFLAG_DAZ) != 0);
    MxCsrFields->IM = ((MxCsr & MXCSRFLAG_IM) != 0);
    MxCsrFields->DM = ((MxCsr & MXCSRFLAG_DM) != 0);
    MxCsrFields->ZM = ((MxCsr & MXCSRFLAG_ZM) != 0);
    MxCsrFields->OM = ((MxCsr & MXCSRFLAG_OM) != 0);
    MxCsrFields->UM = ((MxCsr & MXCSRFLAG_UM) != 0);
    MxCsrFields->PM = ((MxCsr & MXCSRFLAG_PM) != 0);
    MxCsrFields->FZ = ((MxCsr & MXCSRFLAG_FZ) != 0);

    MxCsrFields->RC = (MxCsr & 0x6000) >> 13;
}

#define x87CONTROLWORD_FLAG_IM 0x1
#define x87CONTROLWORD_FLAG_DM 0x2
#define x87CONTROLWORD_FLAG_ZM 0x4
#define x87CONTROLWORD_FLAG_OM 0x8
#define x87CONTROLWORD_FLAG_UM 0x10
#define x87CONTROLWORD_FLAG_PM 0x20
#define x87CONTROLWORD_FLAG_IEM 0x80
#define x87CONTROLWORD_FLAG_IC 0x1000

static void Getx87ControlWordFields(X87CONTROLWORDFIELDS* x87ControlWordFields, WORD ControlWord)
{
    x87ControlWordFields->IM = ((ControlWord & x87CONTROLWORD_FLAG_IM) != 0);
    x87ControlWordFields->DM = ((ControlWord & x87CONTROLWORD_FLAG_DM) != 0);
    x87ControlWordFields->ZM = ((ControlWord & x87CONTROLWORD_FLAG_ZM) != 0);
    x87ControlWordFields->OM = ((ControlWord & x87CONTROLWORD_FLAG_OM) != 0);
    x87ControlWordFields->UM = ((ControlWord & x87CONTROLWORD_FLAG_UM) != 0);
    x87ControlWordFields->PM = ((ControlWord & x87CONTROLWORD_FLAG_PM) != 0);
    x87ControlWordFields->IEM = ((ControlWord & x87CONTROLWORD_FLAG_IEM) != 0);
    x87ControlWordFields->IC = ((ControlWord & x87CONTROLWORD_FLAG_IC) != 0);

    x87ControlWordFields->RC = ((ControlWord & 0xC00) >> 10);
    x87ControlWordFields->PC = ((ControlWord & 0x300) >> 8);
}

#define x87STATUSWORD_FLAG_I 0x1
#define x87STATUSWORD_FLAG_D 0x2
#define x87STATUSWORD_FLAG_Z 0x4
#define x87STATUSWORD_FLAG_O 0x8
#define x87STATUSWORD_FLAG_U 0x10
#define x87STATUSWORD_FLAG_P 0x20
#define x87STATUSWORD_FLAG_SF 0x40
#define x87STATUSWORD_FLAG_ES 0x80
#define x87STATUSWORD_FLAG_C0 0x100
#define x87STATUSWORD_FLAG_C1 0x200
#define x87STATUSWORD_FLAG_C2 0x400
#define x87STATUSWORD_FLAG_C3 0x4000
#define x87STATUSWORD_FLAG_B 0x8000

static void Getx87StatusWordFields(X87STATUSWORDFIELDS* x87StatusWordFields, WORD StatusWord)
{
    x87StatusWordFields->I = ((StatusWord & x87STATUSWORD_FLAG_I) != 0);
    x87StatusWordFields->D = ((StatusWord & x87STATUSWORD_FLAG_D) != 0);
    x87StatusWordFields->Z = ((StatusWord & x87STATUSWORD_FLAG_Z) != 0);
    x87StatusWordFields->O = ((StatusWord & x87STATUSWORD_FLAG_O) != 0);
    x87StatusWordFields->U = ((StatusWord & x87STATUSWORD_FLAG_U) != 0);
    x87StatusWordFields->P = ((StatusWord & x87STATUSWORD_FLAG_P) != 0);
    x87StatusWordFields->SF = ((StatusWord & x87STATUSWORD_FLAG_SF) != 0);
    x87StatusWordFields->ES = ((StatusWord & x87STATUSWORD_FLAG_ES) != 0);
    x87StatusWordFields->C0 = ((StatusWord & x87STATUSWORD_FLAG_C0) != 0);
    x87StatusWordFields->C1 = ((StatusWord & x87STATUSWORD_FLAG_C1) != 0);
    x87StatusWordFields->C2 = ((StatusWord & x87STATUSWORD_FLAG_C2) != 0);
    x87StatusWordFields->C3 = ((StatusWord & x87STATUSWORD_FLAG_C3) != 0);
    x87StatusWordFields->B = ((StatusWord & x87STATUSWORD_FLAG_B) != 0);

    x87StatusWordFields->TOP = ((StatusWord & 0x3800) >> 11);
}

// Definitions From TitanEngine
#define Getx87r0PositionInRegisterArea(STInTopStack) ((8 - STInTopStack) % 8)
#define Calculatex87registerPositionInRegisterArea(x87r0_position, index) (((x87r0_position + index) % 8))
#define GetRegisterAreaOf87register(register_area, x87r0_position, index) (((char *) register_area) + 10 * Calculatex87registerPositionInRegisterArea(x87r0_position, index) )
#define GetSTValueFromIndex(x87r0_position, index) ((x87r0_position + index) % 8)


void dbg_read_regs(msgpack::sbuffer& response_buffer) {
    REGDUMP_AVX512 rd;

    // Calculated
    FLAGS flags;
    memset(&flags, 0, sizeof(flags));
    X87FPUREGISTER x87FPURegisters[8];
    memset(x87FPURegisters, 0, sizeof(x87FPURegisters));
    unsigned long long mmx[8];
    memset(mmx, 0, sizeof(mmx));
    MXCSRFIELDS MxCsrFields;
    memset(&MxCsrFields, 0, sizeof(MxCsrFields));
    X87STATUSWORDFIELDS x87StatusWordFields;
    memset(&x87StatusWordFields, 0, sizeof(x87StatusWordFields));
    X87CONTROLWORDFIELDS x87ControlWordFields;
    memset(&x87ControlWordFields, 0, sizeof(x87ControlWordFields));
    LASTERROR lastError;
    memset(&lastError, 0, sizeof(lastError));
    LASTSTATUS lastStatus;
    memset(&lastStatus, 0, sizeof(lastStatus));

    DbgGetRegDumpEx(&rd, sizeof(rd));
    GetMxCsrFields(&MxCsrFields, rd.regcontext.MxCsr);
    Getx87ControlWordFields(&x87ControlWordFields, rd.regcontext.x87fpu.ControlWord);
    Getx87StatusWordFields(&x87StatusWordFields, rd.regcontext.x87fpu.StatusWord);

    DWORD x87r0_position = Getx87r0PositionInRegisterArea(x87StatusWordFields.TOP);
    for(int i = 0; i < 8; i++)
    {
        memcpy(x87FPURegisters[i].data, GetRegisterAreaOf87register(rd.regcontext.RegisterArea, x87r0_position, i), 10);
        mmx[i] = *((uint64_t*)&x87FPURegisters[i].data);
        x87FPURegisters[i].st_value = GetSTValueFromIndex(x87r0_position, i);
        x87FPURegisters[i].tag = (int)((rd.regcontext.x87fpu.TagWord >> (i * 2)) & 0x3);
    }

    char fmtString[64] = "";
    auto pStringFormatInline = DbgFunctions()->StringFormatInline; // When called before dbgfunctionsinit() this can be NULL!
    lastError.code = rd.lastError;
    if(pStringFormatInline && sprintf_s(fmtString, _TRUNCATE, "{winerrorname@%X}", lastError.code) != -1)
    {
        pStringFormatInline(fmtString, sizeof(lastError.name), lastError.name);
    }
    else
    {
        memset(lastError.name, 0, sizeof(lastError.name));
    }

    lastStatus.code = rd.lastStatus;
    if(pStringFormatInline && sprintf_s(fmtString, _TRUNCATE, "{ntstatusname@%X}", lastStatus.code) != -1)
    {
        pStringFormatInline(fmtString, sizeof(lastStatus.name), lastStatus.name);
    }
    else
    {
        memset(lastStatus.name, 0, sizeof(lastStatus.name));
    }

    std::array<uint8_t, 80> reg_area;
    std::copy_n((uint8_t*)&rd.regcontext.RegisterArea[0], 80, reg_area.begin()); 

    #ifdef _WIN64
    std::array<uint8_t, 64 * 32> zmm_regs;
    std::copy_n((uint8_t*)&rd.regcontext.ZmmRegisters[0], 64 * 32, zmm_regs.begin()); 
    #else
    std::array<uint8_t, 64 * 8> zmm_regs;
    std::copy_n((uint8_t*)&rd.regcontext.ZmmRegisters[0], 64 * 8, zmm_regs.begin());
    #endif

    FpuRegsArr fpu_regs;
    for (size_t i = 0; i < 8; i++) {
        fpu_regs[i] = FpuRegsTup(
            std::array<uint8_t, 10>(),
            x87FPURegisters[i].st_value,
            x87FPURegisters[i].tag
        );
        std::copy_n((uint8_t*)&x87FPURegisters[i].data[0], 10, std::get<0>(fpu_regs[i]).begin()); 
    }

    std::array<uint8_t, 128> _lastError;
    std::copy_n((uint8_t*)&lastError.name[0], 128, _lastError.begin());
    std::array<uint8_t, 128> _lastStatus;
    std::copy_n((uint8_t*)&lastStatus.name[0], 128, _lastStatus.begin());

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
            zmm_regs
        ),
        FlagsTup(flags.c, flags.p, flags.a, flags.z, flags.s, flags.t, flags.i, flags.d, flags.o),
        fpu_regs,
        MmxArr {mmx[0], mmx[1], mmx[2], mmx[3], mmx[4], mmx[5], mmx[6], mmx[7]},
        MxcsrFieldsTup(
            MxCsrFields.FZ, MxCsrFields.PM, MxCsrFields.UM, MxCsrFields.OM,
            MxCsrFields.ZM, MxCsrFields.IM, MxCsrFields.DM, MxCsrFields.DAZ,
            MxCsrFields.PE, MxCsrFields.UE, MxCsrFields.OE, MxCsrFields.ZE,
            MxCsrFields.DE, MxCsrFields.IE, MxCsrFields.RC
        ),
        x87StatusWordFieldsTup(
            x87StatusWordFields.B, x87StatusWordFields.C3, x87StatusWordFields.C2, x87StatusWordFields.C1, x87StatusWordFields.C0,
            x87StatusWordFields.ES, x87StatusWordFields.SF, x87StatusWordFields.P, x87StatusWordFields.U, x87StatusWordFields.O, 
            x87StatusWordFields.Z, x87StatusWordFields.D, x87StatusWordFields.I, x87StatusWordFields.TOP),
        x87ControlWordFieldsTup(
            x87ControlWordFields.IC, x87ControlWordFields.IEM, x87ControlWordFields.PM, x87ControlWordFields.UM, x87ControlWordFields.OM, 
            x87ControlWordFields.ZM, x87ControlWordFields.DM, x87ControlWordFields.IM, x87ControlWordFields.RC, x87ControlWordFields.PC),
        std::tuple<uint32_t, std::array<uint8_t, 128>>(lastError.code, _lastError),
        std::tuple<uint32_t, std::array<uint8_t, 128>>(lastStatus.code, _lastStatus)
    );
    #else
    std::tuple<
        size_t, 
        CtxTup32, 
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
            zmm_regs
        ),
        FlagsTup(flags.c, flags.p, flags.a, flags.z, flags.s, flags.t, flags.i, flags.d, flags.o),
        fpu_regs,
        MmxArr {mmx[0], mmx[1], mmx[2], mmx[3], mmx[4], mmx[5], mmx[6], mmx[7]},
        MxcsrFieldsTup(
            MxCsrFields.FZ, MxCsrFields.PM, MxCsrFields.UM, MxCsrFields.OM,
            MxCsrFields.ZM, MxCsrFields.IM, MxCsrFields.DM, MxCsrFields.DAZ,
            MxCsrFields.PE, MxCsrFields.UE, MxCsrFields.OE, MxCsrFields.ZE,
            MxCsrFields.DE, MxCsrFields.IE, MxCsrFields.RC
        ),
        x87StatusWordFieldsTup(
            x87StatusWordFields.B, x87StatusWordFields.C3, x87StatusWordFields.C2, x87StatusWordFields.C1, x87StatusWordFields.C0,
            x87StatusWordFields.ES, x87StatusWordFields.SF, x87StatusWordFields.P, x87StatusWordFields.U, x87StatusWordFields.O, 
            x87StatusWordFields.Z, x87StatusWordFields.D, x87StatusWordFields.I, x87StatusWordFields.TOP),
        x87ControlWordFieldsTup(
            x87ControlWordFields.IC, x87ControlWordFields.IEM, x87ControlWordFields.PM, x87ControlWordFields.UM, x87ControlWordFields.OM, 
            x87ControlWordFields.ZM, x87ControlWordFields.DM, x87ControlWordFields.IM, x87ControlWordFields.RC, x87ControlWordFields.PC),
        std::tuple<uint32_t, std::array<uint8_t, 128>>(lastError.code, _lastError),
        std::tuple<uint32_t, std::array<uint8_t, 128>>(lastStatus.code, _lastStatus)
    );
    #endif

    msgpack::pack(response_buffer, regdump);
}

void dbg_read_setting_sz(msgpack::object root, msgpack::sbuffer& response_buffer) {
    std::string section;
    std::string setting_name;

    if(root.via.array.size < 3 || root.via.array.ptr[1].type != msgpack::type::STR || root.via.array.ptr[2].type != msgpack::type::STR) {
        XAutoErrorResponse resp_obj = {"XERROR_BAD_SETTING", "Invalid or missing setting string"};
        msgpack::pack(response_buffer, resp_obj);
        return;
    }

    root.via.array.ptr[1].convert(section);
    root.via.array.ptr[2].convert(setting_name);

    char* setting_val = (char*)BridgeAlloc(MAX_SETTING_SIZE);
    bool res = BridgeSettingGet(section.c_str(), setting_name.c_str(), setting_val);
    msgpack::pack(response_buffer, std::tuple<bool, std::string>(res, std::string(setting_val)));
    BridgeFree(setting_val);
}

void dbg_write_setting_sz(msgpack::object root, msgpack::sbuffer& response_buffer) {
    std::string section;
    std::string setting_name;
    std::string setting_val;

    if( root.via.array.size < 3 || 
        root.via.array.ptr[1].type != msgpack::type::STR || 
        root.via.array.ptr[2].type != msgpack::type::STR || 
        root.via.array.ptr[3].type != msgpack::type::STR
    ) {
        XAutoErrorResponse resp_obj = {"XERROR_BAD_SETTING", "Invalid or missing setting string"};
        msgpack::pack(response_buffer, resp_obj);
        return;
    }

    root.via.array.ptr[1].convert(section);
    root.via.array.ptr[2].convert(setting_name);
    root.via.array.ptr[3].convert(setting_val);

    bool res = BridgeSettingSet(section.c_str(), setting_name.c_str(), setting_val.c_str());
    msgpack::pack(response_buffer, res);
}

void dbg_read_setting_uint(msgpack::object root, msgpack::sbuffer& response_buffer) {
    std::string section;
    std::string setting_name;

    if(root.via.array.size < 3 || root.via.array.ptr[1].type != msgpack::type::STR || root.via.array.ptr[2].type != msgpack::type::STR) {
        XAutoErrorResponse resp_obj = {"XERROR_BAD_SETTING", "Invalid or missing setting string"};
        msgpack::pack(response_buffer, resp_obj);
        return;
    }

    root.via.array.ptr[1].convert(section);
    root.via.array.ptr[2].convert(setting_name);

    duint setting_val;
    bool res = BridgeSettingGetUint(section.c_str(), setting_name.c_str(), &setting_val);
    msgpack::pack(response_buffer, std::tuple<bool, size_t>(res, setting_val));
}

void dbg_write_setting_uint(msgpack::object root, msgpack::sbuffer& response_buffer) {
    std::string section;
    std::string setting_name;
    size_t setting_val;

    if( root.via.array.size < 3 || 
        root.via.array.ptr[1].type != msgpack::type::STR || 
        root.via.array.ptr[2].type != msgpack::type::STR || 
        (root.via.array.ptr[3].type != msgpack::type::POSITIVE_INTEGER)
    ) {
        XAutoErrorResponse resp_obj = {"XERROR_BAD_SETTING", "Invalid or missing setting string"};
        msgpack::pack(response_buffer, resp_obj);
        return;
    }

    root.via.array.ptr[1].convert(section);
    root.via.array.ptr[2].convert(setting_name);
    root.via.array.ptr[3].convert(setting_val);

    bool res = BridgeSettingSetUint(section.c_str(), setting_name.c_str(), setting_val);
    msgpack::pack(response_buffer, res);
}

void dbg_is_valid_read_ptr(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t addr;

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER) {
        msgpack::pack(response_buffer, false);
        return;
    }

    root.via.array.ptr[1].convert(addr);
    msgpack::pack(response_buffer, DbgMemIsValidReadPtr(addr));
}

void disassemble_at(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t addr;
    DISASM_INSTR instr;

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER) {
        msgpack::pack(response_buffer, false);
        return;
    }

    root.via.array.ptr[1].convert(addr);
    DbgDisasmAt(addr, &instr);
    msgpack::pack(response_buffer, DisasmTup(
        std::string(instr.instruction),
        instr.argcount,
        instr.instr_size,
        instr.type,
        {
            DisasmArgTup(
                std::string(instr.arg[0].mnemonic),
                instr.arg[0].type,
                instr.arg[0].segment,
                instr.arg[0].constant,
                instr.arg[0].value,
                instr.arg[0].memvalue
            ),
            DisasmArgTup(
                std::string(instr.arg[1].mnemonic),
                instr.arg[1].type,
                instr.arg[1].segment,
                instr.arg[1].constant,
                instr.arg[1].value,
                instr.arg[1].memvalue
            ),
            DisasmArgTup(
                std::string(instr.arg[2].mnemonic),
                instr.arg[2].type,
                instr.arg[2].segment,
                instr.arg[2].constant,
                instr.arg[2].value,
                instr.arg[2].memvalue
            )
        }
    ));
}

void assemble_at(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t addr;
    std::string instr;

    if(root.via.array.size < 3 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER || root.via.array.ptr[2].type != msgpack::type::STR) {
        msgpack::pack(response_buffer, false);
        return;
    }

    root.via.array.ptr[1].convert(addr);
    root.via.array.ptr[2].convert(instr);
    msgpack::pack(response_buffer, DbgAssembleAt(addr, instr.c_str()));
}

void get_breakpoints(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t bp_type;
    BPMAP bp_list;

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER) {
        msgpack::pack(response_buffer, false);
        return;
    }

    root.via.array.ptr[1].convert(bp_type);

    int n_bps = DbgGetBpList((BPXTYPE)bp_type, &bp_list);
    std::vector<BpxTup> bp_vec;
    for (int i = 0; i < n_bps; i++) {
        bp_vec.push_back(BpxTup(
            bp_list.bp[i].type,
            bp_list.bp[i].addr,
            bp_list.bp[i].enabled,
            bp_list.bp[i].singleshoot,
            bp_list.bp[i].active,
            std::string(bp_list.bp[i].name),
            std::string(bp_list.bp[i].mod),
            bp_list.bp[i].slot,
            bp_list.bp[i].typeEx,
            bp_list.bp[i].hwSize,
            bp_list.bp[i].hitCount,
            bp_list.bp[i].fastResume,
            bp_list.bp[i].silent,
            std::string(bp_list.bp[i].breakCondition),
            std::string(bp_list.bp[i].logText),
            std::string(bp_list.bp[i].logCondition),
            std::string(bp_list.bp[i].commandText),
            std::string(bp_list.bp[i].commandCondition)
        ));
        BridgeFree(bp_list.bp);
    }

    msgpack::pack(response_buffer, bp_vec);
}

void get_label_at(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t addr;
    size_t sreg;
    char text[MAX_LABEL_SIZE];
    memset(text, 0, MAX_LABEL_SIZE);

    if(root.via.array.size < 3 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER || root.via.array.ptr[2].type != msgpack::type::POSITIVE_INTEGER) {
        msgpack::pack(response_buffer, false);
        return;
    }

    root.via.array.ptr[1].convert(addr);
    root.via.array.ptr[2].convert(sreg);
    bool res = DbgGetLabelAt(addr, (SEGMENTREG)sreg, text);
    msgpack::pack(response_buffer, std::tuple<bool, std::string>(res, std::string(text)));
}

void get_comment_at(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t addr;
    char text[MAX_COMMENT_SIZE];
    memset(text, 0, MAX_COMMENT_SIZE);

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER) {
        msgpack::pack(response_buffer, false);
        return;
    }

    root.via.array.ptr[1].convert(addr);
    bool res = DbgGetCommentAt(addr, text);
    msgpack::pack(response_buffer, std::tuple<bool, std::string>(res, std::string(text)));
}

void get_symbol_at(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t addr;
    SYMBOLINFO* info = (SYMBOLINFO*)BridgeAlloc(sizeof(SYMBOLINFO));
    memset(info, 0, sizeof(SYMBOLINFO));

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER) {
        msgpack::pack(response_buffer, false);
        return;
    }

    root.via.array.ptr[1].convert(addr);
    bool res = DbgGetSymbolInfoAt(addr, info);
    msgpack::pack(response_buffer, std::tuple<bool, size_t, std::string, std::string, size_t, size_t>(
        res,
        info->addr,
        info->decoratedSymbol ? std::string(info->decoratedSymbol) : std::string(),
        info->undecoratedSymbol ? std::string(info->undecoratedSymbol) : std::string(),
        info->type,
        info->ordinal
    ));

    if (res) {
        if (info->freeDecorated) {
            BridgeFree(info->decoratedSymbol);
        }
        if (info->freeUndecorated) {
            BridgeFree(info->undecoratedSymbol);
        }
    }
    BridgeFree(info);
}

std::wstring get_session_filename(size_t session_pid) {
    wchar_t temp_path[MAX_PATH * 4];
    if (GetTempPathW(MAX_PATH * 2, temp_path) == 0) {
        dprintf("Failed to get temp path\n");
        wcscpy(temp_path, L"c:\\windows\\temp\\");
    }

    return std::wstring(temp_path) + L"xauto_session." + std::to_wstring(session_pid) + L".lock";
}
