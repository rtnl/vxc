#pragma once

#include <kinetic>
#include <memory>
#include <atomic>
#include <thread>
#include <cassert>
#include <condition_variable>

#include "./Proto.hpp"
#include "./Packet.hpp"

namespace vxc {

template <typename IO>
class CraftConnection {
private:
  std::shared_ptr<CraftStream<IO>> _stream;

  std::atomic_bool _flag_alive;

  std::vector<Packet>     _queue_inc;
  std::mutex              _queue_inc_m;
  std::condition_variable _queue_inc_c;

  std::vector<Packet>     _queue_out;
  std::mutex              _queue_out_m;
  std::condition_variable _queue_out_c;

  kinetic::Result<kinetic::Unit> _result;

public:
  explicit CraftConnection(
    const std::shared_ptr<CraftStream<IO>> & stream
  ) noexcept
    : _stream(stream)
    , _flag_alive(false)
    , _queue_inc()
    , _queue_inc_m()
    , _queue_inc_c()
    , _queue_out()
    , _queue_out_m()
    , _queue_out_c()
    , _result(kinetic::Result<kinetic::Unit>::err(kinetic::ErrorKind::ValueNotInitialized, ""))
  {
    assert(stream != nullptr);
  }

  ~CraftConnection() = default;

  bool get_flag_alive() {
    return _flag_alive.load();
  }

  void run() {
    _flag_alive.store(true);

    auto thread_inc = std::thread([this]() {
      Packet packet_read;

      while (get_flag_alive()) {
        const auto read_r = _stream->read_packet(packet_read);
        if (read_r.is_err()) {
          cancel(read_r.get_error());
          break;
        }

        push_packet_inc(packet_read);
      }
    });
    thread_inc.detach();

    auto thread_out = std::thread([this]() {
      while (get_flag_alive()) {
        for (const auto & packet : get_packet_out()) {
          const auto write_r = _stream->write_packet(packet);
          if (write_r.is_err()) {
            cancel(write_r.get_error());
            return;
          }
        }
      }
    });
    thread_out.detach();
  }

  void cancel(const kinetic::Error err) {
    _result = kinetic::Result<kinetic::Unit>::err(err);
    _flag_alive.store(false);

    _queue_inc_c.notify_all();
    _queue_out_c.notify_all();
  }

  void push_packet_inc(const Packet & packet) {
    std::unique_lock<std::mutex> lock(_queue_inc_m);
    _queue_inc.emplace_back(packet);
    lock.unlock();
    _queue_inc_c.notify_one();
  }

  std::vector<Packet> get_packet_inc(const bool take_empty = false, const usize l = 32) {
    std::unique_lock<std::mutex> lock(_queue_inc_m);
    _queue_inc_c.wait(lock, [this, take_empty]() { return take_empty || !this->_queue_inc.empty(); });

    const std::vector<Packet> result = _queue_inc;
    _queue_inc.clear();

    lock.unlock();
    return result;
  }

  void push_packet_out(const Packet & packet) {
    std::unique_lock<std::mutex> lock(_queue_out_m);
    _queue_out.emplace_back(packet);
    lock.unlock();
    _queue_out_c.notify_one();
  }

  std::vector<Packet> get_packet_out(const bool take_empty = false, const usize l = 32) {
    std::unique_lock<std::mutex> lock(_queue_out_m);
    _queue_out_c.wait(lock, [this, take_empty]() { return take_empty || !this->_queue_out.empty(); });

    const std::vector<Packet> result = _queue_out;
    _queue_out.clear();

    lock.unlock();
    return result;
  }
};

}
