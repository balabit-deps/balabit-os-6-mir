/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_SCENE_LEGACY_SURFACE_CHANGE_NOTIFICATION_H_
#define MIR_SCENE_LEGACY_SURFACE_CHANGE_NOTIFICATION_H_

#include "mir/scene/surface_observer.h"

#include <functional>

namespace mir
{
namespace scene
{
class LegacySurfaceChangeNotification : public mir::scene::SurfaceObserver
{
public:
    LegacySurfaceChangeNotification(
        std::function<void()> const& notify_scene_change,
        std::function<void(int)> const& notify_buffer_change);

    void resized_to(geometry::Size const& /*size*/) override;
    void moved_to(geometry::Point const& /*top_left*/) override;
    void hidden_set_to(bool /*hide*/) override;
    void frame_posted(int frames_available, geometry::Size const& size) override;
    void alpha_set_to(float /*alpha*/) override;
    void orientation_set_to(MirOrientation orientation) override;
    void transformation_set_to(glm::mat4 const& /*t*/) override;
    void attrib_changed(MirWindowAttrib, int) override;
    void reception_mode_set_to(input::InputReceptionMode mode) override;
    void cursor_image_set_to(graphics::CursorImage const& image) override;
    void client_surface_close_requested() override;
    void keymap_changed(MirInputDeviceId id, std::string const& model, std::string const& layout,
                        std::string const& variant, std::string const& options) override;
    void renamed(char const*) override;
    void cursor_image_removed() override;
    void placed_relative(geometry::Rectangle const& placement) override;

private:
    std::function<void()> const notify_scene_change;
    std::function<void(int)> const notify_buffer_change;
};
}
}

#endif // MIR_SCENE_LEGACY_SURFACE_CHANGE_NOTIFICATION_H_
