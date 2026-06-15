#pragma once

#include <kinetic>
#include <sstream>

namespace vxc {

class Packet {
private:
  usize _len;

  std::vector<u8> _data;

public:
  explicit Packet()
    : _len(0)
    , _data(1)
  {}

  KINETIC_GETTER(_len, len)

  void set_len(const usize len) {
    while (len > _data.capacity()) {
      _data.resize(_data.capacity() * 2);
    }

    _len = len;
  }

  std::vector<u8> & get_data() {
    return _data;
  }

  const std::vector<u8> & get_data() const {
    return _data;
  }
};

}

static std::string Debug(const vxc::Packet & packet) {
  std::ostringstream ss;

  ss << "Packet(";
  ss << ")";

  return ss.str();
}
