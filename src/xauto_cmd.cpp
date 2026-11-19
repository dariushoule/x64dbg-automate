#include "xauto_cmd.h"
#include "xauto_log.h"
#include <pluginsdk/bridgemain.h>
#include "pluginmain.h"
#include <TlHelp32.h>
#include <Shlwapi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <tuple>
#include <string>
#include <thread>
#include <vector>


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
    char symbolized_disassembly[GUI_MAX_DISASSEMBLY_SIZE];

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER) {
        msgpack::pack(response_buffer, false);
        return;
    }

    root.via.array.ptr[1].convert(addr);
    DbgDisasmAt(addr, &instr);

    std::string symbolized_instruction = std::string(instr.instruction);
    if(GuiGetDisassembly(addr, symbolized_disassembly)) {
        symbolized_instruction = std::string(symbolized_disassembly);
        // GuiGetDisassembly leaves a trailing separator space on operand-less instructions
        while(!symbolized_instruction.empty() && symbolized_instruction.back() == ' ') {
            symbolized_instruction.pop_back();
        }
    }

    msgpack::pack(response_buffer, DisasmTup(
        std::string(instr.instruction),
        symbolized_instruction,
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
    }
    
    BridgeFree(bp_list.bp);
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

void get_log(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t since_index = 0;
    if (root.via.array.size >= 2 &&
        root.via.array.ptr[1].type == msgpack::type::POSITIVE_INTEGER)
        root.via.array.ptr[1].convert(since_index);

    size_t limit = 0;
    if (root.via.array.size >= 3 &&
        root.via.array.ptr[2].type == msgpack::type::POSITIVE_INTEGER)
        root.via.array.ptr[2].convert(limit);

    std::string filter;
    if (root.via.array.size >= 4 &&
        root.via.array.ptr[3].type == msgpack::type::STR)
        root.via.array.ptr[3].convert(filter);

    auto snap = g_log_buffer.get_since(since_index, limit, filter);
    msgpack::pack(response_buffer, std::make_tuple(snap.next_index, snap.entries, snap.remaining, snap.evicted));
}

// ---------------------------------------------------------------------------
// Local extension: native analysis lookups
// ---------------------------------------------------------------------------

void xref_count_at(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t addr;

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER) {
        msgpack::pack(response_buffer, static_cast<size_t>(0));
        return;
    }

    root.via.array.ptr[1].convert(addr);
    msgpack::pack(response_buffer, static_cast<size_t>(DbgGetXrefCountAt(addr)));
}

void xref_type_at(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t addr;

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER) {
        msgpack::pack(response_buffer, static_cast<int>(0));
        return;
    }

    root.via.array.ptr[1].convert(addr);
    msgpack::pack(response_buffer, static_cast<int>(DbgGetXrefTypeAt(addr)));
}

void xref_get(msgpack::object root, msgpack::sbuffer& response_buffer) {
    std::vector<std::vector<size_t>> refs;
    size_t addr;

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER) {
        msgpack::pack(response_buffer, refs);
        return;
    }

    root.via.array.ptr[1].convert(addr);

    // The debug-side DBG_XREF_GET handler fills info.refcount itself and allocates
    // info.references with BridgeAlloc; the caller must release it with BridgeFree.
    XREF_INFO info = {};
    if(DbgXrefGet(addr, &info) && info.references != nullptr) {
        refs.reserve(info.refcount);
        for(duint i = 0; i < info.refcount; i++)
            refs.push_back({static_cast<size_t>(info.references[i].addr),
                            static_cast<size_t>(info.references[i].type)});
        BridgeFree(info.references);
    }

    msgpack::pack(response_buffer, refs);
}

void xref_add(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t addr = 0;
    size_t from = 0;
    bool ok = false;

    if(root.via.array.size >= 3 &&
        root.via.array.ptr[1].type == msgpack::type::POSITIVE_INTEGER &&
        root.via.array.ptr[2].type == msgpack::type::POSITIVE_INTEGER) {
        root.via.array.ptr[1].convert(addr);
        root.via.array.ptr[2].convert(from);
        ok = DbgXrefAdd(static_cast<duint>(addr), static_cast<duint>(from));
    }

    msgpack::pack(response_buffer, ok);
}

void function_type_at(msgpack::object root, msgpack::sbuffer& response_buffer) {
    size_t addr;

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER) {
        msgpack::pack(response_buffer, static_cast<int>(0));
        return;
    }

    root.via.array.ptr[1].convert(addr);
    msgpack::pack(response_buffer, static_cast<int>(DbgGetFunctionTypeAt(addr)));
}

