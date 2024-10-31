/* -*- c++ -*- */
/*
 * Copyright 2013,2014,2019 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "router_msg_sink_impl.h"
#include "tag_headers.h"
#include <gnuradio/io_signature.h>
#include <memory>
#include <thread>

namespace gr {
namespace zeromq {

router_msg_sink::sptr router_msg_sink::make(char* address, int timeout, int linger, bool bind)
{
    return gnuradio::make_block_sptr<router_msg_sink_impl>(address, timeout, linger, bind);
}

router_msg_sink_impl::router_msg_sink_impl(char* address, int timeout, int linger, bool bind)
    : gr::block("router_msg_sink",
                gr::io_signature::make(0, 0, 0),
                gr::io_signature::make(0, 0, 0)),
      d_timeout(timeout),
      d_context(1),
      d_socket(d_context, ZMQ_ROUTER),
      d_bound(bind),
      d_port(pmt::mp("in")),
      d_terminated(0),
      d_addr(address),
      d_connections(0)
{
    int major, minor, patch;
    zmq::version(&major, &minor, &patch);

    if (major < 3) {
        d_timeout = timeout * 1000;
    }

    int time = linger;
#if USE_NEW_CPPZMQ_SET_GET
    d_socket.set(zmq::sockopt::linger, time);
#else
    d_socket.setsockopt(ZMQ_LINGER, &time, sizeof(time));
#endif

    if (bind) {
        std::string ident(address);
#if USE_NEW_CPPZMQ_SET_GET
        d_socket.set(zmq::sockopt::identiy, ident.c_str(), ident.size());
#else
        d_socket.setsockopt(ZMQ_IDENTITY, ident.c_str(), ident.size());
#endif
        d_socket.bind(address);
    } else {
        d_socket.connect(address);
    }

    message_port_register_in(d_port);
}

router_msg_sink_impl::~router_msg_sink_impl()
{
    teardown();
}

bool router_msg_sink_impl::start()
{
    if (d_terminated == 1) return true;
    d_finished = false;
    d_thread = std::make_unique<std::thread>([this] { readloop(); });
    return true;
}

bool router_msg_sink_impl::stop()
{
    if (d_terminated == 1) return true;
    d_finished = true;
    if (d_thread)
        if (d_thread->joinable())
            d_thread->join();
    return true;
}

bool router_msg_sink_impl::beacon(std::string dest)
{
    std::string ping = "ping";
    zmq::message_t zdest(dest.size());
    memcpy(zdest.data(), dest.c_str(), dest.size());
    zmq::message_t zmsg(ping.size());
    memcpy(zmsg.data(), ping.c_str(), ping.size());
#if USE_NEW_CPPZMQ_SEND_RECV
    d_socket.send(zdest, zmq::send_flags::sndmore);
    d_socket.send(zmq::message_t(""), zmq::send_flags::sndmore);
    d_socket.send(zmsg, zmq::send_flags::none);
#else
    d_socket.send(zdest, ZMQ_SNDMORE);
    d_socket.send("", ZMQ_SNDMORE);
    d_socket.send(zmsg);
#endif
    
    zmq::pollitem_t items[] = {
        { static_cast<void*>(d_socket), 0, ZMQ_POLLIN, 0 }
    };
    zmq::poll(&items[0], 1, std::chrono::milliseconds{ d_timeout });
    if (items[0].revents & ZMQ_POLLIN) {
        zmq::message_t requester,blank,pong;
        d_socket.recv(requester);
        if (!requester.more()) return false;
        d_socket.recv(blank);
        if (!blank.more()) return false;
        d_socket.recv(pong);
        if (pong.more()) return false;
        std::string good(pong.to_string());
        // std::cout << "['"<<requester.to_string()<<"', '"<<blank.to_string()<<"', '"<<good<<"']\n";

        if( std::find(d_connections.begin(), d_connections.end(), requester.to_string()) == d_connections.end() ){
            if (good == "pong")
                d_connections.push_back(requester.to_string());
        }
        return good == "pong";
    }
    return false;
}

void router_msg_sink_impl::readloop()
{

    while (!d_finished){

        if (!d_bound){
            // std::cout << "I'm unbound!\n";
            while (!d_finished && !beacon(d_addr)){}
            d_bound = true;
            // std::cout << "I'm bound now\n";
        }

        // while we have data, wait for query...
        while (!empty_p(d_port) && !d_finished) {

            // wait for query...
            zmq::pollitem_t items[] = {
                { static_cast<void*>(d_socket), 0, ZMQ_POLLIN | ZMQ_POLLOUT, 0 }
            };
            zmq::poll(&items[0], 1, std::chrono::milliseconds{ d_timeout });

            //  If we got a reply, process
            if (items[0].revents & ZMQ_POLLIN) {

                // receive data request
                zmq::message_t requester;
                zmq::message_t blank;
                zmq::message_t request;
#if USE_NEW_CPPZMQ_SEND_RECV
                const bool ok = bool(d_socket.recv(requester));
#else
                const bool ok = d_socket.recv(&requester);
#endif
                if (!ok || !requester.more()) {
                    // Should not happen, we've checked POLLIN.
                    d_logger->error("Failed to receive message.");
                    break; // Fall back to re-check d_finished
                }
#if USE_NEW_CPPZMQ_SEND_RECV
                const bool ok2 = bool(d_socket.recv(blank));
#else
                const bool ok2 = d_socket.recv(&blank);
#endif
                if (!ok2 || !blank.more()) {
                    // Should not happen, we've checked POLLIN.
                    d_logger->error("Failed to receive message.");
                    break; // Fall back to re-check d_finished
                }
#if USE_NEW_CPPZMQ_SEND_RECV
                const bool ok3 = bool(d_socket.recv(request));
#else
                const bool ok3 = d_socket.recv(&request);
#endif
                if (!ok3 || request.more()) {
                    // Should not happen, we've checked POLLIN.
                    d_logger->error("Failed to receive message.");
                    break; // Fall back to re-check d_finished
                }

                std::string req = request.to_string();
                // std::cout << "['"<<requester.to_string()<<"', '"<<blank.to_string()<<"', '"<<req<<"']\n";
                if (req != "pong"){

                    int req_output_items = *(static_cast<int*>(request.data()));
                    if (req_output_items != 1)
                        throw std::runtime_error(
                            "Request was not 1 msg for rep/req request!!");

                    if( std::find(d_connections.begin(), d_connections.end(), requester.to_string()) == d_connections.end() ){
                        d_connections.push_back(requester.to_string());
                    }
                }
            }
            
            //  If we got a reply, process
            if (items[0].revents & ZMQ_POLLOUT) {
                // create message copy and send
                pmt::pmt_t msg = delete_head_nowait(d_port);
                zmq::message_t zdest(d_connections[0].size());
                memcpy(zdest.data(), d_connections[0].c_str(), d_connections[0].size());
                std::stringbuf sb("");
                pmt::serialize(msg, sb);
                std::string s = sb.str();
                zmq::message_t zmsg(s.size());
                memcpy(zmsg.data(), s.c_str(), s.size());
#if USE_NEW_CPPZMQ_SEND_RECV
                d_socket.send(zdest, zmq::send_flags::sndmore);
                d_socket.send(zmq::message_t(""), zmq::send_flags::sndmore);
                d_socket.send(zmsg, zmq::send_flags::none);
#else
                d_socket.send(zdest, ZMQ_SNDMORE);
                d_socket.send(zmq::message_t(""), ZMQ_SNDMORE);
                d_socket.send(zmsg);
#endif
            }
        }     // while !empty

    } // while !d_finished
}

void router_msg_sink_impl::teardown()
{
    if(d_terminated == 0){
        stop();
        d_context.shutdown();
        d_socket.close();
        d_context.close();
        d_terminated = 1;
    }
}

} /* namespace zeromq */
} /* namespace gr */
