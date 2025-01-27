/* -*- c++ -*- */
/*
 * Copyright 2013,2014 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "pub_msg_sink_impl.h"
#include "tag_headers.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace zeromq {

pub_msg_sink::sptr pub_msg_sink::make(char* address, int timeout, int linger, bool bind, std::string pkt_filter)
{
    return gnuradio::make_block_sptr<pub_msg_sink_impl>(address, timeout, linger, bind, pkt_filter);
}

pub_msg_sink_impl::pub_msg_sink_impl(char* address, int timeout, int linger, bool bind, std::string pkt_filter)
    : gr::block("pub_msg_sink",
                gr::io_signature::make(0, 0, 0),
                gr::io_signature::make(0, 0, 0)),
      d_timeout(timeout),
      d_context(1),
      d_socket(d_context, ZMQ_PUB),
      d_pkt_filter(pkt_filter),
      d_terminated(0)
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
        d_socket.bind(address);
    } else {
        d_socket.connect(address);
    }

    message_port_register_in(pmt::mp("in"));
    set_msg_handler(pmt::mp("in"), [this](pmt::pmt_t msg) { this->handler(msg); });
}

pub_msg_sink_impl::~pub_msg_sink_impl()
{
    teardown();
}

void pub_msg_sink_impl::handler(pmt::pmt_t msg)
{
    if (d_terminated) return;
    std::stringbuf sb("");
    pmt::serialize(msg, sb);
    std::string s = sb.str();
    zmq::message_t zmsg(s.size());
    zmq::message_t pub_filter(d_pkt_filter.size());

    memcpy(zmsg.data(), s.c_str(), s.size());
    memcpy(pub_filter.data(), d_pkt_filter.c_str(), d_pkt_filter.size());
#if USE_NEW_CPPZMQ_SEND_RECV
    while(!d_socket.send(pub_filter, zmq::send_flags::sndmore));
    while(!d_socket.send(zmsg, zmq::send_flags::none));
#else
    while(!d_socket.send(pub_filter, ZMQ_SNDMORE));
    while(!d_socket.send(zmsg));
#endif
}
void pub_msg_sink_impl::teardown()
{
    if (d_terminated == 0){
        d_context.shutdown();
        d_socket.close();
        d_context.close();
        d_terminated = 1;
    }
}

} /* namespace zeromq */
} /* namespace gr */
