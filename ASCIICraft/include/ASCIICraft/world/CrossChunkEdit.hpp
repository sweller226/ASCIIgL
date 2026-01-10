#include <ASCIICraft/world/Block.hpp>

#include <vector>

#include <ASCIICraft/util/Util.hpp>

constexpr int META_BUCKET_TIME_LIMIT = 300;

struct CrossChunkEdit {
  uint16_t packedPos;   // 4 bits x, 4 bits y, 4 bits z, 4 reserved (example)
  Block block;

  void PackPos(int x, int y, int z) { 
    packedPos = (static_cast<uint16_t>(x) & 0xF) | ((static_cast<uint16_t>(y) & 0xF) << 4) | ((static_cast<uint16_t>(z) & 0xF) << 8);
  }

  void UnpackPos(int& x, int& y, int& z) {
    x =  packedPos        & 0xF;      // bits 0–3
    y = (packedPos >> 4)  & 0xF;      // bits 4–7
    z = (packedPos >> 8)  & 0xF;      // bits 8–11
  }
};

struct MetaBucket {
  MetaBucket() {
    edits = {};
    lastTouched = NowSeconds();
  }
  std::vector<CrossChunkEdit> edits; // small-vector optimized
  uint32_t lastTouched; // timer so we know when to cache the meta bucket, I might just make it 5 minutes (300s)
};