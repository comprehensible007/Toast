#pragma once
#include "types.h"
#include "SaveStateIO.h"
#include <cstdint>
#include <atomic>

class Bus;

class Apu2A03
{
public:
    Apu2A03() { Reset(); }
    void Reset();

    void ConnectBus(Bus* b) { bus = b; }

    void CpuWrite(u16 addr, u8 data);
    u8   CpuRead(u16 addr);

    void Clock();

    bool IrqRequested() const { return frameIrq || dmc.irqPending; }
    bool FrameIrqFlag() const { return frameIrq; }
    bool DmcIrqFlag() const { return dmc.irqPending; }

    void SaveState(StateWriter& w) const;
    void LoadState(StateReader& r);

    static constexpr size_t kRingSize = 1 << 14;
    int16_t ringBuffer[kRingSize]{};
    std::atomic<size_t> ringWrite{ 0 };
    std::atomic<size_t> ringRead{ 0 };
    static constexpr double kSampleRate = 44100.0;

private:
    struct Envelope
    {
        bool start = false, loop = false, constantVolume = false;
        u8 volume = 0, decay = 0, divider = 0;
        void Clock();
        u8 Output() const { return constantVolume ? volume : decay; }
        void SaveState(StateWriter& w) const { w.Bool(start); w.Bool(loop); w.Bool(constantVolume); w.U8(volume); w.U8(decay); w.U8(divider); }
        void LoadState(StateReader& r) { start = r.Bool(); loop = r.Bool(); constantVolume = r.Bool(); volume = r.U8(); decay = r.U8(); divider = r.U8(); }
    };

    struct Sweep
    {
        bool enabled = false, negate = false, reload = false;
        u8 period = 0, shift = 0, divider = 0;
        void SaveState(StateWriter& w) const { w.Bool(enabled); w.Bool(negate); w.Bool(reload); w.U8(period); w.U8(shift); w.U8(divider); }
        void LoadState(StateReader& r) { enabled = r.Bool(); negate = r.Bool(); reload = r.Bool(); period = r.U8(); shift = r.U8(); divider = r.U8(); }
    };

    struct Pulse
    {
        bool enabled = false, lengthHalt = false, isChannel2 = false;
        u8 duty = 0, dutyStep = 0, lengthCounter = 0;
        u16 timerPeriod = 0, timer = 0;
        Envelope env;
        Sweep sweep;

        void WriteReg(int n, u8 data);
        void ClockTimer();
        void ClockSweep();
        void ClockLength() { if (!lengthHalt && lengthCounter) lengthCounter--; }
        u16  SweepTarget() const;
        u8   Output() const;

        void SaveState(StateWriter& w) const
        {
            w.Bool(enabled); w.Bool(lengthHalt); w.Bool(isChannel2);
            w.U8(duty); w.U8(dutyStep); w.U8(lengthCounter);
            w.U16(timerPeriod); w.U16(timer);
            env.SaveState(w); sweep.SaveState(w);
        }
        void LoadState(StateReader& r)
        {
            enabled = r.Bool(); lengthHalt = r.Bool(); isChannel2 = r.Bool();
            duty = r.U8(); dutyStep = r.U8(); lengthCounter = r.U8();
            timerPeriod = r.U16(); timer = r.U16();
            env.LoadState(r); sweep.LoadState(r);
        }
    };

    struct Triangle
    {
        bool enabled = false, lengthHalt = false, linearReloadFlag = false;
        u8 linearReload = 0, linearCounter = 0, seqStep = 0, lengthCounter = 0;
        u16 timerPeriod = 0, timer = 0;

        void WriteReg(int n, u8 data);
        void ClockTimer();
        void ClockLinear();
        void ClockLength() { if (!lengthHalt && lengthCounter) lengthCounter--; }
        u8   Output() const;

