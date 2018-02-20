/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by:
 *   Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 *   Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "gbm_buffer.h"
#include "buffer_texture_binder.h"
#include "native_buffer.h"
#include "gbm_format_conversions.h"

#include <fcntl.h>
#include <xf86drm.h>

#include <boost/throw_exception.hpp>

#include <system_error>

namespace mg=mir::graphics;
namespace mgm=mir::graphics::mesa;
namespace mgc = mir::graphics::common;
namespace geom=mir::geometry;

mgm::GBMBuffer::GBMBuffer(std::shared_ptr<gbm_bo> const& handle,
                          uint32_t bo_flags,
                          std::unique_ptr<mgc::BufferTextureBinder> texture_binder)
    : gbm_handle{handle},
      bo_flags{bo_flags},
      texture_binder{std::move(texture_binder)},
      prime_fd{-1}
{
    auto device = gbm_bo_get_device(gbm_handle.get());
    auto gem_handle = gbm_bo_get_handle(gbm_handle.get()).u32;
    auto drm_fd = gbm_device_get_fd(device);

    auto ret = drmPrimeHandleToFD(drm_fd, gem_handle, DRM_CLOEXEC, &prime_fd);

    if (ret)
    {
        std::string const msg("Failed to get PRIME fd from gbm bo");
        BOOST_THROW_EXCEPTION((std::system_error{errno, std::system_category(), msg}));
    }
}

mgm::GBMBuffer::~GBMBuffer()
{
    if (prime_fd >= 0)
        close(prime_fd);
}

geom::Size mgm::GBMBuffer::size() const
{
    return {gbm_bo_get_width(gbm_handle.get()), gbm_bo_get_height(gbm_handle.get())};
}

geom::Stride mgm::GBMBuffer::stride() const
{
    return geom::Stride(gbm_bo_get_stride(gbm_handle.get()));
}

MirPixelFormat mgm::GBMBuffer::pixel_format() const
{
    return gbm_format_to_mir_format(gbm_bo_get_format(gbm_handle.get()));
}

void mgm::GBMBuffer::gl_bind_to_texture()
{
    texture_binder->gl_bind_to_texture();
}

std::shared_ptr<mg::NativeBuffer> mgm::GBMBuffer::native_buffer_handle() const
{
    auto temp = std::make_shared<NativeBuffer>();

    temp->fd_items = 1;
    temp->fd[0] = prime_fd;
    temp->stride = stride().as_uint32_t();
    temp->flags = (bo_flags & GBM_BO_USE_SCANOUT) ? mir_buffer_flag_can_scanout : 0;
    temp->bo = gbm_handle.get();

    auto const& dim = size();
    temp->width = dim.width.as_int();
    temp->height = dim.height.as_int();

    return temp;
}

mg::NativeBufferBase* mgm::GBMBuffer::native_buffer_base()
{
    return this;
}

void mgm::GBMBuffer::secure_for_render()
{
}

void mgm::GBMBuffer::bind()
{
    gl_bind_to_texture();
}
