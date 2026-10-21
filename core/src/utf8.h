#ifndef NINEGRID_UTF8_H
#define NINEGRID_UTF8_H

#include <cstddef>
#include <string>

namespace ninegrid {

// Number of Unicode code points in a UTF-8 string (invalid bytes count as one).
std::size_t utf8_length(const std::string& text);

// Copy at most cap-1 bytes, never splitting a UTF-8 sequence.
void copy_utf8(char* dst, std::size_t cap, const std::string& src);

}  // namespace ninegrid

#endif  // NINEGRID_UTF8_H
