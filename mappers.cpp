#include "mappers.h"
#include <cstdio>

static FILE* g_mapperLog = nullptr;

class Mapper0 : public Mapper
{
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { data = prgRAM[addr - 0x6000]; return true; }
        u32 mapped = (Prg16kCount() <= 1) ? (addr & 0x3FFF) : (addr - 0x8000);
        data = PrgByte(mapped);
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x6000 && addr < 0x8000) { prgRAM[addr - 0x6000] = data; return true; }
        return false;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        data = ChrByte(addr);
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        if (addr < chrROM.size()) chrROM[addr] = data;
        return true;
    }
};

class Mapper2 : public Mapper
{
    u8 prgBankSelect = 0;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { data = prgRAM[addr - 0x6000]; return true; }
        u32 banks = Prg16kCount();
        u32 bank = (addr < 0xC000) ? (prgBankSelect % banks) : (banks - 1);
        data = PrgByte(bank * 16384 + (addr & 0x3FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x6000 && addr < 0x8000) { prgRAM[addr - 0x6000] = data; return true; }
        if (addr >= 0x8000) { prgBankSelect = data & 0x0F; return true; }
        return false;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        data = ChrByte(addr);
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        if (addr < chrROM.size()) chrROM[addr] = data;
        return true;
    }

    void SaveState(StateWriter& w) const override
    {
        Mapper::SaveState(w);
        w.U8(prgBankSelect);
    }

    void LoadState(StateReader& r) override
    {
        Mapper::LoadState(r);
        prgBankSelect = r.U8();
    }
};

class Mapper3 : public Mapper
{
    u8 chrBankSelect = 0;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { data = prgRAM[addr - 0x6000]; return true; }
        u32 mapped = (Prg16kCount() <= 1) ? (addr & 0x3FFF) : (addr - 0x8000);
        data = PrgByte(mapped);
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x6000 && addr < 0x8000) { prgRAM[addr - 0x6000] = data; return true; }
        if (addr >= 0x8000) { chrBankSelect = data & 0x03; return true; }
        return false;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 banks = Chr8kCount() ? Chr8kCount() : 1;
        data = ChrByte((chrBankSelect % banks) * 8192 + addr);
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 banks = Chr8kCount() ? Chr8kCount() : 1;
        u32 mapped = (chrBankSelect % banks) * 8192 + addr;
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void SaveState(StateWriter& w) const override
    {
        Mapper::SaveState(w);
        w.U8(chrBankSelect);
    }

    void LoadState(StateReader& r) override
    {
        Mapper::LoadState(r);
        chrBankSelect = r.U8();
    }
};

class Mapper7 : public Mapper
{
    u8 prgBankSelect = 0;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x8000) return false;
        u32 banks = Prg32kCount() ? Prg32kCount() : 1;
        u32 bank = prgBankSelect % banks;
        data = PrgByte(bank * 32768 + (addr & 0x7FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        prgBankSelect = data & 0x07;
        mirroring = (data & 0x10) ? Mirroring::SINGLE_SCREEN_HIGH : Mirroring::SINGLE_SCREEN_LOW;
        return true;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        data = ChrByte(addr);
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        if (addr < chrROM.size()) chrROM[addr] = data;
        return true;
    }

    void SaveState(StateWriter& w) const override
    {
        Mapper::SaveState(w);
        w.U8(prgBankSelect);
    }

    void LoadState(StateReader& r) override
    {
        Mapper::LoadState(r);
        prgBankSelect = r.U8();
    }
};

