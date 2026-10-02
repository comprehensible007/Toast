#pragma once

#include "mapper.h"
#include <vector>
#include <cstring>

class BankedMapper : public Mapper
{
public:
    BankedMapper(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : Mapper(std::move(p), std::move(c), r, m)
    {
        for (int i = 0; i < 4; i++) prg[i] = (u32)i;
        for (int i = 0; i < 8; i++) chr[i] = (u32)i;
    }

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x4020) return false;
        if (ReadReg(addr, data)) return true;
        if (addr < 0x6000) return false;
        if (addr < 0x8000)
        {
            if (!hasPrgRam || !prgRamOn) return false;
            data = prgRAM[addr - 0x6000];
            return true;
        }
        data = PrgByte((prg[(addr - 0x8000) >> 13] % NumPrg8()) * 8192 + (addr & 0x1FFF));
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr < 0x4020) return false;
        if (WriteReg(addr, data)) return true;
        if (addr >= 0x6000 && addr < 0x8000)
        {
            if (hasPrgRam && prgRamOn) { prgRAM[addr - 0x6000] = data; return true; }
            return false;
        }
        return addr >= 0x8000;
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        data = ChrByte((chr[addr >> 10] % NumChr1()) * 1024 + (addr & 0x3FF));
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (!chrIsRAM) return false;
        u32 mapped = (chr[addr >> 10] % NumChr1()) * 1024 + (addr & 0x3FF);
        if (mapped < chrROM.size()) chrROM[mapped] = data;
        return true;
    }

    bool IRQState() const override { return irqPending; }
    void IRQClear() override { irqPending = false; }

    void SaveState(StateWriter& w) const override
    {
        Mapper::SaveState(w);
        for (int i = 0; i < 4; i++) w.U32(prg[i]);
        for (int i = 0; i < 8; i++) w.U32(chr[i]);
        for (int i = 0; i < 4; i++) w.U8(ntMap[i]);
        w.Bool(prgRamOn); w.Bool(irqPending);
        SaveExtra(w);
    }

    void LoadState(StateReader& r) override
    {
        Mapper::LoadState(r);
        for (int i = 0; i < 4; i++) prg[i] = r.U32();
        for (int i = 0; i < 8; i++) chr[i] = r.U32();
        for (int i = 0; i < 4; i++) ntMap[i] = r.U8();
        prgRamOn = r.Bool(); irqPending = r.Bool();
        LoadExtra(r);
    }

protected:
    u32 prg[4];
    u32 chr[8];
    bool hasPrgRam = true;
    bool prgRamOn = true;
    bool irqPending = false;

    virtual bool ReadReg(u16, u8&) { return false; }
    virtual bool WriteReg(u16, u8) { return false; }
    virtual void SaveExtra(StateWriter&) const {}
    virtual void LoadExtra(StateReader&) {}

    u32 NumPrg8() const  { u32 n = Prg8kCount();  return n ? n : 1; }
    u32 NumPrg16() const { u32 n = Prg16kCount(); return n ? n : 1; }
    u32 NumPrg32() const { u32 n = Prg32kCount(); return n ? n : 1; }
    u32 NumChr1() const  { u32 n = Chr1kCount();  return n ? n : 1; }
    u32 FromEnd8(u32 k) const { u32 n = NumPrg8(); return n >= k ? n - k : 0; }
    u32 Last16() const { return NumPrg16() - 1; }

    void SetPrg8(int slot, u32 b)   { prg[slot] = b; }
    void SetPrg16(int slot16, u32 b) { prg[slot16 * 2] = b * 2; prg[slot16 * 2 + 1] = b * 2 + 1; }
    void SetPrg32(u32 b)            { for (u32 i = 0; i < 4; i++) prg[i] = b * 4 + i; }
    void SetChr1(int slot, u32 b)   { chr[slot] = b; }
    void SetChr2(int slot2, u32 b)  { chr[slot2 * 2] = b * 2; chr[slot2 * 2 + 1] = b * 2 + 1; }
    void SetChr4(int slot4, u32 b)  { for (u32 i = 0; i < 4; i++) chr[slot4 * 4 + i] = b * 4 + i; }
    void SetChr8(u32 b)             { for (u32 i = 0; i < 8; i++) chr[i] = b * 8 + i; }

    void SetMirror(int v)
    {
        switch (v & 3)
        {
        case 0: mirroring = Mirroring::VERTICAL; break;
        case 1: mirroring = Mirroring::HORIZONTAL; break;
        case 2: mirroring = Mirroring::SINGLE_SCREEN_LOW; break;
        default: mirroring = Mirroring::SINGLE_SCREEN_HIGH; break;
        }
    }
};

