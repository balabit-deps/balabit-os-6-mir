/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_ANDROID_CONFIGURABLE_DISPLAY_BUFFER_H_
#define MIR_GRAPHICS_ANDROID_CONFIGURABLE_DISPLAY_BUFFER_H_

#include "mir/graphics/display_buffer.h"
#include "display_device.h"

namespace mir
{
namespace graphics
{
namespace android
{

class ConfigurableDisplayBuffer : public graphics::DisplayBuffer
{
public:
    virtual void configure(MirPowerMode power_mode, MirOrientation orientation, geometry::Displacement) = 0;
    virtual DisplayContents contents() = 0;
    virtual MirPowerMode power_mode() const = 0;
};

}
}
}

#endif /* MIR_GRAPHICS_ANDROID_CONFIGURABLE_DISPLAY_BUFFER_H_ */
