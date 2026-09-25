#include "ppu2C02.h"

Ppu2C02::Ppu2C02()
{
    InitPaletteTable();
}

void Ppu2C02::InitPaletteTable()
{
    static const u32 pal[64] = {
        0x666666,0x002A88,0x1412A7,0x3B00A4,0x5C007E,0x6E0040,0x6C0600,0x561D00,
        0x333500,0x0B4800,0x005200,0x004F08,0x00404D,0x000000,0x000000,0x000000,
        0xADADAD,0x155FD9,0x4240FF,0x7527FE,0xA01ACC,0xB71E7B,0xB53120,0x994E00,
        0x6B6D00,0x388700,0x0C9300,0x008F32,0x007C8D,0x000000,0x000000,0x000000,
        0xFFFEFF,0x64B0FF,0x9290FF,0xC676FF,0xF36AFF,0xFE6ECC,0xFE8170,0xEA9E22,
        0xBCBE00,0x88D800,0x5CE430,0x45E082,0x48CDDE,0x4F4F4F,0x000000,0x000000,
        0xFFFEFF,0xC0DFFF,0xD3D2FF,0xE8C8FF,0xFBC2FF,0xFEC4EA,0xFECCC5,0xF7D8A5,
        0xE4E594,0xCFEF96,0xBDF4AB,0xB3F3CC,0xB5EBF2,0xB8B8B8,0x000000,0x000000
    };
    for (int i = 0; i < 64; i++)
    {
        u32 c = pal[i];
        u8 r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
        palette[i] = (r << 16) | (g << 8) | b | 0xFF000000u;
    }
}

void Ppu2C02::Reset()
{
    ctrl = mask = status = 0;
    vramAddr = tramAddr = 0;
    fineX = 0;
    addrLatch = false;
    dataBuffer = 0;
    scanline = -1;
    cycle = 0;
    bgNextTileId = bgNextTileAttrib = bgNextTileLsb = bgNextTileMsb = 0;
    bgShiftPatternLo = bgShiftPatternHi = 0;
    bgShiftAttribLo = bgShiftAttribHi = 0;
    for (int i = 0; i < 8; i++)
    {
        spriteScanline[i] = SpriteEntry();
        spriteShifterLo[i] = 0;
        spriteShifterHi[i] = 0;
    }
    spriteCount = 0;
    spriteZeroHitPossible = false;
    ppuAddressBus = 0;
    prevA12 = 0;
    globalDotCounter = 0;
    lowStartDot = 0;
    oddFrame = false;
    renderingWasEnabled = false;
    nmiRequested = false;
    std::memset(nameTable, 0, sizeof(nameTable));
    std::memset(paletteRAM, 0, sizeof(paletteRAM));
    std::memset(oam, 0, sizeof(oam));
}

static inline u16 MirrorNameTable(u16 addr, Mirroring m)
{
    addr &= 0x0FFF;
    int table = addr / 0x400;
    int offset = addr % 0x400;
    int physical;
    switch (m)
    {
    case Mirroring::VERTICAL:   physical = table & 1; break;
    case Mirroring::HORIZONTAL: physical = (table >> 1) & 1; break;
    case Mirroring::SINGLE_SCREEN_LOW:  physical = 0; break;
    case Mirroring::SINGLE_SCREEN_HIGH: physical = 1; break;
    default:                    physical = table & 1; break;
    }
    return static_cast<u16>(physical * 0x400 + offset);
}

static inline u8 ReverseBits(u8 b)
{
    b = (u8)(((b & 0xF0) >> 4) | ((b & 0x0F) << 4));
    b = (u8)(((b & 0xCC) >> 2) | ((b & 0x33) << 2));
    b = (u8)(((b & 0xAA) >> 1) | ((b & 0x55) << 1));
    return b;
}

u8 Ppu2C02::PpuRead(u16 addr) const
{
    addr &= 0x3FFF;
    u8 data = 0;
    if (addr < 0x2000)
    {
        if (cart && cart->PpuRead(addr, data)) return data;
        return 0;
    }
    else if (addr < 0x3F00)
    {
        u16 m = MirrorNameTable(addr, cart ? cart->GetMirroring() : Mirroring::HORIZONTAL);
        return nameTable[m >= 0x400 ? 1 : 0][m & 0x3FF];
    }
    else
    {
        u16 p = addr & 0x1F;
        if (p == 0x10 || p == 0x14 || p == 0x18 || p == 0x1C) p &= 0x0F;
        return paletteRAM[p] & ((mask & 0x01) ? 0x30 : 0x3F);
    }
}

