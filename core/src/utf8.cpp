#include "utf8.h"

#include <cstring>

namespace ninegrid {

std::size_t utf8_length(const std::string& text) {
  std::size_t count = 0;
  for (unsigned char ch : text) {
    if ((ch & 0xC0) != 0x80) {
      ++count;
    }
  }
  return count;
}

void copy_utf8(char* dst, std::size_t cap, const std::string& src) {
  if (dst == nullptr || cap == 0) {
    return;
  }
  std::size_t n = src.size();
  if (n > cap - 1) {
    n = cap - 1;
    // Do not cut in the middle of a multi-byte sequence.
    while (n > 0 && (static_cast<unsigned char>(src[n]) & 0xC0) == 0x80) {
      --n;
    }
  }
  std::memcpy(dst, src.data(), n);
  dst[n] = '\0';
}

}  // namespace ninegrid