class SimpleLatch : public BankedMapper
{
    int id;
    int sub;
    bool prevPrgBit = false, prevChrBit = false;
    u32 block232 = 0, page232 = 0;
    bool chrOn185 = false;

public:
    SimpleLatch(int id_, std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m, int sub_ = 0)
        : BankedMapper(std::move(p), std::move(c), r, m), id(id_), sub(sub_)
    {
        switch (id)
        {
        case 70: case 78: case 89: case 93: case 94: case 72:
            SetPrg16(1, Last16()); break;
        case 92: SetPrg16(0, 0); SetPrg16(1, 0); break;
        case 97: SetPrg16(0, Last16()); SetPrg16(1, 0); break;
        case 86: hasPrgRam = false; SetPrg32(0); break;
        case 87: hasPrgRam = false; break;
        case 232: SetPrg16(0, 0); SetPrg16(1, 3); break;
        default: break;
        }
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (id == 185 && addr < 0x2000 && !chrOn185) { data = 0; return true; }
        return BankedMapper::PpuRead(addr, data);
    }

protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (id == 86)
        {
            if (addr >= 0x6000 && addr < 0x8000)
            {
                if (!(addr & 0x1000)) { SetPrg32((data >> 4) & 3); SetChr8((data & 3) | ((data >> 4) & 4)); }
                return true;
            }
            return false;
        }
        if (id == 87)
        {
            if (addr >= 0x6000 && addr < 0x8000) { SetChr8(((data & 1) << 1) | ((data >> 1) & 1)); return true; }
            return false;
        }
        if (addr < 0x8000) return false;

        switch (id)
        {
        case 70: SetPrg16(0, (data >> 4) & 0x0F); SetChr8(data & 0x0F); break;
        case 78:
            SetPrg16(0, data & 7); SetChr8(data >> 4);
            if (sub == 1) SetMirror((data & 8) ? 3 : 2);
            else          SetMirror((data & 8) ? 0 : 1);
            break;
        case 89:
            SetPrg16(0, (data >> 4) & 7);
            SetChr8(((data >> 4) & 8) | (data & 7));
            SetMirror((data & 8) ? 3 : 2);
            break;
        case 93: SetPrg16(0, (data >> 4) & 7); break;
        case 94: SetPrg16(0, (data >> 2) & 7); break;
        case 97:
        {
            static const int mm[4] = { 2, 1, 0, 3 };
            SetPrg16(1, data & 0x0F);
            SetMirror(mm[(data >> 6) & 3]);
            break;
        }
        case 72: case 92:
        {
            bool pb = (data & 0x80) != 0, cb = (data & 0x40) != 0;
            if (pb && !prevPrgBit) { if (id == 72) SetPrg16(0, data & 0x0F); else SetPrg16(1, data & 0x0F); }
            if (cb && !prevChrBit) SetChr8(data & 0x0F);
            prevPrgBit = pb; prevChrBit = cb;
            break;
        }
        case 232:
            if (addr < 0xC000) block232 = (data >> 3) & 3; else page232 = data & 3;
            SetPrg16(0, block232 * 4 + page232);
            SetPrg16(1, block232 * 4 + 3);
            break;
        case 185:
            chrOn185 = ((data & 0x0F) != 0) && (data != 0x13);
            break;
        default: break;
        }
        return true;
    }

    void SaveExtra(StateWriter& w) const override
    {
        w.Bool(prevPrgBit); w.Bool(prevChrBit); w.U32(block232); w.U32(page232); w.Bool(chrOn185);
    }
    void LoadExtra(StateReader& r) override
    {
        prevPrgBit = r.Bool(); prevChrBit = r.Bool(); block232 = r.U32(); page232 = r.U32(); chrOn185 = r.Bool();
    }
};

class Nina : public BankedMapper
{
    bool is113;
public:
    Nina(bool is113_, std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m), is113(is113_)
    {
        SetPrg32(0); SetChr8(0); hasPrgRam = false;
    }
protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x4020 || addr > 0x5FFF) return false;
        if ((addr & 0xE100) != 0x4100) return true;
        if (is113)
        {
            SetPrg32((data >> 3) & 7);
            SetChr8((data & 7) | ((data >> 3) & 8));
            SetMirror((data & 0x80) ? 0 : 1);
        }
        else
        {
            SetPrg32((data >> 3) & 1);
            SetChr8(data & 7);
        }
        return true;
    }
};

class Unrom512 : public BankedMapper
{
    std::vector<u8> ram;
    u32 chrPage = 0;
public:
    Unrom512(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m), ram(32768, 0)
    {
        SetPrg16(1, Last16());
        hasPrgRam = false;
    }
    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        data = ram[chrPage * 8192 + addr];
        return true;
    }
    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        ram[chrPage * 8192 + addr] = data;
        return true;
    }
protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        SetPrg16(0, data & 0x1F);
        chrPage = (data >> 5) & 3;
        if (headerMirroring == Mirroring::FOUR_SCREEN) SetMirror((data & 0x80) ? 3 : 2);
        return true;
    }
    void SaveExtra(StateWriter& w) const override { w.Bytes(ram.data(), ram.size()); w.U32(chrPage); }
    void LoadExtra(StateReader& r) override { r.Bytes(ram.data(), ram.size()); chrPage = r.U32(); }
};

class NamcoDx : public BankedMapper
{
    int kind;
    u8 sel = 0;
    u8 R[8] = { 0, 0, 0, 0, 0, 0, 0, 1 };

