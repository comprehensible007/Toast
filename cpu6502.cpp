#include "cpu6502.h"
#include "bus.h"

Cpu6502::Cpu6502()
{
    BuildTable();
}

u8 Cpu6502::Read(u16 addr) { return bus->CpuRead(addr); }
void Cpu6502::Write(u16 addr, u8 data) { bus->CpuWrite(addr, data); }

void Cpu6502::Reset()
{
    addrAbs = 0xFFFC;
    u16 lo = Read(addrAbs);
    u16 hi = Read(addrAbs + 1);
    pc = (hi << 8) | lo;

    a = 0; x = 0; y = 0;
    sp = 0xFD;
    status = 0x00 | U | I;

    addrRel = 0; addrAbs = 0; fetched = 0;
    cyclesRemaining = 8;
}

void Cpu6502::IRQ()
{
    if (GetFlag(I)) return;
    if (irqSourceTraceArm);
    Write(0x0100 + sp, (pc >> 8) & 0xFF); sp--;
    Write(0x0100 + sp, pc & 0xFF); sp--;
    SetFlag(B, false);
    SetFlag(U, true);
    SetFlag(I, true);
    Write(0x0100 + sp, status); sp--;

    addrAbs = 0xFFFE;
    u16 lo = Read(addrAbs);
    u16 hi = Read(addrAbs + 1);
    pc = (hi << 8) | lo;
    cyclesRemaining = 7;
}

void Cpu6502::NMI()
{
    Write(0x0100 + sp, (pc >> 8) & 0xFF); sp--;
    Write(0x0100 + sp, pc & 0xFF); sp--;
    SetFlag(B, false);
    SetFlag(U, true);
    SetFlag(I, true);
    Write(0x0100 + sp, status); sp--;

    addrAbs = 0xFFFA;
    u16 lo = Read(addrAbs);
    u16 hi = Read(addrAbs + 1);
    pc = (hi << 8) | lo;
    cyclesRemaining = 7;
}

void Cpu6502::Clock()
{
    if (cyclesRemaining == 1)
    {
        irqSampled = irqLine && !GetFlag(I);
        nmiSampled = nmiPending;
    }

    if (cyclesRemaining == 0)
    {
        if (nmiSampled)
        {
            nmiSampled = false;
            nmiPending = false;
            NMI();
        }
        else if (irqSampled)
        {
            irqSampled = false;
            IRQ();
        }
        else
        {
            if (pc == lastFetchedPc) samePcStreak++;
            else { lastFetchedPc = pc; samePcStreak = 0; }

            opcode = Read(pc);
            pc++;
            SetFlag(U, true);

            const Instruction& ins = table[opcode];
            cyclesRemaining = ins.cycles;

            u8 extra1 = (this->*ins.addrmode)();
            u8 extra2 = (this->*ins.operate)();
            cyclesRemaining += (extra1 & extra2);

            SetFlag(U, true);
        }
    }
    cyclesRemaining--;
}

u8 Cpu6502::Fetch()
{
    if (!(table[opcode].addrmode == &Cpu6502::IMP))
        fetched = Read(addrAbs);
    return fetched;
}

u8 Cpu6502::IMP() { fetched = a; return 0; }
u8 Cpu6502::IMM() { addrAbs = pc++; return 0; }

u8 Cpu6502::ZP0() { addrAbs = Read(pc++) & 0x00FF; return 0; }
u8 Cpu6502::ZPX() { addrAbs = (Read(pc++) + x) & 0x00FF; return 0; }
u8 Cpu6502::ZPY() { addrAbs = (Read(pc++) + y) & 0x00FF; return 0; }

u8 Cpu6502::REL()
{
    addrRel = Read(pc++);
    if (addrRel & 0x80) addrRel |= 0xFF00;
    return 0;
}

u8 Cpu6502::ABS()
{
    u16 lo = Read(pc++);
    u16 hi = Read(pc++);
    addrAbs = (hi << 8) | lo;
    return 0;
}

