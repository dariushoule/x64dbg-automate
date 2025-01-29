#include "pluginmain.h"
#include "plugin.h"
#include <zmq.hpp>
#include <thread>


int pluginHandle;
HWND hwndDlg;
int hMenu;
int hMenuDisasm;
int hMenuDump;
int hMenuStack;
int hMenuGraph;
int hMenuMemmap;
int hMenuSymmod;


uint16_t xauto_session_id = 0;


int xauto_zmq_req_reply_server() 
{
    using namespace std::chrono_literals;

    // initialize the zmq context with a single IO thread
    zmq::context_t context{1};

    // construct a REP (reply) socket and bind to interface
    zmq::socket_t socket{context, zmq::socket_type::rep};
    socket.bind("tcp://localhost:44120");

    // prepare some static data for responses
    const std::string data{"World"};

    for (;;) 
    {
        zmq::message_t request;

        // receive a request from client
        socket.recv(request, zmq::recv_flags::none);
        // std::cout << "Received " << request.to_string() << std::endl;

        // simulate work
        std::this_thread::sleep_for(1s);

        // send the reply to the client
        socket.send(zmq::buffer(data), zmq::send_flags::none);
    }

    return 0;
}

void acquire_session() {
    HANDLE hMutex = CreateMutexA(NULL, TRUE, "x64dbg_automate_mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxA(NULL, "Another instance is already running", "Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }
}

PLUG_EXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{
    acquire_session();
    std::thread(xauto_zmq_req_reply_server).detach();

    initStruct->pluginVersion = PLUGIN_VERSION;
    initStruct->sdkVersion = PLUG_SDKVERSION;
    strncpy_s(initStruct->pluginName, PLUGIN_NAME, _TRUNCATE);
    pluginHandle = initStruct->pluginHandle;
    return pluginInit(initStruct);
}

PLUG_EXPORT bool plugstop()
{
    pluginStop();
    return true;
}

PLUG_EXPORT void plugsetup(PLUG_SETUPSTRUCT* setupStruct)
{
    hwndDlg = setupStruct->hwndDlg;
    hMenu = setupStruct->hMenu;
    hMenuDisasm = setupStruct->hMenuDisasm;
    hMenuDump = setupStruct->hMenuDump;
    hMenuStack = setupStruct->hMenuStack;
    hMenuGraph = setupStruct->hMenuGraph;
    hMenuMemmap = setupStruct->hMenuMemmap;
    hMenuSymmod = setupStruct->hMenuSymmod;
    pluginSetup();
}