void module_path(msgpack::object root, msgpack::sbuffer& response_buffer) {
    char path[MAX_PATH] = {0};
    bool ok = false;

    if(root.via.array.size >= 2) {
        const msgpack::object& arg = root.via.array.ptr[1];
        if(arg.type == msgpack::type::POSITIVE_INTEGER) {
            duint addr = 0;
            arg.convert(addr);
            ok = DbgFunctions()->ModPathFromAddr(addr, path, static_cast<int>(sizeof(path))) > 0;
        } else if(arg.type == msgpack::type::STR) {
            std::string name;
            arg.convert(name);
            ok = DbgFunctions()->ModPathFromName(name.c_str(), path, static_cast<int>(sizeof(path))) > 0;
        }
    }

    msgpack::pack(response_buffer, ok ? std::string(path) : std::string(""));
}

// ---------------------------------------------------------------------------
// Local extension: queue-aware execution (DbgCmdExec + sentinel command)
//
// DbgCmdExecDirect() (used by cmd_sync) runs on the RPC thread and bypasses the
// debugger command queue; commands that depend on queue state never execute.
// DbgCmdExec() queues, but is asynchronous. We therefore queue the real command
// followed by "xauto_barrier <token>": the command thread processes them in
// FIFO order, so the sentinel only runs after the real command finished.
//
// A token (not just a generation counter) is used so a sentinel from a timed-out
// request can never satisfy a later wait. Tokens are monotonic; the callback
// only advances g_barrier_done forward.
// ---------------------------------------------------------------------------

namespace {

std::mutex              g_barrier_cycle_mtx;   // one queue+wait cycle at a time
std::mutex              g_barrier_mtx;
std::condition_variable g_barrier_cv;
std::atomic<uint64_t>   g_barrier_token{1};
uint64_t                g_barrier_done = 0;    // guarded by g_barrier_mtx

struct QueueResult {
    bool     queued   = false;
    bool     executed = false;
    uint64_t elapsed_ms = 0;
};

// Queues every command, then one sentinel, and waits for the sentinel.
QueueResult queue_with_barrier(const std::vector<std::string>& cmds, int timeout_ms) {
    QueueResult r;
    std::lock_guard<std::mutex> cycle(g_barrier_cycle_mtx);

    const uint64_t token = g_barrier_token.fetch_add(1, std::memory_order_relaxed);
    auto t0 = std::chrono::steady_clock::now();

    bool queued = !cmds.empty();
    for(const auto& c : cmds) {
        if(!DbgCmdExec(c.c_str()))
            queued = false;
    }

    char sentinel[64];
    std::snprintf(sentinel, sizeof(sentinel), "xauto_barrier %llu",
                  static_cast<unsigned long long>(token));
    if(!DbgCmdExec(sentinel))
        queued = false;

    bool done = false;
    if(queued) {
        std::unique_lock<std::mutex> lk(g_barrier_mtx);
        done = g_barrier_cv.wait_for(
            lk, std::chrono::milliseconds(timeout_ms),
            [token] { return g_barrier_done >= token; });
    }

    r.queued   = queued;
    r.executed = queued && done;
    r.elapsed_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count());
    return r;
}

// The disassembly widget only accepts a selection inside the range it is
// currently showing (CPUDisassembly::selectionSetSlot rejects anything else),
// and x64dbg's analyse/analxrefs read their range from that selection. So the
// view must be moved to the target first (GuiDisasmAt), then the selection can
// be set. GuiDisasmAt is asynchronous, hence the polling helper.
constexpr duint EXEC_PROTECT_MASK =
    PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