u8 Cpu6502::ABX()
{
    u16 lo = Read(pc++);
    u16 hi = Read(pc++);
    addrAbs = ((hi << 8) | lo) + x;
    return ((addrAbs & 0xFF00) != (hi << 8)) ? 1 : 0;
}

u8 Cpu6502::ABY()
{
    u16 lo = Read(pc++);
    u16 hi = Read(pc++);
    addrAbs = ((hi << 8) | lo) + y;
    return ((addrAbs & 0xFF00) != (hi << 8)) ? 1 : 0;
}

u8 Cpu6502::IND()
{
    u16 ptrLo = Read(pc++);
    u16 ptrHi = Read(pc++);
    u16 ptr = (ptrHi << 8) | ptrLo;

    u16 lo, hi;
    if (ptrLo == 0x00FF)
    {
        lo = Read(ptr);
        hi = Read(ptr & 0xFF00);
    }
    else
    {
        lo = Read(ptr);
        hi = Read(ptr + 1);
    }
    addrAbs = (hi << 8) | lo;
    return 0;
}

u8 Cpu6502::IZX()
{
    u8 t = Read(pc++);
    u16 lo = Read((u16)(t + x) & 0x00FF);
    u16 hi = Read((u16)(t + x + 1) & 0x00FF);
    addrAbs = (hi << 8) | lo;
    return 0;
}

u8 Cpu6502::IZY()
{
    u8 t = Read(pc++);
    u16 lo = Read(t & 0x00FF);
    u16 hi = Read((t + 1) & 0x00FF);
    addrAbs = (hi << 8) | lo;
    addrAbs += y;
    return ((addrAbs & 0xFF00) != (hi << 8)) ? 1 : 0;
}

u8 Cpu6502::ADC()
{
    Fetch();
    u16 tmp = (u16)a + (u16)fetched + (u16)GetFlag(C);
    SetFlag(C, tmp > 255);
    SetFlag(Z, (tmp & 0x00FF) == 0);
    SetFlag(V, (~((u16)a ^ (u16)fetched) & ((u16)a ^ tmp)) & 0x0080);
    SetFlag(N, tmp & 0x80);
    a = tmp & 0xFF;
    return 1;
}

u8 Cpu6502::SBC()
{
    Fetch();
    u16 value = ((u16)fetched) ^ 0x00FF;
    u16 tmp = (u16)a + value + (u16)GetFlag(C);
    SetFlag(C, tmp & 0xFF00);
    SetFlag(Z, (tmp & 0x00FF) == 0);
    SetFlag(V, (tmp ^ (u16)a) & (tmp ^ value) & 0x0080);
    SetFlag(N, tmp & 0x0080);
    a = tmp & 0xFF;
    return 1;
}

u8 Cpu6502::AND() { Fetch(); a &= fetched; SetFlag(Z, a == 0); SetFlag(N, a & 0x80); return 1; }
u8 Cpu6502::EOR() { Fetch(); a ^= fetched; SetFlag(Z, a == 0); SetFlag(N, a & 0x80); return 1; }
u8 Cpu6502::ORA() { Fetch(); a |= fetched; SetFlag(Z, a == 0); SetFlag(N, a & 0x80); return 1; }

u8 Cpu6502::ASL()
{
    Fetch();
    u16 tmp = (u16)fetched << 1;
    SetFlag(C, (tmp & 0xFF00) > 0);
    SetFlag(Z, (tmp & 0xFF) == 0);
    SetFlag(N, tmp & 0x80);
    if (table[opcode].addrmode == &Cpu6502::IMP) a = tmp & 0xFF;
    else Write(addrAbs, tmp & 0xFF);
    return 0;
}

u8 Cpu6502::LSR()
{
    Fetch();
    SetFlag(C, fetched & 0x01);
    u16 tmp = fetched >> 1;
    SetFlag(Z, (tmp & 0xFF) == 0);
    SetFlag(N, tmp & 0x80);
    if (table[opcode].addrmode == &Cpu6502::IMP) a = tmp & 0xFF;
    else Write(addrAbs, tmp & 0xFF);
    return 0;
}

