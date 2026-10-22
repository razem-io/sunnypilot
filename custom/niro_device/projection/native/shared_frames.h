#pragma once
#include <cstdint>
namespace SharedFrames {
constexpr char path[]="/data/local/niro-projection/shared-frames.sock";
constexpr uint32_t magic=0x41414750;
constexpr unsigned count=7;
struct Pool {uint32_t magicValue=magic,buffers=count,width=1280,height=720,stride=1280,scanlines=736,bytes=0;};
struct Packet {uint32_t kind=0,index=0;uint64_t timestampUs=0;};
constexpr unsigned frame=1,release=2;
}