bool wait_view_at(duint addr, int timeout_ms) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    SELECTIONDATA cur{};
    while(std::chrono::steady_clock::now() < deadline) {
        if(GuiSelectionGet(GUI_DISASSEMBLY, &cur)) {
            duint size = 0;
            duint view_base = DbgMemFindBaseAddr(cur.start, &size);
            if(view_base && addr >= view_base && addr < view_base + size)
                return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool set_selection(duint start, duint end) {
    SELECTIONDATA sel{};
    sel.start = start;
    sel.end   = end;
    return GuiSelectionSet(GUI_DISASSEMBLY, &sel);
}

// Restores the disassembly view/selection on every exit path, including
// msgpack exceptions thrown while building the response.
struct SelectionGuard {
    SELECTIONDATA prev{};
    bool          have  = false;
    bool          moved = false;

    SelectionGuard() {
        have = GuiSelectionGet(GUI_DISASSEMBLY, &prev);
    }
    ~SelectionGuard() {
        if(!moved || !have || !prev.start)
            return;
        GuiDisasmAt(prev.start, 0);
        if(wait_view_at(prev.start, 2000))
            set_selection(prev.start, prev.end);
    }
};

std::string hex_addr(duint v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << v;
    return oss.str();
}

} // namespace

bool cb_xauto_barrier(int argc, char** argv) {
    if(argc < 2)
        return false;

    char* end = nullptr;
    unsigned long long token = _strtoui64(argv[1], &end, 10);
    if(end == argv[1])
        return false;

    {
        std::lock_guard<std::mutex> lk(g_barrier_mtx);
        if(g_barrier_done < token)
            g_barrier_done = token;
    }
    g_barrier_cv.notify_all();
    return true;
}

void analyse_range(msgpack::object root, msgpack::sbuffer& response_buffer) {
    duint base = 0;
    duint req_size = 0;
    std::string mode = "all";
    int timeout_s = 20;

    if(root.via.array.size < 2 || root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER) {
        msgpack::pack(response_buffer, std::make_tuple(false, static_cast<uint64_t>(0),
                                                       std::string("missing base address")));
        return;
    }
    root.via.array.ptr[1].convert(base);
    if(root.via.array.size >= 3 && root.via.array.ptr[2].type == msgpack::type::POSITIVE_INTEGER)
        root.via.array.ptr[2].convert(req_size);
    if(root.via.array.size >= 4 && root.via.array.ptr[3].type == msgpack::type::STR)
        root.via.array.ptr[3].convert(mode);
    if(root.via.array.size >= 5 && root.via.array.ptr[4].type == msgpack::type::POSITIVE_INTEGER)
        root.via.array.ptr[4].convert(timeout_s);

    timeout_s = std::clamp(timeout_s, 1, 300);

    if(!base || !DbgMemIsValidReadPtr(base)) {
        msgpack::pack(response_buffer, std::make_tuple(false, static_cast<uint64_t>(0),
                                                       std::string("invalid address (not mapped): ") + hex_addr(base)));
        return;
    }

    duint region_size = 0;
    duint region_base = DbgMemFindBaseAddr(base, &region_size);
    if(!region_base || !region_size) {
        msgpack::pack(response_buffer, std::make_tuple(false, static_cast<uint64_t>(0),
                                                       std::string("no mapped region at ") + hex_addr(base)));
        return;
    }

    // collect the executable image regions of the module that owns base.
    // Windows maps each section as its own region, so the PE header page alone
    // (what DbgMemFindBaseAddr(base) returns for a module base) has no code —
    // analysing only that is what made D17 look broken.
    MEMMAP mm{};
    DbgMemMap(&mm);

    duint alloc_base = 0;
    for(int i = 0; i < mm.count; i++) {
        duint pb = reinterpret_cast<duint>(mm.page[i].mbi.BaseAddress);
        duint ps = static_cast<duint>(mm.page[i].mbi.RegionSize);
        if(base >= pb && base < pb + ps) {
            alloc_base = reinterpret_cast<duint>(mm.page[i].mbi.AllocationBase);
            break;
        }
    }

    duint win_end = (req_size > 1) ? base + req_size - 1 : 0; // 0 = whole module

    struct RegionTarget { duint base; duint size; };
    std::vector<RegionTarget> targets;
    for(int i = 0; i < mm.count && targets.size() < 16; i++) {
        const auto& pg = mm.page[i];
        duint pb = reinterpret_cast<duint>(pg.mbi.BaseAddress);
        duint ps = static_cast<duint>(pg.mbi.RegionSize);
        if(!ps || pg.mbi.Type != MEM_IMAGE)
            continue;
        if(alloc_base && reinterpret_cast<duint>(pg.mbi.AllocationBase) != alloc_base)
            continue;
        if(!(pg.mbi.Protect & EXEC_PROTECT_MASK))
            continue;
        if(win_end && (pb + ps - 1 < base || pb > win_end))
            continue;
        targets.push_back({pb, ps});
    }
    if(targets.empty())
        targets.push_back({region_base, region_size});

    std::vector<std::string> cmds;
    if(mode == "analyse" || mode == "anal") {
        cmds.emplace_back("analyse");
    } else if(mode == "analxrefs") {
        cmds.emplace_back("analxrefs");
    } else {
        cmds.emplace_back("analyse");
        cmds.emplace_back("analxrefs");
    }

    SelectionGuard sel;
    bool all_ok = true;
    uint64_t total_ms = 0;
    std::ostringstream detail;

    for(size_t i = 0; i < targets.size(); i++) {
        const RegionTarget& t = targets[i];
        const duint end = t.base + t.size - 1;

        GuiDisasmAt(t.base, 0);
        if(!wait_view_at(t.base, 2000)) {
            detail << "[" << i << "] " << hex_addr(t.base) << " size=" << hex_addr(t.size)
                   << ": disassembly view did not move (is the CPU window open?) — skipped\n";
            all_ok = false;
            continue;
        }
        if(!set_selection(t.base, end)) {
            detail << "[" << i << "] " << hex_addr(t.base) << " size=" << hex_addr(t.size)
                   << ": selection was rejected — skipped\n";
            all_ok = false;
            continue;
        }
        sel.moved = true;

        QueueResult r = queue_with_barrier(cmds, timeout_s * 1000);
        total_ms += r.elapsed_ms;
        detail << "[" << i << "] " << hex_addr(t.base) << " size=" << hex_addr(t.size)
               << " commands=";
        for(size_t c = 0; c < cmds.size(); c++)
            detail << (c ? "," : "") << cmds[c];
        detail << " -> "
               << (r.executed ? "done" : (r.queued ? "timeout" : "not queued"))
               << " (" << r.elapsed_ms << " ms)\n";
        if(!r.executed)
            all_ok = false;
    }

    msgpack::pack(response_buffer, std::make_tuple(all_ok, total_ms, detail.str()));
}

// Local extension (local.16): linear analysis inside the plugin.
//
// 1:1 port of x64dbg's XrefsAnalysis (src/dbg/analysis/xrefsanalysis.cpp, 54
// lines). The built-in `analxrefs` command takes its range from the GUI
// disassembly selection, which automation cannot rely on: with no CPU window the
// selection is empty, and GuiSelectionSet rejects ranges outside the current
// view. Running the same scan here removes that dependency entirely.
//
// Why it also fixes lookups from Python: one round-trip returns every edge in
// the range, instead of one `disassemble_at` round-trip per instruction. That is
// the difference between ~40 s and well under a second on a 300 KB module.
constexpr duint LINEAR_SCAN_LIMIT = 64 * 1024 * 1024;

void analyse_linear(msgpack::object root, msgpack::sbuffer& response_buffer) {
    using RefPair = std::tuple<uint64_t, uint64_t>;
    auto fail = [&](const std::string& why) {
        msgpack::pack(response_buffer, std::make_tuple(false,
                                                      static_cast<uint64_t>(0),
                                                      static_cast<uint64_t>(0),
                                                      static_cast<uint64_t>(0),
                                                      std::vector<RefPair>{},
                                                      why));
    };

    if(root.via.array.size < 3 ||
       root.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER ||
       root.via.array.ptr[2].type != msgpack::type::POSITIVE_INTEGER) {
        fail("analyse_linear needs scan_base and scan_size");
        return;
    }

    duint scan_base = 0, scan_size = 0;
    duint tgt_base = 0, tgt_size = 0;
    root.via.array.ptr[1].convert(scan_base);
    root.via.array.ptr[2].convert(scan_size);
    // Optional [3]/[4]: keep edges that point anywhere in this window (usually
    // the whole module) while only disassembling the given scan range (usually
    // its executable pages). Defaults to the scan range itself.
    if(root.via.array.size >= 5 &&
       root.via.array.ptr[3].type == msgpack::type::POSITIVE_INTEGER &&
       root.via.array.ptr[4].type == msgpack::type::POSITIVE_INTEGER) {
        root.via.array.ptr[3].convert(tgt_base);
        root.via.array.ptr[4].convert(tgt_size);
    }
    if(!tgt_size) {
        tgt_base = scan_base;
        tgt_size = scan_size;
    }

    if(!scan_base || !scan_size) {
        fail("scan_base/scan_size must be non-zero");
        return;
    }
    if(scan_size > LINEAR_SCAN_LIMIT) {
        fail("scan range too large for a single pass (64 MB limit): " + hex_addr(scan_size));
        return;
    }
    if(!tgt_size) {
        fail("target window must be non-zero");
        return;
    }
    if(!DbgIsDebugging()) {
        fail("no debuggee");
        return;
    }
    if(DbgIsRunning()) {
        fail("debuggee is running — pause first, memory reads would be stale");
        return;
    }
    if(!DbgMemIsValidReadPtr(scan_base)) {
        fail("scan_base is not mapped: " + hex_addr(scan_base));
        return;
    }

    const duint scan_end = scan_base + scan_size;
    const duint tgt_end = tgt_base + tgt_size;

    std::vector<XREF_EDGE> edges;
    std::vector<RefPair> out;
    uint64_t instr_count = 0;

    const auto t0 = std::chrono::steady_clock::now();
    for(duint addr = scan_base; addr < scan_end;) {
        // DbgDisasmFastAt instead of DbgDisasmAt: we only need the instruction
        // length and the resolved operand values, and the fast path skips the
        // mnemonic/symbol formatting that made the full call ~20 us per insn.
        BASIC_INSTRUCTION_INFO info{};
        DbgDisasmFastAt(addr, &info);
        const int len = info.size;
        if(len <= 0) {
            // Undecodable byte: x64dbg's own XrefsAnalysis advances by one too.
            addr++;
            continue;
        }
        instr_count++;

        const duint candidates[3] = {
            static_cast<duint>(info.addr),                                    // branch target
            (info.type & TYPE_MEMORY) ? static_cast<duint>(info.memory.value) : 0, // rip-relative operand
            (info.type & TYPE_VALUE) ? static_cast<duint>(info.value.value) : 0,   // immediate
        };
        for(duint dest : candidates) {
            if(dest >= tgt_base && dest < tgt_end) {
                edges.push_back(XREF_EDGE{dest, addr});
                out.emplace_back(static_cast<uint64_t>(addr), static_cast<uint64_t>(dest));
                break; // one edge per instruction, same as XrefsAnalysis
            }
        }
        addr += len;
    }

    // Feed x64dbg's own table as well, so the GUI and x64dbg-side queries agree
    // with what we return. AddMulti de-duplicates internally.
    if(!edges.empty())
        DbgXrefAddMulti(edges.data(), static_cast<duint>(edges.size()));

    const uint64_t elapsed_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count());

    msgpack::pack(response_buffer, std::make_tuple(true,
                                                  static_cast<uint64_t>(out.size()),
                                                  instr_count,
                                                  elapsed_ms,
                                                  out,
                                                  std::string()));
}

