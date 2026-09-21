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

    void ScanlineTick() { if (mapper) mapper->OnScanline(); }
    bool IRQState() const { return mapper && mapper->IRQState(); }
    void IRQClear() { if (mapper) mapper->IRQClear(); }
    void DebugDumpMapper(FILE* f) const { if (mapper) mapper->DebugDump(f); }

    void SaveState(StateWriter& w) const;
    void LoadState(StateReader& r);
    void SetTracing(bool on) { if (mapper) mapper->SetTracing(on); }

    std::string lastError;

private:
    std::unique_ptr<Mapper> mapper;
    int mapperID = 0;
    u32 romCrc = 0;
    bool loaded = false;
};
