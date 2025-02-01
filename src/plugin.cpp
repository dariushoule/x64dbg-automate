#include "plugin.h"
#include "pluginmain.h"


CBTYPE last_event;
void* last_cbinfo;


void cb_sys_breakpoint(CBTYPE cbType, void* callbackInfo)
{
    PLUG_CB_SYSTEMBREAKPOINT* bp = (PLUG_CB_SYSTEMBREAKPOINT*)callbackInfo;
    msgpack::sbuffer outbuf;
    msgpack::pack(outbuf, std::tuple<std::string, size_t>(std::string("EVENT_SYSTEMBREAKPOINT"), (size_t)bp->reserved));
    srv->pub_sock.send(zmq::buffer(outbuf.data(), outbuf.size()), zmq::send_flags::none);
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
    srv->pub_sock.send(zmq::buffer(outbuf.data(), outbuf.size()), zmq::send_flags::none);
}

bool pluginInit(PLUG_INITSTRUCT* initStruct)
{
    dprintf("pluginInit(pluginHandle: %d)\n", pluginHandle);
    last_cbinfo = nullptr;
    _plugin_registercallback(pluginHandle, CB_BREAKPOINT, cb_breakpoint);
    _plugin_registercallback(pluginHandle, CB_SYSTEMBREAKPOINT, cb_sys_breakpoint);
    return true;
}

void pluginStop()
{
    dprintf("pluginStop(pluginHandle: %d)\n", pluginHandle);
}

void pluginSetup()
{
    dprintf("pluginSetup(pluginHandle: %d)\n", pluginHandle);
}
