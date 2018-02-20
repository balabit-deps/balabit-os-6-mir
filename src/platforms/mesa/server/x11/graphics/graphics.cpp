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

#include "mir/graphics/display_report.h"
#include "mir/options/option.h"
#include "platform.h"
#include "guest_platform.h"
#include "../X11_resources.h"
#include "mir/module_deleter.h"
#include "mir/assert_module_entry_point.h"
#include "mir/libname.h"

#include <boost/throw_exception.hpp>

namespace mo = mir::options;
namespace mg = mir::graphics;
namespace mx = mir::X;
namespace mgx = mg::X;
namespace geom = mir::geometry;

mx::X11Resources x11_resources;

namespace
{
char const* x11_displays_option_name{"x11-displays"};
}

mir::UniqueModulePtr<mg::Platform> create_host_platform(
    std::shared_ptr<mo::Option> const& options,
    std::shared_ptr<mir::EmergencyCleanupRegistry> const&,
    std::shared_ptr<mg::DisplayReport> const& report,
    std::shared_ptr<mir::logging::Logger> const& /*logger*/)
{
    mir::assert_entry_point_signature<mg::CreateHostPlatform>(&create_host_platform);
    if (!x11_resources.get_conn())
        BOOST_THROW_EXCEPTION(std::runtime_error("Need valid x11 display"));

    auto display_dims_str = options->get<std::string>(x11_displays_option_name);
    auto pos = display_dims_str.find('x');
    if (pos == std::string::npos)
        BOOST_THROW_EXCEPTION(std::runtime_error("Malformed display size option"));

    return mir::make_module_ptr<mgx::Platform>(
               x11_resources.get_conn(),
               geom::Size{std::stoi(display_dims_str.substr(0, pos)),
                          std::stoi(display_dims_str.substr(pos+1, display_dims_str.find(':')))},
               report
           );
}

mir::UniqueModulePtr<mg::Platform> create_guest_platform(
    std::shared_ptr<mg::DisplayReport> const& /*report*/,
    std::shared_ptr<mg::NestedContext> const& nested_context)
{
    mir::assert_entry_point_signature<mg::CreateGuestPlatform>(&create_guest_platform);
    return mir::make_module_ptr<mgx::GuestPlatform>(nested_context);
}

void add_graphics_platform_options(boost::program_options::options_description& config)
{
    mir::assert_entry_point_signature<mg::AddPlatformOptions>(&add_graphics_platform_options);
    config.add_options()
        (x11_displays_option_name,
         boost::program_options::value<std::string>()->default_value("1280x1024"),
         "[mir-on-X specific] WIDTHxHEIGHT of \"display\" window.");
}

mg::PlatformPriority probe_graphics_platform(mo::ProgramOption const& /*options*/)
{
    mir::assert_entry_point_signature<mg::PlatformProbe>(&probe_graphics_platform);
    auto dpy = XOpenDisplay(nullptr);
    if (dpy)
    {
        XCloseDisplay(dpy);

        auto udev = std::make_shared<mir::udev::Context>();

        mir::udev::Enumerator drm_devices{udev};
        drm_devices.match_subsystem("drm");
        drm_devices.match_sysname("renderD[0-9]*");
        drm_devices.scan_devices();

        if (drm_devices.begin() != drm_devices.end())
            return mg::PlatformPriority::supported;
    }
    return mg::PlatformPriority::unsupported;
}

namespace
{
mir::ModuleProperties const description = {
    "mir:mesa-x11",
    MIR_VERSION_MAJOR,
    MIR_VERSION_MINOR,
    MIR_VERSION_MICRO,
    mir::libname()
};
}

mir::ModuleProperties const* describe_graphics_module()
{
    mir::assert_entry_point_signature<mg::DescribeModule>(&describe_graphics_module);
    return &description;
}
