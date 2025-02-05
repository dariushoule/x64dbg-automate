#include "plugin.h"
#include "pluginmain.h"


CBTYPE last_event;
void* last_cbinfo;


void cb_sys_breakpoint(CBTYPE cbType, void* callbackInfo)
{
    PLUG_CB_SYSTEMBREAKPOINT* bp = (PLUG_CB_SYSTEMBREAKPOINT*)callbackInfo;
    msgpack::sbuffer outbuf;
    msgpack::pack(outbuf, std::tuple<std::string, size_t>(std::string("EVENT_SYSTEMBREAKPOINT"), (size_t)bp->reserved));
    srv->pub_socket.send(zmq::buffer(outbuf.data(), outbuf.size()), zmq::send_flags::none);
}

void cb_breakpoint(CBTYPE cbType, void* callbackInfo)
{
    PLUG_CB_BREAKPOINT* bp = (PLUG_CB_BREAKPOINT*)callbackInfo;
    msgpack::sbuffer outbuf;
    msgpack::pack(outbuf, std::tuple<
        std::string, 
        size_t, 
        bool, 
        bool, 
        bool,
        std::string,
        std::string,
        size_t,
        uint8_t,
        uint8_t,
        size_t,
        bool, 
        bool,
        std::string,
        std::string,
        std::string,
        std::string,
        std::string
    >(
        std::string("EVENT_BREAKPOINT"),
        bp->breakpoint->type,
        bp->breakpoint->enabled,
        bp->breakpoint->singleshoot,
        bp->breakpoint->active,
        std::string(bp->breakpoint->name),
        std::string(bp->breakpoint->mod),
        bp->breakpoint->slot,
        bp->breakpoint->typeEx,
        bp->breakpoint->hwSize,
        bp->breakpoint->hitCount,
        bp->breakpoint->fastResume,
        bp->breakpoint->silent,
        std::string(bp->breakpoint->breakCondition),
        std::string(bp->breakpoint->logText),
        std::string(bp->breakpoint->logCondition),
        std::string(bp->breakpoint->commandText),
        std::string(bp->breakpoint->commandCondition)
    ));
    srv->pub_socket.send(zmq::buffer(outbuf.data(), outbuf.size()), zmq::send_flags::none);
}

void cb_create_thread(CBTYPE cbType, void* callbackInfo)
{
    PLUG_CB_CREATETHREAD* bp = (PLUG_CB_CREATETHREAD*)callbackInfo;
    msgpack::sbuffer outbuf;
    msgpack::pack(outbuf, std::tuple<std::string, size_t, size_t, size_t>(
        std::string("EVENT_CREATE_THREAD"), (size_t)bp->dwThreadId, (size_t)bp->CreateThread->lpThreadLocalBase, (size_t)bp->CreateThread->lpStartAddress));
    srv->pub_socket.send(zmq::buffer(outbuf.data(), outbuf.size()), zmq::send_flags::none);
}

void cb_exit_thread(CBTYPE cbType, void* callbackInfo)
{
    PLUG_CB_EXITTHREAD* bp = (PLUG_CB_EXITTHREAD*)callbackInfo;
    msgpack::sbuffer outbuf;
    msgpack::pack(outbuf, std::tuple<std::string, size_t, size_t>(
        std::string("EVENT_EXIT_THREAD"), (size_t)bp->dwThreadId, (size_t)bp->ExitThread->dwExitCode));
    srv->pub_socket.send(zmq::buffer(outbuf.data(), outbuf.size()), zmq::send_flags::none);
}

void cb_load_dll(CBTYPE cbType, void* callbackInfo)
{
    PLUG_CB_LOADDLL* bp = (PLUG_CB_LOADDLL*)callbackInfo;
    msgpack::sbuffer outbuf;
    msgpack::pack(outbuf, std::tuple<std::string, std::string, size_t>(
        std::string("EVENT_LOAD_DLL"), std::string(bp->modname), (size_t)bp->LoadDll->lpBaseOfDll));
    srv->pub_socket.send(zmq::buffer(outbuf.data(), outbuf.size()), zmq::send_flags::none);
}

