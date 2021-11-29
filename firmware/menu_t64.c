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

static void t64_format_image_name(char *buffer, T64_HEADER *header)
{
    char *name = header->user_description;

    // Star
    sprint(scratch_buf, " *     ");
    buffer += 7;

    // name
    const uint8_t name_len = 24;
    d64_sanitize_name_pad(buffer, name, name_len);
    buffer += name_len;

    // Entry type
    sprint(buffer, "   --- ");
}

static void t64_format_entry(char *buffer, T64_ENTRY *entry)
{
    // Blocks
    uint16_t size = (entry->end_address - entry->start_address) + 2;
    uint16_t blocks = (size / 254) + 1;

    *buffer++ = ' ';
    sprint_u16_left(buffer, blocks);
    buffer += 5;
    *buffer++ = ' ';

    // Filename
    d64_sanitize_name_pad(buffer, entry->filename, 16);
    buffer += 16;
    d64_sanitize_name_pad(buffer, "", 10);
    buffer += 10;

    // Entry type
    d64_format_entry_type(buffer, entry->file_type);
}

static uint8_t t64_send_page(T64_STATE *state, uint8_t selected_element)
{
    uint8_t element;
    for (element=0; element<MAX_ELEMENTS_PAGE; element++)
    {
        if (element == 0 && state->page == 0)
        {
            sprint(scratch_buf, " ..                               DIR ");
        }
        else if (element == 1 && state->page == 0)
        {
            t64_format_image_name(scratch_buf, &state->image.header);
        }
        else
        {
            if (!t64_read_dir(&state->image))
            {
                state->dir_end = true;
                send_page_end();
                break;
            }

            t64_format_entry(scratch_buf, &state->image.entry);
        }

        if (element == selected_element)
        {
            scratch_buf[0] = SELECTED_ELEMENT;
        }

        c64_send_data(scratch_buf, ELEMENT_LENGTH);
    }

    return element;
}

static bool t64_skip_to_page(T64_STATE *state, uint8_t page)
{
    bool page_found = true;
    state->page = page;

    uint16_t skip = state->page * MAX_ELEMENTS_PAGE;
    for (uint16_t i=2; i<skip; i++)
    {
        if (!t64_read_dir(&state->image))
        {
            page_found = false;
            break;
        }
    }

    if (!page_found)
    {
        t64_rewind_dir(&state->image);
        state->page = 0;
    }

    return page_found;
}

static void t64_dir(T64_STATE *state)
{
    c64_send_reply(REPLY_READ_DIR);

    t64_rewind_dir(&state->image);
    format_path(scratch_buf, true);
    c64_send_data(scratch_buf, DIR_NAME_LENGTH);
    state->dir_end = false;

    // Search for last selected element
    uint8_t selected_element;
    if (dat_file.prg.element == ELEMENT_NOT_SELECTED)
    {
        state->page = 0;
        dat_file.prg.element = 0;
        selected_element = MAX_ELEMENTS_PAGE;
    }
    else
    {
        selected_element = dat_file.prg.element % MAX_ELEMENTS_PAGE;
        uint8_t page = dat_file.prg.element / MAX_ELEMENTS_PAGE;

        if (!t64_skip_to_page(state, page))
        {
            dat_file.prg.element = 0;
            selected_element = MAX_ELEMENTS_PAGE;
        }
    }

    t64_send_page(state, selected_element);
}

static void t64_dir_up(T64_STATE *state, bool root)
{
    dat_file.prg.element = ELEMENT_NOT_SELECTED;    // Do not auto open T64 again
    t64_close(&state->image);

    menu_state = &sd_state.menu;
    if (root)
    {
        menu_state->dir_up(menu_state, root);
    }
    else
    {
        menu_state->dir(menu_state);
    }
}