    void Sync()
    {
        prg[0] = R[6] & 0x0F;
        prg[1] = R[7] & 0x0F;
        prg[2] = FromEnd8(2);
        prg[3] = FromEnd8(1);

        if (kind == 76)
        {
            SetChr2(0, R[2] & 0x3F); SetChr2(1, R[3] & 0x3F);
            SetChr2(2, R[4] & 0x3F); SetChr2(3, R[5] & 0x3F);
            return;
        }
        u32 a0, a1, a2, a3, a4, a5;
        if (kind == 88 || kind == 154)
        {
            a0 = R[0] & 0x3E; a1 = R[1] & 0x3E;
            a2 = (R[2] & 0x3F) | 0x40; a3 = (R[3] & 0x3F) | 0x40;
            a4 = (R[4] & 0x3F) | 0x40; a5 = (R[5] & 0x3F) | 0x40;
        }
        else if (kind == 95)
        {
            a0 = R[0] & 0x1E; a1 = R[1] & 0x1E;
            a2 = R[2] & 0x1F; a3 = R[3] & 0x1F; a4 = R[4] & 0x1F; a5 = R[5] & 0x1F;
            ntMap[0] = ntMap[1] = (u8)((R[0] >> 5) & 1);
            ntMap[2] = ntMap[3] = (u8)((R[1] >> 5) & 1);
        }
        else
        {
            a0 = R[0] & 0x3E; a1 = R[1] & 0x3E;
            a2 = R[2] & 0x3F; a3 = R[3] & 0x3F; a4 = R[4] & 0x3F; a5 = R[5] & 0x3F;
        }
        chr[0] = a0; chr[1] = a0 + 1; chr[2] = a1; chr[3] = a1 + 1;
        chr[4] = a2; chr[5] = a3; chr[6] = a4; chr[7] = a5;
    }

public:
    NamcoDx(int kind_, std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m), kind(kind_)
    {
        if (kind == 95) mirroring = Mirroring::CUSTOM;
        hasPrgRam = false;
        Sync();
    }
protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        if (!(addr & 1))
        {
            sel = data & 7;
            if (kind == 154) SetMirror((data & 0x40) ? 3 : 2);
        }
        else
        {
            R[sel] = data;
        }
        Sync();
        return true;
    }
    void SaveExtra(StateWriter& w) const override { w.U8(sel); for (int i = 0; i < 8; i++) w.U8(R[i]); }
    void LoadExtra(StateReader& r) override { sel = r.U8(); for (int i = 0; i < 8; i++) R[i] = r.U8(); }
};

class Mmc3Variant : public BankedMapper
{
    int kind;
    u8 bankSel = 0;
    u8 R[8] = { 0, 2, 4, 5, 6, 7, 0, 1 };
    u8 irqLatch = 0, irqCounter = 0;
    bool irqReload = false, irqEnabled = false;
    std::vector<u8> chrRam;

    void Sync()
    {
        bool mode = (bankSel & 0x40) != 0;
        prg[1] = R[7] & 0x3F;
        prg[3] = FromEnd8(1);
        if (!mode) { prg[0] = R[6] & 0x3F; prg[2] = FromEnd8(2); }
        else       { prg[0] = FromEnd8(2); prg[2] = R[6] & 0x3F; }

        u32 raw[8] = { (u32)(R[0] & 0xFE), (u32)(R[0] & 0xFE) + 1, (u32)(R[1] & 0xFE), (u32)(R[1] & 0xFE) + 1,
                       R[2], R[3], R[4], R[5] };
        bool inv = (bankSel & 0x80) != 0;
        for (int i = 0; i < 8; i++) chr[i] = raw[inv ? (i ^ 4) : i] & 0x7F;

        if (kind == 118)
        {
            int base = inv ? 4 : 0;
            for (int k = 0; k < 4; k++) ntMap[k] = (u8)((raw[base + k] >> 7) & 1);
        }
    }

public:
    Mmc3Variant(int kind_, std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m), kind(kind_), chrRam(8192, 0)
    {
        if (kind == 118) mirroring = Mirroring::CUSTOM;
        Sync();
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        if (kind == 119 && (chr[addr >> 10] & 0x40))
        {
            data = chrRam[(chr[addr >> 10] & 7) * 1024 + (addr & 0x3FF)];
            return true;
        }
        data = ChrByte(((chr[addr >> 10] & (kind == 119 ? 0x3F : 0x7F)) % NumChr1()) * 1024 + (addr & 0x3FF));
        return true;
    }

    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        if (kind == 119 && (chr[addr >> 10] & 0x40))
        {
            chrRam[(chr[addr >> 10] & 7) * 1024 + (addr & 0x3FF)] = data;
            return true;
        }
        return BankedMapper::PpuWrite(addr, data);
    }

    void OnScanline() override
    {
        if (irqCounter == 0 || irqReload) { irqCounter = irqLatch; irqReload = false; }
        else irqCounter--;
        if (irqCounter == 0 && irqEnabled) irqPending = true;
    }

protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        switch (addr & 0xE001)
        {
        case 0x8000: bankSel = data; Sync(); break;
        case 0x8001: R[bankSel & 7] = data; Sync(); break;
        case 0xA000: if (kind != 118) SetMirror((data & 1) ? 1 : 0); break;
        case 0xA001: break;
        case 0xC000: irqLatch = data; break;
        case 0xC001: irqCounter = 0; irqReload = true; break;
        case 0xE000: irqEnabled = false; irqPending = false; break;
        case 0xE001: irqEnabled = true; break;
        }
        return true;
    }
    void SaveExtra(StateWriter& w) const override
    {
        w.U8(bankSel); for (int i = 0; i < 8; i++) w.U8(R[i]);
        w.U8(irqLatch); w.U8(irqCounter); w.Bool(irqReload); w.Bool(irqEnabled);
        w.Bytes(chrRam.data(), chrRam.size());
    }
    void LoadExtra(StateReader& r) override
    {
        bankSel = r.U8(); for (int i = 0; i < 8; i++) R[i] = r.U8();
        irqLatch = r.U8(); irqCounter = r.U8(); irqReload = r.Bool(); irqEnabled = r.Bool();
        r.Bytes(chrRam.data(), chrRam.size());
    }
};

class Vrc24 : public BankedMapper
{
    int id;
    u8 prgReg[2] = { 0, 0 };
    u16 chrReg[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    bool swapMode = false;
    u8 mirrCtrl = 0;
    u8 irqLatch = 0, irqCounter = 0;
    bool irqEnabled = false, irqEnableAfterAck = false, irqCycleMode = false;
    int prescaler = 0;

    int sub;

    u8 RegOf(u16 a) const
    {
        switch (id)
        {
        case 21:
            if (sub == 1) return (u8)((a >> 1) & 3);
            if (sub == 2) return (u8)((a >> 6) & 3);
            return (u8)(((a >> 1) & 3) | ((a >> 6) & 3));
        case 22: return (u8)(((a >> 1) & 1) | ((a & 1) << 1));
        case 23:
            if (sub == 1 || sub == 3) return (u8)(a & 3);
            if (sub == 2) return (u8)((a >> 2) & 3);
            return (u8)((a & 3) | ((a >> 2) & 3));
        default:
            if (sub == 1 || sub == 3) return (u8)(((a >> 1) & 1) | ((a & 1) << 1));
            if (sub == 2) return (u8)(((a >> 3) & 1) | (((a >> 2) & 1) << 1));
            return (u8)((((a >> 1) & 1) | ((a >> 3) & 1)) | ((((a) & 1) | ((a >> 2) & 1)) << 1));
        }
    }

    void Sync()
    {
        prg[1] = prgReg[1];
        prg[3] = FromEnd8(1);
        if (!swapMode) { prg[0] = prgReg[0]; prg[2] = FromEnd8(2); }
        else           { prg[0] = FromEnd8(2); prg[2] = prgReg[0]; }
        for (int i = 0; i < 8; i++) chr[i] = (id == 22) ? (u32)(chrReg[i] >> 1) : (u32)chrReg[i];
    }

    void ClockCounter()
    {
        if (irqCounter == 0xFF) { irqCounter = irqLatch; irqPending = true; }
        else irqCounter++;
    }

public:
    Vrc24(int id_, std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m, int sub_ = 0)
        : BankedMapper(std::move(p), std::move(c), r, m), id(id_), sub(sub_)
    {
        Sync();
    }

    void ClockCpuCycle() override
    {
        if (!irqEnabled) return;
        if (irqCycleMode) { ClockCounter(); return; }
        prescaler += 3;
        if (prescaler >= 341) { prescaler -= 341; ClockCounter(); }
    }

protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        u8 reg = RegOf(addr);
        switch (addr & 0xF000)
        {
        case 0x8000: prgReg[0] = data & 0x1F; break;
        case 0xA000: prgReg[1] = data & 0x1F; break;
        case 0x9000:
            if (reg < 2) { mirrCtrl = data; SetMirror(data & 3); }
            else swapMode = (data & 2) != 0;
            break;
        case 0xB000: case 0xC000: case 0xD000: case 0xE000:
        {
            int slot = (((addr >> 12) & 7) - 3) * 2 + (reg >> 1);
            if (reg & 1) chrReg[slot] = (u16)((chrReg[slot] & 0x0F) | ((data & 0x1F) << 4));
            else         chrReg[slot] = (u16)((chrReg[slot] & 0x1F0) | (data & 0x0F));
            break;
        }
        case 0xF000:
            switch (reg)
            {
            case 0: irqLatch = (u8)((irqLatch & 0xF0) | (data & 0x0F)); break;
            case 1: irqLatch = (u8)((irqLatch & 0x0F) | ((data & 0x0F) << 4)); break;
            case 2:
                irqEnableAfterAck = (data & 1) != 0;
                irqEnabled = (data & 2) != 0;
                irqCycleMode = (data & 4) != 0;
                irqPending = false;
                if (irqEnabled) { irqCounter = irqLatch; prescaler = 0; }
                break;
            default:
                irqPending = false;
                irqEnabled = irqEnableAfterAck;
                break;
            }
            break;
        }
        Sync();
        return true;
    }
    void SaveExtra(StateWriter& w) const override
    {
        w.U8(prgReg[0]); w.U8(prgReg[1]);
        for (int i = 0; i < 8; i++) w.U16(chrReg[i]);
        w.Bool(swapMode); w.U8(mirrCtrl); w.U8(irqLatch); w.U8(irqCounter);
        w.Bool(irqEnabled); w.Bool(irqEnableAfterAck); w.Bool(irqCycleMode); w.S64(prescaler);
    }
    void LoadExtra(StateReader& r) override
    {
        prgReg[0] = r.U8(); prgReg[1] = r.U8();
        for (int i = 0; i < 8; i++) chrReg[i] = r.U16();
        swapMode = r.Bool(); mirrCtrl = r.U8(); irqLatch = r.U8(); irqCounter = r.U8();
        irqEnabled = r.Bool(); irqEnableAfterAck = r.Bool(); irqCycleMode = r.Bool(); prescaler = (int)r.S64();
    }
};

