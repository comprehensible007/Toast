#include "apu2A03.h"
#include "bus.h"

static const u8  kLengthTable[32] = {
    10,254,20,2,40,4,80,6,160,8,60,10,14,12,26,14,
    12,16,24,18,48,20,96,22,192,24,72,26,16,28,32,30
};
static const u16 kNoisePeriodTable[16] = {
    4,8,16,32,64,96,128,160,202,254,380,508,762,1016,2034,4068
};
static const u16 kDmcRateTable[16] = {
    428,380,340,320,286,254,226,214,190,160,142,128,106,84,72,54
};
static const u8 kDutyTable[4][8] = {
    {0,1,0,0,0,0,0,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,0,0,0},
    {1,0,0,1,1,1,1,1}
};
static const u8 kTriSeq[32] = {
    15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
};

void Apu2A03::Envelope::Clock()
{
    if (start) { start = false; decay = 15; divider = volume; }
    else if (divider == 0) { divider = volume; if (decay) decay--; else if (loop) decay = 15; }
    else divider--;
}

void Apu2A03::Pulse::WriteReg(int n, u8 data)
{
    switch (n)
    {
    case 0:
        duty = (data >> 6) & 3;
        lengthHalt = (data >> 5) & 1;
        env.loop = lengthHalt;
        env.constantVolume = (data >> 4) & 1;
        env.volume = data & 0x0F;
        break;
    case 1:
        sweep.enabled = (data >> 7) & 1;
        sweep.period = (data >> 4) & 7;
        sweep.negate = (data >> 3) & 1;
        sweep.shift = data & 7;
        sweep.reload = true;
        break;
    case 2:
        timerPeriod = (timerPeriod & 0xFF00) | data;
        break;
    case 3:
        timerPeriod = (timerPeriod & 0x00FF) | ((data & 7) << 8);
        if (enabled) lengthCounter = kLengthTable[(data >> 3) & 0x1F];
        env.start = true;
        break;
    }
}

void Apu2A03::Pulse::ClockTimer()
{
    if (timer == 0) { timer = timerPeriod; dutyStep = (dutyStep + 1) & 7; }
    else timer--;
}

u16 Apu2A03::Pulse::SweepTarget() const
{
    u16 change = timerPeriod >> sweep.shift;
    if (sweep.negate) return isChannel2 ? (timerPeriod - change) : (timerPeriod - change - 1);
    return timerPeriod + change;
}

void Apu2A03::Pulse::ClockSweep()
{
    u16 target = SweepTarget();
    bool muted = (timerPeriod < 8) || (!sweep.negate && target > 0x7FF);
    if (sweep.divider == 0 && sweep.enabled && sweep.shift != 0 && !muted)
        timerPeriod = target;
    if (sweep.divider == 0 || sweep.reload) { sweep.divider = sweep.period; sweep.reload = false; }
    else sweep.divider--;
}

u8 Apu2A03::Pulse::Output() const
{
    if (!enabled || lengthCounter == 0 || timerPeriod < 8) return 0;
    u16 target = SweepTarget();
    if (!sweep.negate && target > 0x7FF) return 0;
    if (kDutyTable[duty][dutyStep] == 0) return 0;
    return env.Output();
}

void Apu2A03::Triangle::WriteReg(int n, u8 data)
{
    switch (n)
    {
    case 0: lengthHalt = (data >> 7) & 1; linearReload = data & 0x7F; break;
    case 2: timerPeriod = (timerPeriod & 0xFF00) | data; break;
    case 3:
        timerPeriod = (timerPeriod & 0x00FF) | ((data & 7) << 8);
        if (enabled) lengthCounter = kLengthTable[(data >> 3) & 0x1F];
        linearReloadFlag = true;
        break;
    }
}

void Apu2A03::Triangle::ClockTimer()
{
    if (timer == 0)
    {
        timer = timerPeriod;
        if (linearCounter > 0 && lengthCounter > 0) seqStep = (seqStep + 1) & 31;
    }
    else timer--;
}

void Apu2A03::Triangle::ClockLinear()
{
    if (linearReloadFlag) linearCounter = linearReload;
    else if (linearCounter > 0) linearCounter--;
    if (!lengthHalt) linearReloadFlag = false;
}

