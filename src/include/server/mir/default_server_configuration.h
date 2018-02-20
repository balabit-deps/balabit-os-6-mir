/*
 * Copyright © 2012-2014 Canonical Ltd.
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
#ifndef MIR_DEFAULT_SERVER_CONFIGURATION_H_
#define MIR_DEFAULT_SERVER_CONFIGURATION_H_

#include "mir/cached_ptr.h"
#include "mir/server_configuration.h"
#include "mir/shell/window_manager_builder.h"

#include <memory>
#include <string>

namespace mir
{
class ServerActionQueue;
class SharedLibrary;
class SharedLibraryProberReport;

template<class Observer>
class ObserverRegistrar;

template<class Observer>
class ObserverMultiplexer;

namespace cookie
{
class Authority;
}
namespace dispatch
{
class MultiplexingDispatchable;
}
namespace compositor
{
class BufferStreamFactory;
class Scene;
class Drawer;
class DisplayBufferCompositorFactory;
class Compositor;
class CompositorReport;
class FrameDroppingPolicyFactory;
}
namespace frontend
{
class Shell;
class Connector;
class ConnectorReport;
class ProtobufIpcFactory;
class ConnectionCreator;
class SessionMediatorObserver;
class MessageProcessorReport;
class SessionAuthorizer;
class EventSink;
class DisplayChanger;
class Screencast;
}

namespace shell
{
class DisplayConfigurationController;
class InputTargeter;
class FocusSetter;
class FocusController;
class DisplayLayout;
class HostLifecycleEventListener;
class Shell;
class ShellReport;
class SurfaceStack;
class PersistentSurfaceStore;
namespace detail { class FrontendShell; }
}
namespace time
{
class Clock;
}
namespace scene
{
class SurfaceFactory;
class BroadcastingSessionEventSink;
class BufferStreamFactory;
class MediatingDisplayChanger;
class PixelBuffer;
class SessionContainer;
class SessionEventSink;
class SessionEventHandlerRegister;
class SessionListener;
class SessionCoordinator;
class SnapshotStrategy;
class SurfaceStack;
class SceneReport;
class PromptSessionListener;
class PromptSessionManager;
class CoordinateTranslator;
}
namespace graphics
{
class Platform;
class Display;
class DisplayReport;
class DisplayConfigurationObserver;
class GraphicBufferAllocator;
class Cursor;
class CursorImage;
class GLConfig;
namespace nested
{
class HostConnection;
class MirClientHostConnection;
}
}
namespace input
{
class InputReport;
class SeatObserver;
class Scene;
class InputManager;
class SurfaceInputDispatcher;
class InputDeviceRegistry;
class InputDeviceHub;
class DefaultInputDeviceHub;
class CompositeEventFilter;
class EventFilterChainDispatcher;
class InputChannelFactory;
class CursorListener;
class TouchVisualizer;
class InputRegion;
class InputSender;
class CursorImages;
class Seat;
class KeyMapper;
}

namespace logging
{
class Logger;
}

namespace options
{
class Option;
class Configuration;
}

namespace report
{
class ReportFactory;
class Reports;
}

namespace renderer
{
class RendererFactory;
}

class DefaultServerConfiguration : public virtual ServerConfiguration
{
public:
    DefaultServerConfiguration(int argc, char const* argv[]);
    explicit DefaultServerConfiguration(std::shared_ptr<options::Configuration> const& configuration_options);

    /** @name DisplayServer dependencies
     * dependencies of DisplayServer on the rest of the Mir
     *  @{ */
    std::shared_ptr<frontend::Connector>    the_connector() override;
    std::shared_ptr<frontend::Connector>    the_prompt_connector() override;
    std::shared_ptr<graphics::Display>      the_display() override;
    std::shared_ptr<compositor::Compositor> the_compositor() override;
    std::shared_ptr<input::InputManager>    the_input_manager() override;
    std::shared_ptr<MainLoop>               the_main_loop() override;
    std::shared_ptr<ServerStatusListener>   the_server_status_listener() override;
    std::shared_ptr<DisplayChanger>         the_display_changer() override;
    std::shared_ptr<graphics::Platform>     the_graphics_platform() override;
    std::shared_ptr<input::InputDispatcher> the_input_dispatcher() override;
    std::shared_ptr<EmergencyCleanup>       the_emergency_cleanup() override;
    std::shared_ptr<cookie::Authority>      the_cookie_authority() override;
    std::function<void()>                   the_stop_callback() override;
    /**
     * Function to call when a "fatal" error occurs. This implementation allows
     * the default strategy to be overridden by --on-fatal-error-except to avoid a
     * core.
     * To change the default strategy used FatalErrorStrategy. See acceptance test
     * ServerShutdown.fatal_error_default_can_be_changed_to_abort
     * for an example.
     */
    auto the_fatal_error_strategy() -> void (*)(char const* reason, ...) override final;
    std::shared_ptr<scene::ApplicationNotRespondingDetector> the_application_not_responding_detector() override;
    virtual std::shared_ptr<scene::ApplicationNotRespondingDetector>
        wrap_application_not_responding_detector(
            std::shared_ptr<scene::ApplicationNotRespondingDetector> const& wrapped);
    /** @} */

    /** @name graphics configuration - customization
     * configurable interfaces for modifying graphics
     *  @{ */
    virtual std::shared_ptr<renderer::RendererFactory>   the_renderer_factory();
    virtual std::shared_ptr<shell::DisplayConfigurationController> the_display_configuration_controller();
    virtual std::shared_ptr<graphics::DisplayConfigurationPolicy> the_display_configuration_policy();
    virtual std::shared_ptr<graphics::nested::HostConnection> the_host_connection();
    virtual std::shared_ptr<graphics::GLConfig> the_gl_config();
    /** @} */

    /** @name graphics configuration - dependencies
     * dependencies of graphics on the rest of the Mir
     *  @{ */
    virtual std::shared_ptr<graphics::DisplayReport> the_display_report();
    virtual std::shared_ptr<graphics::Cursor> the_cursor();
    virtual std::shared_ptr<graphics::Cursor> wrap_cursor(std::shared_ptr<graphics::Cursor> const& wrapped);
    virtual std::shared_ptr<graphics::CursorImage> the_default_cursor_image();
    virtual std::shared_ptr<input::CursorImages> the_cursor_images();
    std::shared_ptr<ObserverRegistrar<graphics::DisplayConfigurationObserver>>
        the_display_configuration_observer_registrar();

    /** @} */

    /** @name compositor configuration - customization
     * configurable interfaces for modifying compositor
     *  @{ */
    virtual std::shared_ptr<compositor::CompositorReport> the_compositor_report();
    virtual std::shared_ptr<compositor::DisplayBufferCompositorFactory> the_display_buffer_compositor_factory();
    virtual std::shared_ptr<compositor::DisplayBufferCompositorFactory> wrap_display_buffer_compositor_factory(
        std::shared_ptr<compositor::DisplayBufferCompositorFactory> const& wrapped);
    /** @} */

    /** @name compositor configuration - dependencies
     * dependencies of compositor on the rest of the Mir
     *  @{ */
    virtual std::shared_ptr<graphics::GraphicBufferAllocator> the_buffer_allocator();
    virtual std::shared_ptr<compositor::Scene>                  the_scene();
    virtual std::shared_ptr<compositor::FrameDroppingPolicyFactory> the_frame_dropping_policy_factory();
    /** @} */

    /** @name frontend configuration - dependencies
     * dependencies of frontend on the rest of the Mir
     *  @{ */
    virtual std::shared_ptr<ObserverRegistrar<frontend::SessionMediatorObserver>>
        the_session_mediator_observer_registrar();
    virtual std::shared_ptr<frontend::MessageProcessorReport> the_message_processor_report();
    virtual std::shared_ptr<frontend::SessionAuthorizer>      the_session_authorizer();
    // the_frontend_shell() is an adapter for the_shell().
    // To customize this behaviour it is recommended you override wrap_shell().
    std::shared_ptr<frontend::Shell>                          the_frontend_shell();
    virtual std::shared_ptr<frontend::EventSink>              the_global_event_sink();
    virtual std::shared_ptr<frontend::DisplayChanger>         the_frontend_display_changer();
    virtual std::shared_ptr<frontend::Screencast>             the_screencast();
    /** @name frontend configuration - internal dependencies
     * internal dependencies of frontend
     *  @{ */
    virtual std::shared_ptr<frontend::ConnectionCreator>      the_connection_creator();
    virtual std::shared_ptr<frontend::ConnectionCreator>      the_prompt_connection_creator();
    virtual std::shared_ptr<frontend::ConnectorReport>        the_connector_report();
    /** @} */
    /** @} */

    // the_focus_controller() is an interface for the_shell().
    std::shared_ptr<shell::FocusController> the_focus_controller();

    /** @name shell configuration - customization
     * configurable interfaces for modifying shell
     *  @{ */
    virtual auto the_shell() -> std::shared_ptr<shell::Shell>;
    virtual auto the_window_manager_builder() -> shell::WindowManagerBuilder;
    virtual std::shared_ptr<scene::SessionListener>     the_session_listener();
    virtual std::shared_ptr<shell::DisplayLayout>       the_shell_display_layout();
    virtual std::shared_ptr<scene::PromptSessionListener> the_prompt_session_listener();
    virtual std::shared_ptr<scene::PromptSessionManager>  the_prompt_session_manager();
    virtual std::shared_ptr<shell::HostLifecycleEventListener> the_host_lifecycle_event_listener();
    virtual std::shared_ptr<shell::PersistentSurfaceStore> the_persistent_surface_store();
    virtual std::shared_ptr<shell::ShellReport>         the_shell_report();
    /** @} */

    /** @name internal scene configuration
     * builder functions used in the default implementation.
     * The interfaces returned are not published, so the functions are only useful in tests
     *  @{ */
    virtual std::shared_ptr<scene::PixelBuffer>       the_pixel_buffer();
    virtual std::shared_ptr<scene::SnapshotStrategy>  the_snapshot_strategy();
    virtual std::shared_ptr<scene::SessionContainer>  the_session_container();
    virtual std::shared_ptr<scene::SessionEventSink>  the_session_event_sink();
    virtual std::shared_ptr<scene::SessionEventHandlerRegister> the_session_event_handler_register();
    virtual std::shared_ptr<scene::SurfaceFactory>    the_surface_factory();
    virtual std::shared_ptr<shell::SurfaceStack>      the_surface_stack();
    virtual std::shared_ptr<shell::SurfaceStack>      wrap_surface_stack(std::shared_ptr<shell::SurfaceStack> const& wrapped);
    /** @} */

    /** @name scene configuration - dependencies
     * dependencies of scene on the rest of the Mir
     *  @{ */
    virtual std::shared_ptr<scene::BufferStreamFactory> the_buffer_stream_factory();
    virtual std::shared_ptr<scene::SceneReport>      the_scene_report();
    /** @} */

    /** @name scene configuration - services
     * services provided by scene for the rest of Mir
     *  @{ */
    virtual std::shared_ptr<scene::SessionCoordinator>  the_session_coordinator();
    virtual std::shared_ptr<scene::CoordinateTranslator> the_coordinate_translator();
    /** @} */


    /** @name input configuration
     *  @{ */
    virtual std::shared_ptr<input::InputReport> the_input_report();
    virtual std::shared_ptr<ObserverRegistrar<input::SeatObserver>> the_seat_observer_registrar();
    virtual std::shared_ptr<input::CompositeEventFilter> the_composite_event_filter();

    virtual std::shared_ptr<input::EventFilterChainDispatcher> the_event_filter_chain_dispatcher();

    virtual std::shared_ptr<shell::InputTargeter> the_input_targeter();
    virtual std::shared_ptr<input::Scene>  the_input_scene();
    virtual std::shared_ptr<input::CursorListener> the_cursor_listener();
    virtual std::shared_ptr<input::TouchVisualizer> the_touch_visualizer();
    virtual std::shared_ptr<input::InputRegion>    the_input_region();
    virtual std::shared_ptr<input::InputSender>    the_input_sender();
    virtual std::shared_ptr<input::Seat> the_seat();
    virtual std::shared_ptr<input::KeyMapper> the_key_mapper();

    // new input reading related parts:
    virtual std::shared_ptr<dispatch::MultiplexingDispatchable> the_input_reading_multiplexer();
    virtual std::shared_ptr<input::InputDeviceRegistry> the_input_device_registry();
    virtual std::shared_ptr<input::InputDeviceHub> the_input_device_hub();
    virtual std::shared_ptr<input::SurfaceInputDispatcher> the_surface_input_dispatcher();
    /** @} */

    /** @name logging configuration - customization
     * configurable interfaces for modifying logging
     *  @{ */
    virtual std::shared_ptr<logging::Logger> the_logger();
    /** @} */

    virtual std::shared_ptr<time::Clock> the_clock();
    virtual std::shared_ptr<ServerActionQueue> the_server_action_queue();
    virtual std::shared_ptr<SharedLibraryProberReport>  the_shared_library_prober_report();

