#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _segments_out{}
    , _segments_outstanding{}
    , _nBytes_inflight(0)
    , _recv_ackno(0)
    , _timer{retx_timeout}
    , _window_size(1)
    , _consecutive_retransmissions{0}
    , _stream(capacity)
    , _next_seqno(0)
    , _syn_sent(0)
    , _fin_sent(0) {}

uint64_t TCPSender::bytes_in_flight() const { return _nBytes_inflight; }

void TCPSender::fill_window() {
    TCPSegment seg;
    if(_next_seqno==0){
        seg.header().syn=1;
        _syn_sent=1;
        send_non_empty_segment(seg);
    }else if (_next_seqno == _nBytes_inflight) {
        // state is SYN SENT, don't send SYN
        return;
    }

    //send multiple non-empty segments
    uint16_t win = _window_size;
    if (_window_size == 0)
        win = 1;  // zero window probing

    uint64_t remaining;
    while ((remaining = static_cast<uint64_t>(win) + (_recv_ackno - _next_seqno))){
        // FIN flag occupies space in window
        TCPSegment newseg;
        if (_stream.eof() && !_fin_sent) {
            newseg.header().fin = 1;
            _fin_sent = 1;
            send_non_empty_segment(newseg);
            return;
        } else if (_stream.eof())
            return;
        else {  // SYN_ACKED
            size_t size = min(static_cast<size_t>(remaining), TCPConfig::MAX_PAYLOAD_SIZE);
            newseg.payload() = Buffer(std::move(_stream.read(size)));
            if (newseg.length_in_sequence_space() < win && _stream.eof()) {  // piggy-back FIN
                newseg.header().fin = 1;
                _fin_sent = 1;
            }
            if (newseg.length_in_sequence_space() == 0)
                return;
            send_non_empty_segment(newseg);
        }
    }
}
void TCPSender::send_non_empty_segment(TCPSegment &seg) {
    seg.header().seqno=wrap(_next_seqno,_isn);
    _next_seqno+=seg.length_in_sequence_space();

    _segments_out.push(seg);
    _segments_outstanding.push(seg);
    _nBytes_inflight+=seg.length_in_sequence_space();

    if(!_timer.open()){
        _timer.start();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    WrappingInt32 ack_no=ackno;
    uint64_t abs_ackno=unwrap(ackno,_isn,_recv_ackno);
    if (ackno - next_seqno() > 0){
        return;
    }
    _window_size=window_size;
    if(abs_ackno<=_recv_ackno) return;
    _recv_ackno=abs_ackno;
    _timer._RTO=_timer._initial_RTO;
    _consecutive_retransmissions=0;

    while(!_segments_outstanding.empty()&&ack_no-_segments_outstanding.front().header().seqno>=static_cast<int32_t>(_segments_outstanding.front().length_in_sequence_space())){
        _nBytes_inflight-=_segments_outstanding.front().length_in_sequence_space();
        _segments_outstanding.pop();
    }

    fill_window();

    if(!_segments_outstanding.empty()){
        _timer.start();
    }else{
        _timer.start();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    size_t _tick=ms_since_last_tick;
    if(_timer.tick(_tick)){
        if(!_segments_outstanding.empty()){
            _segments_out.push(_segments_outstanding.front());
            if(_window_size){
                _consecutive_retransmissions++;
                _timer._RTO*=2;
            }
            if(!_timer.open()) //[RFC6298](5.1)
                _timer.start();
            if (syn_sent() && (_next_seqno == _nBytes_inflight)){
                //SYN_SENT, [RFC6298](5.7)
                if(_timer._RTO < _timer._initial_RTO){
                    _timer._RTO = _timer._initial_RTO;
                }
            }
        }else{
            _timer.close();
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {}
