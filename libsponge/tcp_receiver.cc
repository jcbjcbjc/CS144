#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if(seg.header().syn&& !isn.has_value()){
        isn=seg.header().seqno;
    }
    if(seg.header().fin){
        fin = true;
    }
    if(isn.has_value()){
        _reassembler.push_substring(
            seg.payload().copy(),
            unwrap(seg.header().seqno,isn.value(),_reassembler.first_unassembled()),
            seg.header().fin);

        ack_no=wrap(_reassembler.first_unassembled(),isn.value());
    }
    /*这个判断只会进入一次,(!syn)保证了这个判断只会执行一次*/
    if (!syn && seg.header().syn && ack_no.has_value()) {
        /*这里的代码的主要目的是将Init Sequence Number设置为带Payload的第一个Sequence Number*/
        ack_no = WrappingInt32(ack_no.value().raw_value() + 1);
        isn = ack_no;
        syn = true;
    }
    if (isn.has_value() && fin && _reassembler.stream_out().input_ended()) {
        ack_no = WrappingInt32(ack_no.value().raw_value() + 1);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { return ack_no; }

size_t TCPReceiver::window_size() const { return this->stream_out().remaining_capacity(); }
