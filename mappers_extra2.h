#pragma once

#include "mappers_extra.h"
#include <cmath>

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
