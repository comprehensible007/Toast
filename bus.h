#pragma once
#include "types.h"
#include "cartridge.h"
#include "ppu2C02.h"
#include "apu2A03.h"
#include "SaveStateIO.h"
#include <array>
#include <vector>

class Cpu6502;

class Bus
{
public:
    Bus();
    void ConnectCartridge(Cartridge* c) { cart = c; ppu.ConnectCartridge(c); }

    u8   CpuRead(u16 addr);
    void CpuWrite(u16 addr, u8 data);

    void Reset();
    void Clock(); 
    void SaveState(StateWriter& w) const;
    void LoadState(StateReader& r);

    Ppu2C02 ppu;
    Apu2A03 apu;
    Cartridge* cart = nullptr;

    u8 controllerState[2]{};
    u8 controllerShift[2]{};
    u8 controllerLatch[2]{};

    Cpu6502* cpu = nullptr;
    long long totalCycles = 0;
    struct GameGenieCode
    {
    	u16 addr;
    	u8 value;
    	u8 compare;
    	bool hasCompare;
    };
    std::vector<GameGenieCode> gameGenie;

private:
    std::array<u8, 2048> ram{};
    bool dmaInProgress = false;
    bool dmaWaitAlign = true;
    bool dmaLoggedThisSession = false;
    u8 dmaPage = 0;
    u8 dmaAddr = 0;
    u8 dmaData = 0;
    long long dmaStallCount = 0;
};