u8 Apu2A03::Triangle::Output() const
{
    if (!enabled || timerPeriod < 2) return 0;
    return kTriSeq[seqStep];
}

void Apu2A03::Noise::WriteReg(int n, u8 data)
{
    switch (n)
    {
    case 0:
        lengthHalt = (data >> 5) & 1;
        env.loop = lengthHalt;
        env.constantVolume = (data >> 4) & 1;
        env.volume = data & 0x0F;
        break;
    case 2:
        mode = (data >> 7) & 1;
        timerPeriod = kNoisePeriodTable[data & 0x0F];
        break;
    case 3:
        if (enabled) lengthCounter = kLengthTable[(data >> 3) & 0x1F];
        env.start = true;
        break;
    }
}

void Apu2A03::Noise::ClockTimer()
{
    if (timer == 0)
    {
        timer = timerPeriod;
        u16 tapBit = mode ? ((shift >> 6) & 1) : ((shift >> 1) & 1);
        u16 feedback = (shift & 1) ^ tapBit;
        shift = (shift >> 1) | (feedback << 14);
    }
    else timer--;
}

u8 Apu2A03::Noise::Output() const
{
    if (!enabled || lengthCounter == 0 || (shift & 1)) return 0;
    return env.Output();
}

void Apu2A03::InitFilters()
{
    const double dt = 1.0 / kSampleRate;
    const double pi = 3.14159265358979323846;

    double rc90    = 1.0 / (2.0 * pi * 90.0);
    double rc440   = 1.0 / (2.0 * pi * 440.0);
    double rc14000 = 1.0 / (2.0 * pi * 14000.0);

    hp1Alpha = rc90 / (rc90 + dt);
    hp2Alpha = rc440 / (rc440 + dt);
    lpAlpha  = dt / (rc14000 + dt);

    hpPrevIn1 = hpPrevOut1 = 0.0;
    hpPrevIn2 = hpPrevOut2 = 0.0;
    lpPrevOut = 0.0;
}

double Apu2A03::Filter(double x)
{
    double y1 = hp1Alpha * (hpPrevOut1 + x - hpPrevIn1);
    hpPrevIn1 = x; hpPrevOut1 = y1;

    double y2 = hp2Alpha * (hpPrevOut2 + y1 - hpPrevIn2);
    hpPrevIn2 = y1; hpPrevOut2 = y2;

    double y3 = lpPrevOut + lpAlpha * (y2 - lpPrevOut);
    lpPrevOut = y3;

    return y3;
}

void Apu2A03::Reset()
{
    pulse1 = Pulse{}; pulse2 = Pulse{}; pulse2.isChannel2 = true;
    triangle = Triangle{};
    noise = Noise{}; noise.shift = 1;
    dmc = Dmc{};
    frameMode5Step = false;
    frameIrqInhibit = false;
    frameIrq = false;
    cpuCycle = 0;
    frameSeqCounter = 0;
    sampleCycleAccum = 0.0;
    ringWrite = 0; ringRead = 0;
    InitFilters();
}