void Ppu2C02::PpuWrite(u16 addr, u8 data)
{
    addr &= 0x3FFF;
    if (addr < 0x2000)
    {
        if (cart) cart->PpuWrite(addr, data);
    }
    else if (addr < 0x3F00)
    {
        u16 m = MirrorNameTable(addr, cart ? cart->GetMirroring() : Mirroring::HORIZONTAL);
        nameTable[m >= 0x400 ? 1 : 0][m & 0x3FF] = data;
    }
    else
    {
        u16 p = addr & 0x1F;
        if (p == 0x10 || p == 0x14 || p == 0x18 || p == 0x1C) p &= 0x0F;
        paletteRAM[p] = data;
    }
}

u8 Ppu2C02::PpuReadFetch(u16 addr)
{
    ppuAddressBus = addr;
    bool high = (addr & 0x1000) != 0;
    if (high)
    {
        if (!prevA12)
        {
            if ((globalDotCounter - lowStartDot) >= 8)
            {
                if (cart) cart->ScanlineTick();
            }
        }
    }
    else
    {
        if (prevA12)
        {
            lowStartDot = globalDotCounter;
        }
    }
    prevA12 = high ? 1 : 0;
    return PpuRead(addr);
}

u8 Ppu2C02::CpuRead(u16 addr)
{
    addr &= 0x7;
    u8 data = 0;
    switch (addr)
    {
    case 0x2:
        data = (status & 0xE0) | (dataBuffer & 0x1F);
        status &= ~0x80;
        addrLatch = false;
        break;
    case 0x4:
        data = oam[oamAddr];
        break;
    case 0x7:
        data = dataBuffer;
        dataBuffer = PpuRead(vramAddr);
        if (vramAddr >= 0x3F00) data = dataBuffer;
        vramAddr += (ctrl & 0x04) ? 32 : 1;
        break;
    default:
        break;
    }
    return data;
}

void Ppu2C02::CpuWrite(u16 addr, u8 data)
{
    addr &= 0x7;
    switch (addr)
    {
    case 0x0:
        ctrl = data;
        tramAddr = (tramAddr & 0xF3FF) | ((data & 0x03) << 10);
        break;
    case 0x1:
        mask = data;
        break;
    case 0x3:
        oamAddr = data;
        break;
    case 0x4:
        oam[oamAddr++] = data;
        break;
    case 0x5:
        if (!addrLatch)
        {
            fineX = data & 0x07;
            tramAddr = (tramAddr & 0xFFE0) | (data >> 3);
            addrLatch = true;
        }
        else
        {
            tramAddr = (tramAddr & 0x8FFF) | ((data & 0x07) << 12);
            tramAddr = (tramAddr & 0xFC1F) | ((data & 0xF8) << 2);
            addrLatch = false;
        }
        break;
    case 0x6:
        if (!addrLatch)
        {
            tramAddr = (tramAddr & 0x00FF) | ((data & 0x3F) << 8);
            addrLatch = true;
        }
        else
        {
            tramAddr = (tramAddr & 0xFF00) | data;
            vramAddr = tramAddr;
            addrLatch = false;
        }
        break;
    case 0x7:
        PpuWrite(vramAddr, data);
        vramAddr += (ctrl & 0x04) ? 32 : 1;
        break;
    default:
        break;
    }
}

u8 Ppu2C02::GetPalette(u8 pal, u8 pixel)
{
    return PpuRead(0x3F00 + (pal << 2) + pixel) & 0x3F;
}

void Ppu2C02::IncScrollX()
{
    if ((vramAddr & 0x001F) == 31)
    {
        vramAddr &= ~0x001Fu;
        vramAddr ^= 0x0400;
    }
    else
    {
        vramAddr++;
    }
}

void Ppu2C02::IncScrollY()
{
    if ((vramAddr & 0x7000) != 0x7000)
    {
        vramAddr += 0x1000;
    }
    else
    {
        vramAddr &= ~0x7000u;
        int y = (vramAddr & 0x03E0) >> 5;
        if (y == 29) { y = 0; vramAddr ^= 0x0800; }
        else if (y == 31) { y = 0; }
        else { y++; }
        vramAddr = (vramAddr & ~0x03E0u) | (y << 5);
    }
}

void Ppu2C02::TransferAddressX()
{
    vramAddr = (vramAddr & ~0x041Fu) | (tramAddr & 0x041Fu);
}

void Ppu2C02::TransferAddressY()
{
    vramAddr = (vramAddr & ~0x7BE0u) | (tramAddr & 0x7BE0u);
}

