#include "core/log.h"
#include "core/memory.h"
#include "hw/aica/aica.h"
#include "hw/arm7/arm7.h"
#include "hw/dreamcast.h"
#include "hw/memory.h"

using namespace re::hw;
using namespace re::hw::aica;
using namespace re::hw::holly;

enum {
  AICA_CLOCK_FREQ = 22579200,
};

namespace re {
namespace hw {
namespace aica {

template <>
uint32_t AICA::ReadWave(uint32_t addr);
}
}
}

AICA::AICA(Dreamcast &dc)
    : Device(dc),
      ExecuteInterface(this),
      MemoryInterface(this),
      dc_(dc),
      sh4_(nullptr),
      arm7_(nullptr),
      aica_regs_(nullptr),
      wave_ram_(nullptr) {}

bool AICA::Init() {
  sh4_ = dc_.sh4;
  arm7_ = dc_.arm7;
  aica_regs_ = dc_.memory->TranslateVirtual(AICA_REG_BEGIN);
  wave_ram_ = dc_.memory->TranslateVirtual(WAVE_RAM_BEGIN);
  common_data_ = reinterpret_cast<CommonData *>(aica_regs_ + 0x2800);

  // start suspended
  arm7_->Suspend();

  return true;
}

void AICA::Run(const std::chrono::nanoseconds &delta) {
  // int64_t cycles = NANO_TO_CYCLES(delta, AICA_CLOCK_FREQ);

  // for (int i = 0; i < 64; i++) {
  // }
}

void AICA::MapPhysicalMemory(Memory &memory, MemoryMap &memmap) {
  RegionHandle aica_reg_handle = memory.AllocRegion(
      AICA_REG_BEGIN, AICA_REG_SIZE,
      make_delegate(&AICA::ReadRegister<uint8_t>, this),
      make_delegate(&AICA::ReadRegister<uint16_t>, this),
      make_delegate(&AICA::ReadRegister<uint32_t>, this), nullptr,
      make_delegate(&AICA::WriteRegister<uint8_t>, this),
      make_delegate(&AICA::WriteRegister<uint16_t>, this),
      make_delegate(&AICA::WriteRegister<uint32_t>, this), nullptr);

  RegionHandle wave_ram_handle = memory.AllocRegion(
      WAVE_RAM_BEGIN, WAVE_RAM_SIZE,
      make_delegate(&AICA::ReadWave<uint8_t>, this),
      make_delegate(&AICA::ReadWave<uint16_t>, this),
      make_delegate(&AICA::ReadWave<uint32_t>, this), nullptr,
      make_delegate(&AICA::WriteWave<uint8_t>, this),
      make_delegate(&AICA::WriteWave<uint16_t>, this),
      make_delegate(&AICA::WriteWave<uint32_t>, this), nullptr);

  memmap.Mount(aica_reg_handle, AICA_REG_SIZE, AICA_REG_BEGIN);
  memmap.Mount(wave_ram_handle, WAVE_RAM_SIZE, WAVE_RAM_BEGIN);
}

template <typename T>
T AICA::ReadRegister(uint32_t addr) {
  return re::load<T>(&aica_regs_[addr]);
}

template <typename T>
void AICA::WriteRegister(uint32_t addr, T value) {
  re::store(&aica_regs_[addr], value);

  switch (addr) {
    case 0x2c00: {  // ARMRST
      if (value) {
        arm7_->Suspend();
      } else {
        arm7_->Resume();
      }
    } break;
  }
}

template <typename T>
T AICA::ReadWave(uint32_t addr) {
  return re::load<T>(&wave_ram_[addr]);
}

namespace re {
namespace hw {
namespace aica {

template <>
uint32_t AICA::ReadWave(uint32_t addr) {
  // FIXME temp hacks to get Crazy Taxi 1 booting
  if (addr == 0x104 || addr == 0x284 || addr == 0x288) {
    return 0x54494e49;
  }
  // FIXME temp hacks to get Crazy Taxi 2 booting
  if (addr == 0x5c) {
    return 0x54494e49;
  }
  // FIXME temp hacks to get PoP booting
  if (addr == 0xb200 || addr == 0xb210 || addr == 0xb220 || addr == 0xb230 ||
      addr == 0xb240 || addr == 0xb250 || addr == 0xb260 || addr == 0xb270 ||
      addr == 0xb280 || addr == 0xb290 || addr == 0xb2a0 || addr == 0xb2b0 ||
      addr == 0xb2c0 || addr == 0xb2d0 || addr == 0xb2e0 || addr == 0xb2f0 ||
      addr == 0xb300 || addr == 0xb310 || addr == 0xb320 || addr == 0xb330 ||
      addr == 0xb340 || addr == 0xb350 || addr == 0xb360 || addr == 0xb370 ||
      addr == 0xb380 || addr == 0xb390 || addr == 0xb3a0 || addr == 0xb3b0 ||
      addr == 0xb3c0 || addr == 0xb3d0 || addr == 0xb3e0 || addr == 0xb3f0) {
    return 0x0;
  }

  return re::load<uint32_t>(&wave_ram_[addr]);
}
}
}
}

template <typename T>
void AICA::WriteWave(uint32_t addr, T value) {
  re::store(&wave_ram_[addr], value);
}

void AICA::UpdateARMInterrupts() {}

void AICA::UpdateSH4Interrupts() {}
