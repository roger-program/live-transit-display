#include "preview_frame_receiver.h"
#include <array>
#include <unity.h>

void setUp() {}
void tearDown() {}

namespace {
void rgb888_frame_is_copied_from_cumulative_chunk_sizes() {
  constexpr size_t FrameBytes = 64 * 32 * 3;
  constexpr size_t ChunkBytes = 1436;
  std::array<uint8_t, FrameBytes> source{};
  std::array<uint8_t, FrameBytes> destination{};
  for (size_t i = 0; i < source.size(); ++i)
    source[i] = static_cast<uint8_t>((i * 37 + 11) & 0xff);

  size_t received = 0;
  for (size_t offset = 0; offset < FrameBytes;) {
    const size_t currentSize = (FrameBytes - offset < ChunkBytes) ? FrameBytes - offset : ChunkBytes;
    const size_t totalSize = offset + currentSize; // WebServer updates this before RAW_WRITE.
    TEST_ASSERT_TRUE(preview::copyRawFrameChunk(destination.data(), destination.size(), totalSize,
                                                currentSize, source.data() + offset, received));
    TEST_ASSERT_EQUAL_UINT(totalSize, received);
    offset += currentSize;
  }

  TEST_ASSERT_EQUAL_UINT(FrameBytes, received);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(source.data(), destination.data(), FrameBytes);
}
} // namespace

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(rgb888_frame_is_copied_from_cumulative_chunk_sizes);
  return UNITY_END();
}
