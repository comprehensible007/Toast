#include "mappers.h"
#include "mapper.h"
#include <cmath>
#include <cstring>
#include <vector>



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



class Mapper40 : public BankedMapper
{
    u16 irqCounter = 0;
public:
    Mapper40(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m)
    {
        hasPrgRam = false;
        prg[0] = 4; prg[1] = 5; prg[2] = 0; prg[3] = 7;
    }
    void ClockCpuCycle() override
    {
        if (irqCounter > 0)
        {
            irqCounter--;
            if (irqCounter == 0) irqPending = true;
        }
    }
protected:
    bool ReadReg(u16 addr, u8& data) override
    {
        if (addr >= 0x6000 && addr < 0x8000)
        {
            data = PrgByte((6 % NumPrg8()) * 8192 + (addr & 0x1FFF));
            return true;
        }
        return false;
    }
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        switch (addr & 0xE000)
        {
        case 0x8000: irqCounter = 0; irqPending = false; break;
        case 0xA000: irqCounter = 4096; break;
        case 0xE000: prg[2] = data & 7; break;
        }
        return true;
    }
    void SaveExtra(StateWriter& w) const override { w.U16(irqCounter); }
    void LoadExtra(StateReader& r) override { irqCounter = r.U16(); }
};

class Namco210 : public BankedMapper
{
    bool is340 = false;
public:
    Namco210(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m)
    {
        prg[3] = FromEnd8(1);
    }
protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        switch (addr & 0xF800)
        {
        case 0x8000: case 0x8800: case 0x9000: case 0x9800:
        case 0xA000: case 0xA800: case 0xB000: case 0xB800:
            SetChr1((addr >> 11) & 7, data);
            break;
        case 0xE000: case 0xE800: case 0xF000:
        {
            int slot = ((addr >> 11) & 3);
            prg[slot] = data & 0x3F;
            if ((addr & 0xF800) == 0xE000 || (addr & 0xF800) == 0xE800)
            {
                if ((data >> 6) != 0) is340 = true;
                if (is340)
                {
                    static const int mm[4] = { 2, 0, 3, 1 };
                    SetMirror(mm[(data >> 6) & 3]);
                }
            }
            break;
        }
        }
        return true;
    }
    void SaveExtra(StateWriter& w) const override { w.Bool(is340); }
    void LoadExtra(StateReader& r) override { is340 = r.Bool(); }
};

class TaitoX1017 : public BankedMapper
{
    u8 chrReg[6] = { 0, 2, 4, 5, 6, 7 };
    u8 prgReg[3] = { 0, 1, 2 };
    u8 ramPerm[3] = { 0, 0, 0 };
    bool chrInvert = false;
    u8 ram[0x1400];

    void Sync()
    {
        prg[0] = prgReg[0]; prg[1] = prgReg[1]; prg[2] = prgReg[2]; prg[3] = FromEnd8(1);
        u32 raw[8] = { (u32)(chrReg[0] & 0xFE), (u32)(chrReg[0] & 0xFE) + 1, (u32)(chrReg[1] & 0xFE), (u32)(chrReg[1] & 0xFE) + 1,
                       chrReg[2], chrReg[3], chrReg[4], chrReg[5] };
        for (int i = 0; i < 8; i++) chr[i] = raw[chrInvert ? (i ^ 4) : i];
    }
    bool RamAllowed(u16 addr) const
    {
        if (addr < 0x6800) return ramPerm[0] == 0xCA;
        if (addr < 0x7000) return ramPerm[1] == 0x69;
        return ramPerm[2] == 0x84;
    }
public:
    TaitoX1017(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m)
    {
        std::memset(ram, 0, sizeof(ram));
        hasPrgRam = false;
        Sync();
    }
protected:
    bool ReadReg(u16 addr, u8& data) override
    {
        if (addr >= 0x6000 && addr < 0x7400 && RamAllowed(addr)) { data = ram[addr - 0x6000]; return true; }
        return false;
    }
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr >= 0x6000 && addr < 0x7400)
        {
            if (RamAllowed(addr)) ram[addr - 0x6000] = data;
            return true;
        }
        if (addr < 0x7EF0 || addr > 0x7EFC) return false;
        switch (addr)
        {
        case 0x7EF0: case 0x7EF1: case 0x7EF2: case 0x7EF3: case 0x7EF4: case 0x7EF5: chrReg[addr - 0x7EF0] = data; break;
        case 0x7EF6: SetMirror((data & 1) ? 0 : 1); chrInvert = (data & 2) != 0; break;
        case 0x7EF7: case 0x7EF8: case 0x7EF9: ramPerm[addr - 0x7EF7] = data; break;
        case 0x7EFA: case 0x7EFB: case 0x7EFC: prgReg[addr - 0x7EFA] = data >> 2; break;
        }
        Sync();
        return true;
    }
    void SaveExtra(StateWriter& w) const override
    {
        w.Bytes(chrReg, 6); w.Bytes(prgReg, 3); w.Bytes(ramPerm, 3); w.Bool(chrInvert); w.Bytes(ram, sizeof(ram));
    }
    void LoadExtra(StateReader& r) override
    {
        r.Bytes(chrReg, 6); r.Bytes(prgReg, 3); r.Bytes(ramPerm, 3); chrInvert = r.Bool(); r.Bytes(ram, sizeof(ram));
    }
};

class Rambo1 : public BankedMapper
{
    u8 cmd = 0;
    u8 R[16] = { 0, 2, 4, 5, 6, 7, 0, 1, 1, 3, 0, 0, 0, 0, 0, 2 };
    bool chrK = false, prgMode = false, chrInv = false;
    u8 irqLatch = 0;
    int irqCounter = 0;
    bool irqReload = false, irqEnabled = false, irqCycleMode = false;
    int prescaler = 0;

    void Sync()
    {
        u32 p6 = R[6] & 0x3F, p7 = R[7] & 0x3F, p15 = R[15] & 0x3F;
        if (!prgMode) { prg[0] = p6;  prg[1] = p7; prg[2] = p15; }
        else          { prg[0] = p15; prg[1] = p6; prg[2] = p7; }
        prg[3] = FromEnd8(1);

        u32 base[8];
        if (!chrK)
        {
            base[0] = R[0] & 0xFE; base[1] = (R[0] & 0xFE) + 1; base[2] = R[1] & 0xFE; base[3] = (R[1] & 0xFE) + 1;
        }
        else
        {
            base[0] = R[0]; base[1] = R[8]; base[2] = R[1]; base[3] = R[9];
        }
        base[4] = R[2]; base[5] = R[3]; base[6] = R[4]; base[7] = R[5];
        for (int i = 0; i < 8; i++) chr[i] = base[chrInv ? (i ^ 4) : i];
    }

    void ClockCounter()
    {
        if (irqReload) { irqCounter = irqLatch; irqReload = false; }
        else if (irqCounter == 0) irqCounter = irqLatch;
        else irqCounter--;
        if (irqCounter == 0 && irqEnabled) irqPending = true;
    }
public:
    Rambo1(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m) { Sync(); }

    void OnScanline() override { if (!irqCycleMode) ClockCounter(); }
    void ClockCpuCycle() override
    {
        if (!irqCycleMode) return;
        if (++prescaler >= 4) { prescaler = 0; ClockCounter(); }
    }
protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        switch (addr & 0xE001)
        {
        case 0x8000: cmd = data & 0x0F; chrK = (data & 0x20) != 0; prgMode = (data & 0x40) != 0; chrInv = (data & 0x80) != 0; break;
        case 0x8001: if (cmd <= 9 || cmd == 15) R[cmd] = data; break;
        case 0xA000: SetMirror((data & 1) ? 1 : 0); break;
        case 0xC000: irqLatch = data; break;
        case 0xC001: irqCycleMode = (data & 1) != 0; irqReload = true; prescaler = 0; break;
        case 0xE000: irqEnabled = false; irqPending = false; break;
        case 0xE001: irqEnabled = true; break;
        }
        Sync();
        return true;
    }
    void SaveExtra(StateWriter& w) const override
    {
        w.U8(cmd); w.Bytes(R, 16); w.Bool(chrK); w.Bool(prgMode); w.Bool(chrInv);
        w.U8(irqLatch); w.S64(irqCounter); w.Bool(irqReload); w.Bool(irqEnabled); w.Bool(irqCycleMode); w.S64(prescaler);
    }
    void LoadExtra(StateReader& r) override
    {
        cmd = r.U8(); r.Bytes(R, 16); chrK = r.Bool(); prgMode = r.Bool(); chrInv = r.Bool();
        irqLatch = r.U8(); irqCounter = (int)r.S64(); irqReload = r.Bool(); irqEnabled = r.Bool(); irqCycleMode = r.Bool(); prescaler = (int)r.S64();
    }
};