void script_load(msgpack::object root, msgpack::sbuffer& response_buffer) {
    std::string path;
    if(root.via.array.size >= 2 && root.via.array.ptr[1].type == msgpack::type::STR)
        root.via.array.ptr[1].convert(path);

    bool ok = false;
    std::string msg;
    if(path.empty()) {
        msg = "missing script path";
    } else {
        // DbgScriptLoad reports failures only through the log; check the file
        // first so the caller gets a deterministic error.
        const DWORD attrs = GetFileAttributesA(path.c_str());
        if(attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            msg = "script file not found: " + path;
        } else {
            DbgScriptLoad(path.c_str());
            ok = true;
            msg = "loaded " + path + " (parse errors, if any, are in the log)";
        }
    }
    msgpack::pack(response_buffer, std::make_tuple(ok, msg));
}

void script_run(msgpack::object root, msgpack::sbuffer& response_buffer) {
    int destline = 0;
    if(root.via.array.size >= 2 && root.via.array.ptr[1].type == msgpack::type::POSITIVE_INTEGER)
        root.via.array.ptr[1].convert(destline);
    if(destline < 0)
        destline = 0;

    DbgScriptRun(destline);

    char msg[128];
    std::snprintf(msg, sizeof(msg),
                  "script run requested from line %d (script errors go to the log)", destline);
    msgpack::pack(response_buffer, std::make_tuple(true, std::string(msg)));
}

