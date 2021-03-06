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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/graphics/platform.h"
#include "mir/graphics/frame.h"
#include "display_configuration.h"
#include "mir/graphics/display_report.h"
#include "mir/graphics/display_buffer.h"
#include "mir/graphics/egl_resources.h"
#include "display.h"
#include "virtual_output.h"
#include "display_component_factory.h"
#include "interpreter_cache.h"
#include "server_render_window.h"
#include "display_buffer.h"
#include "hwc_layerlist.h"
#include "mir_native_window.h"
#include "mir/geometry/rectangle.h"
#include "mir/graphics/event_handler_register.h"
#include "mir/gl/program_factory.h"
#include "mir/fd.h"

#include <memory>
#include <boost/throw_exception.hpp>
#include <fcntl.h>

#include "mir/geometry/dimensions.h"

namespace mga=mir::graphics::android;
namespace mgl=mir::gl;
namespace mg=mir::graphics;
namespace geom=mir::geometry;

struct mga::DisplayChangePipe
{
    DisplayChangePipe()
    {
        int pipes_raw[2] {-1, -1};
        if (::pipe2(pipes_raw, O_CLOEXEC | O_NONBLOCK))
            BOOST_THROW_EXCEPTION(std::runtime_error("failed to create display change pipe"));
        read_pipe = mir::Fd{pipes_raw[0]}; 
        write_pipe = mir::Fd{pipes_raw[1]}; 
    }

    void notify_change()
    {
        if (::write(write_pipe, &data, sizeof(data)) == -1)
            BOOST_THROW_EXCEPTION(std::runtime_error("failed to write to display change pipe"));
    }

    void ack_change()
    {
        char tmp{'b'};
        if (::read(read_pipe, &tmp, sizeof(tmp)) == -1)
            BOOST_THROW_EXCEPTION(std::runtime_error("failed to read from display change pipe"));
    }
    mir::Fd read_pipe;
    mir::Fd write_pipe;
    char const data{'a'};
};

namespace
{
void power_mode(
    mga::DisplayName name,
    mga::HwcConfiguration& control,
    mg::DisplayConfigurationOutput& config,
    MirPowerMode intended_mode)
{
    if (config.power_mode != intended_mode)
    {
        control.power_mode(name, intended_mode);
        config.power_mode = intended_mode;
    }
}

void power_mode_safe(
    mga::DisplayName name,
    mga::HwcConfiguration& control,
    mg::DisplayConfigurationOutput& config,
    MirPowerMode intended_mode) noexcept
try
{
    power_mode(name, control, config, intended_mode);
} catch (...) {}

void set_powermode_all_displays(
    mga::HwcConfiguration& control,
    mga::DisplayConfiguration& config,
    MirPowerMode intended_mode) noexcept
{
    power_mode_safe(mga::DisplayName::primary, control, config.primary(), intended_mode);
    if (config.external().connected)
        power_mode_safe(mga::DisplayName::external, control, config.external(), intended_mode); 
}

std::unique_ptr<mga::ConfigurableDisplayBuffer> create_display_buffer(
    std::shared_ptr<mga::DisplayDevice> const& display_device,
    mga::DisplayName name,
    mga::DisplayComponentFactory& display_buffer_builder,
    mg::DisplayConfigurationOutput const& config,
    std::shared_ptr<mgl::ProgramFactory> const& gl_program_factory,
    mga::PbufferGLContext const& gl_context,
    geom::Displacement displacement,
    std::shared_ptr<mga::NativeWindowReport> const& report,
    mga::OverlayOptimization overlay_option)
{
    std::shared_ptr<mga::FramebufferBundle> fbs{display_buffer_builder.create_framebuffers(config)};
    auto cache = std::make_shared<mga::InterpreterCache>();
    mga::DeviceQuirks quirks(mga::PropertiesOps{}, gl_context);
    auto interpreter = std::make_shared<mga::ServerRenderWindow>(fbs, config.current_format, cache, quirks); 
    auto native_window = std::make_shared<mga::MirNativeWindow>(interpreter, report);
    return std::unique_ptr<mga::ConfigurableDisplayBuffer>(new mga::DisplayBuffer(
        name,
        display_buffer_builder.create_layer_list(),
        fbs,
        display_device,
        native_window,
        gl_context,
        *gl_program_factory,
        config.orientation,
        displacement,
        overlay_option));
}
}