class Vrc7 : public BankedMapper
{
    u8 prgReg[3] = { 0, 1, 2 };
    u8 chrReg[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    u8 irqLatch = 0, irqCounter = 0;
    bool irqEnabled = false, irqEnableAfterAck = false, irqCycleMode = false;
    int prescaler = 0;

    void Sync()
    {
        for (int i = 0; i < 3; i++) prg[i] = prgReg[i] & 0x3F;
        prg[3] = FromEnd8(1);
        for (int i = 0; i < 8; i++) chr[i] = chrReg[i];
    }
    void ClockCounter()
    {
        if (irqCounter == 0xFF) { irqCounter = irqLatch; irqPending = true; }
        else irqCounter++;
    }
public:
    Vrc7(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m) { Sync(); }

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
        bool second = (addr & 0x18) != 0;
        switch (addr & 0xF000)
        {
        case 0x8000: prgReg[second ? 1 : 0] = data; break;
        case 0x9000: if (!second) prgReg[2] = data; break;
        case 0xA000: chrReg[second ? 1 : 0] = data; break;
        case 0xB000: chrReg[second ? 3 : 2] = data; break;
        case 0xC000: chrReg[second ? 5 : 4] = data; break;
        case 0xD000: chrReg[second ? 7 : 6] = data; break;
        case 0xE000:
            if (second) irqLatch = data;
            else { SetMirror(data & 3); prgRamOn = (data & 0x80) != 0; }
            break;
        case 0xF000:
            if (second) { irqPending = false; irqEnabled = irqEnableAfterAck; }
            else
            {
                irqEnableAfterAck = (data & 1) != 0; irqEnabled = (data & 2) != 0; irqCycleMode = (data & 4) != 0;
                irqPending = false;
                if (irqEnabled) { irqCounter = irqLatch; prescaler = 0; }
            }
            break;
        }
        Sync();
        return true;
    }
    void SaveExtra(StateWriter& w) const override
    {
        w.Bytes(prgReg, 3); w.Bytes(chrReg, 8); w.U8(irqLatch); w.U8(irqCounter);
        w.Bool(irqEnabled); w.Bool(irqEnableAfterAck); w.Bool(irqCycleMode); w.S64(prescaler);
    }
    void LoadExtra(StateReader& r) override
    {
        r.Bytes(prgReg, 3); r.Bytes(chrReg, 8); irqLatch = r.U8(); irqCounter = r.U8();
        irqEnabled = r.Bool(); irqEnableAfterAck = r.Bool(); irqCycleMode = r.Bool(); prescaler = (int)r.S64();
    }
};

class Namco163 : public BankedMapper
{
    u8 chrReg[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    u8 ntReg[4] = { 0xE0, 0xE0, 0xE1, 0xE1 };
    u8 prgReg[3] = { 0, 1, 2 };
    u16 irqCounter = 0;
    bool irqEnabled = false;

    u8 soundRam[128];
    u8 soundAddr = 0;
    bool soundAutoInc = false;
    int soundDivider = 0;
    int currentChannel = 0;
    int chanOut[8];

    void Sync()
    {
        for (int i = 0; i < 3; i++) prg[i] = prgReg[i] & 0x3F;
        prg[3] = FromEnd8(1);
        for (int i = 0; i < 8; i++) chr[i] = chrReg[i];
        for (int t = 0; t < 4; t++) ntMap[t] = (u8)(ntReg[t] & 1);
    }

    int NumChannels() const { return ((soundRam[0x7F] >> 4) & 7) + 1; }

    void UpdateChannel(int ch)
    {
        int a = 0x40 + ch * 8;
        u32 freq = soundRam[a] | (soundRam[a + 2] << 8) | ((soundRam[a + 4] & 3) << 16);
        u32 phase = soundRam[a + 1] | (soundRam[a + 3] << 8) | (soundRam[a + 5] << 16);
        u32 length = 256 - (soundRam[a + 4] & 0xFC);
        u32 offset = soundRam[a + 6];
        int volume = soundRam[a + 7] & 0x0F;
        phase = (u32)(((uint64_t)phase + freq) % ((uint64_t)length << 16));
        soundRam[a + 1] = (u8)phase; soundRam[a + 3] = (u8)(phase >> 8); soundRam[a + 5] = (u8)(phase >> 16);
        u32 sa = ((phase >> 16) + offset) & 0xFF;
        int sample = (sa & 1) ? (soundRam[sa >> 1] >> 4) : (soundRam[sa >> 1] & 0x0F);
        chanOut[ch] = (sample - 8) * volume;
    }
public:
    Namco163(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m)
    {
        std::memset(soundRam, 0, sizeof(soundRam));
        for (int i = 0; i < 8; i++) chanOut[i] = 0;
        mirroring = Mirroring::CUSTOM;
        Sync();
    }

    bool NametableRead(u16 addr, u8& data) override
    {
        int t = (addr >> 10) & 3;
        if (ntReg[t] >= 0xE0) return false;
        data = ChrByte(((u32)ntReg[t] % NumChr1()) * 1024 + (addr & 0x3FF));
        return true;
    }
    bool NametableWrite(u16 addr, u8) override { return ntReg[(addr >> 10) & 3] < 0xE0; }

    void ClockCpuCycle() override
    {
        if (irqEnabled && irqCounter < 0x7FFF)
        {
            irqCounter++;
            if (irqCounter == 0x7FFF) irqPending = true;
        }
        if (++soundDivider >= 15)
        {
            soundDivider = 0;
            int n = NumChannels();
            int ch = 7 - (currentChannel % n);
            UpdateChannel(ch);
            currentChannel = (currentChannel + 1) % n;
        }
    }

    double ExpansionAudio() override
    {
        int n = NumChannels();
        int sum = 0;
        for (int i = 0; i < n; i++) sum += chanOut[7 - i];
        return (double)sum / (double)n * 0.0030;
    }
protected:
    bool ReadReg(u16 addr, u8& data) override
    {
        if (addr >= 0x4800 && addr < 0x5000)
        {
            data = soundRam[soundAddr & 0x7F];
            if (soundAutoInc) soundAddr = (u8)((soundAddr + 1) & 0x7F);
            return true;
        }
        if (addr >= 0x5000 && addr < 0x5800) { data = (u8)(irqCounter & 0xFF); return true; }
        if (addr >= 0x5800 && addr < 0x6000) { data = (u8)((irqCounter >> 8) | (irqEnabled ? 0x80 : 0)); return true; }
        return false;
    }
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr >= 0x4800 && addr < 0x5000)
        {
            soundRam[soundAddr & 0x7F] = data;
            if (soundAutoInc) soundAddr = (u8)((soundAddr + 1) & 0x7F);
            return true;
        }
        if (addr >= 0x5000 && addr < 0x5800) { irqCounter = (u16)((irqCounter & 0x7F00) | data); irqPending = false; return true; }
        if (addr >= 0x5800 && addr < 0x6000)
        {
            irqCounter = (u16)((irqCounter & 0x00FF) | ((data & 0x7F) << 8));
            irqEnabled = (data & 0x80) != 0; irqPending = false;
            return true;
        }
        if (addr < 0x8000) return false;
        switch (addr & 0xF800)
        {
        case 0x8000: case 0x8800: case 0x9000: case 0x9800:
        case 0xA000: case 0xA800: case 0xB000: case 0xB800:
            chrReg[(addr >> 11) & 7] = data; break;
        case 0xC000: case 0xC800: case 0xD000: case 0xD800:
            ntReg[(addr >> 11) & 3] = data; break;
        case 0xE000: prgReg[0] = data & 0x3F; break;
        case 0xE800: prgReg[1] = data & 0x3F; break;
        case 0xF000: prgReg[2] = data & 0x3F; break;
        case 0xF800: soundAddr = data & 0x7F; soundAutoInc = (data & 0x80) != 0; break;
        }
        Sync();
        return true;
    }
    void SaveExtra(StateWriter& w) const override
    {
        w.Bytes(chrReg, 8); w.Bytes(ntReg, 4); w.Bytes(prgReg, 3); w.U16(irqCounter); w.Bool(irqEnabled);
        w.Bytes(soundRam, 128); w.U8(soundAddr); w.Bool(soundAutoInc);
    }
    void LoadExtra(StateReader& r) override
    {
        r.Bytes(chrReg, 8); r.Bytes(ntReg, 4); r.Bytes(prgReg, 3); irqCounter = r.U16(); irqEnabled = r.Bool();
        r.Bytes(soundRam, 128); soundAddr = r.U8(); soundAutoInc = r.Bool();
    }
};

class Mmc5 : public Mapper
{
    std::vector<u8> ram;
    u8 exram[1024];

