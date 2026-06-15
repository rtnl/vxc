#pragma once

#include <./Packet.hpp>

#include <cstdlib>
#include <kinetic>
#include <memory>

// varint and varlong methods from https://minecraft.wiki/w/Java_Edition_protocol/Packets

namespace vxc {

template <typename IO>
class CraftStream : public kinetic::BufReader, public kinetic::Writer {
private:
  std::shared_ptr<IO> _io;

  static const i32 SEGMENT_BITS = 0x7F;
  static const i32 CONTINUE_BIT = 0x80;

public:
  CraftStream(const std::shared_ptr<IO> & io)
    : _io(io)
  {}

  ~CraftStream() = default;

  kinetic::Result<usize> read(u8 * buf, usize size) override {
    return _io->read(buf, size);
  }

  kinetic::Result<usize> write(const u8 * buf, usize size) override {
    return _io->write(buf, size);
  }

  kinetic::Result<i32> read_varint() {
    using ResultT = kinetic::Result<i32>;

    i32 value = 0;
    i32 position = 0;
    u8 currentByte;

    while (true) {
      const auto read_r = read_exact(&currentByte, 1);
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
      const auto read_r = read_exact(&currentByte, 1);
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
        return write_all(&value_byte, 1);
      }

      const u8 value_byte = (value & SEGMENT_BITS) | CONTINUE_BIT;
      const auto res = write_all(&value_byte, 1);
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
        return write_all(&value_byte, 1);
      }

      const u8 value_byte = (value & SEGMENT_BITS) | CONTINUE_BIT;
      const auto res = write_all(&value_byte, 1);
      if (res.is_err()) {
        return res;
      }

      value >>= 7;
    }
  }

  kinetic::Result<std::vector<rune>> read_string() noexcept {
    using ResultT = kinetic::Result<std::vector<rune>>;

    const auto ErrInvalidLeading      = ResultT::err(kinetic::ErrorKind::ValueInvalid, "invalid utf-8 leading byte");
    const auto ErrInvalidContinuation = ResultT::err(kinetic::ErrorKind::ValueInvalid, "invalid utf-8 continuation byte");
    const auto ErrInvalidRange        = ResultT::err(kinetic::ErrorKind::ValueInvalid, "invalid utf-8 range");

    const auto len_r = read_varint();
    if (len_r.is_err()) {
      return ResultT::err(len_r.get_error());
    }

    const auto len = len_r.unwrap();

    std::vector<rune> data;
    data.reserve(len);

    for (i32 i = 0; i < len; i++) {
      u32 r = 0;
      u8  c = 0;

      const auto c_r = read_exact(&c, 1);
      if (c_r.is_err()) {
        return ResultT::err(c_r.get_error());
      }

      u8 b[3] = {0};

      if      ((c & 0b11111000) == 0b11110000)
      {
        const auto b_r = read_exact(b, 3);
        if (b_r.is_err()) return ResultT::err(b_r.get_error());

        const u32 v_1 = c;
        const u32 v_2 = b[0];
        const u32 v_3 = b[1];
        const u32 v_4 = b[2];

        if ((v_2 & 0b11000000) != 0b10000000) return ErrInvalidContinuation;
        if ((v_3 & 0b11000000) != 0b10000000) return ErrInvalidContinuation;
        if ((v_4 & 0b11000000) != 0b10000000) return ErrInvalidContinuation;

        r |= (v_1 & 0b00000111) << 18;
        r |= (v_2 & 0b00111111) << 12;
        r |= (v_3 & 0b00111111) << 06;
        r |= (v_4 & 0b00111111) << 00;

        if (r < 0x10000 || r > 0x10FFFF) return ErrInvalidRange;
        if (r >= 0xD800 && r <= 0xDFFF)  return ErrInvalidRange;
      }
      else if ((c & 0b11110000) == 0b11100000)
      {
        const auto b_r = read_exact(b, 2);
        if (b_r.is_err()) return ResultT::err(b_r.get_error());

        const u32 v_1 = c;
        const u32 v_2 = b[0];
        const u32 v_3 = b[1];

        if ((v_2 & 0b11000000) != 0b10000000) return ErrInvalidContinuation;
        if ((v_3 & 0b11000000) != 0b10000000) return ErrInvalidContinuation;

        r |= (v_1 & 0b00001111) << 12;
        r |= (v_2 & 0b00111111) << 06;
        r |= (v_3 & 0b00111111) << 00;

        if (r < 0x800 || r > 0xFFFF)    return ErrInvalidRange;
        if (r >= 0xD800 && r <= 0xDFFF) return ErrInvalidRange;
      }
      else if ((c & 0b11100000) == 0b11000000)
      {
        const auto b_r = read_exact(b, 1);
        if (b_r.is_err()) return ResultT::err(b_r.get_error());

        const u32 v_1 = c;
        const u32 v_2 = b[0];

        if ((v_2 & 0b11000000) != 0b10000000) return ErrInvalidContinuation;

        r |= (v_1 & 0b00011111) << 06;
        r |= (v_2 & 0b00111111) << 00;

        if (r < 0x80 || r > 0x7FF)      return ErrInvalidRange;
        if (r >= 0xD800 && r <= 0xDFFF) return ErrInvalidRange;
      }
      else if (c < 0b10000000)
      {
        r = c;
      }
      else
      {
        return ErrInvalidLeading;
      }

      data.emplace_back(rune(r));
    }

    return ResultT::ok(std::move(data));
  }

  kinetic::Result<u8> read_packet(Packet & packet) noexcept {
    using ResultT = kinetic::Result<u8>;

    const auto p_len_r = read_varint();
    if (p_len_r.is_err()) {
      return ResultT::err(p_len_r.get_error());
    }
    const auto p_len_v = p_len_r.unwrap();

    packet.set_len(usize(p_len_v));
    const auto p_data_r = read_exact(packet.get_data().data(), p_len_v);
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

    const auto packet_body_r = write_all(packet.get_data().data(), packet_len);
    if (packet_body_r.is_err()) {
      return packet_body_r;
    }

    return ResultT::ok(0);
  }
};

}