private:
    // We need to ensure the platform library is destroyed last as the
    // DisplayConfiguration can hold weak_ptrs to objects created from the library
    // TODO: We need a better way to manage the lifetimes of platform libraries
    std::shared_ptr<mir::SharedLibrary> platform_library;

protected:
    std::shared_ptr<options::Option> the_options() const;
    std::shared_ptr<graphics::nested::MirClientHostConnection>  the_mir_client_host_connection();
    std::shared_ptr<input::DefaultInputDeviceHub>  the_default_input_device_hub();
    std::shared_ptr<graphics::DisplayConfigurationObserver> the_display_configuration_observer();
    std::shared_ptr<input::SeatObserver> the_seat_observer();
    std::shared_ptr<frontend::SessionMediatorObserver> the_session_mediator_observer();

    virtual std::shared_ptr<input::InputChannelFactory> the_input_channel_factory();
    virtual std::shared_ptr<scene::MediatingDisplayChanger> the_mediating_display_changer();
    virtual std::shared_ptr<frontend::ProtobufIpcFactory> new_ipc_factory(
        std::shared_ptr<frontend::SessionAuthorizer> const& session_authorizer);

    /** @} */

    /** @Convenience wrapper functions
     *  @{ */
    virtual std::shared_ptr<graphics::DisplayConfigurationPolicy> wrap_display_configuration_policy(
        std::shared_ptr<graphics::DisplayConfigurationPolicy> const& wrapped);

    virtual std::shared_ptr<shell::Shell>  wrap_shell(
        std::shared_ptr<shell::Shell> const& wrapped);

    virtual std::shared_ptr<input::CursorListener>  wrap_cursor_listener(
        std::shared_ptr<input::CursorListener> const& wrapped);