    u8 prgMode = 3, chrMode = 0, exMode = 0, ramProt1 = 0, ramProt2 = 0;
    u8 ntCfg = 0, fillTile = 0, fillAttr = 0;
    u8 prgReg[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    u8 ramBank6000 = 0;
    u16 chrA[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    u16 chrB[4] = { 0, 0, 0, 0 };
    u8 chrHigh = 0;
    bool lastWasB = false;
    u8 irqTarget = 0;
    bool irqEnabled = false, irqPending = false, inFrame = false;
    int scanCounter = 0;
    u8 mulA = 0xFF, mulB = 0xFF;
    u8 ppuCtrl = 0, ppuMask = 0;
    int fetchKind = 2;
    u16 lastTile = 0;

    struct Pulse
    {
        bool enabled = false, halt = false, constVol = false, startEnv = false;
        u8 duty = 0, volume = 0, decay = 0, envDivider = 0, length = 0, step = 0;
        u16 period = 0, timer = 0;
        int Output() const
        {
            static const u8 seq[4][8] = { {0,1,0,0,0,0,0,0}, {0,1,1,0,0,0,0,0}, {0,1,1,1,1,0,0,0}, {1,0,0,1,1,1,1,1} };
            if (!enabled || length == 0 || !seq[duty][step]) return 0;
            return constVol ? volume : decay;
        }
        void ClockTimer()
        {
            if (timer == 0) { timer = period; step = (u8)((step + 1) & 7); }
            else timer--;
        }
        void ClockEnvelope()
        {
            if (startEnv) { startEnv = false; decay = 15; envDivider = volume; }
            else if (envDivider == 0)
            {
                envDivider = volume;
                if (decay > 0) decay--; else if (halt) decay = 15;
            }
            else envDivider--;
        }
        void ClockLength() { if (!halt && length > 0) length--; }
    } pulse[2];
    u8 pcm = 0;
    int audioCycle = 0, frameDivider = 0;

    static const u8* LengthTable()
    {
        static const u8 t[32] = { 10,254,20,2,40,4,80,6,160,8,60,10,14,12,26,14, 12,16,24,18,48,20,96,22,192,24,72,26,16,28,32,30 };
        return t;
    }

    void ResolvePrg(int w, bool& rom, u32& bank) const
    {
        switch (prgMode & 3)
        {
        case 0: rom = true; bank = ((prgReg[3] & 0x7F) >> 2) * 4 + w; break;
        case 1:
            if (w < 2) { rom = (prgReg[1] & 0x80) != 0; bank = ((prgReg[1] & 0x7F) >> 1) * 2 + (w & 1); }
            else       { rom = true; bank = ((prgReg[3] & 0x7F) >> 1) * 2 + (w & 1); }
            break;
        case 2:
            if (w < 2)       { rom = (prgReg[1] & 0x80) != 0; bank = ((prgReg[1] & 0x7F) >> 1) * 2 + (w & 1); }
            else if (w == 2) { rom = (prgReg[2] & 0x80) != 0; bank = prgReg[2] & 0x7F; }
            else             { rom = true; bank = prgReg[3] & 0x7F; }
            break;
        default:
            if (w < 3) { rom = (prgReg[w] & 0x80) != 0; bank = prgReg[w] & 0x7F; }
            else       { rom = true; bank = prgReg[3] & 0x7F; }
            break;
        }
    }

    bool RamWritable() const { return ramProt1 == 2 && ramProt2 == 1; }
    u32 NumPrg8() const { u32 n = Prg8kCount(); return n ? n : 1; }
    u32 NumChr1() const { u32 n = Chr1kCount(); return n ? n : 1; }

    u32 ChrBank1k(int w, bool useB) const
    {
        u32 b;
        if (!useB)
        {
            switch (chrMode & 3)
            {
            case 3: b = chrA[w]; break;
            case 2: b = (u32)chrA[(w & ~1) | 1] * 2 + (w & 1); break;
            case 1: b = (u32)chrA[(w < 4) ? 3 : 7] * 4 + (w & 3); break;
            default: b = (u32)chrA[7] * 8 + w; break;
            }
        }
        else
        {
            switch (chrMode & 3)
            {
            case 3: b = chrB[w & 3]; break;
            case 2: b = (u32)chrB[((w >> 1) & 1) * 2 + 1] * 2 + (w & 1); break;
            case 1: b = (u32)chrB[3] * 4 + (w & 3); break;
            default: b = (u32)chrB[3] * 8 + w; break;
            }
        }
        return b;
    }

    u32 ChrOffset(u16 addr) const
    {
        if (exMode == 1 && fetchKind == 0)
        {
            u32 bank4k = (u32)(exram[lastTile & 0x3FF] & 0x3F) | ((u32)chrHigh << 6);
            u32 n4 = NumChr1() / 4; if (!n4) n4 = 1;
            return (bank4k % n4) * 4096 + (addr & 0xFFF);
        }
        bool useB;
        if (fetchKind == 1) useB = false;
        else if (fetchKind == 0) useB = ((ppuCtrl & 0x20) != 0) && ((ppuMask & 0x18) != 0);
        else useB = lastWasB;
        u32 b = ChrBank1k(addr >> 10, useB);
        return (b % NumChr1()) * 1024 + (addr & 0x3FF);
    }

    void SyncNt()
    {
        for (int t = 0; t < 4; t++) ntMap[t] = (u8)(((ntCfg >> (t * 2)) & 3) & 1);
    }

public:
    Mmc5(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : Mapper(std::move(p), std::move(c), r, m), ram(65536, 0)
    {
        std::memset(exram, 0, sizeof(exram));
        mirroring = Mirroring::CUSTOM;
        SyncNt();
    }

    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x5000 && addr < 0x6000)
        {
            switch (addr)
            {
            case 0x5015: data = (u8)((pulse[0].length ? 1 : 0) | (pulse[1].length ? 2 : 0)); return true;
            case 0x5204: data = (u8)((irqPending ? 0x80 : 0) | (inFrame ? 0x40 : 0)); irqPending = false; return true;
            case 0x5205: data = (u8)((mulA * mulB) & 0xFF); return true;
            case 0x5206: data = (u8)(((mulA * mulB) >> 8) & 0xFF); return true;
            default: break;
            }
            if (addr >= 0x5C00 && exMode >= 2) { data = exram[addr & 0x3FF]; return true; }
            return false;
        }
        if (addr < 0x6000) return false;
        if (addr < 0x8000)
        {
            data = ram[((u32)(ramBank6000 & 7) * 8192 + (addr & 0x1FFF)) & 0xFFFF];
            return true;
        }
        bool rom; u32 bank;
        ResolvePrg((addr - 0x8000) >> 13, rom, bank);
        if (rom) data = PrgByte((bank % NumPrg8()) * 8192 + (addr & 0x1FFF));
        else     data = ram[((bank & 7) * 8192 + (addr & 0x1FFF)) & 0xFFFF];
        return true;
    }

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000 && addr < 0x4000)
        {
            int reg = addr & 7;
            if (reg == 0) ppuCtrl = data;
            else if (reg == 1) ppuMask = data;
            return false;
        }
        if (addr >= 0x5000 && addr < 0x6000)
        {
            if (addr >= 0x5C00)
            {
                if (exMode != 3) exram[addr & 0x3FF] = data;
                return true;
            }
            switch (addr)
            {
            case 0x5000: pulse[0].duty = data >> 6; pulse[0].halt = (data & 0x20) != 0; pulse[0].constVol = (data & 0x10) != 0; pulse[0].volume = data & 0x0F; break;
            case 0x5002: pulse[0].period = (u16)((pulse[0].period & 0x700) | data); break;
            case 0x5003:
                pulse[0].period = (u16)((pulse[0].period & 0x0FF) | ((data & 7) << 8));
                if (pulse[0].enabled) pulse[0].length = LengthTable()[data >> 3];
                pulse[0].step = 0; pulse[0].startEnv = true; break;
            case 0x5004: pulse[1].duty = data >> 6; pulse[1].halt = (data & 0x20) != 0; pulse[1].constVol = (data & 0x10) != 0; pulse[1].volume = data & 0x0F; break;
            case 0x5006: pulse[1].period = (u16)((pulse[1].period & 0x700) | data); break;
            case 0x5007:
                pulse[1].period = (u16)((pulse[1].period & 0x0FF) | ((data & 7) << 8));
                if (pulse[1].enabled) pulse[1].length = LengthTable()[data >> 3];
                pulse[1].step = 0; pulse[1].startEnv = true; break;
            case 0x5010: break;
            case 0x5011: pcm = data; break;
            case 0x5015:
                pulse[0].enabled = (data & 1) != 0; pulse[1].enabled = (data & 2) != 0;
                if (!pulse[0].enabled) pulse[0].length = 0;
                if (!pulse[1].enabled) pulse[1].length = 0;
                break;
            case 0x5100: prgMode = data & 3; break;
            case 0x5101: chrMode = data & 3; break;
            case 0x5102: ramProt1 = data & 3; break;
            case 0x5103: ramProt2 = data & 3; break;
            case 0x5104: exMode = data & 3; break;
            case 0x5105: ntCfg = data; SyncNt(); break;
            case 0x5106: fillTile = data; break;
            case 0x5107: fillAttr = data & 3; break;
            case 0x5113: ramBank6000 = data & 7; break;
            case 0x5114: case 0x5115: case 0x5116: case 0x5117: prgReg[addr - 0x5114] = data; break;
            case 0x5120: case 0x5121: case 0x5122: case 0x5123: case 0x5124: case 0x5125: case 0x5126: case 0x5127:
                chrA[addr - 0x5120] = (u16)(data | ((chrHigh & 3) << 8)); lastWasB = false; break;
            case 0x5128: case 0x5129: case 0x512A: case 0x512B:
                chrB[addr - 0x5128] = (u16)(data | ((chrHigh & 3) << 8)); lastWasB = true; break;
            case 0x5130: chrHigh = data & 3; break;
            case 0x5203: irqTarget = data; break;
            case 0x5204: irqEnabled = (data & 0x80) != 0; break;
            case 0x5205: mulA = data; break;
            case 0x5206: mulB = data; break;
            default: break;
            }
            return true;
        }
        if (addr < 0x6000) return false;
        if (addr < 0x8000)
        {
            if (RamWritable()) ram[((u32)(ramBank6000 & 7) * 8192 + (addr & 0x1FFF)) & 0xFFFF] = data;
            return true;
        }
        bool rom; u32 bank;
        ResolvePrg((addr - 0x8000) >> 13, rom, bank);
        if (!rom && RamWritable()) ram[((bank & 7) * 8192 + (addr & 0x1FFF)) & 0xFFFF] = data;
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
        u32 off = ChrOffset(addr);
        if (off < chrROM.size()) chrROM[off] = data;
        return true;
    }

    bool NametableRead(u16 addr, u8& data) override
    {
        int t = (addr >> 10) & 3;
        u16 off = addr & 0x3FF;
        bool isAttr = off >= 0x3C0;
        if (fetchKind == 0 && !isAttr) lastTile = off;
        if (exMode == 1 && fetchKind == 0 && isAttr)
        {
            data = (u8)(((exram[lastTile & 0x3FF] >> 6) & 3) * 0x55);
            return true;
        }
        switch ((ntCfg >> (t * 2)) & 3)
        {
        case 2: data = (exMode < 2) ? exram[off] : 0; return true;
        case 3: data = isAttr ? (u8)(fillAttr * 0x55) : fillTile; return true;
        default: return false;
        }
    }
    bool NametableWrite(u16 addr, u8 data) override
    {
        int t = (addr >> 10) & 3;
        switch ((ntCfg >> (t * 2)) & 3)
        {
        case 2: if (exMode < 2) exram[addr & 0x3FF] = data; return true;
        case 3: return true;
        default: return false;
        }
    }

    void SetFetchKind(int k) override { fetchKind = k; }

    void OnPpuScanlineStart(int sl) override
    {
        if (sl == 0 || !inFrame) { inFrame = true; scanCounter = 0; }
        else
        {
            scanCounter++;
            if (scanCounter == irqTarget) irqPending = true;
        }
    }
    void OnPpuVblank() override { inFrame = false; scanCounter = 0; }

    bool IRQState() const override { return irqPending && irqEnabled; }
    void IRQClear() override { irqPending = false; }

    void ClockCpuCycle() override
    {
        if ((++audioCycle & 1) == 0) { pulse[0].ClockTimer(); pulse[1].ClockTimer(); }
        if (++frameDivider >= 7457)
        {
            frameDivider = 0;
            for (int i = 0; i < 2; i++) { pulse[i].ClockEnvelope(); pulse[i].ClockLength(); }
        }
    }

    double ExpansionAudio() override
    {
        double p = pulse[0].Output() + pulse[1].Output();
        double pulseOut = (p == 0) ? 0.0 : 95.88 / ((8128.0 / p) + 100.0);
        return pulseOut + pcm * 0.0035;
    }

    void SaveState(StateWriter& w) const override
    {
        Mapper::SaveState(w);
        w.Bytes(ram.data(), ram.size()); w.Bytes(exram, sizeof(exram));
        w.U8(prgMode); w.U8(chrMode); w.U8(exMode); w.U8(ramProt1); w.U8(ramProt2);
        w.U8(ntCfg); w.U8(fillTile); w.U8(fillAttr);
        w.Bytes(prgReg, 4); w.U8(ramBank6000);
        for (int i = 0; i < 8; i++) w.U16(chrA[i]);
        for (int i = 0; i < 4; i++) w.U16(chrB[i]);
        w.U8(chrHigh); w.Bool(lastWasB);
        w.U8(irqTarget); w.Bool(irqEnabled); w.Bool(irqPending); w.Bool(inFrame); w.S64(scanCounter);
        w.U8(mulA); w.U8(mulB); w.U8(ppuCtrl); w.U8(ppuMask);
        w.U8(pcm);
        for (int i = 0; i < 2; i++)
        {
            const Pulse& q = pulse[i];
            w.Bool(q.enabled); w.Bool(q.halt); w.Bool(q.constVol); w.Bool(q.startEnv);
            w.U8(q.duty); w.U8(q.volume); w.U8(q.decay); w.U8(q.envDivider); w.U8(q.length); w.U8(q.step);
            w.U16(q.period); w.U16(q.timer);
        }
    }
    void LoadState(StateReader& r) override
    {
        Mapper::LoadState(r);
        r.Bytes(ram.data(), ram.size()); r.Bytes(exram, sizeof(exram));
        prgMode = r.U8(); chrMode = r.U8(); exMode = r.U8(); ramProt1 = r.U8(); ramProt2 = r.U8();
        ntCfg = r.U8(); fillTile = r.U8(); fillAttr = r.U8();
        r.Bytes(prgReg, 4); ramBank6000 = r.U8();
        for (int i = 0; i < 8; i++) chrA[i] = r.U16();
        for (int i = 0; i < 4; i++) chrB[i] = r.U16();
        chrHigh = r.U8(); lastWasB = r.Bool();
        irqTarget = r.U8(); irqEnabled = r.Bool(); irqPending = r.Bool(); inFrame = r.Bool(); scanCounter = (int)r.S64();
        mulA = r.U8(); mulB = r.U8(); ppuCtrl = r.U8(); ppuMask = r.U8();
        pcm = r.U8();
        for (int i = 0; i < 2; i++)
        {
            Pulse& q = pulse[i];
            q.enabled = r.Bool(); q.halt = r.Bool(); q.constVol = r.Bool(); q.startEnv = r.Bool();
            q.duty = r.U8(); q.volume = r.U8(); q.decay = r.U8(); q.envDivider = r.U8(); q.length = r.U8(); q.step = r.U8();
            q.period = r.U16(); q.timer = r.U16();
        }
        SyncNt();
    }
};



