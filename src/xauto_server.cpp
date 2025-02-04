#include "pluginmain.h"
#include "xauto_server.h"
#include "xauto_cmd.h"

#include <thread>


using namespace msgpack;


constexpr int DISPATCH_CONTINUE = 0;
constexpr int DISPATCH_EXIT = -1;


int XAutoServer::_dispatch_cmd(msgpack::object root, msgpack::sbuffer& response_buffer) {
    if (root.type == msgpack::type::STR) {
        std::string str;
        root.convert(str);
        if (str == "PING") {
            msgpack::pack(response_buffer, "PONG");
        }
    } else if (root.type == msgpack::type::ARRAY && root.via.array.size > 0 && root.via.array.ptr[0].type == msgpack::type::STR) {
        std::string cmd;
        root.via.array.ptr[0].convert(cmd);
        if (cmd == XAUTO_REQ_DEBUGGER_PID) {
            get_debugger_pid(response_buffer);
        } else if (cmd == XAUTO_REQ_COMPAT_VERSION) {
            get_compat_v(response_buffer);
        } else if (cmd == XAUTO_REQ_DEBUGGER_VERSION) {
            get_debugger_version(response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_EVAL) {
            dbg_eval(root, response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_CMD_EXEC_DIRECT) {
            dbg_cmd_exec_direct(root, response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_IS_RUNNING) {
            dbg_is_running(response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_IS_DEBUGGING) {
            dbg_is_debugging(response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_IS_ELEVATED) {
            dbg_is_elevated(response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_MEMMAP) {
            dbg_memmap(response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_GET_BITNESS) {
            dbg_get_bitness(response_buffer);
        } else if (cmd == XAUTO_REQ_GUI_REFRESH_VIEWS) {
            gui_refresh_views(response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_READ_MEMORY) {
            dbg_read_memory(root, response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_WRITE_MEMORY) {
            dbg_write_memory(root, response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_READ_REGISTERS) {
            dbg_read_regs(response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_READ_SETTING_SZ) {
            dbg_read_setting_sz(root, response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_WRITE_SETTING_SZ) {
            dbg_write_setting_sz(root, response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_READ_SETTING_UINT) {
            dbg_read_setting_uint(root, response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_WRITE_SETTING_UINT) {
            dbg_write_setting_uint(root, response_buffer);
        } else if (cmd == XAUTO_REQ_DBG_IS_VALID_READ_PTR) {
            dbg_is_valid_read_ptr(root, response_buffer);
        } else if (cmd == XAUTO_REQ_DISASSEMBLE) {
            disassemble_at(root, response_buffer);
        } else if (cmd == XAUTO_REQ_ASSEMBLE) {
            assemble_at(root, response_buffer);
        } else if (cmd == XAUTO_REQ_GET_BREAKPOINTS) {
            get_breakpoints(root, response_buffer);
        } else if (cmd == XAUTO_REQ_GET_LABEL) {
            get_label_at(root, response_buffer);
        } else if (cmd == XAUTO_REQ_GET_COMMENT) {
            get_comment_at(root, response_buffer);
        } else if (cmd == XAUTO_REQ_GET_SYMBOL) {
            get_symbol_at(root, response_buffer);
        } else if (cmd == XAUTO_REQ_QUIT) {
            msgpack::pack(response_buffer, "OK_QUITTING");
            return DISPATCH_EXIT;
        }
    }

    return DISPATCH_CONTINUE;
}


void XAutoServer::xauto_srv_req_rep_thread() {
    zmq::socket_t socket{context, zmq::socket_type::rep};
    socket.bind(("tcp://localhost:" + std::to_string(SESS_REQ_REP_PORT)).c_str());
    dprintf("Allocated REQ/REP port: %d\n", SESS_REQ_REP_PORT);

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

void XAutoServer::acquire_session() {
    hMutex = INVALID_HANDLE_VALUE;

    while (hMutex == INVALID_HANDLE_VALUE) {
        xauto_session_id++;
        const char* mutex_name = ("x64dbg_automate_mutex_s_" + std::to_string(xauto_session_id)).c_str();

        HANDLE hTryMutex = OpenMutexA(SYNCHRONIZE, FALSE, mutex_name);
        if (hTryMutex != NULL) {
            // CloseHandle(hTryMutex);
            continue;
        }

        hMutex = CreateMutexA(NULL, TRUE, mutex_name);
    }
    dprintf("Allocated session id: %d\n", xauto_session_id);
}

XAutoServer::XAutoServer() {
    context = zmq::context_t(1);
    acquire_session();

    // Request-Reply Sock
    std::thread(std::bind(&XAutoServer::xauto_srv_req_rep_thread, this)).detach();

    // Pub-Sub Sock
    pub_sock = zmq::socket_t(context, zmq::socket_type::pub);
    pub_sock.bind(("tcp://localhost:" + std::to_string(SESS_PUB_SUB_PORT)).c_str());
    dprintf("Allocated PUB/SUB port: %d\n", SESS_PUB_SUB_PORT);
}
