/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_SCENE_SURFACE_OBSERVER_H_
#define MIR_SCENE_SURFACE_OBSERVER_H_

#include "mir_toolkit/common.h"
#include "mir_toolkit/events/event.h"

#include "mir/input/input_reception_mode.h"
#include "mir/geometry/rectangle.h"

#include <glm/glm.hpp>
#include <string>

namespace mir
{
namespace geometry
{
struct Size;
struct Point;
}
namespace graphics
{
class CursorImage;
}

namespace scene
{
class SurfaceObserver
{
public:
    virtual void attrib_changed(MirWindowAttrib attrib, int value) = 0;
    virtual void resized_to(geometry::Size const& size) = 0;
    virtual void moved_to(geometry::Point const& top_left) = 0;
    virtual void hidden_set_to(bool hide) = 0;
    virtual void frame_posted(int frames_available, geometry::Size const& size) = 0;
    virtual void alpha_set_to(float alpha) = 0;
    virtual void orientation_set_to(MirOrientation orientation) = 0;
    virtual void transformation_set_to(glm::mat4 const& t) = 0;
    virtual void reception_mode_set_to(input::InputReceptionMode mode) = 0;
    virtual void cursor_image_set_to(graphics::CursorImage const& image) = 0;
    virtual void client_surface_close_requested() = 0;
    virtual void keymap_changed(MirInputDeviceId id, std::string const& model, std::string const& layout,
                                std::string const& variant, std::string const& options) = 0;
    virtual void renamed(char const* name) = 0;
    virtual void cursor_image_removed() = 0;
    virtual void placed_relative(geometry::Rectangle const& placement) = 0;

protected:
    SurfaceObserver() = default;
    virtual ~SurfaceObserver() = default;
    SurfaceObserver(SurfaceObserver const&) = delete;
    SurfaceObserver& operator=(SurfaceObserver const&) = delete;
};
}
}

#endif // MIR_SCENE_SURFACE_OBSERVER_H_