class Mapper31 : public Mapper
{
    u8 reg[8];
public:
    Mapper31(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : Mapper(std::move(p), std::move(c), r, m)
    {
        for (int i = 0; i < 8; i++) reg[i] = 0;
        reg[7] = 0xFF;
    }
    bool CpuRead(u16 addr, u8& data) override
    {
        if (addr < 0x6000) return false;
        if (addr < 0x8000) { data = prgRAM[addr - 0x6000]; return true; }
        u32 n4 = (u32)(prgROM.size() / 4096); if (!n4) n4 = 1;
        data = PrgByte((reg[(addr >> 12) & 7] % n4) * 4096 + (addr & 0xFFF));
        return true;
    }
    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x5000 && addr < 0x6000)
        {
            if ((addr & 0x0FF8) == 0x0FF8) reg[addr & 7] = data;
            return true;
        }
        if (addr >= 0x6000 && addr < 0x8000) { prgRAM[addr - 0x6000] = data; return true; }
        return addr >= 0x8000;
    }
    bool PpuRead(u16 addr, u8& data) override { if (addr >= 0x2000) return false; data = ChrByte(addr); return true; }
    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000 || !chrIsRAM) return false;
        if (addr < chrROM.size()) chrROM[addr] = data;
        return true;
    }
    void SaveState(StateWriter& w) const override { Mapper::SaveState(w); w.Bytes(reg, 8); }
    void LoadState(StateReader& r) override { Mapper::LoadState(r); r.Bytes(reg, 8); }
};

class MultiLatch : public BankedMapper
{
    int id;
    u32 s0 = 0, s1 = 0;
    u16 chrLo[8], chrHi[8];
    u16 irqCounter = 0;
    bool irqEnabled = false;
    u8 cmd = 0;
    u8 reg112[8];

    void UpdateChr156() { for (int i = 0; i < 8; i++) chr[i] = (u32)chrLo[i] | ((u32)chrHi[i] << 8); }
    void Update112()
    {
        prg[0] = reg112[0]; prg[1] = reg112[1];
        SetChr2(0, reg112[2] >> 1); SetChr2(1, reg112[3] >> 1);
        chr[4] = reg112[4]; chr[5] = reg112[5]; chr[6] = reg112[6]; chr[7] = reg112[7];
    }
public:
    MultiLatch(int id_, std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m), id(id_)
    {
        for (int i = 0; i < 8; i++) { chrLo[i] = (u16)i; chrHi[i] = 0; reg112[i] = (u8)i; }
        switch (id)
        {
        case 38: case 133: case 143: case 145: case 148: case 149: case 107: case 240: case 241: case 242: case 244:
        case 46: case 41: case 200: case 201: case 202: case 203: case 212: case 225: case 255: case 228: case 229:
            if (id != 241 && id != 242 && id != 240) hasPrgRam = false;
            break;
        case 42:
            hasPrgRam = false;
            prg[0] = FromEnd8(4); prg[1] = FromEnd8(3); prg[2] = FromEnd8(2); prg[3] = FromEnd8(1);
            break;
        case 50:
            hasPrgRam = false;
            prg[0] = 8; prg[1] = 9; prg[2] = 0; prg[3] = 0x0B;
            break;
        case 91:
            hasPrgRam = false;
            prg[0] = 0; prg[1] = 1; prg[2] = FromEnd8(2); prg[3] = FromEnd8(1);
            break;
        case 112:
            prg[0] = 0; prg[1] = 1; prg[2] = FromEnd8(2); prg[3] = FromEnd8(1);
            break;
        case 156:
            SetPrg16(1, Last16());
            hasPrgRam = false;
            break;
        case 246:
            prg[3] = FromEnd8(1);
            break;
        default: break;
        }
        if (id == 156) UpdateChr156();
    }

