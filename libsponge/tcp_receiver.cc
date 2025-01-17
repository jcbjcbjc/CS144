#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if(isn_==nullopt&&!seg.header().syn){
        return;
    }
    if (seg.header().syn&&!isn_) {
        isn_ = seg.header().seqno;
    }
    //Seq->AboSeq
    int64_t abo_seqno = unwrap(seg.header().seqno + static_cast<int>(seg.header().syn), isn_.value(), this->_reassembler.first_unassembled());

    _reassembler.push_substring(seg.payload().copy(), abo_seqno - 1, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    // ISN还未设置
    if (isn_ == nullopt)
        return nullopt;
    // Stream Indices->AboSeq
    uint64_t written = stream_out().bytes_written() + 1;
    if (stream_out().input_ended()) {
        written++;
    }
    // AboSeq->Seq
    return wrap(written, isn_.value());
}

size_t TCPReceiver::window_size() const { return this->stream_out().remaining_capacity(); }
