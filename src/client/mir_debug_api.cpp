/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "mir_toolkit/debug/surface.h"

#include "mir_surface.h"
#include "mir/mir_buffer_stream.h"

int mir_debug_window_id(MirWindow* window)
{
    return window->id();
}

uint32_t mir_debug_window_current_buffer_id(MirWindow* window)
{
    return window->get_buffer_stream()->get_current_buffer_id();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

bool mir_debug_surface_coords_to_screen(MirSurface* surface,
                                       int x, int y,
                                       int* screen_x, int* screen_y)
{
    return surface->translate_to_screen_coordinates(x, y, screen_x, screen_y);
}

uint32_t mir_debug_surface_current_buffer_id(MirSurface* surface)
{
    return mir_debug_window_current_buffer_id(surface);
}

int mir_debug_surface_id(MirSurface* surface)
{
    return mir_debug_window_id(surface);
}

#pragma GCC diagnostic pop