    void ClockCpuCycle() override
    {
        if (id == 42)
        {
            if (irqEnabled)
            {
                irqCounter = (u16)((irqCounter + 1) & 0x7FFF);
                if (irqCounter == 0x6000) irqPending = true;
            }
        }
        else if (id == 50)
        {
            if (irqEnabled)
            {
                irqCounter++;
                if (irqCounter == 4096) { irqPending = true; irqEnabled = false; }
            }
        }
    }
    void OnScanline() override
    {
        if (id == 91 && irqEnabled)
        {
            if (++irqCounter >= 8) { irqPending = true; }
        }
    }

protected:
    bool ReadReg(u16 addr, u8& data) override
    {
        if (id == 143 && addr >= 0x4020 && addr < 0x6000 && (addr & 0xE100) == 0x4100) { data = (u8)((~addr) & 0x3F); return true; }
        if (id == 42 && addr >= 0x6000 && addr < 0x8000) { data = PrgByte(((u32)s0 % NumPrg8()) * 8192 + (addr & 0x1FFF)); return true; }
        if (id == 50 && addr >= 0x6000 && addr < 0x8000) { data = PrgByte((0x0F % NumPrg8()) * 8192 + (addr & 0x1FFF)); return true; }
        return false;
    }

    bool WriteReg(u16 addr, u8 data) override
    {
        const bool hi = addr >= 0x8000;
        const bool inLow = addr >= 0x4020 && addr < 0x6000;
        switch (id)
        {
        case 38:
            if (addr >= 0x7000 && addr < 0x8000) { SetPrg32(data & 3); SetChr8((data >> 2) & 3); return true; }
            return false;
        case 41:
            if (addr >= 0x6000 && addr < 0x6800)
            {
                s0 = addr; s1 = 0;
                SetPrg32(addr & 7); SetMirror((addr & 8) ? 1 : 0); SetChr8((((addr >> 4) & 3) << 2));
                return true;
            }
            if (hi && (s0 & 4)) { s1 = data & 3; SetChr8((((s0 >> 4) & 3) << 2) | s1); }
            return hi;
        case 46:
            if (addr >= 0x6000 && addr < 0x8000) s0 = data;
            else if (hi) s1 = data;
            else return false;
            SetPrg32(((s0 & 0x0F) << 1) | (s1 & 1));
            SetChr8(((s0 >> 4) << 3) | ((s1 >> 4) & 7));
            return true;
        case 107: if (hi) { SetPrg32(data >> 1); SetChr8(data); } return hi;
        case 133: if (inLow && (addr & 0xE100) == 0x4100) { SetPrg32((data >> 2) & 1); SetChr8(data & 3); return true; } return inLow;
        case 145: if (inLow && (addr & 0xE100) == 0x4100) { SetChr8((data >> 7) & 1); return true; } return inLow;
        case 143: return inLow;
        case 148: if (hi) { SetPrg32((data >> 3) & 1); SetChr8(data & 7); } return hi;
        case 149: if (hi) { SetChr8((data >> 7) & 1); } return hi;
        case 200:
            if (!hi) return false;
            { u32 b = addr & 7; SetPrg16(0, b); SetPrg16(1, b); SetChr8(b); SetMirror((addr & 8) ? 0 : 1); }
            return true;
        case 201: if (hi) { SetPrg32(addr & 0xFF); SetChr8(addr & 0xFF); } return hi;
        case 202:
            if (!hi) return false;
            {
                u32 b = (addr >> 1) & 7; SetChr8(b);
                if ((addr & 9) == 9) SetPrg32(b >> 1); else { SetPrg16(0, b); SetPrg16(1, b); }
                SetMirror((addr & 1) ? 1 : 0);
            }
            return true;
        case 203: if (hi) { SetPrg16(0, data >> 2); SetPrg16(1, data >> 2); SetChr8(data & 3); } return hi;
        case 212:
            if (!hi) return false;
            {
                u32 b = addr & 7; SetChr8(b);
                if (addr & 0x4000) SetPrg32(b >> 1); else { SetPrg16(0, b); SetPrg16(1, b); }
                SetMirror((addr & 8) ? 1 : 0);
            }
            return true;
        case 225: case 255:
            if (!hi) return false;
            {
                u32 h = (addr >> 14) & 1;
                u32 p = ((addr >> 6) & 0x3F) | (h << 6);
                u32 c = (addr & 0x3F) | (h << 6);
                SetChr8(c);
                if (addr & 0x1000) { SetPrg16(0, p); SetPrg16(1, p); }
                else { SetPrg16(0, p & ~1u); SetPrg16(1, p | 1u); }
                SetMirror((addr & 0x2000) ? 1 : 0);
            }
            return true;
        case 228:
            if (!hi) return false;
            {
                u32 sel = (addr >> 11) & 3, chip = (sel == 3) ? 2 : sel;
                u32 p = ((addr >> 6) & 0x1F) | (chip << 5);
                SetChr8(((addr & 0x0F) << 2) | (data & 3));
                if (addr & 0x20) { SetPrg16(0, p); SetPrg16(1, p); }
                else { SetPrg16(0, p & ~1u); SetPrg16(1, p | 1u); }
                SetMirror((addr & 0x2000) ? 1 : 0);
            }
            return true;
        case 229:
            if (!hi) return false;
            SetChr8(addr & 0xFF);
            if ((addr & 0x1E) == 0) SetPrg32(0); else { SetPrg16(0, addr & 0x1F); SetPrg16(1, addr & 0x1F); }
            SetMirror((addr & 0x20) ? 1 : 0);
            return true;
        case 240: if (inLow) { SetPrg32(data >> 4); SetChr8(data & 0x0F); return true; } return false;
        case 241: if (hi) SetPrg32(data); return hi;
        case 242: if (hi) { SetPrg32((addr >> 3) & 0x0F); SetMirror((addr & 2) ? 1 : 0); } return hi;
        case 244:
            if (!hi) return false;
            if (addr >= 0x8065 && addr <= 0x80A4) SetPrg32((addr - 0x8065) & 3);
            else if (addr >= 0x80A5 && addr <= 0x80E4) SetChr8((addr - 0x80A5) & 7);
            return true;
        case 246:
            if (addr >= 0x6000 && addr < 0x6800)
            {
                int n = addr & 7;
                if (n < 4) prg[n] = data; else SetChr2(n - 4, data);
                return true;
            }
            return false;
        case 156:
            if (!hi) return false;
            if (addr >= 0xC000 && addr <= 0xC003) { chrLo[addr & 3] = data; UpdateChr156(); }
            else if (addr >= 0xC004 && addr <= 0xC007) { chrLo[4 + (addr & 3)] = data; UpdateChr156(); }
            else if (addr >= 0xC008 && addr <= 0xC00B) { chrHi[addr & 3] = data; UpdateChr156(); }
            else if (addr >= 0xC00C && addr <= 0xC00F) { chrHi[4 + (addr & 3)] = data; UpdateChr156(); }
            else if (addr == 0xC010) SetPrg16(0, data);
            else if (addr == 0xC014) SetMirror((data & 1) ? 1 : 0);
            return true;
        case 42:
            if (!hi) return false;
            switch (addr & 0xE003)
            {
            case 0x8000: SetChr8(data & 0x0F); break;
            case 0xE000: s0 = data & 0x0F; break;
            case 0xE001: SetMirror((data & 8) ? 1 : 0); break;
            case 0xE002:
                irqEnabled = (data & 2) != 0;
                if (!irqEnabled) { irqCounter = 0; irqPending = false; }
                break;
            }
            return true;
        case 50:
            if (!inLow) return hi;
            if ((addr & 0x4120) == 0x4120) { irqEnabled = (data & 1) != 0; if (!irqEnabled) { irqCounter = 0; irqPending = false; } }
            else if ((addr & 0x4120) == 0x4020) prg[2] = (data & 8) | ((data & 1) << 2) | ((data & 6) >> 1);
            return true;
        case 91:
            if (addr >= 0x6000 && addr < 0x8000)
            {
                switch (addr & 0xF003)
                {
                case 0x6000: case 0x6001: case 0x6002: case 0x6003: SetChr2(addr & 3, data); break;
                case 0x7000: prg[0] = data & 0x0F; break;
                case 0x7001: prg[1] = data & 0x0F; break;
                case 0x7002: irqEnabled = false; irqCounter = 0; irqPending = false; break;
                case 0x7003: irqEnabled = true; break;
                }
                return true;
            }
            return false;
        case 112:
            if (!hi) return false;
            switch (addr & 0xE000)
            {
            case 0x8000: cmd = data & 7; break;
            case 0xA000: reg112[cmd] = data; Update112(); break;
            case 0xE000: SetMirror((data & 1) ? 1 : 0); break;
            }
            return true;
        default: return false;
        }
    }
    void SaveExtra(StateWriter& w) const override
    {
        w.U32(s0); w.U32(s1); w.U16(irqCounter); w.Bool(irqEnabled); w.U8(cmd);
        for (int i = 0; i < 8; i++) { w.U16(chrLo[i]); w.U16(chrHi[i]); w.U8(reg112[i]); }
    }
    void LoadExtra(StateReader& r) override
    {
        s0 = r.U32(); s1 = r.U32(); irqCounter = r.U16(); irqEnabled = r.Bool(); cmd = r.U8();
        for (int i = 0; i < 8; i++) { chrLo[i] = r.U16(); chrHi[i] = r.U16(); reg112[i] = r.U8(); }
    }
};

