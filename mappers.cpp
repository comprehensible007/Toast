#include "mappers.h"

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
    bool irqTraceArm = false;
    int irqTraceLeft = 0;

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
            else
            {
                prgRamWriteProtect = (data & 0x40) != 0;
                prgRamEnabled = (data & 0x80) != 0;
            }
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

        if (irqCounter == 0 && irqEnabled)
        {
            irqPending = true;
        }

        if (irqTraceArm && irqTraceLeft > 0)
        {
            irqTraceLeft--;
            if (irqTraceLeft == 0) irqTraceArm = false;
        }
    }

    bool IRQState() const override { return irqPending; }
    void IRQClear() override { irqPending = false; }
    void ArmIrqTrace(int count) override { irqTraceArm = true; irqTraceLeft = count; }

    void SetTracing(bool on) override
    {
        tracing = on;
        if (on)
        {
            traceWritesLeft = 64;
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

class Mapper9 : public Mapper
{
    u8 prgBank = 0;
    u8 chr0FD = 0, chr0FE = 0, chr1FD = 0, chr1FE = 0;
    bool latch0FE = false;
    bool latch1FE = false;

public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { data = prgRAM[addr - 0x6000]; return true; }
        u32 banks = Prg8kCount() ? Prg8kCount() : 1;
        u32 bank;
        if (addr < 0xA000) bank = prgBank % banks;
        else if (addr < 0xC000) bank = (banks >= 3) ? banks - 3 : 0;
        else if (addr < 0xE000) bank = (banks >= 2) ? banks - 2 : 0;
        else bank = banks - 1;
        data = PrgByte(bank * 8192 + (addr & 0x1FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { prgRAM[addr - 0x6000] = data; return true; }
        if (addr < 0xA000) return true;
        if (addr < 0xB000) { prgBank = data & 0x0F; return true; }
        if (addr < 0xC000) { chr0FD = data & 0x1F; return true; }
        if (addr < 0xD000) { chr0FE = data & 0x1F; return true; }
        if (addr < 0xE000) { chr1FD = data & 0x1F; return true; }
        if (addr < 0xF000) { chr1FE = data & 0x1F; return true; }
        mirroring = (data & 1) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
        return true;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 banks4k = Chr4kCount() ? Chr4kCount() : 1;
        u32 bank = (addr < 0x1000) ? ((latch0FE ? chr0FE : chr0FD) % banks4k)
                                    : ((latch1FE ? chr1FE : chr1FD) % banks4k);
        u32 mapped = bank * 4096 + (addr & 0x0FFF);
        data = ChrByte(mapped);

        if (addr >= 0x0FD8 && addr <= 0x0FDF) latch0FE = false;
        else if (addr >= 0x0FE8 && addr <= 0x0FEF) latch0FE = true;
        else if (addr >= 0x1FD8 && addr <= 0x1FDF) latch1FE = false;
        else if (addr >= 0x1FE8 && addr <= 0x1FEF) latch1FE = true;

        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 banks4k = Chr4kCount() ? Chr4kCount() : 1;
        u32 bank = (addr < 0x1000) ? ((latch0FE ? chr0FE : chr0FD) % banks4k)
                                    : ((latch1FE ? chr1FE : chr1FD) % banks4k);
        u32 mapped = bank * 4096 + (addr & 0x0FFF);
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void SaveState(StateWriter& w) const override
    {
        Mapper::SaveState(w);
        w.U8(prgBank); w.U8(chr0FD); w.U8(chr0FE); w.U8(chr1FD); w.U8(chr1FE);
        w.Bool(latch0FE); w.Bool(latch1FE);
    }
    void LoadState(StateReader& r) override
    {
        Mapper::LoadState(r);
        prgBank = r.U8(); chr0FD = r.U8(); chr0FE = r.U8(); chr1FD = r.U8(); chr1FE = r.U8();
        latch0FE = r.Bool(); latch1FE = r.Bool();
    }
};

class Mapper10 : public Mapper
{
    u8 prgBank = 0;
    u8 chr0FD = 0, chr0FE = 0, chr1FD = 0, chr1FE = 0;
    bool latch0FE = false;
    bool latch1FE = false;

public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { data = prgRAM[addr - 0x6000]; return true; }
        u32 banks = Prg16kCount() ? Prg16kCount() : 1;
        u32 bank = (addr < 0xC000) ? (prgBank % banks) : (banks - 1);
        data = PrgByte(bank * 16384 + (addr & 0x3FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { prgRAM[addr - 0x6000] = data; return true; }
        if (addr < 0xA000) return true;
        if (addr < 0xB000) { prgBank = data & 0x0F; return true; }
        if (addr < 0xC000) { chr0FD = data & 0x1F; return true; }
        if (addr < 0xD000) { chr0FE = data & 0x1F; return true; }
        if (addr < 0xE000) { chr1FD = data & 0x1F; return true; }
        if (addr < 0xF000) { chr1FE = data & 0x1F; return true; }
        mirroring = (data & 1) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
        return true;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 banks4k = Chr4kCount() ? Chr4kCount() : 1;
        u32 bank = (addr < 0x1000) ? ((latch0FE ? chr0FE : chr0FD) % banks4k)
                                    : ((latch1FE ? chr1FE : chr1FD) % banks4k);
        u32 mapped = bank * 4096 + (addr & 0x0FFF);
        data = ChrByte(mapped);

        if (addr >= 0x0FD8 && addr <= 0x0FDF) latch0FE = false;
        else if (addr >= 0x0FE8 && addr <= 0x0FEF) latch0FE = true;
        else if (addr >= 0x1FD8 && addr <= 0x1FDF) latch1FE = false;
        else if (addr >= 0x1FE8 && addr <= 0x1FEF) latch1FE = true;

        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 banks4k = Chr4kCount() ? Chr4kCount() : 1;
        u32 bank = (addr < 0x1000) ? ((latch0FE ? chr0FE : chr0FD) % banks4k)
                                    : ((latch1FE ? chr1FE : chr1FD) % banks4k);
        u32 mapped = bank * 4096 + (addr & 0x0FFF);
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void SaveState(StateWriter& w) const override
    {
        Mapper::SaveState(w);
        w.U8(prgBank); w.U8(chr0FD); w.U8(chr0FE); w.U8(chr1FD); w.U8(chr1FE);
        w.Bool(latch0FE); w.Bool(latch1FE);
    }
    void LoadState(StateReader& r) override
    {
        Mapper::LoadState(r);
        prgBank = r.U8(); chr0FD = r.U8(); chr0FE = r.U8(); chr1FD = r.U8(); chr1FE = r.U8();
        latch0FE = r.Bool(); latch1FE = r.Bool();
    }
};

class Mapper11 : public Mapper
{
    u8 prgBank = 0, chrBank = 0;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x8000) return false;
        u32 banks = Prg32kCount() ? Prg32kCount() : 1;
        data = PrgByte((prgBank % banks) * 32768 + (addr & 0x7FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        prgBank = data & 0x03;
        chrBank = (data >> 4) & 0x0F;
        return true;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 banks = Chr8kCount() ? Chr8kCount() : 1;
        data = ChrByte((chrBank % banks) * 8192 + addr);
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 banks = Chr8kCount() ? Chr8kCount() : 1;
        u32 mapped = (chrBank % banks) * 8192 + addr;
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void SaveState(StateWriter& w) const override { Mapper::SaveState(w); w.U8(prgBank); w.U8(chrBank); }
    void LoadState(StateReader& r) override { Mapper::LoadState(r); prgBank = r.U8(); chrBank = r.U8(); }
};

class Mapper13 : public Mapper
{
    u8 chrBank = 0;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { data = prgRAM[addr - 0x6000]; return true; }
        data = PrgByte(addr - 0x8000);
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x6000 && addr < 0x8000) { prgRAM[addr - 0x6000] = data; return true; }
        if (addr >= 0x8000) { chrBank = data & 0x03; return true; }
        return false;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 mapped = (addr < 0x1000) ? addr : (0x1000u + chrBank * 0x1000u + (addr & 0x0FFF));
        data = ChrByte(mapped);
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        u32 mapped = (addr < 0x1000) ? addr : (0x1000u + chrBank * 0x1000u + (addr & 0x0FFF));
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void SaveState(StateWriter& w) const override { Mapper::SaveState(w); w.U8(chrBank); }
    void LoadState(StateReader& r) override { Mapper::LoadState(r); chrBank = r.U8(); }
};

class Mapper24 : public Mapper
{
    u8 prg16kBank = 0;
    u8 prg8kBank = 0;
    u8 chrBank[8] = {0,0,0,0,0,0,0,0};
    u8 mirrCtrl = 0;

    u8 irqLatch = 0, irqCounter = 0;
    bool irqEnabled = false, irqAckEnable = false, irqModeCycle = false, irqPending = false;
    int irqPrescaler = 0;

public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { data = prgRAM[addr - 0x6000]; return true; }
        if (addr < 0xC000)
        {
            u32 banks = Prg16kCount() ? Prg16kCount() : 1;
            data = PrgByte((prg16kBank % banks) * 16384 + (addr & 0x3FFF));
        }
        else if (addr < 0xE000)
        {
            u32 banks = Prg8kCount() ? Prg8kCount() : 1;
            data = PrgByte((prg8kBank % banks) * 8192 + (addr & 0x1FFF));
        }
        else
        {
            u32 banks = Prg8kCount() ? Prg8kCount() : 1;
            data = PrgByte((banks - 1) * 8192 + (addr & 0x1FFF));
        }
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { prgRAM[addr - 0x6000] = data; return true; }

        u16 reg = addr & 0xF003;

        if (addr >= 0x8000 && addr <= 0x8003) { prg16kBank = data & 0x0F; return true; }
        if (addr >= 0xC000 && addr <= 0xC003) { prg8kBank = data & 0x1F; return true; }

        if (addr >= 0x9000 && addr <= 0xB002) return true;

        if (reg == 0xB003)
        {
            switch (data & 0x03)
            {
            case 0: mirroring = Mirroring::VERTICAL; break;
            case 1: mirroring = Mirroring::HORIZONTAL; break;
            case 2: mirroring = Mirroring::SINGLE_SCREEN_LOW; break;
            default: mirroring = Mirroring::SINGLE_SCREEN_HIGH; break;
            }
            mirrCtrl = data;
            return true;
        }

        if (addr >= 0xD000 && addr <= 0xD003) { chrBank[addr - 0xD000] = data; return true; }
        if (addr >= 0xE000 && addr <= 0xE003) { chrBank[4 + (addr - 0xE000)] = data; return true; }

        if (addr == 0xF000) { irqLatch = data; return true; }
        if (addr == 0xF001)
        {
            irqEnabled = (data & 0x02) != 0;
            irqAckEnable = (data & 0x01) != 0;
            irqModeCycle = (data & 0x04) != 0;
            if (irqEnabled) { irqCounter = irqLatch; irqPrescaler = 0; }
            irqPending = false;
            return true;
        }
        if (addr == 0xF002)
        {
            irqPending = false;
            irqEnabled = irqAckEnable;
            return true;
        }
        return true;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 banks1k = Chr1kCount() ? Chr1kCount() : 1;
        int slot = addr / 0x400;
        data = ChrByte((chrBank[slot] % banks1k) * 1024 + (addr & 0x3FF));
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 banks1k = Chr1kCount() ? Chr1kCount() : 1;
        int slot = addr / 0x400;
        u32 mapped = (chrBank[slot] % banks1k) * 1024 + (addr & 0x3FF);
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void ClockCpuCycle() override
    {
        if (!irqEnabled) return;

        if (irqModeCycle)
        {
            ClockIrqCounter();
        }
        else
        {
            irqPrescaler += 1;
            if (irqPrescaler >= 341)
            {
                irqPrescaler -= 341;
                ClockIrqCounter();
            }
        }
    }

    void ClockIrqCounter()
    {
        if (irqCounter == 0xFF)
        {
            irqCounter = irqLatch;
            irqPending = true;
        }
        else
        {
            irqCounter++;
        }
    }

    bool IRQState() const override { return irqPending; }
    void IRQClear() override { irqPending = false; }

    void SaveState(StateWriter& w) const override
    {
        Mapper::SaveState(w);
        w.U8(prg16kBank); w.U8(prg8kBank);
        for (int i = 0; i < 8; i++) w.U8(chrBank[i]);
        w.U8(mirrCtrl);
        w.U8(irqLatch); w.U8(irqCounter);
        w.Bool(irqEnabled); w.Bool(irqAckEnable); w.Bool(irqModeCycle); w.Bool(irqPending);
        w.S64(irqPrescaler);
    }
    void LoadState(StateReader& r) override
    {
        Mapper::LoadState(r);
        prg16kBank = r.U8(); prg8kBank = r.U8();
        for (int i = 0; i < 8; i++) chrBank[i] = r.U8();
        mirrCtrl = r.U8();
        irqLatch = r.U8(); irqCounter = r.U8();
        irqEnabled = r.Bool(); irqAckEnable = r.Bool(); irqModeCycle = r.Bool(); irqPending = r.Bool();
        irqPrescaler = (int)r.S64();
    }
};

class Mapper34 : public Mapper
{
    u8 prgBank = 0;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x8000) return false;
        u32 banks = Prg32kCount() ? Prg32kCount() : 1;
        data = PrgByte((prgBank % banks) * 32768 + (addr & 0x7FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        prgBank = data;
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

    void SaveState(StateWriter& w) const override { Mapper::SaveState(w); w.U8(prgBank); }
    void LoadState(StateReader& r) override { Mapper::LoadState(r); prgBank = r.U8(); }
};

class Mapper69 : public Mapper
{
    u8 command = 0;
    u8 chrBank[8] = {0,0,0,0,0,0,0,0};
    u8 prgBank[4] = {0,0,0,0};
    bool prgRamSelected = false;
    bool prgRamEnabled = false;

    bool irqEnabled = false;
    bool irqCountEnabled = false;
    u16 irqCounter = 0xFFFF;
    bool irqPending = false;

public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000)
        {
            if (prgRamSelected)
            {
                data = prgRamEnabled ? prgRAM[addr - 0x6000] : 0;
            }
            else
            {
                u32 banks = Prg8kCount() ? Prg8kCount() : 1;
                data = PrgByte((prgBank[0] % banks) * 8192 + (addr - 0x6000));
            }
            return true;
        }
        u32 banks = Prg8kCount() ? Prg8kCount() : 1;
        int slot = (addr - 0x8000) / 0x2000;
        u32 bank = (slot == 3) ? (banks - 1) : (prgBank[slot + 1] % banks);
        data = PrgByte(bank * 8192 + (addr & 0x1FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000)
        {
            if (prgRamSelected && prgRamEnabled) prgRAM[addr - 0x6000] = data;
            return true;
        }

        if (addr < 0xA000) { command = data & 0x0F; return true; }
        if (addr < 0xC000)
        {
            switch (command)
            {
            case 0x0: case 0x1: case 0x2: case 0x3:
            case 0x4: case 0x5: case 0x6: case 0x7:
                chrBank[command] = data;
                break;
            case 0x8:
                prgRamSelected = (data & 0x80) != 0;
                prgRamEnabled = (data & 0x40) != 0;
                prgBank[0] = data & 0x3F;
                break;
            case 0x9: prgBank[1] = data & 0x3F; break;
            case 0xA: prgBank[2] = data & 0x3F; break;
            case 0xB: prgBank[3] = data & 0x3F; break;
            case 0xC:
                switch (data & 0x03)
                {
                case 0: mirroring = Mirroring::VERTICAL; break;
                case 1: mirroring = Mirroring::HORIZONTAL; break;
                case 2: mirroring = Mirroring::SINGLE_SCREEN_LOW; break;
                default: mirroring = Mirroring::SINGLE_SCREEN_HIGH; break;
                }
                break;
            case 0xD:
                irqEnabled = (data & 0x80) != 0;
                irqCountEnabled = (data & 0x01) != 0;
                irqPending = false;
                break;
            case 0xE: irqCounter = (irqCounter & 0xFF00) | data; break;
            case 0xF: irqCounter = (u16)((irqCounter & 0x00FF) | (data << 8)); break;
            default: break;
            }
            return true;
        }
        return true;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 banks1k = Chr1kCount() ? Chr1kCount() : 1;
        int slot = addr / 0x400;
        data = ChrByte((chrBank[slot] % banks1k) * 1024 + (addr & 0x3FF));
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 banks1k = Chr1kCount() ? Chr1kCount() : 1;
        int slot = addr / 0x400;
        u32 mapped = (chrBank[slot] % banks1k) * 1024 + (addr & 0x3FF);
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void ClockCpuCycle() override
    {
        if (!irqCountEnabled) return;
        if (irqCounter == 0)
        {
            irqCounter = 0xFFFF;
            if (irqEnabled) irqPending = true;
        }
        else
        {
            irqCounter--;
        }
    }

    bool IRQState() const override { return irqPending; }
    void IRQClear() override { irqPending = false; }

    void SaveState(StateWriter& w) const override
    {
        Mapper::SaveState(w);
        w.U8(command);
        for (int i = 0; i < 8; i++) w.U8(chrBank[i]);
        for (int i = 0; i < 4; i++) w.U8(prgBank[i]);
        w.Bool(prgRamSelected); w.Bool(prgRamEnabled);
        w.Bool(irqEnabled); w.Bool(irqCountEnabled); w.U16(irqCounter); w.Bool(irqPending);
    }
    void LoadState(StateReader& r) override
    {
        Mapper::LoadState(r);
        command = r.U8();
        for (int i = 0; i < 8; i++) chrBank[i] = r.U8();
        for (int i = 0; i < 4; i++) prgBank[i] = r.U8();
        prgRamSelected = r.Bool(); prgRamEnabled = r.Bool();
        irqEnabled = r.Bool(); irqCountEnabled = r.Bool(); irqCounter = r.U16(); irqPending = r.Bool();
    }
};

class Mapper71 : public Mapper
{
    u8 prgBank = 0;
    bool singleScreenHigh = false;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x8000) return false;
        u32 banks = Prg16kCount() ? Prg16kCount() : 1;
        u32 bank = (addr < 0xC000) ? (prgBank % banks) : (banks - 1);
        data = PrgByte(bank * 16384 + (addr & 0x3FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        if (addr >= 0x9000 && addr <= 0x9FFF)
        {
            singleScreenHigh = (data & 0x10) != 0;
            mirroring = singleScreenHigh ? Mirroring::SINGLE_SCREEN_HIGH : Mirroring::SINGLE_SCREEN_LOW;
            return true;
        }
        if (addr >= 0xC000)
        {
            prgBank = data & 0x0F;
            return true;
        }
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

    void SaveState(StateWriter& w) const override { Mapper::SaveState(w); w.U8(prgBank); w.Bool(singleScreenHigh); }
    void LoadState(StateReader& r) override { Mapper::LoadState(r); prgBank = r.U8(); singleScreenHigh = r.Bool(); }
};

class Mapper75 : public Mapper
{
    u8 prgBank0 = 0, prgBank1 = 0, prgBank2 = 0;
    u8 chrBank0 = 0, chrBank1 = 0;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { data = prgRAM[addr - 0x6000]; return true; }
        u32 banks = Prg8kCount() ? Prg8kCount() : 1;
        u32 bank;
        if (addr < 0xA000) bank = prgBank0 % banks;
        else if (addr < 0xC000) bank = prgBank1 % banks;
        else if (addr < 0xE000) bank = prgBank2 % banks;
        else bank = banks - 1;
        data = PrgByte(bank * 8192 + (addr & 0x1FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { prgRAM[addr - 0x6000] = data; return true; }
        if (addr < 0x9000) { prgBank0 = data & 0x0F; return true; }
        if (addr < 0xA000)
        {
            mirroring = (data & 0x01) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
            chrBank0 = (u8)((chrBank0 & 0x0F) | ((data & 0x02) << 3));
            chrBank1 = (u8)((chrBank1 & 0x0F) | ((data & 0x04) << 2));
            return true;
        }
        if (addr < 0xB000) { prgBank1 = data & 0x0F; return true; }
        if (addr < 0xC000) { chrBank0 = (u8)((chrBank0 & 0x10) | (data & 0x0F)); return true; }
        if (addr < 0xD000) { prgBank2 = data & 0x0F; return true; }
        if (addr < 0xE000) { chrBank1 = (u8)((chrBank1 & 0x10) | (data & 0x0F)); return true; }
        return true;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 banks4k = Chr4kCount() ? Chr4kCount() : 1;
        u32 bank = (addr < 0x1000) ? (chrBank0 % banks4k) : (chrBank1 % banks4k);
        data = ChrByte(bank * 4096 + (addr & 0x0FFF));
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 banks4k = Chr4kCount() ? Chr4kCount() : 1;
        u32 bank = (addr < 0x1000) ? (chrBank0 % banks4k) : (chrBank1 % banks4k);
        u32 mapped = bank * 4096 + (addr & 0x0FFF);
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void SaveState(StateWriter& w) const override
    {
        Mapper::SaveState(w);
        w.U8(prgBank0); w.U8(prgBank1); w.U8(prgBank2); w.U8(chrBank0); w.U8(chrBank1);
    }
    void LoadState(StateReader& r) override
    {
        Mapper::LoadState(r);
        prgBank0 = r.U8(); prgBank1 = r.U8(); prgBank2 = r.U8(); chrBank0 = r.U8(); chrBank1 = r.U8();
    }
};

class Mapper79 : public Mapper
{
    u8 prgBank = 0, chrBank = 0;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x8000) return false;
        u32 banks = Prg32kCount() ? Prg32kCount() : 1;
        data = PrgByte((prgBank % banks) * 32768 + (addr & 0x7FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x4020 && addr <= 0x5FFF)
        {
            chrBank = data & 0x07;
            prgBank = (data >> 3) & 0x01;
            return true;
        }
        return false;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 banks = Chr8kCount() ? Chr8kCount() : 1;
        data = ChrByte((chrBank % banks) * 8192 + addr);
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 banks = Chr8kCount() ? Chr8kCount() : 1;
        u32 mapped = (chrBank % banks) * 8192 + addr;
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void SaveState(StateWriter& w) const override { Mapper::SaveState(w); w.U8(prgBank); w.U8(chrBank); }
    void LoadState(StateReader& r) override { Mapper::LoadState(r); prgBank = r.U8(); chrBank = r.U8(); }
};

using Mapper113 = Mapper79;

class Mapper140 : public Mapper
{
    u8 prgBank = 0, chrBank = 0;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x8000) return false;
        u32 banks = Prg32kCount() ? Prg32kCount() : 1;
        data = PrgByte((prgBank % banks) * 32768 + (addr & 0x7FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x6000 && addr < 0x8000)
        {
            prgBank = (data >> 4) & 0x03;
            chrBank = data & 0x0F;
            return true;
        }
        return false;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 banks = Chr8kCount() ? Chr8kCount() : 1;
        data = ChrByte((chrBank % banks) * 8192 + addr);
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 banks = Chr8kCount() ? Chr8kCount() : 1;
        u32 mapped = (chrBank % banks) * 8192 + addr;
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void SaveState(StateWriter& w) const override { Mapper::SaveState(w); w.U8(prgBank); w.U8(chrBank); }
    void LoadState(StateReader& r) override { Mapper::LoadState(r); prgBank = r.U8(); chrBank = r.U8(); }
};

class Mapper152 : public Mapper
{
    u8 prgBank = 0, chrBank = 0;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x8000) return false;
        u32 banks = Prg16kCount() ? Prg16kCount() : 1;
        u32 bank = (addr < 0xC000) ? (prgBank % banks) : (banks - 1);
        data = PrgByte(bank * 16384 + (addr & 0x3FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        mirroring = (data & 0x80) ? Mirroring::SINGLE_SCREEN_HIGH : Mirroring::SINGLE_SCREEN_LOW;
        prgBank = (data >> 4) & 0x07;
        chrBank = data & 0x0F;
        return true;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 banks = Chr8kCount() ? Chr8kCount() : 1;
        data = ChrByte((chrBank % banks) * 8192 + addr);
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 banks = Chr8kCount() ? Chr8kCount() : 1;
        u32 mapped = (chrBank % banks) * 8192 + addr;
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void SaveState(StateWriter& w) const override { Mapper::SaveState(w); w.U8(prgBank); w.U8(chrBank); }
    void LoadState(StateReader& r) override { Mapper::LoadState(r); prgBank = r.U8(); chrBank = r.U8(); }
};

class Mapper180 : public Mapper
{
    u8 prgBank = 0;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x8000) return false;
        if (addr < 0xC000) { data = PrgByte(addr - 0x8000); return true; }
        u32 banks = Prg16kCount() ? Prg16kCount() : 1;
        data = PrgByte((prgBank % banks) * 16384 + (addr & 0x3FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        prgBank = data & 0x07;
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

    void SaveState(StateWriter& w) const override { Mapper::SaveState(w); w.U8(prgBank); }
    void LoadState(StateReader& r) override { Mapper::LoadState(r); prgBank = r.U8(); }
};

class Mapper184 : public Mapper
{
    u8 chrLow = 0, chrHigh = 0;
public:
    using Mapper::Mapper;

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { data = prgRAM[addr - 0x6000]; return true; }
        data = PrgByte(addr - 0x8000);
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000)
        {
            chrLow = data & 0x07;
            chrHigh = (data >> 4) & 0x07;
            return true;
        }
        return true;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 banks4k = Chr4kCount() ? Chr4kCount() : 1;
        u32 bank = (addr < 0x1000) ? (chrLow % banks4k) : (chrHigh % banks4k);
        data = ChrByte(bank * 4096 + (addr & 0x0FFF));
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 banks4k = Chr4kCount() ? Chr4kCount() : 1;
        u32 bank = (addr < 0x1000) ? (chrLow % banks4k) : (chrHigh % banks4k);
        u32 mapped = bank * 4096 + (addr & 0x0FFF);
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    void SaveState(StateWriter& w) const override { Mapper::SaveState(w); w.U8(chrLow); w.U8(chrHigh); }
    void LoadState(StateReader& r) override { Mapper::LoadState(r); chrLow = r.U8(); chrHigh = r.U8(); }
};

std::unique_ptr<Mapper> CreateMapper(int mapperID, std::vector<u8> prg, std::vector<u8> chr,
                                      bool chrIsRAM, Mirroring headerMirroring)
{
    switch (mapperID)
    {
    case 0:   return std::unique_ptr<Mapper>(new Mapper0(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 1:   return std::unique_ptr<Mapper>(new Mapper1(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 2:   return std::unique_ptr<Mapper>(new Mapper2(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 3:   return std::unique_ptr<Mapper>(new Mapper3(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 4:   return std::unique_ptr<Mapper>(new Mapper4(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 7:   return std::unique_ptr<Mapper>(new Mapper7(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 9:   return std::unique_ptr<Mapper>(new Mapper9(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 10:  return std::unique_ptr<Mapper>(new Mapper10(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 11:  return std::unique_ptr<Mapper>(new Mapper11(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 13:  return std::unique_ptr<Mapper>(new Mapper13(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 24:  return std::unique_ptr<Mapper>(new Mapper24(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 34:  return std::unique_ptr<Mapper>(new Mapper34(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 66:  return std::unique_ptr<Mapper>(new Mapper66(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 69:  return std::unique_ptr<Mapper>(new Mapper69(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 71:  return std::unique_ptr<Mapper>(new Mapper71(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 75:  return std::unique_ptr<Mapper>(new Mapper75(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 79:  return std::unique_ptr<Mapper>(new Mapper79(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 113: return std::unique_ptr<Mapper>(new Mapper113(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 140: return std::unique_ptr<Mapper>(new Mapper140(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 152: return std::unique_ptr<Mapper>(new Mapper152(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 180: return std::unique_ptr<Mapper>(new Mapper180(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 184: return std::unique_ptr<Mapper>(new Mapper184(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    default:  return nullptr;
    }
}