class IremG101 : public BankedMapper
{
    u8 p0 = 0, p1 = 0;
    bool mode = false;

    void Sync()
    {
        prg[1] = p1; prg[3] = FromEnd8(1);
        if (!mode) { prg[0] = p0; prg[2] = FromEnd8(2); }
        else       { prg[0] = FromEnd8(2); prg[2] = p0; }
    }
public:
    IremG101(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m) { Sync(); }
protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        switch (addr & 0xF000)
        {
        case 0x8000: p0 = data & 0x1F; break;
        case 0x9000: SetMirror((data & 1) ? 1 : 0); mode = (data & 2) != 0; break;
        case 0xA000: p1 = data & 0x1F; break;
        case 0xB000: SetChr1(addr & 7, data); break;
        }
        Sync();
        return true;
    }
    void SaveExtra(StateWriter& w) const override { w.U8(p0); w.U8(p1); w.Bool(mode); }
    void LoadExtra(StateReader& r) override { p0 = r.U8(); p1 = r.U8(); mode = r.Bool(); }
};

class TaitoTc : public BankedMapper
{
    bool is48;
    u8 irqLatch = 0, irqCounter = 0;
    bool irqReload = false, irqEnabled = false;
public:
    TaitoTc(bool is48_, std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m), is48(is48_)
    {
        prg[2] = FromEnd8(2); prg[3] = FromEnd8(1);
    }

    void OnScanline() override
    {
        if (!is48) return;
        if (irqCounter == 0 || irqReload) { irqCounter = irqLatch; irqReload = false; }
        else irqCounter--;
        if (irqCounter == 0 && irqEnabled) irqPending = true;
    }
protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        switch (addr & 0xE003)
        {
        case 0x8000: prg[0] = data & 0x3F; if (!is48) SetMirror((data & 0x40) ? 1 : 0); break;
        case 0x8001: prg[1] = data & 0x3F; break;
        case 0x8002: SetChr2(0, data); break;
        case 0x8003: SetChr2(1, data); break;
        case 0xA000: case 0xA001: case 0xA002: case 0xA003: SetChr1(4 + (addr & 3), data); break;
        case 0xC000: if (is48) irqLatch = data ^ 0xFF; break;
        case 0xC001: if (is48) { irqCounter = 0; irqReload = true; } break;
        case 0xC002: if (is48) irqEnabled = true; break;
        case 0xC003: if (is48) { irqEnabled = false; irqPending = false; } break;
        case 0xE000: if (is48) SetMirror((data & 0x40) ? 1 : 0); break;
        }
        return true;
    }
    void SaveExtra(StateWriter& w) const override { w.U8(irqLatch); w.U8(irqCounter); w.Bool(irqReload); w.Bool(irqEnabled); }
    void LoadExtra(StateReader& r) override { irqLatch = r.U8(); irqCounter = r.U8(); irqReload = r.Bool(); irqEnabled = r.Bool(); }
};

class IremH3001 : public BankedMapper
{
    bool irqEnabled = false;
    u16 irqCounter = 0, irqReload = 0;
public:
    IremH3001(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m)
    {
        prg[0] = 0; prg[1] = 1; prg[2] = FromEnd8(2); prg[3] = FromEnd8(1);
    }
    void ClockCpuCycle() override
    {
        if (irqEnabled && irqCounter > 0)
        {
            irqCounter--;
            if (irqCounter == 0) irqPending = true;
        }
    }
protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        switch (addr & 0xF007)
        {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003: case 0x8004: case 0x8005: case 0x8006: case 0x8007:
            prg[0] = data; break;
        case 0xA000: case 0xA001: case 0xA002: case 0xA003: case 0xA004: case 0xA005: case 0xA006: case 0xA007:
            prg[1] = data; break;
        case 0xC000: case 0xC001: case 0xC002: case 0xC003: case 0xC004: case 0xC005: case 0xC006: case 0xC007:
            prg[2] = data; break;
        case 0x9001: SetMirror((data & 0x80) ? 1 : 0); break;
        case 0x9003: irqEnabled = (data & 0x80) != 0; irqPending = false; break;
        case 0x9004: irqCounter = irqReload; irqPending = false; break;
        case 0x9005: irqReload = (u16)((irqReload & 0x00FF) | (data << 8)); break;
        case 0x9006: irqReload = (u16)((irqReload & 0xFF00) | data); break;
        default:
            if ((addr & 0xF000) == 0xB000) SetChr1(addr & 7, data);
            break;
        }
        return true;
    }
    void SaveExtra(StateWriter& w) const override { w.Bool(irqEnabled); w.U16(irqCounter); w.U16(irqReload); }
    void LoadExtra(StateReader& r) override { irqEnabled = r.Bool(); irqCounter = r.U16(); irqReload = r.U16(); }
};