u8 Cpu6502::ROL()
{
    Fetch();
    u16 tmp = ((u16)fetched << 1) | GetFlag(C);
    SetFlag(C, tmp & 0xFF00);
    SetFlag(Z, (tmp & 0xFF) == 0);
    SetFlag(N, tmp & 0x80);
    if (table[opcode].addrmode == &Cpu6502::IMP) a = tmp & 0xFF;
    else Write(addrAbs, tmp & 0xFF);
    return 0;
}

u8 Cpu6502::ROR()
{
    Fetch();
    u16 tmp = ((u16)GetFlag(C) << 7) | (fetched >> 1);
    SetFlag(C, fetched & 0x01);
    SetFlag(Z, (tmp & 0xFF) == 0);
    SetFlag(N, tmp & 0x80);
    if (table[opcode].addrmode == &Cpu6502::IMP) a = tmp & 0xFF;
    else Write(addrAbs, tmp & 0xFF);
    return 0;
}

u8 Cpu6502::INC() { Fetch(); u8 tmp = fetched + 1; Write(addrAbs, tmp); SetFlag(Z, tmp == 0); SetFlag(N, tmp & 0x80); return 0; }
u8 Cpu6502::DEC() { Fetch(); u8 tmp = fetched - 1; Write(addrAbs, tmp); SetFlag(Z, tmp == 0); SetFlag(N, tmp & 0x80); return 0; }
u8 Cpu6502::INX() { x++; SetFlag(Z, x == 0); SetFlag(N, x & 0x80); return 0; }
u8 Cpu6502::INY() { y++; SetFlag(Z, y == 0); SetFlag(N, y & 0x80); return 0; }
u8 Cpu6502::DEX() { x--; SetFlag(Z, x == 0); SetFlag(N, x & 0x80); return 0; }
u8 Cpu6502::DEY() { y--; SetFlag(Z, y == 0); SetFlag(N, y & 0x80); return 0; }

u8 Cpu6502::CMP() { Fetch(); u16 tmp = (u16)a - (u16)fetched; SetFlag(C, a >= fetched); SetFlag(Z, (tmp & 0xFF) == 0); SetFlag(N, tmp & 0x80); return 1; }
u8 Cpu6502::CPX() { Fetch(); u16 tmp = (u16)x - (u16)fetched; SetFlag(C, x >= fetched); SetFlag(Z, (tmp & 0xFF) == 0); SetFlag(N, tmp & 0x80); return 0; }
u8 Cpu6502::CPY() { Fetch(); u16 tmp = (u16)y - (u16)fetched; SetFlag(C, y >= fetched); SetFlag(Z, (tmp & 0xFF) == 0); SetFlag(N, tmp & 0x80); return 0; }

u8 Cpu6502::BIT()
{
    Fetch();
    u8 tmp = a & fetched;
    SetFlag(Z, tmp == 0);
    SetFlag(N, fetched & 0x80);
    SetFlag(V, fetched & 0x40);
    return 0;
}

static inline u8 BranchHelper(Cpu6502& cpu, bool cond, u16& pc, u16 addrRel)
{
    if (!cond) return 0;
    u16 oldPc = pc;
    pc += addrRel;
    return ((pc & 0xFF00) != (oldPc & 0xFF00)) ? 2 : 1;
}

