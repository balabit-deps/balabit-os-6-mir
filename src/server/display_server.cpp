/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by:
 *   Alan Griffiths <alan@octopull.co.uk>
 *   Thomas Voss <thomas.voss@canonical.com>
 */

#include "mir/display_server.h"
#include "mir/server_configuration.h"
#include "mir/main_loop.h"
#include "mir/server_status_listener.h"
#include "mir/display_changer.h"

#include "mir/compositor/compositor.h"
#include "mir/frontend/connector.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_configuration.h"
#include "mir/input/input_manager.h"
#include "mir/input/input_dispatcher.h"
#include "mir/log.h"
#include "mir/unwind_helpers.h"

#include <stdexcept>

namespace mc = mir::compositor;
namespace mf = mir::frontend;
namespace mg = mir::graphics;
namespace mi = mir::input;
namespace msh = mir::shell;

struct mir::DisplayServer::Private
{
    Private(ServerConfiguration& config)
        : emergency_cleanup{config.the_emergency_cleanup()},
          graphics_platform{config.the_graphics_platform()},
          display{config.the_display()},
          input_dispatcher{config.the_input_dispatcher()},
          compositor{config.the_compositor()},
          connector{config.the_connector()},
          prompt_connector{config.the_prompt_connector()},
          input_manager{config.the_input_manager()},
          main_loop{config.the_main_loop()},
          server_status_listener{config.the_server_status_listener()},
          display_changer{config.the_display_changer()},
          stop_callback{config.the_stop_callback()}
    {
        display->register_configuration_change_handler(
            *main_loop,
            [this] { return configure_display(); });

        display->register_pause_resume_handlers(
            *main_loop,
            [this] { return pause(); },
            [this] { return resume(); });
    }

    bool pause()
    {
        try
        {
            // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=62258
            // After using rethrow_exception() (and catching the exception),
            // all subsequent calls to uncaught_exception() return `true'.
            bool really_unwinding = true;
            
            auto comm = try_but_revert_if_unwinding(
                [this] { connector->stop(); },
                [&, this] { if (really_unwinding) connector->start(); });

            auto prompt = try_but_revert_if_unwinding(
                [this] { prompt_connector->stop(); },
                [&, this] { if (really_unwinding) prompt_connector->start(); });

            auto dispatcher = try_but_revert_if_unwinding(
                [this] { input_dispatcher->stop(); },
                [&, this] { if (really_unwinding) input_dispatcher->start(); });

            auto input = try_but_revert_if_unwinding(
                [this] { input_manager->stop(); },
                [&, this] { if (really_unwinding) input_manager->start(); });

            auto display_config_processing = try_but_revert_if_unwinding(
                [this] { display_changer->pause_display_config_processing(); },
                [&, this] { if (really_unwinding) display_changer->resume_display_config_processing(); });

            auto comp = try_but_revert_if_unwinding(
                [this] { compositor->stop(); },
                [&, this] { if (really_unwinding) compositor->start(); });

            display->pause();

            really_unwinding = false;
        }
        catch(std::runtime_error const&)
        {
            return false;
        }

        server_status_listener->paused();

        return true;
    }

    bool resume()
    {
        try
        {
            // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=62258
            // After using rethrow_exception() (and catching the exception),
            // all subsequent calls to uncaught_exception() return `true'.
            bool really_unwinding = true;

            auto disp = try_but_revert_if_unwinding(
                [this] { display->resume(); },
                [&, this] { if (really_unwinding) display->pause(); });

            auto comp = try_but_revert_if_unwinding(
                [this] { compositor->start(); },
                [&, this] { if (really_unwinding) compositor->stop(); });

            auto display_config_processing = try_but_revert_if_unwinding(
                [this] { display_changer->resume_display_config_processing(); },
                [&, this] { if (really_unwinding) display_changer->pause_display_config_processing(); });

            auto input = try_but_revert_if_unwinding(
                [this] { input_manager->start(); },
                [&, this] { if (really_unwinding) input_manager->stop(); });

            auto dispatcher = try_but_revert_if_unwinding(
                [this] { input_dispatcher->start(); },
                [&, this] { if (really_unwinding) input_dispatcher->stop(); });

            auto prompt = try_but_revert_if_unwinding(
                [this] { prompt_connector->start(); },
                [&, this] { if (really_unwinding) prompt_connector->stop(); });

            connector->start();
            
            really_unwinding = false;
        }
        catch(std::runtime_error const&)
        {
            return false;
        }

        server_status_listener->resumed();

        return true;
    }

    void configure_display()
    {
        std::shared_ptr<graphics::DisplayConfiguration> conf =
            display->configuration();

        display_changer->configure_for_hardware_change(conf);
    }

    std::shared_ptr<EmergencyCleanup> const emergency_cleanup; // Hold this so it does not get freed prematurely
    std::shared_ptr<mg::Platform> const graphics_platform; // Hold this so the platform is loaded once
    std::shared_ptr<mg::Display> const display;
    std::shared_ptr<mi::InputDispatcher> const input_dispatcher;
    std::shared_ptr<mc::Compositor> const compositor;
    std::shared_ptr<mf::Connector> const connector;
    std::shared_ptr<mf::Connector> const prompt_connector;
    std::shared_ptr<mi::InputManager> const input_manager;
    std::shared_ptr<mir::MainLoop> const main_loop;
    std::shared_ptr<mir::ServerStatusListener> const server_status_listener;
    std::shared_ptr<mir::DisplayChanger> const display_changer;
    std::function<void()> const stop_callback;

};

mir::DisplayServer::DisplayServer(ServerConfiguration& config) :
    p(new DisplayServer::Private{config})
{
}

/*
 * Need to define the destructor in the source file, so that we
 * can define the 'p' member variable as a unique_ptr to an
 * incomplete type (DisplayServerPrivate) in the header.
 */
mir::DisplayServer::~DisplayServer()
{
    delete p.load();
}

void mir::DisplayServer::run()
{
    mir::log_info("Mir version " MIR_VERSION);

    auto const& server = *p.load();

    server.compositor->start();
    server.input_manager->start();
    server.input_dispatcher->start();
    server.prompt_connector->start();
    server.connector->start();

    server.server_status_listener->started();

    server.main_loop->run();

    server.connector->stop();
    server.prompt_connector->stop();
    server.input_dispatcher->stop();
    server.input_manager->stop();
    server.compositor->stop();
}

void mir::DisplayServer::stop()
{
    p.load()->stop_callback();
    p.load()->main_loop->stop();
}
