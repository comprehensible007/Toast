#pragma once
#include "SaveStateIO.h"
#include "types.h"
#include "cartridge.h"
#include <array>
#include <cstring>

class Ppu2C02
{
public:
    Ppu2C02();

    std::array<u32, 256 * 240> screen{};
    bool frameComplete = false;

    void ConnectCartridge(Cartridge* c) { cart = c; }
    void Reset();
    void Clock();

    u8   CpuRead(u16 addr);
    void CpuWrite(u16 addr, u8 data);

    bool nmiRequested = false;

    u8 oam[256]{};
    u8 oamAddr = 0;

    void SaveState(StateWriter& w) const;
    void LoadState(StateReader& r);
    
    int GetScanline() const { return scanline; }
    int GetCycle() const { return cycle; }

private:
    struct SpriteEntry
    {
        u8 y = 0xFF, tile = 0xFF, attr = 0xFF, x = 0xFF;
    };

    Cartridge* cart = nullptr;

    u8 nameTable[2][1024]{};
    u8 paletteRAM[32]{};

    u8 ctrl = 0;
    u8 mask = 0;
    u8 status = 0;

    u16 vramAddr = 0;
    u16 tramAddr = 0;
    u8  fineX = 0;
    bool addrLatch = false;

    u8 dataBuffer = 0;

    int scanline = -1;
    int cycle = 0;
    bool oddFrame = false;

    u8 bgNextTileId = 0, bgNextTileAttrib = 0, bgNextTileLsb = 0, bgNextTileMsb = 0;
    u16 bgShiftPatternLo = 0, bgShiftPatternHi = 0;
    u16 bgShiftAttribLo = 0, bgShiftAttribHi = 0;

    SpriteEntry spriteScanline[8];
    int spriteCount = 0;
    u8 spriteShifterLo[8]{};
    u8 spriteShifterHi[8]{};
    bool spriteZeroHitPossible = false;
    bool renderingWasEnabled = false;

    u16 ppuAddressBus = 0;
    u8 prevA12 = 0;
    long long globalDotCounter = 0;
    long long lowStartDot = 0;

    u32 palette[64];
    void InitPaletteTable();

    u8 PpuRead(u16 addr) const;
    void PpuWrite(u16 addr, u8 data);
    u8 GetPalette(u8 palette, u8 pixel);
    u8 PpuReadFetch(u16 addr);

    void IncScrollX();
    void IncScrollY();
    void TransferAddressX();
    void TransferAddressY();
    void LoadBgShifters();
    void UpdateShifters();
    void EvaluateSprites();
    void FetchSpritePatternByte(int slot, bool high);
    void TouchDummySpriteFetch();
    void ComputePixel(int sl, int x);
};