u8 Cpu6502::BCC() { if (!GetFlag(C)) { cyclesRemaining += BranchHelper(*this, true, pc, addrRel); } return 0; }
u8 Cpu6502::BCS() { if (GetFlag(C))  { cyclesRemaining += BranchHelper(*this, true, pc, addrRel); } return 0; }
u8 Cpu6502::BEQ() { if (GetFlag(Z))  { cyclesRemaining += BranchHelper(*this, true, pc, addrRel); } return 0; }
u8 Cpu6502::BNE() { if (!GetFlag(Z)) { cyclesRemaining += BranchHelper(*this, true, pc, addrRel); } return 0; }
u8 Cpu6502::BMI() { if (GetFlag(N))  { cyclesRemaining += BranchHelper(*this, true, pc, addrRel); } return 0; }
u8 Cpu6502::BPL() { if (!GetFlag(N)) { cyclesRemaining += BranchHelper(*this, true, pc, addrRel); } return 0; }
u8 Cpu6502::BVC() { if (!GetFlag(V)) { cyclesRemaining += BranchHelper(*this, true, pc, addrRel); } return 0; }
u8 Cpu6502::BVS() { if (GetFlag(V))  { cyclesRemaining += BranchHelper(*this, true, pc, addrRel); } return 0; }

u8 Cpu6502::CLC() { SetFlag(C, false); return 0; }
u8 Cpu6502::CLD() { SetFlag(D, false); return 0; }
u8 Cpu6502::CLI() { SetFlag(I, false); return 0; }
u8 Cpu6502::CLV() { SetFlag(V, false); return 0; }
u8 Cpu6502::SEC() { SetFlag(C, true); return 0; }
u8 Cpu6502::SED() { SetFlag(D, true); return 0; }
u8 Cpu6502::SEI() { SetFlag(I, true); return 0; }

u8 Cpu6502::JMP() { pc = addrAbs; return 0; }
u8 Cpu6502::JSR()
{
    pc--;
    Write(0x0100 + sp, (pc >> 8) & 0xFF); sp--;
    Write(0x0100 + sp, pc & 0xFF); sp--;
    pc = addrAbs;
    return 0;
}
u8 Cpu6502::RTS()
{
    sp++; u16 lo = Read(0x0100 + sp);
    sp++; u16 hi = Read(0x0100 + sp);
    pc = (hi << 8) | lo;
    pc++;
    return 0;
}

u8 Cpu6502::BRK()
{
    pc++;
    SetFlag(I, true);
    Write(0x0100 + sp, (pc >> 8) & 0xFF); sp--;
    Write(0x0100 + sp, pc & 0xFF); sp--;
    SetFlag(B, true);
    Write(0x0100 + sp, status); sp--;
    SetFlag(B, false);

    pc = (u16)Read(0xFFFE) | ((u16)Read(0xFFFF) << 8);
    return 0;
}

u8 Cpu6502::RTI()
{
    sp++; status = Read(0x0100 + sp);
    status &= ~B;
    status &= ~U;
    sp++; u16 lo = Read(0x0100 + sp);
    sp++; u16 hi = Read(0x0100 + sp);
    pc = (hi << 8) | lo;
    return 0;
}

u8 Cpu6502::LDA() { Fetch(); a = fetched; SetFlag(Z, a == 0); SetFlag(N, a & 0x80); return 1; }
u8 Cpu6502::LDX() { Fetch(); x = fetched; SetFlag(Z, x == 0); SetFlag(N, x & 0x80); return 1; }
u8 Cpu6502::LDY() { Fetch(); y = fetched; SetFlag(Z, y == 0); SetFlag(N, y & 0x80); return 1; }

u8 Cpu6502::STA() { Write(addrAbs, a); return 0; }
u8 Cpu6502::STX() { Write(addrAbs, x); return 0; }
u8 Cpu6502::STY() { Write(addrAbs, y); return 0; }

u8 Cpu6502::TAX() { x = a; SetFlag(Z, x == 0); SetFlag(N, x & 0x80); return 0; }
u8 Cpu6502::TAY() { y = a; SetFlag(Z, y == 0); SetFlag(N, y & 0x80); return 0; }
u8 Cpu6502::TXA() { a = x; SetFlag(Z, a == 0); SetFlag(N, a & 0x80); return 0; }
u8 Cpu6502::TYA() { a = y; SetFlag(Z, a == 0); SetFlag(N, a & 0x80); return 0; }
u8 Cpu6502::TSX() { x = sp; SetFlag(Z, x == 0); SetFlag(N, x & 0x80); return 0; }
u8 Cpu6502::TXS() { sp = x; return 0; }

