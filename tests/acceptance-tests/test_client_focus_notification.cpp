/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir_toolkit/mir_client_library.h"

#include "mir/test/signal.h"
#include "mir/test/event_matchers.h"

#include "mir_test_framework/connected_client_headless_server.h"
#include "mir_test_framework/process.h"
#include "mir/test/cross_process_sync.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mt = mir::test;
namespace mtf = mir_test_framework;

using namespace ::testing;

namespace
{
struct FocusSurface
{
    FocusSurface(MirConnection* connection) :
        connection(connection)
    {
        auto spec = mir_create_normal_window_spec(connection, 100, 100);
        mir_window_spec_set_pixel_format(spec, mir_pixel_format_abgr_8888);
        mir_window_spec_set_event_handler(spec, FocusSurface::handle_event, this);

        window = mir_create_window_sync(spec);
        mir_window_spec_release(spec);

        mir_buffer_stream_swap_buffers_sync(mir_window_get_buffer_stream(window));
    }

    ~FocusSurface()
    {
        if (!released) release();
    }

    static void handle_event(MirWindow* window, MirEvent const* ev, void* context)
    {
        if (mir_event_type_window == mir_event_get_type(ev))
        {
            auto surface_ev = mir_event_get_window_event(ev);
            auto attrib = mir_window_event_get_attribute(surface_ev);
            if (mir_window_attrib_focus == attrib)
            {
                auto self = static_cast<FocusSurface*>(context);
                self->log_focus_event(window,
                    static_cast<MirWindowFocusState>(mir_window_event_get_attribute_value(surface_ev)));
            }
        }
    }

    void log_focus_event(MirWindow*, MirWindowFocusState state)
    {
        std::lock_guard<std::mutex> lk(mutex);
        focus_events.push_back(state);
        cv.notify_all();
    }

    MirWindow* native_handle() const
    {
        return window;
    }

    void release()
    {
        mir_window_release_sync(window);
        mir_connection_release(connection);
        released = true;
    }

    void expect_focus_event_sequence(std::vector<MirWindowFocusState> const& seq)
    {
        std::unique_lock<std::mutex> lk(mutex);
        if (!cv.wait_for(lk, timeout, [this, &seq]
            {
                return focus_events.size() >= seq.size();
            }))
        {
            throw std::logic_error("timeout waiting for events");
        }
        EXPECT_THAT(focus_events, ContainerEq(seq));
    }

private:
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<MirWindowFocusState> focus_events;
    bool released {false};
    MirConnection* connection = nullptr;
    MirWindow* window = nullptr;
    std::chrono::seconds timeout{5};
};

struct ClientFocusNotification : mtf::ConnectedClientHeadlessServer
{
    std::unique_ptr<FocusSurface> make_surface()
    {
        auto connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);
        EXPECT_THAT(connection, NotNull());
        return std::make_unique<FocusSurface>(connection);
    }
};
}

TEST_F(ClientFocusNotification, a_surface_is_notified_of_receiving_focus)
{
    std::vector<MirWindowFocusState> const focus_sequence = {mir_window_focus_state_focused, mir_window_focus_state_unfocused};
    auto window = make_surface();
    window->release();
    window->expect_focus_event_sequence(focus_sequence);
}

TEST_F(ClientFocusNotification, two_surfaces_are_notified_of_gaining_and_losing_focus)
{
    std::vector<MirWindowFocusState> const initial_focus = {mir_window_focus_state_focused};
    std::vector<MirWindowFocusState> const focus_sequence1 =
        {mir_window_focus_state_focused, mir_window_focus_state_unfocused, mir_window_focus_state_focused, mir_window_focus_state_unfocused};
    std::vector<MirWindowFocusState> const focus_sequence2 = {mir_window_focus_state_focused, mir_window_focus_state_unfocused};

    auto surface1 = make_surface();
    surface1->expect_focus_event_sequence(initial_focus);

    auto surface2 = make_surface();

    surface2->release();
    surface1->release();

    surface1->expect_focus_event_sequence(focus_sequence1);
    surface2->expect_focus_event_sequence(focus_sequence2);
}
