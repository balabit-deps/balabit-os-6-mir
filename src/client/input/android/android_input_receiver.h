/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_INPUT_RECEIVER_ANDROID_INPUT_RECEIVER_H_
#define MIR_INPUT_RECEIVER_ANDROID_INPUT_RECEIVER_H_

#include "mir_toolkit/event.h"

#include "mir/dispatch/dispatchable.h"
#include "mir/dispatch/multiplexing_dispatchable.h"

#include <utils/StrongPointer.h>
#include <androidfw/Input.h>

#include <memory>
#include <chrono>
#include <functional>

namespace droidinput = android;

namespace android
{
class InputChannel;
class InputConsumer;
class Looper;
}

namespace mir
{
namespace input
{
namespace receiver
{
class XKBMapper;
class InputReceiverReport;

namespace android
{

/// Synchronously receives input events in a blocking manner
class InputReceiver : public dispatch::Dispatchable
{
public:
    InputReceiver(droidinput::sp<droidinput::InputChannel> const& input_channel,
                  std::shared_ptr<XKBMapper> const& keymapper,
                  std::function<void(MirEvent*)> const& event_handling_callback,
                  std::shared_ptr<InputReceiverReport> const& report);
    InputReceiver(int fd,
                  std::shared_ptr<XKBMapper> const& keymapper,
                  std::function<void(MirEvent*)> const& event_handling_callback,
                  std::shared_ptr<InputReceiverReport> const& report);

    virtual ~InputReceiver();

    Fd watch_fd() const override;
    bool dispatch(dispatch::FdEvents events) override;
    dispatch::FdEvents relevant_events() const override;

protected:
    InputReceiver(const InputReceiver&) = delete;
    InputReceiver& operator=(const InputReceiver&) = delete;

private:
    dispatch::MultiplexingDispatchable dispatcher;
    Fd const wake_fd;

    droidinput::sp<droidinput::InputChannel> input_channel;
    std::function<void(MirEvent*)> const handler;
    std::shared_ptr<XKBMapper> const xkb_mapper;
    std::shared_ptr<InputReceiverReport> const report;

    std::shared_ptr<droidinput::InputConsumer> input_consumer;
    droidinput::PreallocatedInputEventFactory event_factory;

    void woke();
    void wake();
};

}
}
}
} // namespace mir

#endif // MIR_INPUT_RECEIVER_ANDROID_INPUT_RECEIVER_H_
