#include "cartridge.h"
#include "mappers.h"
#include <fstream>

static u32 CartCrc32(const std::vector<u8>& a, const std::vector<u8>& b)
{
    static u32 table[256];
    static bool init = false;
    if (!init)
    {
        for (u32 i = 0; i < 256; i++)
        {
            u32 c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    u32 crc = 0xFFFFFFFFu;
    for (u8 v : a) crc = table[(crc ^ v) & 0xFF] ^ (crc >> 8);
    for (u8 v : b) crc = table[(crc ^ v) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

bool Cartridge::LoadFromFile(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
    {
        lastError = "Could not open file: " + path;
        return false;
    }

    u8 header[16];
    f.read(reinterpret_cast<char*>(header), 16);
    if (f.gcount() != 16 || header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A)
    {
        lastError = "Not a valid iNES file";
        return false;
    }

    u8 prgBanks = header[4];
    u8 chrBanks = header[5];
    u8 flags6 = header[6];
    u8 flags7 = header[7];

    mapperID = (flags7 & 0xF0) | (flags6 >> 4);

    Mirroring headerMirroring = (flags6 & 0x08) ? Mirroring::FOUR_SCREEN
                              : (flags6 & 0x01) ? Mirroring::VERTICAL
                                                 : Mirroring::HORIZONTAL;

    bool hasTrainer = (flags6 & 0x04) != 0;
    if (hasTrainer)
        f.seekg(512, std::ios::cur);

    std::vector<u8> prgROM(static_cast<size_t>(prgBanks) * 16384);
    f.read(reinterpret_cast<char*>(prgROM.data()), prgROM.size());

    bool chrIsRAM = (chrBanks == 0);
    std::vector<u8> chrROM(chrIsRAM ? 8192 : static_cast<size_t>(chrBanks) * 8192, 0);
    if (!chrIsRAM)
        f.read(reinterpret_cast<char*>(chrROM.data()), chrROM.size());

    romCrc = CartCrc32(prgROM, chrROM);

    mapper = CreateMapper(mapperID, std::move(prgROM), std::move(chrROM), chrIsRAM, headerMirroring);
    if (!mapper)
    {
        lastError = "Mapper " + std::to_string(mapperID) + " is not implemented.";
        loaded = false;
        return false;
    }

    loaded = true;
    return true;
}

void Cartridge::SaveState(StateWriter& w) const
{
    w.U32((u32)mapperID);
    if (mapper) mapper->SaveState(w);
}

void Cartridge::LoadState(StateReader& r)
{
    u32 savedMapperID = r.U32();
    if (!mapper || (int)savedMapperID != mapperID)
    {
        r.ok = false;
        return;
    }
    mapper->LoadState(r);
}
