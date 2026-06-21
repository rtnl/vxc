#pragma once

#include <./Packet.hpp>

#include <cstdlib>
#include <kinetic>
#include <memory>

// varint and varlong methods from https://minecraft.wiki/w/Java_Edition_protocol/Packets

namespace vxc {

template <typename IO>
class CraftStream {
private:
  std::shared_ptr<IO> _io;

  static const i32 SEGMENT_BITS = 0x7F;
  static const i32 CONTINUE_BIT = 0x80;

public:
  CraftStream(const std::shared_ptr<IO> & io)
    : _io(io)
  {}

  ~CraftStream() = default;

  kinetic::Result<i32> read_varint() {
    using ResultT = kinetic::Result<i32>;

    i32 value = 0;
    i32 position = 0;
    u8 currentByte;

    while (true) {
      const auto read_r = _io->read_exact(&currentByte, 1);
      if (read_r.is_err()) {
        return ResultT::err(read_r.get_error());
      }

      value |= (currentByte & SEGMENT_BITS) << position;

      if ((currentByte & CONTINUE_BIT) == 0) break;

      position += 7;

      if (position >= 32) {
        return ResultT::err(kinetic::ErrorKind::ValueInvalid, "VarInt is too large");
      }
    }

    return ResultT::ok(value);
  }

  kinetic::Result<i64> read_varlong() {
    using ResultT = kinetic::Result<i64>;

    i64 value = 0;
    i32 position = 0;
    u8 currentByte;

    while (true) {
      const auto read_r = _io->read_exact(&currentByte, 1);
      if (read_r.is_err()) {
        return ResultT::err(read_r.get_error());
      }

      value |= (long) (currentByte & SEGMENT_BITS) << position;

      if ((currentByte & CONTINUE_BIT) == 0) break;

      position += 7;

      if (position >= 64) {
        return ResultT::err(kinetic::ErrorKind::ValueInvalid, "VarLong is too large");
      }
    }

    return ResultT::ok(value);
  }

  kinetic::Result<usize> write_varint(i32 value) {
    while (true) {
      if ((value & ~SEGMENT_BITS) == 0) {
        const u8 value_byte = value;
        return _io->write_all(&value_byte, 1);
      }

      const u8 value_byte = (value & SEGMENT_BITS) | CONTINUE_BIT;
      const auto res = _io->write_all(&value_byte, 1);
      if (res.is_err()) {
        return res;
      }

      value >>= 7;
    }
  }

  kinetic::Result<usize> write_varlong(i64 value) {
    while (true) {
      if ((value & ~((i64)SEGMENT_BITS)) == 0) {
        const u8 value_byte = value;
        return _io->write_all(&value_byte, 1);
      }

      const u8 value_byte = (value & SEGMENT_BITS) | CONTINUE_BIT;
      const auto res = _io->write_all(&value_byte, 1);
      if (res.is_err()) {
        return res;
      }

      value >>= 7;
    }
  }

  kinetic::Result<u8> read_packet(Packet & packet) noexcept {
    using ResultT = kinetic::Result<u8>;

    const auto p_len_r = read_varint();
    if (p_len_r.is_err()) {
      return ResultT::err(p_len_r.get_error());
    }
    const auto p_len_v = p_len_r.unwrap();
    packet.set_len(usize(p_len_v));
    const auto p_data_r = _io->read_exact(packet.get_data().data(), p_len_v);
    if (p_data_r.is_err()) {
      return ResultT::err(p_data_r.get_error());
    }

    return ResultT::ok(1);
  }

  kinetic::Result<usize> write_packet(const Packet & packet) noexcept {
    using ResultT = kinetic::Result<usize>;

    const i32 packet_len = packet.get_len();

    const auto packet_len_r = write_varint(packet_len);
    if (packet_len_r.is_err()) {
      return packet_len_r;
    }

    const auto packet_body_r = _io->write_all(packet.get_data().data(), packet_len);
    if (packet_body_r.is_err()) {
      return packet_body_r;
    }

    return ResultT::ok(0);
  }
};

}
