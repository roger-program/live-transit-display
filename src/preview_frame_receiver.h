#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace preview {
inline bool copyRawFrameChunk(uint8_t *destination, size_t capacity, size_t totalSize,
                              size_t currentSize, const uint8_t *chunk, size_t &received) {
  if (totalSize > capacity || currentSize > totalSize)
    return false;

  const size_t offset = totalSize - currentSize;
  if (currentSize > capacity - offset)
    return false;

  std::memcpy(destination + offset, chunk, currentSize);
  received = totalSize;
  return true;
}
} // namespace preview