class Mmc3Gen : public BankedMapper
{
    int kind;
    u8 bankSel = 0;
    u8 R[8] = { 0, 2, 4, 5, 6, 7, 0, 1 };
    u8 irqLatch = 0, irqCounter = 0;
    bool irqReload = false, irqEnabled = false;
    u32 prgAnd = 0xFF, prgOr = 0, chrAnd = 0xFF, chrOr = 0;
    u32 prg32 = 0;
    u8 outer = 0;
    std::vector<u8> chrRam;
    static const u32 kRam = 0x80000000u;

    void SetOuter(u32 pa, u32 po, u32 ca, u32 co) { prgAnd = pa; prgOr = po; chrAnd = ca; chrOr = co; }

    void Sync()
    {
        if (kind == 189) SetPrg32(prg32);
        else
        {
            bool mode = (bankSel & 0x40) != 0;
            u32 pm = (kind == 198) ? 0xFFu : 0x3Fu;
            u32 raw[4] = { mode ? 0xFEu : (u32)(R[6] & pm), (u32)(R[7] & pm), mode ? (u32)(R[6] & pm) : 0xFEu, 0xFFu };
            for (int i = 0; i < 4; i++)
            {
                u32 v = (raw[i] & prgAnd) | prgOr;
                if (kind == 198 && v >= 0x50) v &= 0x4F;
                prg[i] = v;
            }
        }
        u32 base[8] = { (u32)(R[0] & 0xFE), (u32)(R[0] & 0xFE) + 1, (u32)(R[1] & 0xFE), (u32)(R[1] & 0xFE) + 1, R[2], R[3], R[4], R[5] };
        bool inv = (bankSel & 0x80) != 0;
        for (int i = 0; i < 8; i++)
        {
            u32 rawc = base[inv ? (i ^ 4) : i];
            u32 cb = (rawc & chrAnd) | chrOr;
            u32 ramIdx = 0; bool ram = false;
            switch (kind)
            {
            case 74:  if (cb == 8 || cb == 9) { ram = true; ramIdx = cb - 8; } break;
            case 191: if (rawc & 0x80) { ram = true; ramIdx = cb & 1; } break;
            case 192: if ((cb & 0xFC) == 0x08) { ram = true; ramIdx = cb & 3; } break;
            case 194: if ((cb & 0xFE) == 0x00) { ram = true; ramIdx = cb & 1; } break;
            case 195: if ((cb & 0xFC) == 0x00) { ram = true; ramIdx = cb & 3; } break;
            default: break;
            }
            chr[i] = ram ? (kRam | ramIdx) : cb;
        }
    }

public:
    Mmc3Gen(int kind_, std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m), kind(kind_), chrRam(8192, 0)
    {
        if (kind == 47) { SetOuter(0x0F, 0, 0x7F, 0); hasPrgRam = false; }
        if (kind == 44) SetOuter(0x0F, 0, 0x7F, 0);
        if (kind == 205) { SetOuter(0x1F, 0, 0xFF, 0); }
        if (kind == 189 || kind == 205) hasPrgRam = false;
        Sync();
    }

    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 b = chr[addr >> 10];
        if (b & kRam) { data = chrRam[(b & 7) * 1024 + (addr & 0x3FF)]; return true; }
        data = ChrByte((b % NumChr1()) * 1024 + (addr & 0x3FF));
        return true;
    }
    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        u32 b = chr[addr >> 10];
        if (b & kRam) { chrRam[(b & 7) * 1024 + (addr & 0x3FF)] = data; return true; }
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
        if (kind == 189 && addr >= 0x4120 && addr < 0x8000)
        {
            prg32 = (data & 0x0F) | (data >> 4);
            Sync();
            return true;
        }
        if (addr >= 0x6000 && addr < 0x8000)
        {
            if (kind == 47) { outer = data & 1; SetOuter(0x0F, (u32)outer << 4, 0x7F, (u32)outer << 7); Sync(); return true; }
            if (kind == 205)
            {
                switch (data & 3)
                {
                case 0: SetOuter(0x1F, 0x00, 0xFF, 0x000); break;
                case 1: SetOuter(0x1F, 0x10, 0xFF, 0x080); break;
                case 2: SetOuter(0x0F, 0x20, 0x7F, 0x100); break;
                default: SetOuter(0x0F, 0x30, 0x7F, 0x180); break;
                }
                Sync();
                return true;
            }
            return false;
        }
        if (addr < 0x8000) return false;
        switch (addr & 0xE001)
        {
        case 0x8000: bankSel = data; break;
        case 0x8001: R[bankSel & 7] = data; break;
        case 0xA000: SetMirror((data & 1) ? 1 : 0); break;
        case 0xA001:
            if (kind == 44)
            {
                u32 block = data & 7; if (block == 7) block = 6;
                SetOuter(block >= 6 ? 0x1F : 0x0F, block << 4, block >= 6 ? 0xFF : 0x7F, block << 7);
            }
            break;
        case 0xC000: irqLatch = data; break;
        case 0xC001: irqCounter = 0; irqReload = true; break;
        case 0xE000: irqEnabled = false; irqPending = false; break;
        case 0xE001: irqEnabled = true; break;
        }
        Sync();
        return true;
    }
    void SaveExtra(StateWriter& w) const override
    {
        w.U8(bankSel); w.Bytes(R, 8); w.U8(irqLatch); w.U8(irqCounter); w.Bool(irqReload); w.Bool(irqEnabled);
        w.U32(prgAnd); w.U32(prgOr); w.U32(chrAnd); w.U32(chrOr); w.U32(prg32); w.U8(outer);
        w.Bytes(chrRam.data(), chrRam.size());
    }
    void LoadExtra(StateReader& r) override
    {
        bankSel = r.U8(); r.Bytes(R, 8); irqLatch = r.U8(); irqCounter = r.U8(); irqReload = r.Bool(); irqEnabled = r.Bool();
        prgAnd = r.U32(); prgOr = r.U32(); chrAnd = r.U32(); chrOr = r.U32(); prg32 = r.U32(); outer = r.U8();
        r.Bytes(chrRam.data(), chrRam.size());
    }
};

class OekaKids : public BankedMapper
{
    std::vector<u8> ram;
    u32 outer = 0, inner = 0;
public:
    OekaKids(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m), ram(32768, 0)
    {
        hasPrgRam = false;
        SetPrg32(0);
    }
    bool PpuRead(u16 addr, u8& data) override
    {
        if (addr >= 0x2000) return false;
        u32 page = (addr < 0x1000) ? (outer | inner) : (outer | 3);
        data = ram[page * 4096 + (addr & 0xFFF)];
        return true;
    }
    bool PpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x2000) return false;
        u32 page = (addr < 0x1000) ? (outer | inner) : (outer | 3);
        ram[page * 4096 + (addr & 0xFFF)] = data;
        return true;
    }
    bool NametableRead(u16 addr, u8&) override
    {
        if ((addr & 0x3000) == 0x2000 && (addr & 0x3FF) < 0x3C0) inner = (addr >> 8) & 3;
        return false;
    }
protected:
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr < 0x8000) return false;
        SetPrg32(data & 3);
        outer = (data & 4) ? 4 : 0;
        return true;
    }
    void SaveExtra(StateWriter& w) const override { w.Bytes(ram.data(), ram.size()); w.U32(outer); w.U32(inner); }
    void LoadExtra(StateReader& r) override { r.Bytes(ram.data(), ram.size()); outer = r.U32(); inner = r.U32(); }
};