void Apu2A03::CpuWrite(u16 addr, u8 data)
{
    switch (addr)
    {
    case 0x4000: pulse1.WriteReg(0, data); break;
    case 0x4001: pulse1.WriteReg(1, data); break;
    case 0x4002: pulse1.WriteReg(2, data); break;
    case 0x4003: pulse1.WriteReg(3, data); break;
    case 0x4004: pulse2.WriteReg(0, data); break;
    case 0x4005: pulse2.WriteReg(1, data); break;
    case 0x4006: pulse2.WriteReg(2, data); break;
    case 0x4007: pulse2.WriteReg(3, data); break;
    case 0x4008: triangle.WriteReg(0, data); break;
    case 0x400A: triangle.WriteReg(2, data); break;
    case 0x400B: triangle.WriteReg(3, data); break;
    case 0x400C: noise.WriteReg(0, data); break;
    case 0x400E: noise.WriteReg(2, data); break;
    case 0x400F: noise.WriteReg(3, data); break;
    case 0x4010:
        dmc.irqEnabled = data & 0x80;
        dmc.loop = data & 0x40;
        dmc.rate = kDmcRateTable[data & 0x0F];
        if (!dmc.irqEnabled) dmc.irqPending = false;
        break;
    case 0x4011:
        dmc.outputLevel = data & 0x7F;
        break;
    case 0x4012:
        dmc.sampleAddress = 0xC000 + (u16)data * 64;
        break;
    case 0x4013:
        dmc.sampleLength = (u16)data * 16 + 1;
        break;
    case 0x4015:
        pulse1.enabled = data & 1;       if (!pulse1.enabled) pulse1.lengthCounter = 0;
        pulse2.enabled = (data >> 1) & 1; if (!pulse2.enabled) pulse2.lengthCounter = 0;
        triangle.enabled = (data >> 2) & 1; if (!triangle.enabled) triangle.lengthCounter = 0;
        noise.enabled = (data >> 3) & 1;  if (!noise.enabled) noise.lengthCounter = 0;
        dmc.enabled = (data >> 4) & 1;
        if (!dmc.enabled)
        {
            dmc.bytesRemaining = 0;
        }
        else if (dmc.bytesRemaining == 0)
        {
            dmc.currentAddress = dmc.sampleAddress;
            dmc.bytesRemaining = dmc.sampleLength;
        }
        dmc.irqPending = false;
        break;
    case 0x4017:
        frameMode5Step = (data >> 7) & 1;
        frameIrqInhibit = (data >> 6) & 1;
        if (frameIrqInhibit) frameIrq = false;
        frameSeqCounter = 0;
        if (frameMode5Step) { ClockQuarterFrame(); ClockHalfFrame(); }
        break;
    default: break;
    }
}

u8 Apu2A03::CpuRead(u16 addr)
{
    if (addr != 0x4015) return 0;
    u8 status = 0;
    if (pulse1.lengthCounter > 0)   status |= 0x01;
    if (pulse2.lengthCounter > 0)   status |= 0x02;
    if (triangle.lengthCounter > 0) status |= 0x04;
    if (noise.lengthCounter > 0)    status |= 0x08;
    if (dmc.bytesRemaining > 0)     status |= 0x10;
    if (frameIrq) status |= 0x40;
    if (dmc.irqPending) status |= 0x80;
    frameIrq = false;
    return status;
}

void Apu2A03::ClockQuarterFrame()
{
    pulse1.env.Clock(); pulse2.env.Clock(); noise.env.Clock();
    triangle.ClockLinear();
}

void Apu2A03::ClockHalfFrame()
{
    pulse1.ClockLength(); pulse1.ClockSweep();
    pulse2.ClockLength(); pulse2.ClockSweep();
    triangle.ClockLength();
    noise.ClockLength();
}

void Apu2A03::ClockFrameSequencer()
{
    frameSeqCounter++;
    if (!frameMode5Step)
    {
        static const long long s[4] = { 7457,14913,22371,29829 };
        if (frameSeqCounter == s[0] || frameSeqCounter == s[1] || frameSeqCounter == s[2] || frameSeqCounter == s[3])
        {
            ClockQuarterFrame();
            if (frameSeqCounter == s[1] || frameSeqCounter == s[3]) ClockHalfFrame();
            if (frameSeqCounter == s[3])
            {
                if (!frameIrqInhibit) frameIrq = true;
                frameSeqCounter = 0;
            }
        }
    }
    else
    {
        static const long long s[5] = { 7457,14913,22371,29829,37281 };
        if (frameSeqCounter == s[0] || frameSeqCounter == s[1] || frameSeqCounter == s[2] || frameSeqCounter == s[4])
        {
            ClockQuarterFrame();
            if (frameSeqCounter == s[1] || frameSeqCounter == s[4]) ClockHalfFrame();
        }
        if (frameSeqCounter == s[4]) frameSeqCounter = 0;
    }
}

