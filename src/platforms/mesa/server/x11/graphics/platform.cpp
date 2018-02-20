/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Cemil Azizoglu <cemil.azizoglu@canonical.com>
 */

#include "platform.h"
#include "display.h"
#include "buffer_allocator.h"
#include "ipc_operations.h"

namespace mg = mir::graphics;
namespace mgm = mg::mesa;
namespace mgx = mg::X;
namespace geom = mir::geometry;

mgx::Platform::Platform(std::shared_ptr<::Display> const& conn,
                        geom::Size const size,
                        std::shared_ptr<mg::DisplayReport> const& report)
    : x11_connection{conn},
      udev{std::make_shared<mir::udev::Context>()},
      drm{std::make_shared<mesa::helpers::DRMHelper>(mesa::helpers::DRMNodeToUse::render)},
      report{report},
      size{size}
{
    if (!x11_connection)
        BOOST_THROW_EXCEPTION(std::runtime_error("Need valid x11 display"));

    drm->setup(udev);
    gbm.setup(*drm);
}

mir::UniqueModulePtr<mg::GraphicBufferAllocator> mgx::Platform::create_buffer_allocator()
{
    return make_module_ptr<mgm::BufferAllocator>(gbm.device, mgm::BypassOption::prohibited, mgm::BufferImportMethod::dma_buf);
}

mir::UniqueModulePtr<mg::Display> mgx::Platform::create_display(
    std::shared_ptr<DisplayConfigurationPolicy> const& /*initial_conf_policy*/,
    std::shared_ptr<GLConfig> const& gl_config)
{
    return make_module_ptr<mgx::Display>(x11_connection.get(), size, gl_config,
                                         report);
}

mir::UniqueModulePtr<mg::PlatformIpcOperations> mgx::Platform::make_ipc_operations() const
{
    return make_module_ptr<mg::mesa::IpcOperations>(drm);
}
