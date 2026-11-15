/*
 * Copyright (c) 2019-2026 Kim Jørgensen
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/*
 * The Soft Group Super Explode V5.0 (CRT type 48).
 *
 * Two 8 KiB banks at $8000-$9FFF. A write anywhere in $DF00-$DFFF latches
 * data bit 7 as the bank (0 or 1). The same window reads the last page of
 * the selected bank, and that page is identical in both banks so the ROM
 * can call it while the $8000 window is off.
 *
 * /EXROM is forced low while /ROML, /IO1, or reset is low. A 0.47 uF
 * capacitor then holds /EXROM low for about 300 us. /IO1 does not enable
 * the ROM data bus; those reads are open bus. A read of the reset vector
 * clears the bank, which is what /RESET does to the 7474 on the cartridge.
 */

/* One count per PHI2 cycle. 300 cycles is about 300 us on PAL and NTSC. */
#define SE5_HOLD_CYCLES 300

static u16 se5_cycles;

FORCE_INLINE void se5_enable(void)
{
    se5_cycles = SE5_HOLD_CYCLES;
    C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_8K);
}

/*************************************************
* C64 bus read callback
*************************************************/
FORCE_INLINE bool se5_read_handler(u32 control, u32 addr)
{
    if (!(control & C64_ROML))
    {
        se5_cycles = SE5_HOLD_CYCLES;
        C64_DATA_WRITE(crt_ptr[addr & 0x1fff]);
        return true;
    }

    if (!(control & C64_IO2))
    {
        C64_DATA_WRITE(crt_ptr[addr & 0x1fff]);
        return true;
    }

    /* /RESET clears the bank register. The handler cannot see the pin,
       so the reset-vector fetch stands in for it. cycles == 0 holds
       /EXROM low until the kernal's cartridge check touches $8000. */
    if ((addr & 0xfffe) == 0xfffc)
    {
        crt_ptr = crt_banks[0];
        se5_cycles = 0;
        C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_8K);
    }
    else if (!(control & C64_IO1))
    {
        se5_enable();
    }
    else if (se5_cycles && --se5_cycles == 0)
    {
        C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_NONE);
    }

    return false;
}

/*************************************************
* C64 bus write callback
*************************************************/
FORCE_INLINE void se5_write_handler(u32 control, u32 addr, u32 data)
{
    if (!(control & C64_IO1))
    {
        se5_enable();
        return;
    }

    if (!(control & C64_IO2))
    {
        crt_ptr = crt_banks[(data >> 7) & 0x01];
        return;
    }

    if (se5_cycles && --se5_cycles == 0)
    {
        C64_CRT_CONTROL(STATUS_LED_OFF|CRT_PORT_NONE);
    }
}

static void se5_init(void)
{
    crt_ptr = crt_banks[0];
    se5_cycles = 0;
    C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_8K);
}

C64_BUS_HANDLER(se5)