void cb_unload_dll(CBTYPE cbType, void* callbackInfo)
{
    PLUG_CB_UNLOADDLL* bp = (PLUG_CB_UNLOADDLL*)callbackInfo;
    msgpack::sbuffer outbuf;
    msgpack::pack(outbuf, std::tuple<std::string, size_t>(
        std::string("EVENT_UNLOAD_DLL"), (size_t)bp->UnloadDll->lpBaseOfDll));
    srv->pub_socket.send(zmq::buffer(outbuf.data(), outbuf.size()), zmq::send_flags::none);
}

void cb_debugstr(CBTYPE cbType, void* callbackInfo)
{
    PLUG_CB_OUTPUTDEBUGSTRING* bp = (PLUG_CB_OUTPUTDEBUGSTRING*)callbackInfo;
    msgpack::sbuffer outbuf;

    std::vector<uint8_t> membuf(bp->DebugString->nDebugStringLength);
    if(!DbgMemRead((size_t)bp->DebugString->lpDebugStringData, membuf.data(), bp->DebugString->nDebugStringLength)) {
        dprintf("Failed to read debug string memory\n");
        return;
    }

    msgpack::pack(outbuf, std::tuple<std::string, std::vector<uint8_t>>(
        std::string("EVENT_OUTPUT_DEBUG_STRING"), membuf));
    srv->pub_socket.send(zmq::buffer(outbuf.data(), outbuf.size()), zmq::send_flags::none);
}

void cb_exception(CBTYPE cbType, void* callbackInfo)
{
    PLUG_CB_EXCEPTION* bp = (PLUG_CB_EXCEPTION*)callbackInfo;
    msgpack::sbuffer outbuf;
    std::vector<size_t> params;

    for (size_t i = 0; i < bp->Exception->ExceptionRecord.NumberParameters; i++) {
        params.push_back(bp->Exception->ExceptionRecord.ExceptionInformation[i]);
    }

    msgpack::pack(outbuf, std::tuple<std::string, size_t, size_t, size_t, size_t, size_t, std::vector<size_t>, size_t>(
        std::string("EVENT_EXCEPTION"), 
        (size_t)bp->Exception->ExceptionRecord.ExceptionCode,
        (size_t)bp->Exception->ExceptionRecord.ExceptionFlags,
        (size_t)bp->Exception->ExceptionRecord.ExceptionRecord,
        (size_t)bp->Exception->ExceptionRecord.ExceptionAddress,
        (size_t)bp->Exception->ExceptionRecord.NumberParameters,
        params,
        bp->Exception->dwFirstChance));
    srv->pub_socket.send(zmq::buffer(outbuf.data(), outbuf.size()), zmq::send_flags::none);
}

bool pluginInit(PLUG_INITSTRUCT* initStruct)
{
    dprintf("pluginInit(pluginHandle: %d)\n", pluginHandle);
    last_cbinfo = nullptr;
    _plugin_registercallback(pluginHandle, CB_BREAKPOINT, cb_breakpoint);
    _plugin_registercallback(pluginHandle, CB_SYSTEMBREAKPOINT, cb_sys_breakpoint);
    _plugin_registercallback(pluginHandle, CB_CREATETHREAD, cb_create_thread);
    _plugin_registercallback(pluginHandle, CB_EXITTHREAD, cb_exit_thread);
    _plugin_registercallback(pluginHandle, CB_LOADDLL, cb_load_dll);
    _plugin_registercallback(pluginHandle, CB_UNLOADDLL, cb_unload_dll);
    _plugin_registercallback(pluginHandle, CB_OUTPUTDEBUGSTRING, cb_debugstr);
    _plugin_registercallback(pluginHandle, CB_EXCEPTION, cb_exception);
    return true;
}

void pluginStop()
{
    dprintf("pluginStop(pluginHandle: %d)\n", pluginHandle);
    srv->release_session();
}

void pluginSetup()
{
    dprintf("pluginSetup(pluginHandle: %d)\n", pluginHandle);
}