class JyCompany : public BankedMapper
{
    u8 prgReg[4] = { 0, 1, 2, 0xFF };
    u8 chrLo[8] = { 0, 1, 2, 3, 4, 5, 6, 7 }, chrHi[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    u8 ntLo[4] = { 0, 0, 0, 0 }, ntHi[4] = { 0, 0, 0, 0 };
    u8 prgMode = 3, chrMode = 3;
    bool advNt = false, romAt6000 = false, ntRomSel = false, chrBlockMode = false;
    u8 mirrorCtl = 0, d003 = 0;
    u8 mulA = 0, mulB = 0, testReg = 0;

    bool jyIrqEnabled = false;
    u8 irqSource = 0, irqPrescaler = 0, irqCounter = 0, irqXor = 0;
    bool irqSmallPrescaler = false;
    int irqDir = 0;

    u32 PrgOuter() const { return (u32)((d003 & 0x06) << 5); }
    u32 ChrOuter() const { return (u32)(((d003 & 1) | ((d003 & 0x18) >> 2)) << 8); }

    u32 ChrReg(int i) const
    {
        u32 v = chrLo[i] | ((u32)chrHi[i] << 8);
        if (chrBlockMode)
        {
            static const u32 mask[4] = { 0x1F, 0x3F, 0x7F, 0xFF };
            v = (v & mask[chrMode & 3]) | ChrOuter();
        }
        return v;
    }

    void Sync()
    {
        u32 outer = PrgOuter();
        switch (prgMode & 3)
        {
        case 0:
        {
            u32 b = ((prgReg[3] & 0x0F) << 2) | outer;
            for (u32 i = 0; i < 4; i++) prg[i] = b + i;
            break;
        }
        case 1:
        {
            u32 a = ((prgReg[1] & 0x1F) << 1) | outer, c = ((prgReg[3] & 0x1F) << 1) | outer;
            prg[0] = a; prg[1] = a + 1; prg[2] = c; prg[3] = c + 1;
            break;
        }
        default:
            for (int i = 0; i < 4; i++) prg[i] = (prgReg[i] & 0x3F) | outer;
            break;
        }

        switch (chrMode & 3)
        {
        case 0: SetChr8(ChrReg(0)); break;
        case 1: SetChr4(0, ChrReg(0)); SetChr4(1, ChrReg(4)); break;
        case 2: for (int i = 0; i < 4; i++) SetChr2(i, ChrReg(i * 2)); break;
        default: for (int i = 0; i < 8; i++) chr[i] = ChrReg(i); break;
        }

        if (advNt) mirroring = Mirroring::CUSTOM;
        else SetMirror(mirrorCtl == 0 ? 0 : mirrorCtl == 1 ? 1 : mirrorCtl == 2 ? 2 : 3);
        if (advNt) for (int t = 0; t < 4; t++) ntMap[t] = (u8)(ntLo[t] & 1);
    }

    void ClockIrqCounter()
    {
        if (irqDir == 1)
        {
            if (++irqCounter == 0 && jyIrqEnabled) irqPending = true;
        }
        else if (irqDir == 2)
        {
            if (--irqCounter == 0xFF && jyIrqEnabled) irqPending = true;
        }
    }
    void ClockPrescaler()
    {
        u8 mask = irqSmallPrescaler ? 0x07 : 0xFF;
        if (irqDir == 1)
        {
            u8 p = (u8)(((irqPrescaler & mask) + 1) & mask);
            irqPrescaler = (u8)((irqPrescaler & ~mask) | p);
            if (p == 0) ClockIrqCounter();
        }
        else if (irqDir == 2)
        {
            u8 p = (u8)(((irqPrescaler & mask) - 1) & mask);
            irqPrescaler = (u8)((irqPrescaler & ~mask) | p);
            if (p == mask) ClockIrqCounter();
        }
    }

public:
    JyCompany(std::vector<u8> p, std::vector<u8> c, bool r, Mirroring m)
        : BankedMapper(std::move(p), std::move(c), r, m)
    {
        Sync();
    }

    void ClockCpuCycle() override { if (irqSource == 0) ClockPrescaler(); }
    void OnScanline() override { if (irqSource == 1 || irqSource == 2) ClockPrescaler(); }

    bool NametableRead(u16 addr, u8& data) override
    {
        if (!advNt || !ntRomSel) return false;
        int t = (addr >> 10) & 3;
        u32 bank = ntLo[t] | ((u32)ntHi[t] << 8);
        data = ChrByte((bank % NumChr1()) * 1024 + (addr & 0x3FF));
        return true;
    }
    bool NametableWrite(u16 addr, u8) override { (void)addr; return advNt && ntRomSel; }

protected:
    bool ReadReg(u16 addr, u8& data) override
    {
        if (addr >= 0x5000 && addr < 0x6000)
        {
            switch (addr & 0xF803)
            {
            case 0x5000: data = 0x00; return true;
            case 0x5800: data = (u8)((mulA * mulB) & 0xFF); return true;
            case 0x5801: data = (u8)(((mulA * mulB) >> 8) & 0xFF); return true;
            case 0x5803: data = testReg; return true;
            default: return false;
            }
        }
        if (addr >= 0x6000 && addr < 0x8000 && romAt6000)
        {
            u32 bank8 = (prgMode & 3) == 3 || (prgMode & 3) == 2 ? ((prgReg[3] & 0x3F) | PrgOuter()) : prg[3];
            data = PrgByte((bank8 % NumPrg8()) * 8192 + (addr & 0x1FFF));
            return true;
        }
        return false;
    }
    bool WriteReg(u16 addr, u8 data) override
    {
        if (addr >= 0x5000 && addr < 0x6000)
        {
            switch (addr & 0xF803)
            {
            case 0x5800: mulA = data; break;
            case 0x5801: mulB = data; break;
            case 0x5803: testReg = data; break;
            }
            return true;
        }
        if (addr < 0x8000) return false;
        switch (addr & 0xF007)
        {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003: prgReg[addr & 3] = data; break;
        case 0x9000: case 0x9001: case 0x9002: case 0x9003: case 0x9004: case 0x9005: case 0x9006: case 0x9007: chrLo[addr & 7] = data; break;
        case 0xA000: case 0xA001: case 0xA002: case 0xA003: case 0xA004: case 0xA005: case 0xA006: case 0xA007: chrHi[addr & 7] = data; break;
        case 0xB000: case 0xB001: case 0xB002: case 0xB003: ntLo[addr & 3] = data; break;
        case 0xB004: case 0xB005: case 0xB006: case 0xB007: ntHi[addr & 3] = data; break;
        case 0xC000: jyIrqEnabled = (data & 1) != 0; if (!jyIrqEnabled) irqPending = false; break;
        case 0xC001:
            irqSource = data & 3; irqSmallPrescaler = (data & 4) != 0;
            irqDir = ((data >> 6) & 3) == 1 ? 1 : (((data >> 6) & 3) == 2 ? 2 : 0);
            break;
        case 0xC002: jyIrqEnabled = false; irqPending = false; break;
        case 0xC003: jyIrqEnabled = true; break;
        case 0xC004: irqPrescaler = data ^ irqXor; break;
        case 0xC005: irqCounter = data ^ irqXor; break;
        case 0xC006: irqXor = data; break;
        case 0xD000:
            prgMode = data & 7; chrMode = (data >> 3) & 3; advNt = (data & 0x20) != 0; romAt6000 = (data & 0x80) != 0;
            break;
        case 0xD001: mirrorCtl = data & 3; break;
        case 0xD002: ntRomSel = (data & 0x80) != 0; break;
        case 0xD003: d003 = data; chrBlockMode = (data & 0x20) == 0; break;
        default: break;
        }
        Sync();
        return true;
    }
    void SaveExtra(StateWriter& w) const override
    {
        w.Bytes(prgReg, 4); w.Bytes(chrLo, 8); w.Bytes(chrHi, 8); w.Bytes(ntLo, 4); w.Bytes(ntHi, 4);
        w.U8(prgMode); w.U8(chrMode); w.Bool(advNt); w.Bool(romAt6000); w.Bool(ntRomSel); w.Bool(chrBlockMode);
        w.U8(mirrorCtl); w.U8(d003); w.U8(mulA); w.U8(mulB); w.U8(testReg);
        w.Bool(jyIrqEnabled); w.U8(irqSource); w.U8(irqPrescaler); w.U8(irqCounter); w.U8(irqXor);
        w.Bool(irqSmallPrescaler); w.S64(irqDir);
    }
    void LoadExtra(StateReader& r) override
    {
        r.Bytes(prgReg, 4); r.Bytes(chrLo, 8); r.Bytes(chrHi, 8); r.Bytes(ntLo, 4); r.Bytes(ntHi, 4);
        prgMode = r.U8(); chrMode = r.U8(); advNt = r.Bool(); romAt6000 = r.Bool(); ntRomSel = r.Bool(); chrBlockMode = r.Bool();
        mirrorCtl = r.U8(); d003 = r.U8(); mulA = r.U8(); mulB = r.U8(); testReg = r.U8();
        jyIrqEnabled = r.Bool(); irqSource = r.U8(); irqPrescaler = r.U8(); irqCounter = r.U8(); irqXor = r.U8();
        irqSmallPrescaler = r.Bool(); irqDir = (int)r.S64();
    }
};


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
        }
        else if (addr < 0xC000)
        {

            if (even)
            {
                if (headerMirroring != Mirroring::FOUR_SCREEN)
                    mirroring = (data & 1) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
            }
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
    }

    bool IRQState() const override { return irqPending; }
    void IRQClear() override { irqPending = false; }

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

    struct V6Pulse { u8 ctrl = 0; u16 freq = 0; bool enable = false; u16 timer = 0; u8 step = 0; };
    struct V6Saw   { u8 rate = 0; u16 freq = 0; bool enable = false; u16 timer = 0; u8 phase = 0; u8 acc = 0; };
    V6Pulse v6p[2];
    V6Saw v6s;
    bool v6Halt = false;
    int v6Shift = 0;

    bool WriteAudio(u16 reg, u8 data)
    {
        switch (reg)
        {
        case 0x9000: v6p[0].ctrl = data; return true;
        case 0x9001: v6p[0].freq = (u16)((v6p[0].freq & 0xF00) | data); return true;
        case 0x9002: v6p[0].freq = (u16)((v6p[0].freq & 0x0FF) | ((data & 0x0F) << 8)); v6p[0].enable = (data & 0x80) != 0; if (!v6p[0].enable) v6p[0].step = 0; return true;
        case 0x9003: v6Halt = (data & 1) != 0; v6Shift = (data & 4) ? 8 : (data & 2) ? 4 : 0; return true;
        case 0xA000: v6p[1].ctrl = data; return true;
        case 0xA001: v6p[1].freq = (u16)((v6p[1].freq & 0xF00) | data); return true;
        case 0xA002: v6p[1].freq = (u16)((v6p[1].freq & 0x0FF) | ((data & 0x0F) << 8)); v6p[1].enable = (data & 0x80) != 0; if (!v6p[1].enable) v6p[1].step = 0; return true;
        case 0xB000: v6s.rate = data & 0x3F; return true;
        case 0xB001: v6s.freq = (u16)((v6s.freq & 0xF00) | data); return true;
        case 0xB002: v6s.freq = (u16)((v6s.freq & 0x0FF) | ((data & 0x0F) << 8)); v6s.enable = (data & 0x80) != 0; if (!v6s.enable) { v6s.phase = 0; v6s.acc = 0; } return true;
        default: return false;
        }
    }

    void ClockAudio()
    {
        if (v6Halt) return;
        for (int i = 0; i < 2; i++)
        {
            V6Pulse& p = v6p[i];
            if (!p.enable) continue;
            if (p.timer == 0) { p.timer = (u16)(p.freq >> v6Shift); p.step = (u8)((p.step + 1) & 15); }
            else p.timer--;
        }
        if (v6s.enable)
        {
            if (v6s.timer == 0)
            {
                v6s.timer = (u16)(v6s.freq >> v6Shift);
                v6s.phase = (u8)((v6s.phase + 1) % 14);
                if (v6s.phase == 0) v6s.acc = 0;
                else if ((v6s.phase & 1) == 0) v6s.acc = (u8)(v6s.acc + v6s.rate);
            }
            else v6s.timer--;
        }
    }

