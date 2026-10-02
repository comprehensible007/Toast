#pragma once
#include "types.h"
#include "mapper.h"
#include <string>
#include <memory>

class Cartridge
{
public:
    bool LoadFromFile(const std::string& path);
    bool IsLoaded() const { return loaded; }
    int  MapperID() const { return mapperID; }
    u32  GetRomCrc() const { return romCrc; }
    Mirroring GetMirroring() const { return mapper ? mapper->GetMirroring() : Mirroring::HORIZONTAL; }

    bool CpuRead(u16 addr, u8& data) { return mapper && mapper->CpuRead(addr, data); }
    bool CpuWrite(u16 addr, u8 data) { return mapper && mapper->CpuWrite(addr, data); }
    bool PpuRead(u16 addr, u8& data) { return mapper && mapper->PpuRead(addr, data); }
    bool PpuWrite(u16 addr, u8 data) { return mapper && mapper->PpuWrite(addr, data); }
    bool NametableRead(u16 addr, u8& data) { return mapper && mapper->NametableRead(addr, data); }
    bool NametableWrite(u16 addr, u8 data) { return mapper && mapper->NametableWrite(addr, data); }
    u8   NtMapOf(int logical) const { return mapper ? mapper->NtMapOf(logical) : (u8)((logical >> 1) & 1); }
    void SetFetchKind(int kind) { if (mapper) mapper->SetFetchKind(kind); }
    void OnPpuScanlineStart(int scanline) { if (mapper) mapper->OnPpuScanlineStart(scanline); }
    void OnPpuVblank() { if (mapper) mapper->OnPpuVblank(); }
    double ExpansionAudio() { return mapper ? mapper->ExpansionAudio() : 0.0; }

    void ScanlineTick() { if (mapper) mapper->OnScanline(); }
    void ClockCpuCycle() { if (mapper) mapper->ClockCpuCycle(); }
    bool IRQState() const { return mapper && mapper->IRQState(); }
    void IRQClear() { if (mapper) mapper->IRQClear(); }

    void SaveState(StateWriter& w) const;
    void LoadState(StateReader& r);

    std::string lastError;

private:
    std::unique_ptr<Mapper> mapper;
    int mapperID = 0;
    u32 romCrc = 0;
    bool loaded = false;
};
