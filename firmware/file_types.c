/*
 * Copyright (c) 2019-2021 Kim Jørgensen
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

#include "file_types.h"
#include "t64.h"
#include "d64.h"

DAT_HEADER dat_file;

static inline bool prg_size_valid(uint32_t size)
{
    // PRG should at least have a 2 byte load address and 1 byte of data
    return size > 2 && size < 64*1024;
}

static bool compare_extension(char *ext1, const char *ext2)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        if (ext1[i] >= 'a' && ext1[i] <= 'z')
        {
            if ((ext1[i] - 0x20) != ext2[i])
            {
                return false;
            }
        }
        else if (ext1[i] != ext2[i])
        {
            return false;
        }
    }

    return true;
}

static uint8_t get_filename_length(const char *filename, uint8_t *extension)
{
    *extension = FF_LFN_BUF;

    uint8_t length = 0;
    for (; length < FF_LFN_BUF; length++)
    {
        uint8_t chr = filename[length];
        if (chr)
        {
            if (chr == '.')
            {
                *extension = length;
            }
        }
        else
        {
            break;
        }
    }

    if (*extension > length)
    {
        *extension = length;
    }

    return length;
}

static uint8_t get_file_type(FILINFO *info)
{
    if (info->fattrib & AM_DIR)
    {
        return FILE_NONE;
    }

    char *filename = info->fname;
    uint8_t extension;
    uint8_t length = get_filename_length(filename, &extension);

    uint8_t extension_length = length - extension;
    if (extension_length == 0)
    {
        // Treat extensionless files as PRG
        if (prg_size_valid(info->fsize))
        {
            return FILE_PRG;
        }
    }
    else if (extension_length >= 4)
    {
        filename += extension + 1;
        if (compare_extension(filename, "PRG"))
        {
            if (prg_size_valid(info->fsize))
            {
                return FILE_PRG;
            }
        }
        else if (compare_extension(filename, "P00"))
        {
            if (info->fsize > sizeof(P00_HEADER))
            {
                return FILE_P00;
            }
        }
        else if (compare_extension(filename, "T64"))
        {
            if (info->fsize > sizeof(T64_HEADER))
            {
                return FILE_T64;
            }
        }
        else if (compare_extension(filename, "CRT"))
        {
            if (info->fsize > sizeof(CRT_HEADER))
            {
                return FILE_CRT;
            }
        }
        else if (compare_extension(filename, "D64") ||
                 compare_extension(filename, "D71") ||
                 compare_extension(filename, "D81"))
        {
            if (d64_get_type(info->fsize) != D64_TYPE_UNKNOWN)
            {
                return FILE_D64;
            }
        }
        else if (compare_extension(filename, "ROM") ||
                 compare_extension(filename, "BIN"))
        {
            if (info->fsize <= sizeof(dat_buffer))
            {
                return FILE_ROM;
            }
        }
        else if (compare_extension(filename, "UPD"))
        {
            if (info->fsize >= sizeof(dat_buffer))
            {
                return FILE_UPD;
            }
        }
        else if (compare_extension(filename, "DAT"))
        {
            if (info->fsize == (sizeof(DAT_HEADER) + sizeof(dat_buffer)))
            {
                return FILE_DAT;
            }
        }
    }

    return FILE_UNKNOWN;
}