public:
    using Mapper::Mapper;

    double ExpansionAudio() override
    {
        int out = 0;
        for (int i = 0; i < 2; i++)
        {
            const V6Pulse& p = v6p[i];
            if (p.enable && ((p.ctrl & 0x80) || p.step <= ((p.ctrl >> 4) & 7))) out += p.ctrl & 0x0F;
        }
        if (v6s.enable) out += v6s.acc >> 3;
        return out * 0.0085;
    }

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

        if (reg >= 0x8000 && reg <= 0x8003) { prg16kBank = data & 0x0F; return true; }
        if (reg >= 0xC000 && reg <= 0xC003) { prg8kBank = data & 0x1F; return true; }

        if (reg >= 0x9000 && reg <= 0xB002) { WriteAudio(reg, data); return true; }

        if (reg == 0xB003)
        {
            switch ((data >> 2) & 0x03)
            {
            case 0: mirroring = Mirroring::VERTICAL; break;
            case 1: mirroring = Mirroring::HORIZONTAL; break;
            case 2: mirroring = Mirroring::SINGLE_SCREEN_LOW; break;
            default: mirroring = Mirroring::SINGLE_SCREEN_HIGH; break;
            }
            mirrCtrl = data;
            return true;
        }

        if (reg >= 0xD000 && reg <= 0xD003) { chrBank[reg - 0xD000] = data; return true; }
        if (reg >= 0xE000 && reg <= 0xE003) { chrBank[4 + (reg - 0xE000)] = data; return true; }

        if (reg == 0xF000) { irqLatch = data; return true; }
        if (reg == 0xF001)
        {
            irqEnabled = (data & 0x02) != 0;
            irqAckEnable = (data & 0x01) != 0;
            irqModeCycle = (data & 0x04) != 0;
            if (irqEnabled) { irqCounter = irqLatch; irqPrescaler = 0; }
            irqPending = false;
            return true;
        }
        if (reg == 0xF002)
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
        ClockAudio();
        if (!irqEnabled) return;

        if (irqModeCycle)
        {
            ClockIrqCounter();
        }
        else
        {
            irqPrescaler += 3;
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

class Mapper26 : public Mapper24
{
public:
    using Mapper24::Mapper24;

    bool CpuWrite(u16 addr, u8 data) override
    {
        if (addr >= 0x8000)
            addr = (u16)((addr & 0xFFFC) | ((addr & 1) << 1) | ((addr >> 1) & 1));
        return Mapper24::CpuWrite(addr, data);
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

    u8 ayReg[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    u8 aySel = 0;
    u16 ayCnt[3] = { 0, 0, 0 };
    u8 ayOut[3] = { 0, 0, 0 };
    int ayDiv = 0;

public:
    using Mapper::Mapper;

    double ExpansionAudio() override
    {
        static double tbl[16];
        static bool init = false;
        if (!init) { tbl[0] = 0.0; for (int v = 1; v < 16; v++) tbl[v] = std::pow(2.0, (v - 15) / 2.0); init = true; }
        double s = 0.0;
        for (int ch = 0; ch < 3; ch++)
        {
            int vol = ayReg[8 + ch] & 0x0F;
            bool toneOn = !(ayReg[7] & (1 << ch));
            int out = toneOn ? ayOut[ch] : 1;
            s += tbl[vol] * out;
        }
        return s * 0.07;
    }

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
        if (addr < 0xE000) aySel = data & 0x0F;
        else ayReg[aySel] = data;
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
        if (++ayDiv >= 16)
        {
            ayDiv = 0;
            for (int ch = 0; ch < 3; ch++)
            {
                u16 p = (u16)(ayReg[ch * 2] | ((ayReg[ch * 2 + 1] & 0x0F) << 8));
                if (p == 0) p = 1;
                if (++ayCnt[ch] >= p) { ayCnt[ch] = 0; ayOut[ch] ^= 1; }
            }
        }
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
                                      bool chrIsRAM, Mirroring headerMirroring, int submapper)
{
    if (mapperID == 13 && chr.size() < 16384) chr.resize(16384, 0);
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
    case 79:  return std::unique_ptr<Mapper>(new Nina(false, std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 113: return std::unique_ptr<Mapper>(new Nina(true, std::move(prg), std::move(chr), chrIsRAM, headerMirroring));

    case 16:
    case 159: return std::unique_ptr<Mapper>(new BandaiFcg(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 18:  return std::unique_ptr<Mapper>(new JalecoSs88006(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 21: case 22: case 23: case 25:
              return std::unique_ptr<Mapper>(new Vrc24(mapperID, std::move(prg), std::move(chr), chrIsRAM, headerMirroring, submapper));
    case 26:  return std::unique_ptr<Mapper>(new Mapper26(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 30:  return std::unique_ptr<Mapper>(new Unrom512(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 32:  return std::unique_ptr<Mapper>(new IremG101(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 33:  return std::unique_ptr<Mapper>(new TaitoTc(false, std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 48:  return std::unique_ptr<Mapper>(new TaitoTc(true, std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 65:  return std::unique_ptr<Mapper>(new IremH3001(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 67:  return std::unique_ptr<Mapper>(new Sunsoft3(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 68:  return std::unique_ptr<Mapper>(new Sunsoft4(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 73:  return std::unique_ptr<Mapper>(new Vrc3(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 70: case 72: case 78: case 86: case 87: case 89: case 92: case 93: case 94: case 97: case 185: case 232:
              return std::unique_ptr<Mapper>(new SimpleLatch(mapperID, std::move(prg), std::move(chr), chrIsRAM, headerMirroring, submapper));
    case 76: case 88: case 95: case 154: case 206:
              return std::unique_ptr<Mapper>(new NamcoDx(mapperID, std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 80:  return std::unique_ptr<Mapper>(new TaitoX1005(false, std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 207: return std::unique_ptr<Mapper>(new TaitoX1005(true, std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 118: case 119:
              return std::unique_ptr<Mapper>(new Mmc3Variant(mapperID, std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 155: return std::unique_ptr<Mapper>(new Mapper1(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));

    case 31:  return std::unique_ptr<Mapper>(new Mapper31(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 38: case 41: case 42: case 46: case 50: case 91: case 107: case 112: case 133: case 143: case 145:
    case 148: case 149: case 156: case 200: case 201: case 202: case 203: case 212: case 225: case 255:
    case 228: case 229: case 240: case 241: case 242: case 244: case 246:
              return std::unique_ptr<Mapper>(new MultiLatch(mapperID, std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 146: return std::unique_ptr<Mapper>(new Nina(false, std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 74: case 189: case 191: case 192: case 194: case 195: case 198: case 47: case 44: case 205:
              return std::unique_ptr<Mapper>(new Mmc3Gen(mapperID, std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 96:  return std::unique_ptr<Mapper>(new OekaKids(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 90: case 209: case 211:
              return std::unique_ptr<Mapper>(new JyCompany(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));

    case 5:   return std::unique_ptr<Mapper>(new Mmc5(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 19:  return std::unique_ptr<Mapper>(new Namco163(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 40:  return std::unique_ptr<Mapper>(new Mapper40(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 64:  return std::unique_ptr<Mapper>(new Rambo1(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 82:  return std::unique_ptr<Mapper>(new TaitoX1017(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 85:  return std::unique_ptr<Mapper>(new Vrc7(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 153: return std::unique_ptr<Mapper>(new BandaiFcg(std::move(prg), std::move(chr), chrIsRAM, headerMirroring, true));
    case 210: return std::unique_ptr<Mapper>(new Namco210(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 140: return std::unique_ptr<Mapper>(new Mapper140(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 152: return std::unique_ptr<Mapper>(new Mapper152(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 180: return std::unique_ptr<Mapper>(new Mapper180(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    case 184: return std::unique_ptr<Mapper>(new Mapper184(std::move(prg), std::move(chr), chrIsRAM, headerMirroring));
    default:  return nullptr;
    }
}