void dbg_cmd_exec(msgpack::object root, msgpack::sbuffer& response_buffer) {
    std::string cmd;
    int timeout_s = 20;

    if(root.via.array.size >= 2 && root.via.array.ptr[1].type == msgpack::type::STR)
        root.via.array.ptr[1].convert(cmd);
    if(root.via.array.size >= 3 && root.via.array.ptr[2].type == msgpack::type::POSITIVE_INTEGER)
        root.via.array.ptr[2].convert(timeout_s);
    timeout_s = std::clamp(timeout_s, 1, 300);

    if(cmd.empty()) {
        msgpack::pack(response_buffer, std::make_tuple(false, false, static_cast<uint64_t>(0)));
        return;
    }

    QueueResult r = queue_with_barrier({cmd}, timeout_s * 1000);
    // executed == the sentinel ran after the command; it says nothing about the
    // command's own success (that is in the log, as with the GUI console).
    msgpack::pack(response_buffer, std::make_tuple(r.queued, r.executed, r.elapsed_ms));
}

std::wstring get_session_filename(size_t session_pid) {
    wchar_t temp_path[MAX_PATH * 4];
    if (GetTempPathW(MAX_PATH * 2, temp_path) == 0) {
        dprintf("Failed to get temp path\n");
        wcscpy(temp_path, L"c:\\windows\\temp\\");
    }

    return std::wstring(temp_path) + L"xauto_session." + std::to_wstring(session_pid) + L".lock";
}