class Sunsoft3 : public BankedMapper
{
    bool irqEnabled = false, highByte = true;
    u16 irqCounter = 0;
public:
    Sunsoft3(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m)
    {
        SetPrg16(1, Last16());
    }
    void ClockCpuCycle() override
    {
        if (!irqEnabled) return;
        irqCounter--;
        if (irqCounter == 0xFFFF) { irqPending = true; irqEnabled = false; }
    }
protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        switch (addr & 0xF800)
        {
        case 0x8800: SetChr2(0, data); break;
        case 0x9800: SetChr2(1, data); break;
        case 0xA800: SetChr2(2, data); break;
        case 0xB800: SetChr2(3, data); break;
        case 0xC800:
            if (highByte) irqCounter = (u16)((irqCounter & 0x00FF) | (data << 8));
            else          irqCounter = (u16)((irqCounter & 0xFF00) | data);
            highByte = !highByte;
            break;
        case 0xD800: irqEnabled = (data & 0x10) != 0; highByte = true; irqPending = false; break;
        case 0xE800: SetMirror(data & 3); break;
        case 0xF800: SetPrg16(0, data & 0x0F); break;
        }
        return true;
    }
    void SaveExtra(StateWriter& w) const override { w.Bool(irqEnabled); w.Bool(highByte); w.U16(irqCounter); }
    void LoadExtra(StateReader& r) override { irqEnabled = r.Bool(); highByte = r.Bool(); irqCounter = r.U16(); }
};

class Sunsoft4 : public BankedMapper
{
    u8 ntReg[2] = { 0x80, 0x80 };
    u8 ctrl = 0;

    int NtSource(int t) const
    {
        switch (ctrl & 3)
        {
        case 0: return t & 1;
        case 1: return (t >> 1) & 1;
        case 2: return 0;
        default: return 1;
        }
    }
public:
    Sunsoft4(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m)
    {
        SetPrg16(1, Last16());
    }
    bool NametableRead(u16 addr, u8& data) override
    {
        if (!(ctrl & 0x10)) return false;
        int t = (addr >> 10) & 3;
        u32 bank = (u32)(ntReg[NtSource(t)] | 0x80) % NumChr1();
        data = ChrByte(bank * 1024 + (addr & 0x3FF));
        return true;
    }
    bool NametableWrite(u16, u8) override { return (ctrl & 0x10) != 0; }
protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        switch (addr & 0xF000)
        {
        case 0x8000: SetChr2(0, data); break;
        case 0x9000: SetChr2(1, data); break;
        case 0xA000: SetChr2(2, data); break;
        case 0xB000: SetChr2(3, data); break;
        case 0xC000: ntReg[0] = data | 0x80; break;
        case 0xD000: ntReg[1] = data | 0x80; break;
        case 0xE000: ctrl = data; SetMirror(data & 3); break;
        case 0xF000: SetPrg16(0, data & 0x0F); prgRamOn = (data & 0x10) != 0; break;
        }
        return true;
    }
    void SaveExtra(StateWriter& w) const override { w.U8(ntReg[0]); w.U8(ntReg[1]); w.U8(ctrl); }
    void LoadExtra(StateReader& r) override { ntReg[0] = r.U8(); ntReg[1] = r.U8(); ctrl = r.U8(); }
};

