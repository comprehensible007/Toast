#pragma once

#include "mappers_extra2.h"

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