u8 Cpu6502::PHA() { Write(0x0100 + sp, a); sp--; return 0; }
u8 Cpu6502::PHP() { Write(0x0100 + sp, status | B | U); sp--; return 0; }
u8 Cpu6502::PLA() { sp++; a = Read(0x0100 + sp); SetFlag(Z, a == 0); SetFlag(N, a & 0x80); return 0; }
u8 Cpu6502::PLP() { sp++; status = Read(0x0100 + sp); SetFlag(U, true); SetFlag(B, false); return 0; }

u8 Cpu6502::NOP() { return 1; }
u8 Cpu6502::XXX() { return 0; }

void Cpu6502::BuildTable()
{
    table.assign(256, Instruction{ &Cpu6502::XXX, &Cpu6502::IMP, 2 });

    auto set = [&](u8 op, u8 (Cpu6502::*o)(), u8 (Cpu6502::*m)(), u8 c)
    {
        table[op] = Instruction{ o, m, c };
    };

    set(0x00,&Cpu6502::BRK,&Cpu6502::IMP,7); set(0x01,&Cpu6502::ORA,&Cpu6502::IZX,6);
    set(0x05,&Cpu6502::ORA,&Cpu6502::ZP0,3); set(0x06,&Cpu6502::ASL,&Cpu6502::ZP0,5);
    set(0x08,&Cpu6502::PHP,&Cpu6502::IMP,3); set(0x09,&Cpu6502::ORA,&Cpu6502::IMM,2);
    set(0x0A,&Cpu6502::ASL,&Cpu6502::IMP,2); set(0x0D,&Cpu6502::ORA,&Cpu6502::ABS,4);
    set(0x0E,&Cpu6502::ASL,&Cpu6502::ABS,6);

    set(0x10,&Cpu6502::BPL,&Cpu6502::REL,2); set(0x11,&Cpu6502::ORA,&Cpu6502::IZY,5);
    set(0x15,&Cpu6502::ORA,&Cpu6502::ZPX,4); set(0x16,&Cpu6502::ASL,&Cpu6502::ZPX,6);
    set(0x18,&Cpu6502::CLC,&Cpu6502::IMP,2); set(0x19,&Cpu6502::ORA,&Cpu6502::ABY,4);
    set(0x1D,&Cpu6502::ORA,&Cpu6502::ABX,4); set(0x1E,&Cpu6502::ASL,&Cpu6502::ABX,7);

    set(0x20,&Cpu6502::JSR,&Cpu6502::ABS,6); set(0x21,&Cpu6502::AND,&Cpu6502::IZX,6);
    set(0x24,&Cpu6502::BIT,&Cpu6502::ZP0,3); set(0x25,&Cpu6502::AND,&Cpu6502::ZP0,3);
    set(0x26,&Cpu6502::ROL,&Cpu6502::ZP0,5); set(0x28,&Cpu6502::PLP,&Cpu6502::IMP,4);
    set(0x29,&Cpu6502::AND,&Cpu6502::IMM,2); set(0x2A,&Cpu6502::ROL,&Cpu6502::IMP,2);
    set(0x2C,&Cpu6502::BIT,&Cpu6502::ABS,4); set(0x2D,&Cpu6502::AND,&Cpu6502::ABS,4);
    set(0x2E,&Cpu6502::ROL,&Cpu6502::ABS,6);

    set(0x30,&Cpu6502::BMI,&Cpu6502::REL,2); set(0x31,&Cpu6502::AND,&Cpu6502::IZY,5);
    set(0x35,&Cpu6502::AND,&Cpu6502::ZPX,4); set(0x36,&Cpu6502::ROL,&Cpu6502::ZPX,6);
    set(0x38,&Cpu6502::SEC,&Cpu6502::IMP,2); set(0x39,&Cpu6502::AND,&Cpu6502::ABY,4);
    set(0x3D,&Cpu6502::AND,&Cpu6502::ABX,4); set(0x3E,&Cpu6502::ROL,&Cpu6502::ABX,7);

    set(0x40,&Cpu6502::RTI,&Cpu6502::IMP,6); set(0x41,&Cpu6502::EOR,&Cpu6502::IZX,6);
    set(0x45,&Cpu6502::EOR,&Cpu6502::ZP0,3); set(0x46,&Cpu6502::LSR,&Cpu6502::ZP0,5);
    set(0x48,&Cpu6502::PHA,&Cpu6502::IMP,3); set(0x49,&Cpu6502::EOR,&Cpu6502::IMM,2);
    set(0x4A,&Cpu6502::LSR,&Cpu6502::IMP,2); set(0x4C,&Cpu6502::JMP,&Cpu6502::ABS,3);
    set(0x4D,&Cpu6502::EOR,&Cpu6502::ABS,4); set(0x4E,&Cpu6502::LSR,&Cpu6502::ABS,6);

    set(0x50,&Cpu6502::BVC,&Cpu6502::REL,2); set(0x51,&Cpu6502::EOR,&Cpu6502::IZY,5);
    set(0x55,&Cpu6502::EOR,&Cpu6502::ZPX,4); set(0x56,&Cpu6502::LSR,&Cpu6502::ZPX,6);
    set(0x58,&Cpu6502::CLI,&Cpu6502::IMP,2); set(0x59,&Cpu6502::EOR,&Cpu6502::ABY,4);
    set(0x5D,&Cpu6502::EOR,&Cpu6502::ABX,4); set(0x5E,&Cpu6502::LSR,&Cpu6502::ABX,7);

    set(0x60,&Cpu6502::RTS,&Cpu6502::IMP,6); set(0x61,&Cpu6502::ADC,&Cpu6502::IZX,6);
    set(0x65,&Cpu6502::ADC,&Cpu6502::ZP0,3); set(0x66,&Cpu6502::ROR,&Cpu6502::ZP0,5);
    set(0x68,&Cpu6502::PLA,&Cpu6502::IMP,4); set(0x69,&Cpu6502::ADC,&Cpu6502::IMM,2);
    set(0x6A,&Cpu6502::ROR,&Cpu6502::IMP,2); set(0x6C,&Cpu6502::JMP,&Cpu6502::IND,5);
    set(0x6D,&Cpu6502::ADC,&Cpu6502::ABS,4); set(0x6E,&Cpu6502::ROR,&Cpu6502::ABS,6);

    set(0x70,&Cpu6502::BVS,&Cpu6502::REL,2); set(0x71,&Cpu6502::ADC,&Cpu6502::IZY,5);
    set(0x75,&Cpu6502::ADC,&Cpu6502::ZPX,4); set(0x76,&Cpu6502::ROR,&Cpu6502::ZPX,6);
    set(0x78,&Cpu6502::SEI,&Cpu6502::IMP,2); set(0x79,&Cpu6502::ADC,&Cpu6502::ABY,4);
    set(0x7D,&Cpu6502::ADC,&Cpu6502::ABX,4); set(0x7E,&Cpu6502::ROR,&Cpu6502::ABX,7);

    set(0x81,&Cpu6502::STA,&Cpu6502::IZX,6); set(0x84,&Cpu6502::STY,&Cpu6502::ZP0,3);
    set(0x85,&Cpu6502::STA,&Cpu6502::ZP0,3); set(0x86,&Cpu6502::STX,&Cpu6502::ZP0,3);
    set(0x88,&Cpu6502::DEY,&Cpu6502::IMP,2); set(0x8A,&Cpu6502::TXA,&Cpu6502::IMP,2);
    set(0x8C,&Cpu6502::STY,&Cpu6502::ABS,4); set(0x8D,&Cpu6502::STA,&Cpu6502::ABS,4);
    set(0x8E,&Cpu6502::STX,&Cpu6502::ABS,4);

    set(0x90,&Cpu6502::BCC,&Cpu6502::REL,2); set(0x91,&Cpu6502::STA,&Cpu6502::IZY,6);
    set(0x94,&Cpu6502::STY,&Cpu6502::ZPX,4); set(0x95,&Cpu6502::STA,&Cpu6502::ZPX,4);
    set(0x96,&Cpu6502::STX,&Cpu6502::ZPY,4); set(0x98,&Cpu6502::TYA,&Cpu6502::IMP,2);
    set(0x99,&Cpu6502::STA,&Cpu6502::ABY,5); set(0x9A,&Cpu6502::TXS,&Cpu6502::IMP,2);
    set(0x9D,&Cpu6502::STA,&Cpu6502::ABX,5);

    set(0xA0,&Cpu6502::LDY,&Cpu6502::IMM,2); set(0xA1,&Cpu6502::LDA,&Cpu6502::IZX,6);
    set(0xA2,&Cpu6502::LDX,&Cpu6502::IMM,2); set(0xA4,&Cpu6502::LDY,&Cpu6502::ZP0,3);
    set(0xA5,&Cpu6502::LDA,&Cpu6502::ZP0,3); set(0xA6,&Cpu6502::LDX,&Cpu6502::ZP0,3);
    set(0xA8,&Cpu6502::TAY,&Cpu6502::IMP,2); set(0xA9,&Cpu6502::LDA,&Cpu6502::IMM,2);
    set(0xAA,&Cpu6502::TAX,&Cpu6502::IMP,2); set(0xAC,&Cpu6502::LDY,&Cpu6502::ABS,4);
    set(0xAD,&Cpu6502::LDA,&Cpu6502::ABS,4); set(0xAE,&Cpu6502::LDX,&Cpu6502::ABS,4);

    set(0xB0,&Cpu6502::BCS,&Cpu6502::REL,2); set(0xB1,&Cpu6502::LDA,&Cpu6502::IZY,5);
    set(0xB4,&Cpu6502::LDY,&Cpu6502::ZPX,4); set(0xB5,&Cpu6502::LDA,&Cpu6502::ZPX,4);
    set(0xB6,&Cpu6502::LDX,&Cpu6502::ZPY,4); set(0xB8,&Cpu6502::CLV,&Cpu6502::IMP,2);
    set(0xB9,&Cpu6502::LDA,&Cpu6502::ABY,4); set(0xBA,&Cpu6502::TSX,&Cpu6502::IMP,2);
    set(0xBC,&Cpu6502::LDY,&Cpu6502::ABX,4); set(0xBD,&Cpu6502::LDA,&Cpu6502::ABX,4);
    set(0xBE,&Cpu6502::LDX,&Cpu6502::ABY,4);

    set(0xC0,&Cpu6502::CPY,&Cpu6502::IMM,2); set(0xC1,&Cpu6502::CMP,&Cpu6502::IZX,6);
    set(0xC4,&Cpu6502::CPY,&Cpu6502::ZP0,3); set(0xC5,&Cpu6502::CMP,&Cpu6502::ZP0,3);
    set(0xC6,&Cpu6502::DEC,&Cpu6502::ZP0,5); set(0xC8,&Cpu6502::INY,&Cpu6502::IMP,2);
    set(0xC9,&Cpu6502::CMP,&Cpu6502::IMM,2); set(0xCA,&Cpu6502::DEX,&Cpu6502::IMP,2);
    set(0xCC,&Cpu6502::CPY,&Cpu6502::ABS,4); set(0xCD,&Cpu6502::CMP,&Cpu6502::ABS,4);
    set(0xCE,&Cpu6502::DEC,&Cpu6502::ABS,6);

    set(0xD0,&Cpu6502::BNE,&Cpu6502::REL,2); set(0xD1,&Cpu6502::CMP,&Cpu6502::IZY,5);
    set(0xD5,&Cpu6502::CMP,&Cpu6502::ZPX,4); set(0xD6,&Cpu6502::DEC,&Cpu6502::ZPX,6);
    set(0xD8,&Cpu6502::CLD,&Cpu6502::IMP,2); set(0xD9,&Cpu6502::CMP,&Cpu6502::ABY,4);
    set(0xDD,&Cpu6502::CMP,&Cpu6502::ABX,4); set(0xDE,&Cpu6502::DEC,&Cpu6502::ABX,7);

    set(0xE0,&Cpu6502::CPX,&Cpu6502::IMM,2); set(0xE1,&Cpu6502::SBC,&Cpu6502::IZX,6);
    set(0xE4,&Cpu6502::CPX,&Cpu6502::ZP0,3); set(0xE5,&Cpu6502::SBC,&Cpu6502::ZP0,3);
    set(0xE6,&Cpu6502::INC,&Cpu6502::ZP0,5); set(0xE8,&Cpu6502::INX,&Cpu6502::IMP,2);
    set(0xE9,&Cpu6502::SBC,&Cpu6502::IMM,2); set(0xEA,&Cpu6502::NOP,&Cpu6502::IMP,2);
    set(0xEC,&Cpu6502::CPX,&Cpu6502::ABS,4); set(0xED,&Cpu6502::SBC,&Cpu6502::ABS,4);
    set(0xEE,&Cpu6502::INC,&Cpu6502::ABS,6);

    set(0xF0,&Cpu6502::BEQ,&Cpu6502::REL,2); set(0xF1,&Cpu6502::SBC,&Cpu6502::IZY,5);
    set(0xF5,&Cpu6502::SBC,&Cpu6502::ZPX,4); set(0xF6,&Cpu6502::INC,&Cpu6502::ZPX,6);
    set(0xF8,&Cpu6502::SED,&Cpu6502::IMP,2); set(0xF9,&Cpu6502::SBC,&Cpu6502::ABY,4);
    set(0xFD,&Cpu6502::SBC,&Cpu6502::ABX,4); set(0xFE,&Cpu6502::INC,&Cpu6502::ABX,7);

    const u8 nopImm[]  = {0x80,0x82,0x89,0xC2,0xE2};
    for (u8 op : nopImm) set(op,&Cpu6502::NOP,&Cpu6502::IMM,2);
    const u8 nopImp[]  = {0x1A,0x3A,0x5A,0x7A,0xDA,0xFA};
    for (u8 op : nopImp) set(op,&Cpu6502::NOP,&Cpu6502::IMP,2);
    const u8 nopZp[]   = {0x04,0x44,0x64};
    for (u8 op : nopZp) set(op,&Cpu6502::NOP,&Cpu6502::ZP0,3);
    const u8 nopZpx[]  = {0x14,0x34,0x54,0x74,0xD4,0xF4};
    for (u8 op : nopZpx) set(op,&Cpu6502::NOP,&Cpu6502::ZPX,4);
    const u8 nopAbs[]  = {0x0C};
    for (u8 op : nopAbs) set(op,&Cpu6502::NOP,&Cpu6502::ABS,4);
    const u8 nopAbx[]  = {0x1C,0x3C,0x5C,0x7C,0xDC,0xFC};
    for (u8 op : nopAbx) set(op,&Cpu6502::NOP,&Cpu6502::ABX,4);
}

void Cpu6502::SaveState(StateWriter& w) const
{
    w.U8(a); w.U8(x); w.U8(y); w.U8(sp);
    w.U16(pc); w.U8(status);
    w.U8(fetched); w.U16(addrAbs); w.U16(addrRel);
    w.U8(opcode); w.U8(cyclesRemaining);
    w.Bool(nmiPending); w.Bool(irqLine);
}

void Cpu6502::LoadState(StateReader& r)
{
    a = r.U8(); x = r.U8(); y = r.U8(); sp = r.U8();
    pc = r.U16(); status = r.U8();
    fetched = r.U8(); addrAbs = r.U16(); addrRel = r.U16();
    opcode = r.U8(); cyclesRemaining = r.U8();
    nmiPending = r.Bool(); irqLine = r.Bool();
}
