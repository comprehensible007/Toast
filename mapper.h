#pragma once
#include "types.h"
#include "SaveStateIO.h"
#include <vector>
#include <cstdint>
#include <cstdio>

enum class Mirroring { HORIZONTAL, VERTICAL, FOUR_SCREEN, SINGLE_SCREEN_LOW, SINGLE_SCREEN_HIGH };

class Mapper
{
public:
    Mapper(std::vector<u8> prg, std::vector<u8> chr, bool chrIsRAM_, Mirroring headerMirroring_)
        : prgROM(std::move(prg)), chrROM(std::move(chr)), chrIsRAM(chrIsRAM_),
          headerMirroring(headerMirroring_), mirroring(headerMirroring_)
    {
        prgRAM.resize(8192, 0);
    }
    virtual ~Mapper() = default;

    virtual bool CpuRead(u16 addr, u8& data) = 0;
    virtual bool CpuWrite(u16 addr, u8 data) = 0;
    virtual bool PpuRead(u16 addr, u8& data) = 0;
    virtual bool PpuWrite(u16 addr, u8 data) = 0;

    virtual Mirroring GetMirroring() const { return mirroring; }

    virtual void OnScanline() {}
    virtual bool IRQState() const { return false; }
    virtual void IRQClear() {}
    virtual void DebugDump(FILE* f) const {}
    virtual void SetTracing(bool /*on*/) {}

    virtual void SaveState(StateWriter& w) const
    {
        w.U8((u8)mirroring);
        w.Bytes(prgRAM.data(), prgRAM.size());
        if (chrIsRAM) w.Bytes(chrROM.data(), chrROM.size());
    }

    virtual void LoadState(StateReader& r)
    {
        mirroring = (Mirroring)r.U8();
        r.Bytes(prgRAM.data(), prgRAM.size());
        if (chrIsRAM) r.Bytes(chrROM.data(), chrROM.size());
    }

protected:
    std::vector<u8> prgROM;
    std::vector<u8> chrROM;
    std::vector<u8> prgRAM;
    bool chrIsRAM;
    Mirroring headerMirroring;
    Mirroring mirroring;

    u32 Prg16kCount() const { return (u32)(prgROM.size() / 16384); }
    u32 Prg8kCount() const { return (u32)(prgROM.size() / 8192); }
    u32 Prg32kCount() const { return (u32)(prgROM.size() / 32768); }
    u32 Chr4kCount() const { return (u32)(chrROM.size() / 4096); }
    u32 Chr8kCount() const { return (u32)(chrROM.size() / 8192); }
    u32 Chr1kCount() const { return (u32)(chrROM.size() / 1024); }

    u8 PrgByte(u32 addr) const { return addr < prgROM.size() ? prgROM[addr] : 0; }
    u8 ChrByte(u32 addr) const { return addr < chrROM.size() ? chrROM[addr] : 0; }
};
