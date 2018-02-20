/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_INPUT_ANDROID_INPUT_SENDER_H_
#define MIR_INPUT_ANDROID_INPUT_SENDER_H_

#include "mir/input/input_sender.h"
#include "mir/scene/null_observer.h"
#include "mir_toolkit/event.h"
#include "mir/events/event_private.h"

#include "androidfw/InputTransport.h"

#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <atomic>

namespace droidinput = android;

namespace mir
{
class MainLoop;
namespace compositor
{
class Scene;
}
namespace scene
{
class InputRegistrar;
}
namespace input
{
class InputReport;
class Surface;
namespace android
{

class InputSender : public input::InputSender
{
public:
    InputSender(std::shared_ptr<compositor::Scene> const& scene,
                std::shared_ptr<MainLoop> const& main_loop,
                std::shared_ptr<InputReport> const& report);

    void send_event(MirEvent const& event, std::shared_ptr<InputChannel> const& channel) override;

private:
    struct InputSenderState;

    class SceneObserver : public scene::NullObserver
    {
    public:
        explicit SceneObserver(InputSenderState & state);
    private:
        void surface_added(scene::Surface* surface) override;
        void surface_removed(scene::Surface* surface) override;
        void surface_exists(scene::Surface* surface) override;
        void scene_changed() override;

        void remove_transfer_for(input::Surface* surface);
        InputSenderState & state;
    };

    class ActiveTransfer
    {
    public:
        ActiveTransfer(InputSenderState & state, std::shared_ptr<InputChannel> const& channel, input::Surface* surface);
        ~ActiveTransfer();
        void send(uint32_t sequence_id, MirEvent const& event);
        bool used_for_surface(input::Surface const* surface) const;
        void subscribe();
        void unsubscribe();

    private:
        void on_finish_signal();
        droidinput::status_t send_key_event(uint32_t sequence_id, MirEvent const& event);
        droidinput::status_t send_touch_event(uint32_t sequence_id, MirEvent const& event);
        droidinput::status_t send_pointer_event(uint32_t sequence_id, MirEvent const& event);

        InputSenderState & state;
        droidinput::InputPublisher publisher;
        input::Surface const* surface;
        std::shared_ptr<InputChannel> const channel;
        std::atomic<bool> subscribed{false};

        ActiveTransfer& operator=(ActiveTransfer const&) = delete;
        ActiveTransfer(ActiveTransfer const&) = delete;
    };

    struct InputSenderState
    {
        InputSenderState(std::shared_ptr<MainLoop> const& main_loop,
                         std::shared_ptr<InputReport> const& report);
        void send_event(std::shared_ptr<InputChannel> const& channel, MirEvent const& event);
        void add_transfer(std::shared_ptr<InputChannel> const& channel, input::Surface* surface);
        void remove_transfer(int fd);

        std::shared_ptr<MainLoop> const main_loop;
        std::shared_ptr<InputReport> const report;

    private:
        std::shared_ptr<ActiveTransfer> get_transfer(int fd);
        uint32_t next_seq();
        uint32_t seq;

        std::unordered_map<int,std::shared_ptr<ActiveTransfer>> transfers;
        std::mutex sender_mutex;
    };

    InputSenderState state;
    std::shared_ptr<compositor::Scene> scene;
};

}
}
}

#endif
