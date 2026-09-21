#pragma once
#include "types.h"
#include <vector>
#include <cstring>
#include <cstdint>

class StateWriter
{
public:
    std::vector<u8> buf;

    void U8(u8 v) { buf.push_back(v); }
    void Bool(bool v) { buf.push_back(v ? 1 : 0); }
    void U16(u16 v) { buf.push_back(v & 0xFF); buf.push_back((v >> 8) & 0xFF); }
    void U32(u32 v) { for (int i = 0; i < 4; i++) buf.push_back((v >> (i * 8)) & 0xFF); }
    void U64(uint64_t v) { for (int i = 0; i < 8; i++) buf.push_back((v >> (i * 8)) & 0xFF); }
    void S64(int64_t v) { U64((uint64_t)v); }
    void F64(double v) { uint64_t bits; std::memcpy(&bits, &v, 8); U64(bits); }
    void Bytes(const u8* p, size_t n) { buf.insert(buf.end(), p, p + n); }
};

class StateReader
{
public:
    const u8* p;
    size_t len;
    size_t pos = 0;
    bool ok = true;

    StateReader(const u8* data, size_t n) : p(data), len(n) {}

    u8 U8() { if (pos + 1 > len) { ok = false; return 0; } return p[pos++]; }
    bool Bool() { return U8() != 0; }
    u16 U16() { u16 lo = U8(); u16 hi = U8(); return lo | (hi << 8); }
    u32 U32() { u32 v = 0; for (int i = 0; i < 4; i++) v |= ((u32)U8()) << (i * 8); return v; }
    uint64_t U64() { uint64_t v = 0; for (int i = 0; i < 8; i++) v |= ((uint64_t)U8()) << (i * 8); return v; }
    int64_t S64() { return (int64_t)U64(); }
    double F64() { uint64_t bits = U64(); double v; std::memcpy(&v, &bits, 8); return v; }
    void Bytes(u8* dst, size_t n)
    {
        if (pos + n > len) { ok = false; std::memset(dst, 0, n); return; }
        std::memcpy(dst, p + pos, n);
        pos += n;
    }
};