/** @} */

    CachedPtr<frontend::Connector>   connector;
    CachedPtr<frontend::Connector>   prompt_connector;

    CachedPtr<input::InputReport> input_report;
    CachedPtr<input::EventFilterChainDispatcher> event_filter_chain_dispatcher;
    CachedPtr<input::CompositeEventFilter> composite_event_filter;
    CachedPtr<input::InputManager>    input_manager;
    CachedPtr<input::SurfaceInputDispatcher>    surface_input_dispatcher;
    CachedPtr<input::DefaultInputDeviceHub>    default_input_device_hub;
    CachedPtr<dispatch::MultiplexingDispatchable> input_reading_multiplexer;
    CachedPtr<input::InputDispatcher> input_dispatcher;
    CachedPtr<input::InputSender>     input_sender;
    CachedPtr<input::InputRegion>     input_region;
    CachedPtr<shell::InputTargeter> input_targeter;
    CachedPtr<input::CursorListener> cursor_listener;
    CachedPtr<input::TouchVisualizer> touch_visualizer;
    CachedPtr<input::Seat> seat;
    CachedPtr<graphics::Platform>     graphics_platform;
    CachedPtr<graphics::GraphicBufferAllocator> buffer_allocator;
    CachedPtr<graphics::Display>      display;
    CachedPtr<graphics::Cursor>       cursor;
    CachedPtr<graphics::CursorImage>  default_cursor_image;
    CachedPtr<input::CursorImages> cursor_images;

    CachedPtr<frontend::ConnectorReport>   connector_report;
    CachedPtr<frontend::MessageProcessorReport> message_processor_report;
    CachedPtr<frontend::SessionAuthorizer> session_authorizer;
    CachedPtr<frontend::EventSink> global_event_sink;
    CachedPtr<frontend::ConnectionCreator> connection_creator;
    CachedPtr<frontend::ConnectionCreator> prompt_connection_creator;
    CachedPtr<frontend::Screencast> screencast;
    CachedPtr<renderer::RendererFactory> renderer_factory;
    CachedPtr<compositor::BufferStreamFactory> buffer_stream_factory;
    CachedPtr<compositor::FrameDroppingPolicyFactory> frame_dropping_policy_factory;
    CachedPtr<scene::SurfaceStack> scene_surface_stack;
    CachedPtr<shell::SurfaceStack> surface_stack;
    CachedPtr<scene::SceneReport> scene_report;

    CachedPtr<scene::SurfaceFactory> surface_factory;
    CachedPtr<scene::SessionContainer>  session_container;
    CachedPtr<scene::SessionListener> session_listener;
    CachedPtr<scene::PixelBuffer>       pixel_buffer;
    CachedPtr<scene::SnapshotStrategy>  snapshot_strategy;
    CachedPtr<shell::DisplayLayout>     shell_display_layout;
    CachedPtr<compositor::DisplayBufferCompositorFactory> display_buffer_compositor_factory;
    CachedPtr<compositor::Compositor> compositor;
    CachedPtr<compositor::CompositorReport> compositor_report;
    CachedPtr<logging::Logger> logger;
    CachedPtr<graphics::DisplayReport> display_report;
    CachedPtr<time::Clock> clock;
    CachedPtr<MainLoop> main_loop;
    CachedPtr<ServerStatusListener> server_status_listener;
    CachedPtr<graphics::DisplayConfigurationPolicy> display_configuration_policy;
    CachedPtr<graphics::nested::MirClientHostConnection> host_connection;
    CachedPtr<scene::MediatingDisplayChanger> mediating_display_changer;
    CachedPtr<graphics::GLConfig> gl_config;
    CachedPtr<scene::PromptSessionListener> prompt_session_listener;
    CachedPtr<scene::PromptSessionManager> prompt_session_manager;
    CachedPtr<scene::SessionCoordinator> session_coordinator;
    CachedPtr<scene::CoordinateTranslator> coordinate_translator;
    CachedPtr<EmergencyCleanup> emergency_cleanup;
    CachedPtr<shell::HostLifecycleEventListener> host_lifecycle_event_listener;
    CachedPtr<shell::PersistentSurfaceStore> persistent_surface_store;
    CachedPtr<SharedLibraryProberReport> shared_library_prober_report;
    CachedPtr<shell::Shell> shell;
    CachedPtr<shell::ShellReport> shell_report;
    CachedPtr<scene::ApplicationNotRespondingDetector> application_not_responding_detector;
    CachedPtr<cookie::Authority> cookie_authority;
    CachedPtr<input::KeyMapper> key_mapper;

private:
    std::shared_ptr<options::Configuration> const configuration_options;
    std::shared_ptr<input::EventFilter> const default_filter;
    CachedPtr<ObserverMultiplexer<graphics::DisplayConfigurationObserver>>
        display_configuration_observer_multiplexer;
    CachedPtr<ObserverMultiplexer<input::SeatObserver>>
        seat_observer_multiplexer;
    CachedPtr<ObserverMultiplexer<frontend::SessionMediatorObserver>>
        session_mediator_observer_multiplexer;
    std::shared_ptr<report::Reports> const reports;

    virtual std::string the_socket_file() const;

    // The following caches and factory functions are internal to the
    // default implementations of corresponding the Mir components
    CachedPtr<scene::BroadcastingSessionEventSink> broadcasting_session_event_sink;

    std::shared_ptr<scene::BroadcastingSessionEventSink> the_broadcasting_session_event_sink();

    auto report_factory(char const* report_opt) -> std::unique_ptr<report::ReportFactory>;
    auto initialise_reports() -> std::shared_ptr<report::Reports>;

    CachedPtr<shell::detail::FrontendShell> frontend_shell;
};
}


#endif /* MIR_DEFAULT_SERVER_CONFIGURATION_H_ */