        void SaveState(StateWriter& w) const
        {
            w.Bool(enabled); w.Bool(lengthHalt); w.Bool(linearReloadFlag);
            w.U8(linearReload); w.U8(linearCounter); w.U8(seqStep); w.U8(lengthCounter);
            w.U16(timerPeriod); w.U16(timer);
        }
        void LoadState(StateReader& r)
        {
            enabled = r.Bool(); lengthHalt = r.Bool(); linearReloadFlag = r.Bool();
            linearReload = r.U8(); linearCounter = r.U8(); seqStep = r.U8(); lengthCounter = r.U8();
            timerPeriod = r.U16(); timer = r.U16();
        }
    };

    struct Noise
    {
        bool enabled = false, lengthHalt = false, mode = false;
        u8 lengthCounter = 0;
        u16 timerPeriod = 0, timer = 0, shift = 1;
        Envelope env;

        void WriteReg(int n, u8 data);
        void ClockTimer();
        void ClockLength() { if (!lengthHalt && lengthCounter) lengthCounter--; }
        u8   Output() const;

        void SaveState(StateWriter& w) const
        {
            w.Bool(enabled); w.Bool(lengthHalt); w.Bool(mode);
            w.U8(lengthCounter);
            w.U16(timerPeriod); w.U16(timer); w.U16(shift);
            env.SaveState(w);
        }
        void LoadState(StateReader& r)
        {
            enabled = r.Bool(); lengthHalt = r.Bool(); mode = r.Bool();
            lengthCounter = r.U8();
            timerPeriod = r.U16(); timer = r.U16(); shift = r.U16();
            env.LoadState(r);
        }
    };

    struct Dmc
    {
        bool enabled = false, loop = false, irqEnabled = false, irqPending = false;
        u16 rate = 428, timer = 428;
        u16 sampleAddress = 0xC000, sampleLength = 1;
        u16 currentAddress = 0xC000, bytesRemaining = 0;
        bool bufferHasValue = false;
        u8 sampleBuffer = 0;
        u8 shiftReg = 0, bitsRemaining = 8;
        bool silence = true;
        u8 outputLevel = 0;

        void SaveState(StateWriter& w) const
        {
            w.Bool(enabled); w.Bool(loop); w.Bool(irqEnabled); w.Bool(irqPending);
            w.U16(rate); w.U16(timer);
            w.U16(sampleAddress); w.U16(sampleLength);
            w.U16(currentAddress); w.U16(bytesRemaining);
            w.Bool(bufferHasValue); w.U8(sampleBuffer);
            w.U8(shiftReg); w.U8(bitsRemaining);
            w.Bool(silence); w.U8(outputLevel);
        }
        void LoadState(StateReader& r)
        {
            enabled = r.Bool(); loop = r.Bool(); irqEnabled = r.Bool(); irqPending = r.Bool();
            rate = r.U16(); timer = r.U16();
            sampleAddress = r.U16(); sampleLength = r.U16();
            currentAddress = r.U16(); bytesRemaining = r.U16();
            bufferHasValue = r.Bool(); sampleBuffer = r.U8();
            shiftReg = r.U8(); bitsRemaining = r.U8();
            silence = r.Bool(); outputLevel = r.U8();
        }
    };

    Bus* bus = nullptr;

    Pulse pulse1, pulse2;
    Triangle triangle;
    Noise noise;
    Dmc dmc;

    bool frameMode5Step = false;
    bool frameIrqInhibit = false;
    bool frameIrq = false;
    long long cpuCycle = 0;
    long long frameSeqCounter = 0;

    double sampleCycleAccum = 0.0;
    double cyclesPerSample = 1789773.0 / kSampleRate;

    double hpPrevIn1 = 0.0, hpPrevOut1 = 0.0;
    double hpPrevIn2 = 0.0, hpPrevOut2 = 0.0;
    double lpPrevOut = 0.0;
    double hp1Alpha = 0.0, hp2Alpha = 0.0, lpAlpha = 0.0;
    void InitFilters();
    double Filter(double sample);

    void ClockQuarterFrame();
    void ClockHalfFrame();
    void ClockFrameSequencer();
    void ClockDmc();
    void PushSample();
};
