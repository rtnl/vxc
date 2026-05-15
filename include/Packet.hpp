#pragma once

#include <kinetic>
#include <sstream>

namespace vxc {

class Packet {
private:
  usize _len;

  usize _kind;

  std::vector<u8> _data;

public:
  Packet()
    : _len(0)
    , _kind(0)
    , _data()
  {}

  Packet(usize len, usize kind)
    : _len(len)
    , _kind(kind)
    , _data()
  {}

  KINETIC_GETTER(_len, len)

  KINETIC_GETTER(_kind, kind)

  KINETIC_GETTER(_data, data)

  void push_data(const std::vector<u8> & in) {
    _data.reserve(in.size());
    _data.insert(_data.begin(), in.begin(), in.end());
  }
};


}

std::string Debug(const vxc::Packet & packet) {
  std::ostringstream ss;

  ss << "Packet(";
  ss << packet.get_len() << " ";
  ss << packet.get_kind();
  ss << ")";

  return ss.str();
}