void Apu2A03::ClockDmc()
{
    if (dmc.timer == 0)
    {
        dmc.timer = dmc.rate;

        if (!dmc.silence)
        {
            if (dmc.shiftReg & 1)
            {
                if (dmc.outputLevel <= 125) dmc.outputLevel += 2;
            }
            else
            {
                if (dmc.outputLevel >= 2) dmc.outputLevel -= 2;
            }
        }
        dmc.shiftReg >>= 1;
        if (dmc.bitsRemaining > 0) dmc.bitsRemaining--;
        if (dmc.bitsRemaining == 0)
        {
            dmc.bitsRemaining = 8;
            if (!dmc.bufferHasValue)
            {
                dmc.silence = true;
            }
            else
            {
                dmc.silence = false;
                dmc.shiftReg = dmc.sampleBuffer;
                dmc.bufferHasValue = false;
            }
        }
    }
    else
    {
        dmc.timer--;
    }

    if (!dmc.bufferHasValue && dmc.bytesRemaining > 0)
    {
        if (bus) dmc.sampleBuffer = bus->CpuRead(dmc.currentAddress);
        dmc.bufferHasValue = true;
        dmc.currentAddress = (dmc.currentAddress == 0xFFFF) ? 0x8000 : (dmc.currentAddress + 1);
        dmc.bytesRemaining--;
        if (dmc.bytesRemaining == 0)
        {
            if (dmc.loop)
            {
                dmc.currentAddress = dmc.sampleAddress;
                dmc.bytesRemaining = dmc.sampleLength;
            }
            else if (dmc.irqEnabled)
            {
                dmc.irqPending = true;
            }
        }
    }
}

void Apu2A03::PushSample()
{
    double p1 = pulse1.Output(), p2 = pulse2.Output();
    double t = triangle.Output(), n = noise.Output();
    double d = dmc.outputLevel;

    double pulseOut = (p1 + p2 == 0) ? 0.0 : 95.88 / ((8128.0 / (p1 + p2)) + 100.0);
    double tndSum = (t / 8227.0) + (n / 12241.0) + (d / 22638.0);
    double tndOut = (tndSum == 0.0) ? 0.0 : 159.79 / ((1.0 / tndSum) + 100.0);

    double mixed = Filter(pulseOut + tndOut);
    int16_t sample = (int16_t)(mixed * 30000.0);

    size_t w = ringWrite.load(std::memory_order_relaxed);
    size_t next = (w + 1) & (kRingSize - 1);
    if (next != ringRead.load(std::memory_order_acquire))
    {
        ringBuffer[w] = sample;
        ringWrite.store(next, std::memory_order_release);
    }
}

void Apu2A03::Clock()
{
    cpuCycle++;
    if (cpuCycle % 2 == 0) { pulse1.ClockTimer(); pulse2.ClockTimer(); noise.ClockTimer(); }
    triangle.ClockTimer();
    ClockDmc();

    ClockFrameSequencer();

    sampleCycleAccum += 1.0;
    if (sampleCycleAccum >= cyclesPerSample)
    {
        sampleCycleAccum -= cyclesPerSample;
        PushSample();
    }
}

void Apu2A03::SaveState(StateWriter& w) const
{
    pulse1.SaveState(w); pulse2.SaveState(w);
    triangle.SaveState(w); noise.SaveState(w); dmc.SaveState(w);
    w.Bool(frameMode5Step); w.Bool(frameIrqInhibit); w.Bool(frameIrq);
    w.S64(cpuCycle); w.S64(frameSeqCounter);
    w.F64(sampleCycleAccum);
    w.F64(hpPrevIn1); w.F64(hpPrevOut1);
    w.F64(hpPrevIn2); w.F64(hpPrevOut2);
    w.F64(lpPrevOut);
}

void Apu2A03::LoadState(StateReader& r)
{
    pulse1.LoadState(r); pulse2.LoadState(r);
    triangle.LoadState(r); noise.LoadState(r); dmc.LoadState(r);
    frameMode5Step = r.Bool(); frameIrqInhibit = r.Bool(); frameIrq = r.Bool();
    cpuCycle = r.S64(); frameSeqCounter = r.S64();
    sampleCycleAccum = r.F64();
    hpPrevIn1 = r.F64(); hpPrevOut1 = r.F64();
    hpPrevIn2 = r.F64(); hpPrevOut2 = r.F64();
    lpPrevOut = r.F64();
}
