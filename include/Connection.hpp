#pragma once

#include <kinetic>
#include <memory>
#include <atomic>
#include <thread>
#include <cassert>
#include <condition_variable>

#include "./Proto.hpp"
#include "./Packet.hpp"
#include "./Channel.hpp"

namespace vxc {

template <typename IO>
class CraftConnection {
private:
  std::shared_ptr<CraftStream<IO>> _stream;

  std::atomic_bool _flag_alive;

  std::vector<Packet> _queue_inc;

  std::mutex _queue_inc_m;

  std::condition_variable _queue_inc_c;

  Channel<Packet> _queue_out;

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
    , _queue_out(1024)
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
      while (get_flag_alive()) {
        Packet packet{};

        const auto read_r = _stream->read_packet(packet);
        if (read_r.is_err()) {
          cancel(read_r.get_error());
          return;
        }

        push_packet_inc(packet);
      }
    });
    thread_inc.detach();

    auto thread_out = std::thread([this]() {
      while (get_flag_alive()) {
        const Packet packet = _queue_out.recv();

        const auto write_r = _stream->write_packet(packet);
        if (write_r.is_err()) {
          cancel(write_r.get_error());
          return;
        }
      }
    });
    thread_out.detach();
  }

  void cancel(const kinetic::Error err) {
    _result = kinetic::Result<kinetic::Unit>::err(err);
    _flag_alive.store(false);

    std::cerr << "cancel!" << std::endl;
  }

  void wait() { // Todo: replace with lock
    while (_flag_alive.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
  }

  void push_packet_inc(const Packet & packet) {
    {
      std::lock_guard<std::mutex> lock_queue_inc(_queue_inc_m);
      _queue_inc.emplace_back(packet);
    }
    _queue_inc_c.notify_one();
  }

  std::vector<Packet> get_packet_inc(const usize) {
    std::unique_lock<std::mutex> lock_queue_inc(_queue_inc_m);

    _queue_inc_c.wait(lock_queue_inc);

    const std::vector<Packet> result = _queue_inc;
    _queue_inc.clear();

    lock_queue_inc.unlock();

    return result;
  }

  void push_packet_out(const Packet & packet) {
    _queue_out.send(packet);
  }
};

}