class Mapper66 : public Mapper
{
    u8 prgBankSelect = 0;
    u8 chrBankSelect = 0;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x8000) return false;
        u32 banks = Prg32kCount() ? Prg32kCount() : 1;
        u32 bank = prgBankSelect % banks;
        data = PrgByte(bank * 32768 + (addr & 0x7FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        chrBankSelect = data & 0x03;
        prgBankSelect = (data >> 4) & 0x03;
        return true;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 banks = Chr8kCount() ? Chr8kCount() : 1;
        data = ChrByte((chrBankSelect % banks) * 8192 + addr);
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 banks = Chr8kCount() ? Chr8kCount() : 1;
        u32 mapped = (chrBankSelect % banks) * 8192 + addr;
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void SaveState(StateWriter& w) const override
    {
        Mapper::SaveState(w);
        w.U8(prgBankSelect);
        w.U8(chrBankSelect);
    }

    void LoadState(StateReader& r) override
    {
        Mapper::LoadState(r);
        prgBankSelect = r.U8();
        chrBankSelect = r.U8();
    }
};

class Mapper1 : public Mapper
{
    u8 shiftReg = 0x10;
    u8 control = 0x0C;
    u8 chrBank0 = 0;
    u8 chrBank1 = 0;
    u8 prgBank = 0;

    u32 MapChr(u16 addr) const
    {
        bool chr4k = (control & 0x10) != 0;
        u32 banks4k = Chr4kCount() ? Chr4kCount() : 1;
        if (chr4k)
        {
            u32 bank = (addr < 0x1000) ? (chrBank0 % banks4k) : (chrBank1 % banks4k);
            u32 base = (addr < 0x1000) ? 0 : 0x1000;
            return bank * 4096 + (addr - base);
        }
        u32 banks8k = Chr8kCount() ? Chr8kCount() : 1;
        u32 bank8 = (chrBank0 >> 1) % banks8k;
        return bank8 * 8192 + addr;
    }

public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { data = prgRAM[addr - 0x6000]; return true; }

        u8 prgMode = (control >> 2) & 0x03;
        u32 banks16 = Prg16kCount() ? Prg16kCount() : 1;

        if (prgMode <= 1)
        {
            u32 banks32 = Prg32kCount() ? Prg32kCount() : 1;
            u32 bank32 = (prgBank >> 1) % banks32;
            data = PrgByte(bank32 * 32768 + (addr & 0x7FFF));
            return true;
        }

        u32 bank;
        if (prgMode == 2)
            bank = (addr < 0xC000) ? 0 : (prgBank % banks16);
        else
            bank = (addr < 0xC000) ? (prgBank % banks16) : (banks16 - 1);

        data = PrgByte(bank * 16384 + (addr & 0x3FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { prgRAM[addr - 0x6000] = data; return true; }

        if (data & 0x80)
        {
            shiftReg = 0x10;
            control |= 0x0C;
            return true;
        }

        bool complete = (shiftReg & 1) != 0;
        shiftReg = (shiftReg >> 1) | ((data & 1) << 4);

        if (complete)
        {
            u8 value = shiftReg;
            shiftReg = 0x10;
            if (addr < 0xA000) control = value;
            else if (addr < 0xC000) chrBank0 = value;
            else if (addr < 0xE000) chrBank1 = value;
            else prgBank = value;
        }
        return true;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        data = ChrByte(MapChr(addr));
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 mapped = MapChr(addr);
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    Mirroring GetMirroring() const override
    {
        switch (control & 0x03)
        {
        case 0: return Mirroring::SINGLE_SCREEN_LOW;
        case 1: return Mirroring::SINGLE_SCREEN_HIGH;
        case 2: return Mirroring::VERTICAL;
        default: return Mirroring::HORIZONTAL;
        }
    }

    void SaveState(StateWriter& w) const override
    {
        Mapper::SaveState(w);
        w.U8(shiftReg);
        w.U8(control);
        w.U8(chrBank0);
        w.U8(chrBank1);
        w.U8(prgBank);
    }

    void LoadState(StateReader& r) override
    {
        Mapper::LoadState(r);
        shiftReg = r.U8();
        control = r.U8();
        chrBank0 = r.U8();
        chrBank1 = r.U8();
        prgBank = r.U8();
    }
};

class Mapper4 : public Mapper
{
    u8 bankSelect = 0;
    u8 R[8] = {0,0,0,0,0,0,0,0};
    bool prgRamEnabled = true;
    bool prgRamWriteProtect = false;
    u8 irqLatch = 0;
    u8 irqCounter = 0;
    bool irqReloadFlag = false;
    bool irqEnabled = false;
    bool irqPending = false;
    bool tracing = false;
	int traceWritesLeft = 0;
    FILE* traceFile = nullptr;

    u32 PrgOffset(int slot) const
    {
        u32 banks = Prg8kCount() ? Prg8kCount() : 1;
        bool mode = (bankSelect & 0x40) != 0;
        u32 bank;
        if (slot == 0) bank = mode ? (banks - 2) : (R[6] % banks);
        else if (slot == 1) bank = R[7] % banks;
        else if (slot == 2) bank = mode ? (R[6] % banks) : (banks - 2);
        else bank = banks - 1;
        return bank * 8192;
    }

    u32 ChrOffset(u16 addr) const
    {
        bool inv = (bankSelect & 0x80) != 0;
        u32 banks1k = Chr1kCount() ? Chr1kCount() : 1;
        int region = addr / 0x0400;
        if (inv) region ^= 0x04;

        u32 bank;
        switch (region)
        {
        case 0: bank = (R[0] & 0xFE) % banks1k; break;
        case 1: bank = ((R[0] & 0xFE) + 1) % banks1k; break;
        case 2: bank = (R[1] & 0xFE) % banks1k; break;
        case 3: bank = ((R[1] & 0xFE) + 1) % banks1k; break;
        case 4: bank = R[2] % banks1k; break;
        case 5: bank = R[3] % banks1k; break;
        case 6: bank = R[4] % banks1k; break;
        default: bank = R[5] % banks1k; break;
        }
        return bank * 1024 + (addr & 0x3FF);
    }

public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000)
        {
            data = prgRamEnabled ? prgRAM[addr - 0x6000] : 0;
            return true;
        }
        int slot = (addr - 0x8000) / 0x2000;
        data = PrgByte(PrgOffset(slot) + (addr & 0x1FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000)
        {
            if (prgRamEnabled && !prgRamWriteProtect) prgRAM[addr - 0x6000] = data;
            return true;
        }

        bool even = (addr & 1) == 0;
        if (addr < 0xA000)
{
    if (even) bankSelect = data;
    else R[bankSelect & 0x07] = data;

    if (tracing && traceWritesLeft > 0 && traceFile)
    {
        fprintf(traceFile, "%s addr=%04X data=%02X -> target=%d\n",
                even ? "SELECT" : "DATA  ", addr, data, bankSelect & 0x07);
        fflush(traceFile);
        traceWritesLeft--;
        if (traceWritesLeft == 0) { fclose(traceFile); traceFile = nullptr; tracing = false; }
    }
}
        else if (addr < 0xC000)
        {
            if (even) mirroring = (data & 1) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
            else { prgRamWriteProtect = (data & 0x40) != 0; prgRamEnabled = (data & 0x80) != 0; }
        }
        else if (addr < 0xE000)
        {
            if (even) irqLatch = data;
            else irqReloadFlag = true;
        }
        else
        {
            if (even) { irqEnabled = false; irqPending = false; }
            else irqEnabled = true;
        }
        return true;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        data = ChrByte(ChrOffset(addr));
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 mapped = ChrOffset(addr);
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void OnScanline() override
    {
        if (irqCounter == 0 || irqReloadFlag)
        {
            irqCounter = irqLatch;
            irqReloadFlag = false;
        }
        else
        {
            irqCounter--;
        }
        if (irqCounter == 0 && irqEnabled) irqPending = true;

        if (!g_mapperLog) g_mapperLog = fopen("mmc3_log.txt", "w");
        if (g_mapperLog)
        {
            fprintf(g_mapperLog, "bankSelect=%02X R=%02X %02X %02X %02X %02X %02X %02X %02X latch=%02X counter=%02X reload=%d enabled=%d pending=%d\n",
                bankSelect, R[0], R[1], R[2], R[3], R[4], R[5], R[6], R[7], irqLatch, irqCounter, irqReloadFlag, irqEnabled, irqPending);
            fflush(g_mapperLog);
        }
    }

    bool IRQState() const override { return irqPending; }
    void IRQClear() override { irqPending = false; }

        void DebugDump(FILE* f) const override
    {
        fprintf(f, "MMC3: bankSelect=0x%02X R=[%02X %02X %02X %02X %02X %02X %02X %02X] mirroring=%d prgRamEnabled=%d irqLatch=%d irqCounter=%d irqReloadFlag=%d irqEnabled=%d irqPending=%d\n",
            bankSelect, R[0], R[1], R[2], R[3], R[4], R[5], R[6], R[7],
            (int)mirroring, prgRamEnabled ? 1 : 0, irqLatch, irqCounter,
            irqReloadFlag ? 1 : 0, irqEnabled ? 1 : 0, irqPending ? 1 : 0);
    }
    
    void SetTracing(bool on) override
{
    tracing = on;
    if (on)
    {
        traceWritesLeft = 64;
        if (!traceFile) traceFile = fopen("mmc3_trace.txt", "w");
    }
}

    void SaveState(StateWriter& w) const override
    {
        Mapper::SaveState(w);
        w.U8(bankSelect);
        for (int i = 0; i < 8; i++) w.U8(R[i]);
        w.Bool(prgRamEnabled);
        w.Bool(prgRamWriteProtect);
        w.U8(irqLatch);
        w.U8(irqCounter);
        w.Bool(irqReloadFlag);
        w.Bool(irqEnabled);
        w.Bool(irqPending);
    }

    void LoadState(StateReader& r) override
    {
        Mapper::LoadState(r);
        bankSelect = r.U8();
        for (int i = 0; i < 8; i++) R[i] = r.U8();
        prgRamEnabled = r.Bool();
        prgRamWriteProtect = r.Bool();
        irqLatch = r.U8();
        irqCounter = r.U8();
        irqReloadFlag = r.Bool();
        irqEnabled = r.Bool();
        irqPending = r.Bool();
    }
};

std::unique_ptr<Mapper> CreateMapper(int mapperID, std::vector<u8> prg, std::vector<u8> chr,
                                      bool chrIsRAM, Mirroring headerMirroring)
{
    switch (mapperID)
    {
    case 0:  return std::unique_ptr<Mapper>(new Mapper0(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 1:  return std::unique_ptr<Mapper>(new Mapper1(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 2:  return std::unique_ptr<Mapper>(new Mapper2(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 3:  return std::unique_ptr<Mapper>(new Mapper3(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 4:  return std::unique_ptr<Mapper>(new Mapper4(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 7:  return std::unique_ptr<Mapper>(new Mapper7(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 66: return std::unique_ptr<Mapper>(new Mapper66(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    default: return nullptr;
    }
}
