/*
 * Copyright © 2016 Canonical Ltd.
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

#include "mir_toolkit/mir_client_library.h"
#include "client_platform.h"
#include "client_buffer_factory.h"
#include "mir/client_buffer_factory.h"
#include "mir/client_context.h"
#include "native_buffer.h"

#include <cstring>
#include <boost/throw_exception.hpp>

namespace mcl=mir::client;
namespace mcle=mir::client::eglstream;
namespace geom=mir::geometry;

mcle::ClientPlatform::ClientPlatform(ClientContext* const)
{
}

std::shared_ptr<mcl::ClientBufferFactory> mcle::ClientPlatform::create_buffer_factory()
{
    return std::make_shared<mcle::ClientBufferFactory>();
}

void mcle::ClientPlatform::use_egl_native_window(std::shared_ptr<void> /*native_window*/, EGLNativeSurface* /*surface*/)
{
}

std::shared_ptr<void> mcle::ClientPlatform::create_egl_native_window(EGLNativeSurface* /*surface*/)
{
    return nullptr;
}

std::shared_ptr<EGLNativeDisplayType> mcle::ClientPlatform::create_egl_native_display()
{
    return nullptr;
}

MirPlatformType mcle::ClientPlatform::platform_type() const
{
    return mir_platform_type_eglstream;
}

void mcle::ClientPlatform::populate(MirPlatformPackage& /*package*/) const
{
}

MirPlatformMessage* mcle::ClientPlatform::platform_operation(MirPlatformMessage const* /*msg*/)
{
    return nullptr;
}

MirNativeBuffer* mcle::ClientPlatform::convert_native_buffer(graphics::NativeBuffer* buf) const
{
    if (auto native = dynamic_cast<mir::graphics::eglstream::NativeBuffer*>(buf))
        return native;
    BOOST_THROW_EXCEPTION(std::invalid_argument("could not convert to NativeBuffer")); 
}


MirPixelFormat mcle::ClientPlatform::get_egl_pixel_format(
    EGLDisplay /*disp*/, EGLConfig /*conf*/) const
{
    BOOST_THROW_EXCEPTION(std::runtime_error{"EGL support unimplemented"});
}

void* mcle::ClientPlatform::request_interface(char const*, int)
{
    return nullptr;
}

uint32_t mcle::ClientPlatform::native_format_for(MirPixelFormat) const
{
    BOOST_THROW_EXCEPTION(std::runtime_error{"no buffer support"});
}

uint32_t mcle::ClientPlatform::native_flags_for(MirBufferUsage, mir::geometry::Size) const
{
    BOOST_THROW_EXCEPTION(std::runtime_error{"no buffer support"});
}
