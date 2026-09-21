#include "bus.h"
#include "cpu6502.h"

Bus::Bus()
{
    apu.ConnectBus(this);
}

void Bus::Reset()
{
    ram.fill(0);
    ppu.Reset();
    apu.Reset();
    totalCycles = 0;
    dmaInProgress = false;
    dmaWaitAlign = true;
}

u8 Bus::CpuRead(u16 addr)
{
    u8 data = 0;
    if (cart && cart->CpuRead(addr, data)) return data;

    if (addr <= 0x1FFF) return ram[addr & 0x07FF];
    if (addr >= 0x2000 && addr <= 0x3FFF) return ppu.CpuRead(addr & 0x0007);

    if (addr == 0x4016 || addr == 0x4017)
    {
        int i = addr - 0x4016;
        u8 bit = controllerShift[i] & 0x01;
        controllerShift[i] = (controllerShift[i] >> 1) | 0x80;
        return bit;
    }

    if (addr == 0x4015) return apu.CpuRead(addr);

    return 0;
}

void Bus::CpuWrite(u16 addr, u8 data)
{
    if (cart && cart->CpuWrite(addr, data)) return;

    if (addr <= 0x1FFF) { ram[addr & 0x07FF] = data; return; }
    if (addr >= 0x2000 && addr <= 0x3FFF) { ppu.CpuWrite(addr & 0x0007, data); return; }

    if (addr == 0x4014)
    {
        dmaPage = data;
        dmaAddr = 0;
        dmaInProgress = true;
        return;
    }

    if (addr == 0x4016)
    {
        if (data & 1)
        {
            controllerLatch[0] = controllerState[0];
            controllerLatch[1] = controllerState[1];
        }
        controllerShift[0] = controllerLatch[0];
        controllerShift[1] = controllerLatch[1];
        return;
    }

    if ((addr >= 0x4000 && addr <= 0x4013) || addr == 0x4015 || addr == 0x4017)
    {
        apu.CpuWrite(addr, data);
        return;
    }
}

void Bus::Clock()
{
    ppu.Clock();

    if (totalCycles % 3 == 0)
    {
        apu.Clock();

        if (dmaInProgress)
        {
        	if (!dmaLoggedThisSession && !(ppu.GetScanline() >= 241 && ppu.GetScanline() <= 260) && ppu.GetScanline() != -1)
        {
        FILE* f = fopen("dma_log.txt", "w");
        if (f)
        {
            fprintf(f, "DMA active during visible picture: scanline=%d cycle=%d dmaAddr=%d\n",
                    ppu.GetScanline(), ppu.GetCycle(), dmaAddr);
            fclose(f);
        }
        dmaLoggedThisSession = true;
        }
            if (dmaWaitAlign)
            {
                if (totalCycles % 2 == 1) dmaWaitAlign = false;
            }
            else
            {
                if (totalCycles % 2 == 0)
                {
                    dmaData = CpuRead((dmaPage << 8) | dmaAddr);
                }
                else
                {
                    ppu.oam[dmaAddr] = dmaData;
                    dmaAddr++;
                    if (dmaAddr == 0)
                    {
                        dmaInProgress = false;
                        dmaWaitAlign = true;
                    }
                }
            }
        }
        else
        {

            if (cpu) { cpu->SetIRQLine(apu.IrqRequested() || (cart && cart->IRQState())); cpu->Clock(); }
        }
    }

    if (ppu.nmiRequested)
    {
        ppu.nmiRequested = false;
        if (cpu) cpu->RequestNMI();
    }

    totalCycles++;
}

void Bus::SaveState(StateWriter& w) const
{
    w.U32(cart ? cart->GetRomCrc() : 0);

    w.Bytes(ram.data(), ram.size());

    for (int i = 0; i < 2; i++)
    {
        w.U8(controllerState[i]);
        w.U8(controllerShift[i]);
        w.U8(controllerLatch[i]);
    }

    w.S64((int64_t)totalCycles);
    w.Bool(dmaInProgress);
    w.Bool(dmaWaitAlign);
    w.U8(dmaPage);
    w.U8(dmaAddr);
    w.U8(dmaData);

    ppu.SaveState(w);
    apu.SaveState(w);
    if (cart) cart->SaveState(w);
    if (cpu) cpu->SaveState(w);
}

void Bus::LoadState(StateReader& r)
{
    u32 savedCrc = r.U32();
    if (!cart || cart->GetRomCrc() != savedCrc)
    {
        r.ok = false;
        return;
    }

    r.Bytes(ram.data(), ram.size());

    for (int i = 0; i < 2; i++)
    {
        controllerState[i] = r.U8();
        controllerShift[i] = r.U8();
        controllerLatch[i] = r.U8();
    }

    totalCycles = (long long)r.S64();
    dmaInProgress = r.Bool();
    dmaWaitAlign = r.Bool();
    dmaPage = r.U8();
    dmaAddr = r.U8();
    dmaData = r.U8();

    ppu.LoadState(r);
    apu.LoadState(r);
    if (cart) cart->LoadState(r);
    if (cpu) cpu->LoadState(r);
}