mga::Display::Display(
    std::shared_ptr<mga::DisplayComponentFactory> const& display_buffer_builder,
    std::shared_ptr<mgl::ProgramFactory> const& gl_program_factory,
    std::shared_ptr<GLConfig> const& gl_config,
    std::shared_ptr<DisplayReport> const& display_report,
    std::shared_ptr<NativeWindowReport> const& native_window_report,
    mga::OverlayOptimization overlay_option) :
    display_report{display_report},
    native_window_report{native_window_report},
    display_buffer_builder{display_buffer_builder},
    hwc_config{display_buffer_builder->create_hwc_configuration()},
    hotplug_subscription{hwc_config->subscribe_to_config_changes(
        std::bind(&mga::Display::on_hotplug, this),
        std::bind(&mga::Display::on_vsync, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2))},
    config(
        hwc_config->active_config_for(mga::DisplayName::primary),
        mir_power_mode_off,
        hwc_config->active_config_for(mga::DisplayName::external),
        mir_power_mode_off),
    gl_context{config.primary().current_format, *gl_config, *display_report},
    display_device(display_buffer_builder->create_display_device()),
    display_change_pipe(new DisplayChangePipe),
    gl_program_factory(gl_program_factory),
    displays(
        display_device,
        create_display_buffer(
            display_device,
            mga::DisplayName::primary,
            *display_buffer_builder,
            config.primary(),
            gl_program_factory,
            gl_context,
            geom::Displacement{0,0},
            native_window_report,
            overlay_option),
            [this] { on_hotplug(); }), //Recover from exception by forcing a configuration change
    overlay_option(overlay_option)
{
    //Some drivers (depending on kernel state) incorrectly report an error code indicating that the display is already on. Ignore the first failure.
    set_powermode_all_displays(*hwc_config, config, mir_power_mode_on);

    if (config.external().connected)
    {
        displays.add(mga::DisplayName::external,
            create_display_buffer(
                display_device,
                mga::DisplayName::external,
                *display_buffer_builder,
                config.external(),
                gl_program_factory,
                gl_context,
                geom::Displacement{0,0},
                native_window_report,
                overlay_option));
    }

    display_report->report_successful_setup_of_native_resources();

    gl_context.make_current();

    display_report->report_successful_egl_make_current_on_construction();
    display_report->report_successful_display_construction();
}

mga::Display::~Display() noexcept
{
    set_powermode_all_displays(*hwc_config, config, mir_power_mode_off);
}

void mga::Display::update_configuration(std::lock_guard<std::mutex> const&) const
{
    if (configuration_dirty)
    {
        auto external_config = hwc_config->active_config_for(mga::DisplayName::external);
        if (external_config.connected)
            power_mode(mga::DisplayName::external, *hwc_config, config.external(), mir_power_mode_on);
        else
            config.external().power_mode = mir_power_mode_off;

        config = mga::DisplayConfiguration(
            hwc_config->active_config_for(mga::DisplayName::primary),
            config.primary().power_mode,
            std::move(external_config),
            config.external().power_mode,
            config.virt());
        configuration_dirty = false;
    }
}

void mga::Display::for_each_display_sync_group(std::function<void(mg::DisplaySyncGroup&)> const& f)
{
    std::lock_guard<decltype(configuration_mutex)> lock{configuration_mutex};
    f(displays);
}

std::unique_ptr<mg::DisplayConfiguration> mga::Display::configuration() const
{
    std::lock_guard<decltype(configuration_mutex)> lock{configuration_mutex};
    update_configuration(lock);
    return std::unique_ptr<mg::DisplayConfiguration>(new mga::DisplayConfiguration(config));
}

void mga::Display::configure(mg::DisplayConfiguration const& new_configuration)
{
    std::lock_guard<decltype(configuration_mutex)> lock{configuration_mutex};

    configure_locked(new_configuration, lock);
}

//NOTE: We avoid calling back to hwc from within the hotplug callback. Only arrange for an update.
void mga::Display::on_hotplug()
{
    std::lock_guard<decltype(configuration_mutex)> lock{configuration_mutex};
    configuration_dirty = true;
    display_change_pipe->notify_change();
}

void mga::Display::on_vsync(DisplayName name, mg::Frame::Timestamp timestamp)
{
    std::lock_guard<decltype(vsync_mutex)> lock{vsync_mutex};
    /*
     * XXX It's presently useful but idealistically inefficient that we
     *     get a callback on every frame, even when the compositor is idle.
     *     (LP: #1374318)
     *     Although we possibly shouldn't fix that at all because the
     *     call to increment_with_timestamp would produce incorrect MSC values
     *     if it were to miss a physical frame. The X11 platform has that
     *     problem already, but it's less important for production use than
     *     Android.
     */
    auto& f = last_frame[as_output_id(name).as_value()];
    f.increment_with_timestamp(timestamp);
    display_report->report_vsync(as_output_id(name).as_value(), f.load());
}