class Vrc3 : public BankedMapper
{
    u16 irqReload = 0, irqCounter = 0;
    bool irqEnabled = false, irqEnableAfterAck = false, irqSmall = false;
public:
    Vrc3(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m)
    {
        SetPrg16(1, Last16());
    }
    void ClockCpuCycle() override
    {
        if (!irqEnabled) return;
        if (irqSmall)
        {
            if ((irqCounter & 0xFF) == 0xFF) { irqCounter = (u16)((irqCounter & 0xFF00) | (irqReload & 0xFF)); irqPending = true; }
            else irqCounter = (u16)((irqCounter & 0xFF00) | ((irqCounter & 0xFF) + 1));
        }
        else
        {
            if (irqCounter == 0xFFFF) { irqCounter = irqReload; irqPending = true; }
            else irqCounter++;
        }
    }
protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        switch (addr & 0xF000)
        {
        case 0x8000: irqReload = (u16)((irqReload & 0xFFF0) | (data & 0x0F)); break;
        case 0x9000: irqReload = (u16)((irqReload & 0xFF0F) | ((data & 0x0F) << 4)); break;
        case 0xA000: irqReload = (u16)((irqReload & 0xF0FF) | ((data & 0x0F) << 8)); break;
        case 0xB000: irqReload = (u16)((irqReload & 0x0FFF) | ((data & 0x0F) << 12)); break;
        case 0xC000:
            irqSmall = (data & 4) != 0; irqEnableAfterAck = (data & 1) != 0; irqEnabled = (data & 2) != 0;
            irqPending = false;
            if (irqEnabled) irqCounter = irqReload;
            break;
        case 0xD000: irqPending = false; irqEnabled = irqEnableAfterAck; break;
        case 0xF000: SetPrg16(0, data & 7); break;
        }
        return true;
    }
    void SaveExtra(StateWriter& w) const override
    {
        w.U16(irqReload); w.U16(irqCounter); w.Bool(irqEnabled); w.Bool(irqEnableAfterAck); w.Bool(irqSmall);
    }
    void LoadExtra(StateReader& r) override
    {
        irqReload = r.U16(); irqCounter = r.U16(); irqEnabled = r.Bool(); irqEnableAfterAck = r.Bool(); irqSmall = r.Bool();
    }
};

class BandaiFcg : public BankedMapper
{
    bool is153;
    u32 prgSel = 0, outer = 0;
    bool irqEnabled = false;
    u16 irqCounter = 0, irqLatch = 0;

    void SyncPrg153()
    {
        SetPrg16(0, (prgSel & 0x0F) | outer);
        SetPrg16(1, outer | 0x0F);
    }
public:
    BandaiFcg(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m, bool is153_ = false)
        : BankedMapper(std::move(p), std::move(c), r, m), is153(is153_)
    {
        if (is153) { hasPrgRam = true; SyncPrg153(); }
        else       { SetPrg16(1, Last16()); hasPrgRam = false; }
    }
    void ClockCpuCycle() override
    {
        if (!irqEnabled) return;
        if (irqCounter == 0) irqPending = true;
        irqCounter--;
    }
protected:
    bool ReadReg(u16 addr, u8& data) override
    {
        if (!is153 && addr >= 0x6000 && addr < 0x8000) { data = 0x00; return true; }
        return false;
    }
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < (is153 ? 0x8000 : 0x6000)) return false;
        switch (addr & 0x0F)
        {
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
            if (is153)
            {
                if ((addr & 0x0F) < 4) { outer = (u32)(data & 1) << 4; SyncPrg153(); }
            }
            else SetChr1(addr & 7, data);
            break;
        case 8:
            if (is153) { prgSel = data; SyncPrg153(); }
            else SetPrg16(0, data & 0x0F);
            break;
        case 9: SetMirror(data & 3); break;
        case 10: irqEnabled = (data & 1) != 0; irqCounter = irqLatch; irqPending = false; break;
        case 11: irqLatch = (u16)((irqLatch & 0xFF00) | data); break;
        case 12: irqLatch = (u16)((irqLatch & 0x00FF) | (data << 8)); break;
        default: break;
        }
        return true;
    }
    void SaveExtra(StateWriter& w) const override { w.Bool(irqEnabled); w.U16(irqCounter); w.U16(irqLatch); w.U32(prgSel); w.U32(outer); }
    void LoadExtra(StateReader& r) override { irqEnabled = r.Bool(); irqCounter = r.U16(); irqLatch = r.U16(); prgSel = r.U32(); outer = r.U32(); }
};

