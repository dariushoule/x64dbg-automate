#include "pluginmain.h"
#include "xauto_server.h"
#include "xauto_cmd.h"

#include <thread>
#include <fstream>
#include <random>


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
    try {
        for (;;) 
        {
            zmq::message_t request;
            msgpack::sbuffer outbuf;

            auto res = rep_socket.recv(request, zmq::recv_flags::none);
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

            rep_socket.send(zmq::buffer(outbuf.data(), outbuf.size()), zmq::send_flags::none);

            if (dispatch_exit == DISPATCH_EXIT) {
                rep_socket.close();
                break;
            }
        }

        dprintf("Caught request to terminate\n");
        GuiCloseApplication();
    } catch (const zmq::error_t& e) {
        dprintf("ZMQ Error: %s\n", e.what());
    }
}


bool XAutoServer::acquire_session() {
    session_pid = GetCurrentProcessId();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0xc000, 0xFFFF);

    rep_socket = zmq::socket_t(context, zmq::socket_type::rep);
    while(true) {
        try {
            sess_req_rep_port = distrib(gen);
            rep_socket.bind(("tcp://localhost:" + std::to_string(sess_req_rep_port)).c_str());
            break;
        } catch (const zmq::error_t& e) {
            dprintf("Failed to bind REQ/REP socket, retrying: %s\n", e.what());
            continue;
        }
    }
    dprintf("Allocated REQ/REP port: %d\n", sess_req_rep_port);

    pub_sock = zmq::socket_t(context, zmq::socket_type::pub);
    while(true) {
        try {
            sess_pub_sub_port = distrib(gen);
            pub_sock.bind(("tcp://localhost:" + std::to_string(sess_pub_sub_port)).c_str());
            break;
        } catch (const zmq::error_t& e) {
            dprintf("Failed to bind PUB/SUB socket, retrying: %s\n", e.what());
            continue;
        }
    }
    dprintf("Allocated PUB/SUB port: %d\n", sess_pub_sub_port);

    std::wstring session_file = get_session_filename(session_pid);
    std::ofstream session_out(session_file);
    if (!session_out.is_open()) {
        dprintf("Failed to open session file: %s\n", session_file.c_str());
        return false;
    }
    session_out << sess_req_rep_port << std::endl << sess_pub_sub_port << std::endl;
    session_out.close();

    dprintf("Allocated session ID: %d\n", session_pid);
    return true;
}


void XAutoServer::release_session() {
    auto sess_filename = get_session_filename(session_pid);
    if (_wremove(sess_filename.c_str()) != 0) {
        dprintf("Failed to release session file: %s\n", sess_filename.c_str());
        return;
    } else {
        dprintf("Culled session ID: %d\n", session_pid);
    }
}


XAutoServer::XAutoServer() {
    context = zmq::context_t(1);
    if(!acquire_session()){
        dprintf("Failed to acquire session, plugin execution cannot continue\n");
        return;
    }

    std::thread(std::bind(&XAutoServer::xauto_srv_req_rep_thread, this)).detach();
}