void Ppu2C02::LoadBgShifters()
{
    bgShiftPatternLo = (u16)((bgShiftPatternLo & 0xFF00) | bgNextTileLsb);
    bgShiftPatternHi = (u16)((bgShiftPatternHi & 0xFF00) | bgNextTileMsb);
    bgShiftAttribLo = (u16)((bgShiftAttribLo & 0xFF00) | ((bgNextTileAttrib & 0x01) ? 0xFF : 0x00));
    bgShiftAttribHi = (u16)((bgShiftAttribHi & 0xFF00) | ((bgNextTileAttrib & 0x02) ? 0xFF : 0x00));
}

void Ppu2C02::UpdateShifters()
{
    if (mask & 0x08)
    {
        bgShiftPatternLo <<= 1;
        bgShiftPatternHi <<= 1;
        bgShiftAttribLo <<= 1;
        bgShiftAttribHi <<= 1;
    }
    if ((mask & 0x10) && cycle >= 1 && cycle < 258)
    {
        for (int i = 0; i < spriteCount; i++)
        {
            if (spriteScanline[i].x > 0)
            {
                spriteScanline[i].x--;
            }
            else
            {
                spriteShifterLo[i] <<= 1;
                spriteShifterHi[i] <<= 1;
            }
        }
    }
}

void Ppu2C02::EvaluateSprites()
{
    for (int i = 0; i < 8; i++)
    {
        spriteScanline[i] = SpriteEntry();
        spriteShifterLo[i] = 0;
        spriteShifterHi[i] = 0;
    }
    spriteCount = 0;
    spriteZeroHitPossible = false;

    int height = (ctrl & 0x20) ? 16 : 8;
    int total = 0;
    for (int i = 0; i < 64; i++)
    {
        int diff = scanline - (int)oam[i * 4 + 0];
        if (diff >= 0 && diff < height)
        {
            total++;
            if (spriteCount < 8)
            {
                if (i == 0) spriteZeroHitPossible = true;
                spriteScanline[spriteCount].y = oam[i * 4 + 0];
                spriteScanline[spriteCount].tile = oam[i * 4 + 1];
                spriteScanline[spriteCount].attr = oam[i * 4 + 2];
                spriteScanline[spriteCount].x = oam[i * 4 + 3];
                spriteCount++;
            }
        }
    }
    if (total > 8) status |= 0x20;
}

void Ppu2C02::FetchSpritePatternByte(int slot, bool high)
{
    SpriteEntry& e = spriteScanline[slot];
    int height = (ctrl & 0x20) ? 16 : 8;
    int row = scanline - (int)e.y;
    bool flipV = (e.attr & 0x80) != 0;
    bool flipH = (e.attr & 0x40) != 0;
    int r = flipV ? (height - 1 - row) : row;

    u16 addr;
    if (height == 8)
    {
        u16 base = (ctrl & 0x08) ? 0x1000 : 0x0000;
        addr = (u16)(base + e.tile * 16 + r + (high ? 8 : 0));
    }
    else
    {
        u16 base = (e.tile & 1) ? 0x1000 : 0x0000;
        u8 t = e.tile & 0xFE;
        int rr = r;
        if (rr >= 8) { t = (u8)(t + 1); rr -= 8; }
        addr = (u16)(base + t * 16 + rr + (high ? 8 : 0));
    }

    u8 data = PpuReadFetch(addr);
    if (flipH) data = ReverseBits(data);
    if (high) spriteShifterHi[slot] = data;
    else spriteShifterLo[slot] = data;
}

void Ppu2C02::TouchDummySpriteFetch()
{
    u16 addr;
    if (ctrl & 0x20) addr = 0x1FF0;
    else addr = (ctrl & 0x08) ? 0x1FF0 : 0x0FF0;
    PpuReadFetch(addr);
}

