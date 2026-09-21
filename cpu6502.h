#pragma once
#include "SaveStateIO.h"
#include "types.h"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

class Bus;

class Cpu6502
{
public:
    Cpu6502();

    void ConnectBus(Bus* b) { bus = b; }

    u8  a = 0, x = 0, y = 0, sp = 0xFD;
    u16 pc = 0;
    u8  status = 0x24;

    enum Flags : u8
    {
        C = 1 << 0, Z = 1 << 1, I = 1 << 2, D = 1 << 3,
        B = 1 << 4, U = 1 << 5, V = 1 << 6, N = 1 << 7
    };

    void Reset();
    void IRQ();
    void NMI();
    void RequestNMI() { nmiPending = true; }
    void SetIRQLine(bool level) { irqLine = level; }
    void Clock();

    bool InstructionComplete() const { return cyclesRemaining == 0; }

    u8 fetched = 0;
    u16 addrAbs = 0;
    u16 addrRel = 0;
    u8  opcode = 0;
    u8  cyclesRemaining = 0;
    
    void SaveState(StateWriter& w) const;
    void LoadState(StateReader& r);
    void DebugDump(FILE* f) const;
    

private:
    Bus* bus = nullptr;
    bool nmiPending = false;
    bool irqLine = false;

    u8 Read(u16 addr);
    void Write(u16 addr, u8 data);

    u8 GetFlag(Flags f) const { return (status & f) ? 1 : 0; }
    void SetFlag(Flags f, bool v) { if (v) status |= f; else status &= ~f; }

    u8 Fetch();

    u8 IMP(); u8 IMM(); u8 ZP0(); u8 ZPX(); u8 ZPY(); u8 REL();
    u8 ABS(); u8 ABX(); u8 ABY(); u8 IND(); u8 IZX(); u8 IZY();

    u8 ADC(); u8 AND(); u8 ASL(); u8 BCC(); u8 BCS(); u8 BEQ(); u8 BIT(); u8 BMI();
    u8 BNE(); u8 BPL(); u8 BRK(); u8 BVC(); u8 BVS(); u8 CLC(); u8 CLD(); u8 CLI();
    u8 CLV(); u8 CMP(); u8 CPX(); u8 CPY(); u8 DEC(); u8 DEX(); u8 DEY(); u8 EOR();
    u8 INC(); u8 INX(); u8 INY(); u8 JMP(); u8 JSR(); u8 LDA(); u8 LDX(); u8 LDY();
    u8 LSR(); u8 NOP(); u8 ORA(); u8 PHA(); u8 PHP(); u8 PLA(); u8 PLP(); u8 ROL();
    u8 ROR(); u8 RTI(); u8 RTS(); u8 SBC(); u8 SEC(); u8 SED(); u8 SEI(); u8 STA();
    u8 STX(); u8 STY(); u8 TAX(); u8 TAY(); u8 TSX(); u8 TXA(); u8 TXS(); u8 TYA();
    u8 XXX();
    
    u16 lastFetchedPc = 0xFFFF;
    long long samePcStreak = 0;

    struct Instruction
    {
        u8 (Cpu6502::*operate)();
        u8 (Cpu6502::*addrmode)();
        u8 cycles;

        Instruction(u8 (Cpu6502::*o)() = nullptr, u8 (Cpu6502::*m)() = nullptr, u8 c = 0)
            : operate(o), addrmode(m), cycles(c) {}
    };
    std::vector<Instruction> table;
    void BuildTable();
};