static void t64_next_page(T64_STATE *state)
{
    c64_send_reply(REPLY_READ_DIR_PAGE);

    if (!state->dir_end)
    {
        state->page++;
        if (!t64_send_page(state, MAX_ELEMENTS_PAGE))
        {
            state->page--;
        }
    }
    else
    {
        send_page_end();
    }
}

static void t64_prev_page(T64_STATE *state)
{
    c64_send_reply(REPLY_READ_DIR_PAGE);

    if (state->page)
    {
        t64_rewind_dir(&state->image);
        state->dir_end = false;

        t64_skip_to_page(state, state->page-1);
        t64_send_page(state, MAX_ELEMENTS_PAGE);
    }
    else
    {
        send_page_end();
    }
}

static bool t64_select(T64_STATE *state, uint8_t flags, uint8_t element_no)
{
    uint16_t element = element_no + state->page * MAX_ELEMENTS_PAGE;

    dat_file.prg.element = element;
    dat_file.boot_type = DAT_NONE;
    dat_file.prg.name[0] = 0;

    if (element == 0)
    {
        if (flags & SELECT_FLAG_OPTIONS)
        {
            handle_file_options("..", FILE_NONE, element_no);
        }
        else
        {
            t64_dir_up(state, false);
        }

        return false;
    }
    else if (element == 1)
    {
        if (flags & SELECT_FLAG_OPTIONS)
        {
            handle_file_options("*", FILE_T64_PRG, element_no);
            return false;
        }

        element = ELEMENT_NOT_SELECTED; // Find first PRG
    }

    t64_rewind_dir(&state->image);
    bool found = false;
    for (uint16_t i=2; i<=element; i++)
    {
        if (!(found = t64_read_dir(&state->image)))
        {
            break;
        }

        if (element == ELEMENT_NOT_SELECTED)
        {
            element = 1;
            break;
        }
    }

    if (!found)
    {
        // File not not found
        if (element == 1 || element == ELEMENT_NOT_SELECTED)
        {
            dat_file.prg.element = 1;
            handle_unsupported_ex("Not Found", "No PRG files were found in image",
                                  dat_file.file);
        }
        else
        {
            dat_file.prg.element = 0;
            t64_dir(state);
        }

        return false;
    }

    d64_sanitize_name_pad(dat_file.prg.name, state->image.entry.filename, 16);
    dat_file.prg.name[16] = 0;

    if (flags & SELECT_FLAG_OPTIONS)
    {
        handle_file_options(dat_file.prg.name, FILE_T64_PRG, element_no);
        return false;
    }

    dat_file.prg.size = t64_read_prg(&state->image, dat_buffer,
                                     sizeof(dat_buffer));
    if (!prg_size_valid(dat_file.prg.size))
    {
        handle_unsupported(dat_file.prg.name);
        return false;
    }

    c64_send_exit_menu();
    dat_file.boot_type = DAT_PRG;
    t64_close(&state->image);
    return true;
}

static void t64_open_image(const char *file_name)
{
    if (!t64_open(&t64_state.image, file_name))
    {
        handle_failed_to_read_sd();
    }
}

static bool t64_load_first(const char *file_name)
{
    t64_state.page = 0;
    t64_open_image(file_name);

    if (!t64_select(&t64_state, 0, 1))
    {
        t64_close(&t64_state.image);
        return false;
    }

    return true;
}

static MENU_STATE * t64_menu_init(const char *file_name)
{
    if (!t64_state.menu.dir)
    {
        t64_state.menu.dir = (void (*)(MENU_STATE *))t64_dir;
        t64_state.menu.dir_up = (void (*)(MENU_STATE *, bool))t64_dir_up;
        t64_state.menu.prev_page = (void (*)(MENU_STATE *))t64_prev_page;
        t64_state.menu.next_page = (void (*)(MENU_STATE *))t64_next_page;
        t64_state.menu.select = (bool (*)(MENU_STATE *, uint8_t, uint8_t))t64_select;
    }

    t64_open_image(file_name);

    return &t64_state.menu;
}