void Ppu2C02::ComputePixel(int sl, int x)
{
    u8 bgPixel = 0, bgPal = 0;
    if (mask & 0x08)
    {
        u16 bitMux = (u16)(0x8000 >> fineX);
        u8 p0 = (bgShiftPatternLo & bitMux) ? 1 : 0;
        u8 p1 = (bgShiftPatternHi & bitMux) ? 1 : 0;
        bgPixel = (u8)((p1 << 1) | p0);
        u8 pal0 = (bgShiftAttribLo & bitMux) ? 1 : 0;
        u8 pal1 = (bgShiftAttribHi & bitMux) ? 1 : 0;
        bgPal = (u8)((pal1 << 1) | pal0);
    }
    if (x < 8 && !(mask & 0x02)) bgPixel = 0;

    u8 fgPixel = 0, fgPal = 0;
    bool fgPriority = false;
    int fgIndexHit = -1;
    if (mask & 0x10)
    {
        for (int i = 0; i < spriteCount; i++)
        {
            if (spriteScanline[i].x == 0)
            {
                u8 p0 = (spriteShifterLo[i] & 0x80) ? 1 : 0;
                u8 p1 = (spriteShifterHi[i] & 0x80) ? 1 : 0;
                u8 px = (u8)((p1 << 1) | p0);
                if (px != 0)
                {
                    fgPixel = px;
                    fgPal = (u8)((spriteScanline[i].attr & 0x03) + 4);
                    fgPriority = !(spriteScanline[i].attr & 0x20);
                    fgIndexHit = i;
                    break;
                }
            }
        }
    }
    if (x < 8 && !(mask & 0x04)) fgPixel = 0;

    u8 pixel, pal;
    if (bgPixel == 0 && fgPixel == 0) { pixel = 0; pal = 0; }
    else if (bgPixel == 0 && fgPixel > 0) { pixel = fgPixel; pal = fgPal; }
    else if (bgPixel > 0 && fgPixel == 0) { pixel = bgPixel; pal = bgPal; }
    else
    {
        if (fgPriority) { pixel = fgPixel; pal = fgPal; }
        else { pixel = bgPixel; pal = bgPal; }

        if (spriteZeroHitPossible && fgIndexHit == 0 && x != 255)
        {
            if ((mask & 0x08) && (mask & 0x10))
            {
                bool edgeOk = (x >= 8) || ((mask & 0x02) && (mask & 0x04));
                if (edgeOk) status |= 0x40;
            }
        }
    }

    u8 col = pixel ? GetPalette(pal, pixel) : GetPalette(0, 0);
    screen[sl * 256 + x] = palette[col & 0x3F];
}

void Ppu2C02::Clock()
{
    globalDotCounter++;

    bool renderingEnabled = (mask & 0x18) != 0;
    if (renderingEnabled && !renderingWasEnabled)
    {
        lowStartDot = globalDotCounter;
        prevA12 = 0;
    }
    renderingWasEnabled = renderingEnabled;

    if (scanline == -1 && cycle == 1)
    {
        status &= ~0xE0;
    }

    if (scanline >= -1 && scanline < 240 && (mask & 0x18))
    {
        if (scanline == -1 && cycle >= 280 && cycle < 305)
            TransferAddressY();

        if ((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338))
        {
            UpdateShifters();
            switch ((cycle - 1) % 8)
            {
            case 0:
                LoadBgShifters();
                bgNextTileId = PpuReadFetch((u16)(0x2000 | (vramAddr & 0x0FFF)));
                break;
            case 2:
            {
                u16 attAddr = (u16)(0x23C0 | (vramAddr & 0x0C00) | ((vramAddr >> 4) & 0x38) | ((vramAddr >> 2) & 0x07));
                bgNextTileAttrib = PpuReadFetch(attAddr);
                if ((vramAddr >> 6) & 0x01) bgNextTileAttrib >>= 4;
                if ((vramAddr >> 1) & 0x01) bgNextTileAttrib >>= 2;
                bgNextTileAttrib &= 0x03;
                break;
            }
            case 4:
            {
                u16 patternBase = (ctrl & 0x10) ? 0x1000 : 0x0000;
                u16 fineY = (vramAddr >> 12) & 0x7;
                bgNextTileLsb = PpuReadFetch((u16)(patternBase + ((u16)bgNextTileId << 4) + fineY));
                break;
            }
            case 6:
            {
                u16 patternBase = (ctrl & 0x10) ? 0x1000 : 0x0000;
                u16 fineY = (vramAddr >> 12) & 0x7;
                bgNextTileMsb = PpuReadFetch((u16)(patternBase + ((u16)bgNextTileId << 4) + fineY + 8));
                break;
            }
            case 7:
                IncScrollX();
                break;
            default:
                break;
            }
        }

        if (cycle == 256) IncScrollY();
        if (cycle == 257)
        {
            LoadBgShifters();
            TransferAddressX();
            EvaluateSprites();
        }
        if (cycle == 338 || cycle == 340)
            bgNextTileId = PpuReadFetch((u16)(0x2000 | (vramAddr & 0x0FFF)));

        if (cycle >= 257 && cycle <= 320)
        {
            int slot = (cycle - 257) / 8;
            int within = (cycle - 257) % 8;
            if (within == 0 || within == 2)
            {
                PpuReadFetch((u16)(0x2000 | (vramAddr & 0x0FFF)));
            }
            else if (within == 4 || within == 6)
            {
                bool high = (within == 6);
                if (slot < spriteCount) FetchSpritePatternByte(slot, high);
                else if (slot < 8) TouchDummySpriteFetch();
            }
        }
    }

    if (scanline >= 0 && scanline < 240 && cycle >= 1 && cycle <= 256)
    {
        if (mask & 0x18)
        {
            ComputePixel(scanline, cycle - 1);
        }
        else
        {
            u8 col = GetPalette(0, 0);
            screen[scanline * 256 + (cycle - 1)] = palette[col & 0x3F];
        }
    }

    if (scanline == 241 && cycle == 1)
    {
        status |= 0x80;
        if (ctrl & 0x80) nmiRequested = true;
        frameComplete = true;
    }

    cycle++;
    if (scanline == -1 && cycle == 339 && (mask & 0x18) && oddFrame)
    {
        cycle = 340;
    }
    if (cycle > 340)
    {
        cycle = 0;
        scanline++;
        if (scanline > 260)
        {
            scanline = -1;
            oddFrame = !oddFrame;
        }
    }
}