class JalecoSs88006 : public BankedMapper
{
    u8 prgReg[3] = { 0, 0, 0 };
    u8 chrReg[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    u16 irqReload = 0, irqCounter = 0, irqMask = 0xFFFF;
    bool irqEnabled = false;

    void Sync()
    {
        for (int i = 0; i < 3; i++) prg[i] = prgReg[i] & 0x3F;
        prg[3] = FromEnd8(1);
        for (int i = 0; i < 8; i++) chr[i] = chrReg[i];
    }
public:
    JalecoSs88006(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m) { Sync(); }

    void ClockCpuCycle() override
    {
        if (!irqEnabled) return;
        u16 c = (u16)(irqCounter & irqMask);
        c = (u16)((c - 1) & irqMask);
        irqCounter = (u16)((irqCounter & ~irqMask) | c);
        if (c == 0) irqPending = true;
    }
protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        int sub = addr & 3;
        switch (addr & 0xF000)
        {
        case 0x8000: case 0x9000:
        {
            if ((addr & 0xF000) == 0x9000 && sub >= 2) break;
            int idx = ((addr & 0xF000) == 0x8000) ? (sub >> 1) : 2;
            if (sub & 1) prgReg[idx] = (u8)((prgReg[idx] & 0x0F) | ((data & 0x0F) << 4));
            else         prgReg[idx] = (u8)((prgReg[idx] & 0xF0) | (data & 0x0F));
            break;
        }
        case 0xA000: case 0xB000: case 0xC000: case 0xD000:
        {
            int group = (((addr >> 12) & 7) - 2) * 2 + (sub >> 1);
            if (sub & 1) chrReg[group] = (u8)((chrReg[group] & 0x0F) | ((data & 0x0F) << 4));
            else         chrReg[group] = (u8)((chrReg[group] & 0xF0) | (data & 0x0F));
            break;
        }
        case 0xE000:
        {
            int shift = sub * 4;
            irqReload = (u16)((irqReload & ~(0xF << shift)) | ((data & 0x0F) << shift));
            break;
        }
        case 0xF000:
            if (sub == 0) { irqCounter = irqReload; irqPending = false; }
            else if (sub == 1)
            {
                irqEnabled = (data & 1) != 0;
                irqPending = false;
                irqMask = (data & 8) ? 0x000F : (data & 4) ? 0x00FF : (data & 2) ? 0x0FFF : 0xFFFF;
            }
            else if (sub == 2)
            {
                static const int mm[4] = { 1, 0, 2, 3 };
                SetMirror(mm[data & 3]);
            }
            break;
        }
        Sync();
        return true;
    }
    void SaveExtra(StateWriter& w) const override
    {
        for (int i = 0; i < 3; i++) w.U8(prgReg[i]);
        for (int i = 0; i < 8; i++) w.U8(chrReg[i]);
        w.U16(irqReload); w.U16(irqCounter); w.U16(irqMask); w.Bool(irqEnabled);
    }
    void LoadExtra(StateReader& r) override
    {
        for (int i = 0; i < 3; i++) prgReg[i] = r.U8();
        for (int i = 0; i < 8; i++) chrReg[i] = r.U8();
        irqReload = r.U16(); irqCounter = r.U16(); irqMask = r.U16(); irqEnabled = r.Bool();
    }
};

class TaitoX1005 : public BankedMapper
{
    bool is207;
    u8 ram[128];
    u8 ramPerm = 0;
    u8 chr0 = 0, chr1 = 0;

    void SyncChr(u8 c2, u8 c3, u8 c4, u8 c5)
    {
        u32 mask = is207 ? 0x7F : 0xFF;
        chr[0] = (chr0 & 0xFE) & mask; chr[1] = ((chr0 & 0xFE) + 1) & mask;
        chr[2] = (chr1 & 0xFE) & mask; chr[3] = ((chr1 & 0xFE) + 1) & mask;
        chr[4] = c2; chr[5] = c3; chr[6] = c4; chr[7] = c5;
        if (is207)
        {
            ntMap[0] = ntMap[1] = (u8)((chr0 >> 7) & 1);
            ntMap[2] = ntMap[3] = (u8)((chr1 >> 7) & 1);
        }
    }
    u8 c2 = 4, c3 = 5, c4 = 6, c5 = 7;
public:
    TaitoX1005(bool is207_, std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m), is207(is207_)
    {
        std::memset(ram, 0, sizeof(ram));
        hasPrgRam = false;
        prg[0] = 0; prg[1] = 1; prg[2] = 2; prg[3] = FromEnd8(1);
        if (is207) mirroring = Mirroring::CUSTOM;
        SyncChr(c2, c3, c4, c5);
    }
protected:
    bool ReadReg(u16 addr, u8& data) override
    {
        if (addr >= 0x7F00 && addr < 0x8000 && ramPerm == 0xA3) { data = ram[addr & 0x7F]; return true; }
        return false;
    }
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr >= 0x7F00 && addr < 0x8000)
        {
            if (ramPerm == 0xA3) ram[addr & 0x7F] = data;
            return true;
        }
        if (addr < 0x7EF0 || addr > 0x7EFF) return false;
        switch (addr)
        {
        case 0x7EF0: chr0 = data; break;
        case 0x7EF1: chr1 = data; break;
        case 0x7EF2: c2 = data; break;
        case 0x7EF3: c3 = data; break;
        case 0x7EF4: c4 = data; break;
        case 0x7EF5: c5 = data; break;
        case 0x7EF6: case 0x7EF7: if (!is207) SetMirror((data & 1) ? 0 : 1); break;
        case 0x7EF8: case 0x7EF9: ramPerm = data; break;
        case 0x7EFA: case 0x7EFB: prg[0] = data; break;
        case 0x7EFC: case 0x7EFD: prg[1] = data; break;
        case 0x7EFE: case 0x7EFF: prg[2] = data; break;
        }
        SyncChr(c2, c3, c4, c5);
        return true;
    }
    void SaveExtra(StateWriter& w) const override
    {
        w.Bytes(ram, sizeof(ram)); w.U8(ramPerm); w.U8(chr0); w.U8(chr1); w.U8(c2); w.U8(c3); w.U8(c4); w.U8(c5);
    }
    void LoadExtra(StateReader& r) override
    {
        r.Bytes(ram, sizeof(ram)); ramPerm = r.U8(); chr0 = r.U8(); chr1 = r.U8(); c2 = r.U8(); c3 = r.U8(); c4 = r.U8(); c5 = r.U8();
    }
};