mg::Frame mga::Display::last_frame_on(unsigned output_id) const
{
    std::lock_guard<decltype(vsync_mutex)> lock{vsync_mutex};
    auto last = last_frame.find(output_id);
    if (last == last_frame.end())
         return {};  // Not an error. It might be a valid output_id pre-vsync
    else
         return last->second.load();
}

void mga::Display::register_configuration_change_handler(
    EventHandlerRegister& event_handler,
    DisplayConfigurationChangeHandler const& change_handler)
{
    event_handler.register_fd_handler({display_change_pipe->read_pipe}, this,
        make_module_ptr<std::function<void(int)>>(
            [change_handler, this](int)
            {
                change_handler();
                display_change_pipe->ack_change();
            }));
}

void mga::Display::register_pause_resume_handlers(
    EventHandlerRegister& /*handlers*/,
    DisplayPauseHandler const& /*pause_handler*/,
    DisplayResumeHandler const& /*resume_handler*/)
{
}

void mga::Display::pause()
{
}

void mga::Display::resume()
{
}

auto mga::Display::create_hardware_cursor(std::shared_ptr<mg::CursorImage> const& /* initial_image */) -> std::shared_ptr<Cursor>
{
    return nullptr;
}

std::unique_ptr<mg::VirtualOutput> mga::Display::create_virtual_output(int width, int height)
{
    auto enable_virtual_output = [this, width, height]
    {
        config.set_virtual_output_to(width, height);
        on_hotplug();
    };
    auto disable_virtual_output = [this]
    {
        config.disable_virtual_output();
        on_hotplug();
    };
    return {std::make_unique<mga::VirtualOutput>(enable_virtual_output, disable_virtual_output)};
}

mg::NativeDisplay* mga::Display::native_display()
{
    return this;
}

std::unique_ptr<mir::renderer::gl::Context> mga::Display::create_gl_context()
{
    return std::make_unique<mga::PbufferGLContext>(gl_context);
}

bool mga::Display::apply_if_configuration_preserves_display_buffers(
    mg::DisplayConfiguration const& conf)
{
    /*
     * We never invalidate display buffers based on the configuration to apply.
     *
     * The only way we invalidate a display buffer is if we detect that a previously-connected
     * external display has been removed. In that case, regardless of whether or not it's enabled
     * in the configuration, we destroy the display.
     *
     * Take the configuration lock to ensure consistency between our checking for external
     * connections and configure's checking.
     */
    std::lock_guard<decltype(configuration_mutex)> lock{configuration_mutex};
    if (!config.external().connected || displays.display_present(mga::DisplayName::external))
    {
        configure_locked(conf, lock);
        return true;
    }
    return false;
}

void mga::Display::configure_locked(
    mir::graphics::DisplayConfiguration const& new_configuration,
    std::lock_guard<decltype(configuration_mutex)> const&)
{
    using namespace geometry;
    if (!new_configuration.valid())
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid or inconsistent display configuration"));

    if ((config.external().connected) && !displays.display_present(mga::DisplayName::external))
        displays.add(
            mga::DisplayName::external,
            create_display_buffer(
                display_device,
                mga::DisplayName::external,
                *display_buffer_builder,
                config.external(),
                gl_program_factory,
                gl_context,
                config.external().top_left - origin,
                native_window_report,
                overlay_option));
    if ((!config.external().connected) && displays.display_present(mga::DisplayName::external))
        displays.remove(mga::DisplayName::external);

    new_configuration.for_each_output(
        [this](mg::DisplayConfigurationOutput const& output)
        {
            if (output.current_format != config[output.id].current_format)
                BOOST_THROW_EXCEPTION(std::logic_error("could not change display buffer format"));

            config[output.id].orientation = output.orientation;
            config[output.id].form_factor = output.form_factor;
            config[output.id].scale = output.scale;

            geom::Displacement offset(output.top_left - origin);
            config[output.id].top_left = output.top_left;

            if (config.primary().id == output.id)
            {
                power_mode(mga::DisplayName::primary, *hwc_config, config.primary(), output.power_mode);
                displays.configure(mga::DisplayName::primary, output.power_mode, output.orientation, offset);
            }
            else if (config.external().id == output.id && config.external().connected)
            {
                power_mode(mga::DisplayName::external, *hwc_config, config.external(), output.power_mode);
                displays.configure(mga::DisplayName::external, output.power_mode, output.orientation, offset);
            }
        });
}
