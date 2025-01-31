#include "pluginmain.h"
#include "xauto_server.h"
#include "xauto_cmd.h"

#include <thread>


using namespace msgpack;


constexpr int DISPATCH_CONTINUE = 0;
constexpr int DISPATCH_EXIT = -1;


int XAuto::XAutoServer::_dispatch_cmd(msgpack::object root, msgpack::sbuffer& response_buffer) {
    if (root.type == msgpack::type::STR) {
        std::string str;
        root.convert(str);
        if (str == "PING") {
            msgpack::pack(response_buffer, "PONG");
        }
    } else if (root.type == msgpack::type::ARRAY && root.via.array.size > 0 && root.via.array.ptr[0].type == msgpack::type::STR) {
        std::string cmd;
        root.via.array.ptr[0].convert(cmd);
        // dprintf("Received command: %s\n", cmd.c_str());
        if (cmd == XAUTO_REQ_DBG_EVENT) {
            // msgpack::pack(response_buffer, "EVENT");
            // TODO subscribe to plugin events and yield them?
        } else if (cmd == XAUTO_REQ_DEBUGGER_PID) {
            get_debugger_pid(response_buffer);
        } else if (cmd == XAUTO_REQ_COMPAT_VERSION) {
            get_compat_v(response_buffer);
        }else if (cmd == XAUTO_REQ_DBG_EVAL) {
            dbg_eval(root, response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_CMD_EXEC_DIRECT) {
            dbg_cmd_exec_direct(root, response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_IS_RUNNING) {
            dbg_is_running(response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_IS_DEBUGGING) {
            dbg_is_debugging(response_buffer);
        } else if (cmd == XAUTO_REQ_QUIT) {
            msgpack::pack(response_buffer, "OK_QUITTING");
            return DISPATCH_EXIT;
        }
    }

    return DISPATCH_CONTINUE;
}


void XAuto::XAutoServer::xauto_srv_thread() {
    zmq::context_t context{1};
    zmq::socket_t socket{context, zmq::socket_type::rep};
    socket.bind(("tcp://localhost:" + std::to_string(SESS_PORT)).c_str());
    dprintf("Allocated port: %d\n", SESS_PORT);

    try {
        for (;;) 
        {
            zmq::message_t request;
            msgpack::sbuffer outbuf;

            auto res = socket.recv(request, zmq::recv_flags::none);
            if (!res.has_value()) {
                dprintf("zmq error, failed to recv message: %s (0x%X)\n", zmq_strerror(zmq_errno()), zmq_errno());
                continue;
            }

            msgpack::object_handle oh = msgpack::unpack((const char*)request.data(), request.size());
            msgpack::object root = oh.get();
            auto dispatch_exit = _dispatch_cmd(root, outbuf);

            if (outbuf.size() == 0) {
                dprintf("Received: unknown\n");
                XAutoErrorResponse err_resp_obj = {"XERROR_UNK", "Could not understand input"};
                msgpack::pack(outbuf, err_resp_obj);
            }

            socket.send(zmq::buffer(outbuf.data(), outbuf.size()), zmq::send_flags::none);

            if (dispatch_exit == DISPATCH_EXIT) {
                socket.close();
                break;
            }
        }

        dprintf("Caught request to terminate\n");
        GuiCloseApplication();
    } catch (const zmq::error_t& e) {
        dprintf("ZMQ Error: %s\n", e.what());
    }
}

void XAuto::XAutoServer::acquire_session() {
    HANDLE hMutex;
    do {
        xauto_session_id++;
        hMutex = CreateMutexA(NULL, TRUE, ("x64dbg_automate_mutex_s_" + std::to_string(xauto_session_id)).c_str());
    } while (GetLastError() == ERROR_ALREADY_EXISTS);
    dprintf("Allocated session id: %d\n", xauto_session_id);
}

XAuto::XAutoServer::XAutoServer() {
    acquire_session();
    std::thread(std::bind(&XAutoServer::xauto_srv_thread, this)).detach();
}