void Ppu2C02::SaveState(StateWriter& w) const
{
    w.Bytes(reinterpret_cast<const u8*>(screen.data()), screen.size() * sizeof(u32));
    w.Bool(frameComplete);
    w.Bytes(&nameTable[0][0], sizeof(nameTable));
    w.Bytes(paletteRAM, sizeof(paletteRAM));
    w.U8(ctrl); w.U8(mask); w.U8(status);
    w.U16(vramAddr); w.U16(tramAddr); w.U8(fineX); w.Bool(addrLatch);
    w.U8(dataBuffer);
    w.S64(scanline); w.S64(cycle);
    w.U8(bgNextTileId); w.U8(bgNextTileAttrib); w.U8(bgNextTileLsb); w.U8(bgNextTileMsb);
    w.U16(bgShiftPatternLo); w.U16(bgShiftPatternHi);
    w.U16(bgShiftAttribLo); w.U16(bgShiftAttribHi);
    w.S64(spriteCount);
    for (int i = 0; i < 8; i++)
    {
        w.U8(spriteScanline[i].y); w.U8(spriteScanline[i].tile);
        w.U8(spriteScanline[i].attr); w.U8(spriteScanline[i].x);
        w.U8(spriteShifterLo[i]); w.U8(spriteShifterHi[i]);
    }
    w.U8(oamAddr);
    w.Bytes(oam, sizeof(oam));
    w.Bool(spriteZeroHitPossible);
    w.U16(ppuAddressBus);
    w.U8(prevA12);
    w.S64((int64_t)globalDotCounter);
    w.S64((int64_t)lowStartDot);
    w.Bool(oddFrame);
    w.Bool(renderingWasEnabled);
    w.Bool(nmiRequested);
}

void Ppu2C02::LoadState(StateReader& r)
{
    r.Bytes(reinterpret_cast<u8*>(screen.data()), screen.size() * sizeof(u32));
    frameComplete = r.Bool();
    r.Bytes(&nameTable[0][0], sizeof(nameTable));
    r.Bytes(paletteRAM, sizeof(paletteRAM));
    ctrl = r.U8(); mask = r.U8(); status = r.U8();
    vramAddr = r.U16(); tramAddr = r.U16(); fineX = r.U8(); addrLatch = r.Bool();
    dataBuffer = r.U8();
    scanline = (int)r.S64(); cycle = (int)r.S64();
    bgNextTileId = r.U8(); bgNextTileAttrib = r.U8(); bgNextTileLsb = r.U8(); bgNextTileMsb = r.U8();
    bgShiftPatternLo = r.U16(); bgShiftPatternHi = r.U16();
    bgShiftAttribLo = r.U16(); bgShiftAttribHi = r.U16();
    spriteCount = (int)r.S64();
    for (int i = 0; i < 8; i++)
    {
        spriteScanline[i].y = r.U8(); spriteScanline[i].tile = r.U8();
        spriteScanline[i].attr = r.U8(); spriteScanline[i].x = r.U8();
        spriteShifterLo[i] = r.U8(); spriteShifterHi[i] = r.U8();
    }
    oamAddr = r.U8();
    r.Bytes(oam, sizeof(oam));
    spriteZeroHitPossible = r.Bool();
    ppuAddressBus = r.U16();
    prevA12 = r.U8();
    globalDotCounter = (long long)r.S64();
    lowStartDot = (long long)r.S64();
    oddFrame = r.Bool();
    renderingWasEnabled = r.Bool();
    nmiRequested = r.Bool();
}
