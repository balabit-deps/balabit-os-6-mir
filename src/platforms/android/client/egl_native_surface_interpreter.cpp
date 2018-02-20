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

#include "egl_native_surface_interpreter.h"
#include "sync_fence.h"
#include "mir/client_buffer.h"
#include <system/window.h>
#include <hardware/gralloc.h>
#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace mcla=mir::client::android;
namespace mga=mir::graphics::android;

mcla::EGLNativeSurfaceInterpreter::EGLNativeSurfaceInterpreter(EGLNativeSurface& surface)
 :  surface(surface),
    driver_pixel_format(-1),
    sync_ops(std::make_shared<mga::RealSyncFileOps>()),
    hardware_bits( GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER ),
    software_bits(
        GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_OFTEN |
        GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_TEXTURE )
{
}

mga::NativeBuffer* mcla::EGLNativeSurfaceInterpreter::driver_requests_buffer()
{
    auto buffer = surface.get_current_buffer();
    auto buffer_to_driver = mga::to_native_buffer_checked(buffer->native_buffer_handle());
    
    ANativeWindowBuffer* anwb = buffer_to_driver->anwb();
    anwb->format = driver_pixel_format;
    return buffer_to_driver.get();
}

void mcla::EGLNativeSurfaceInterpreter::driver_returns_buffer(ANativeWindowBuffer*, int fence_fd)
{
    //TODO: pass fence to server instead of waiting here
    mga::SyncFence sync_fence(sync_ops, mir::Fd(fence_fd));
    sync_fence.wait();

    surface.swap_buffers_sync();
}

void mcla::EGLNativeSurfaceInterpreter::dispatch_driver_request_format(int format)
{
    driver_pixel_format = format;
}

int mcla::EGLNativeSurfaceInterpreter::driver_requests_info(int key) const
{
    switch (key)
    {
        case NATIVE_WINDOW_WIDTH:
        case NATIVE_WINDOW_DEFAULT_WIDTH:
            return surface.get_parameters().width;
        case NATIVE_WINDOW_HEIGHT:
        case NATIVE_WINDOW_DEFAULT_HEIGHT:
            return surface.get_parameters().height;
        case NATIVE_WINDOW_FORMAT:
            return driver_pixel_format;
        case NATIVE_WINDOW_TRANSFORM_HINT:
            return 0;
        case NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS:
            return 2;
        case NATIVE_WINDOW_CONCRETE_TYPE:
            return NATIVE_WINDOW_SURFACE;
        case NATIVE_WINDOW_CONSUMER_USAGE_BITS:
            if (surface.get_parameters().buffer_usage ==  mir_buffer_usage_hardware)
                return hardware_bits;
            else
                return software_bits;
        default:
            throw std::runtime_error("driver requested unsupported query");
    }
}

void mcla::EGLNativeSurfaceInterpreter::sync_to_display(bool should_sync)
{ 
    surface.request_and_wait_for_configure(mir_window_attrib_swapinterval, should_sync);
}

void mcla::EGLNativeSurfaceInterpreter::dispatch_driver_request_buffer_count(unsigned int count)
{
    surface.set_buffer_cache_size(count);
}

void mcla::EGLNativeSurfaceInterpreter::dispatch_driver_request_buffer_size(geometry::Size size)
{
    auto params = surface.get_parameters();
    if (geometry::Size{params.width, params.height} == size)
        return;
    surface.set_size(size);
}
