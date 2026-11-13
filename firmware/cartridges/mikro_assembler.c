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
 * Supersoft Mikro Assembler (CRT type 28).
 *
 * One 8 KiB ROM at $8000-$9FFF, EXROM low and GAME high.
 * $9E00-$9EFF is mirrored at $DE00-$DEFF and $9F00-$9FFF at
 * $DF00-$DFFF. Both I/O windows are read-only. The ROM is not
 * decoded on /ROMH: this mode never asserts it.
 *
 * $9E00 and $DE00 both mask to $1E00, and $9F00 and $DF00 both mask
 * to $1F00, so addr & $1FFF serves the ROM and both mirrors.
 */

/*************************************************
* C64 bus read callback
*************************************************/
FORCE_INLINE bool mikro_assembler_read_handler(u32 control, u32 addr)
{
    if ((control & (C64_IO1|C64_IO2|C64_ROML)) != (C64_IO1|C64_IO2|C64_ROML))
    {
        C64_DATA_WRITE(crt_ptr[addr & 0x1fff]);
        return true;
    }

    return false;
}

/*************************************************
* C64 bus write callback
*************************************************/
FORCE_INLINE void mikro_assembler_write_handler(u32 control, u32 addr, u32 data)
{
    // No write support
}

static void mikro_assembler_init(void)
{
    C64_CRT_CONTROL(STATUS_LED_ON|CRT_PORT_8K);
}

C64_BUS_HANDLER(mikro_assembler)
