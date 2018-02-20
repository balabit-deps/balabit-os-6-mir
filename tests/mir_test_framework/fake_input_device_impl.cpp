/*
 * Copyright © 2015 Canonical Ltd.
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

#include "fake_input_device_impl.h"
#include "mir_test_framework/stub_input_platform.h"

#include "mir/input/input_device.h"
#include "mir/input/input_device_info.h"
#include "mir/input/input_sink.h"
#include "mir/input/pointer_settings.h"
#include "mir/input/touchpad_settings.h"
#include "mir/input/event_builder.h"
#include "mir/dispatch/action_queue.h"
#include "mir/geometry/displacement.h"
#include "src/platforms/evdev/button_utils.h"

#include "boost/throw_exception.hpp"

#include "mir/events/event_builders.h"

#include <chrono>
#include <thread>

namespace mi = mir::input;
namespace mie = mi::evdev;
namespace md = mir::dispatch;
namespace mtf = mir_test_framework;
namespace synthesis = mir::input::synthesis;

mtf::FakeInputDeviceImpl::FakeInputDeviceImpl(mi::InputDeviceInfo const& info)
    : queue{std::make_shared<md::ActionQueue>()}, device{std::make_shared<InputDevice>(info, queue)}
{
    mtf::StubInputPlatform::add(device);
}

void mtf::FakeInputDeviceImpl::emit_device_removal()
{
    mtf::StubInputPlatform::remove(device);
}

void mtf::FakeInputDeviceImpl::emit_runtime_error()
{
    queue->enqueue([]()
                   {
                       throw std::runtime_error("runtime error in input device");
                   });
}

void mtf::FakeInputDeviceImpl::emit_event(synthesis::KeyParameters const& key)
{
    queue->enqueue([this, key]()
                   {
                       device->synthesize_events(key);
                   });
}

void mtf::FakeInputDeviceImpl::emit_event(synthesis::ButtonParameters const& button)
{
    queue->enqueue([this, button]()
                   {
                       device->synthesize_events(button);
                   });
}

void mtf::FakeInputDeviceImpl::emit_event(synthesis::MotionParameters const& motion)
{
    queue->enqueue([this, motion]()
                   {
                       device->synthesize_events(motion);
                   });
}

void mtf::FakeInputDeviceImpl::emit_event(synthesis::TouchParameters const& touch)
{
    queue->enqueue([this, touch]()
                   {
                       device->synthesize_events(touch);
                   });
}

void mtf::FakeInputDeviceImpl::emit_touch_sequence(std::function<mir::input::synthesis::TouchParameters(int)> const& event_generator,
                                                   int count,
                                                   std::chrono::duration<double> delay)
{
    queue->enqueue(
        [this, event_generator, count, delay]()
        {
            auto start = std::chrono::steady_clock::now();
            for (int i = 0;i < count;++i)
            {
                std::this_thread::sleep_until(start + i * delay);
                device->synthesize_events(event_generator(i++));
                std::this_thread::yield();
            }
        });
}


mtf::FakeInputDeviceImpl::InputDevice::InputDevice(mi::InputDeviceInfo const& info,
                                                   std::shared_ptr<mir::dispatch::Dispatchable> const& dispatchable)
    : info(info), queue{dispatchable}, buttons{0}
{
    // the default setup results in a direct mapping of input velocity to output velocity.
    settings.acceleration = mir_pointer_acceleration_none;
    settings.cursor_acceleration_bias = 0.0;
}

void mtf::FakeInputDeviceImpl::InputDevice::synthesize_events(synthesis::KeyParameters const& key_params)
{
    xkb_keysym_t key_code = 0;

    auto event_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch());

    auto input_action =
        (key_params.action == synthesis::EventAction::Down) ? mir_keyboard_action_down : mir_keyboard_action_up;

    auto key_event = builder->key_event(event_time, input_action, key_code, key_params.scancode);

    if (!sink)
        BOOST_THROW_EXCEPTION(std::runtime_error("Device is not started."));
    sink->handle_input(*key_event);
}

void mtf::FakeInputDeviceImpl::InputDevice::synthesize_events(synthesis::ButtonParameters const& button)
{
    auto event_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
    auto action = update_buttons(button.action, mie::to_pointer_button(button.button, settings.handedness));
    auto button_event = builder->pointer_event(event_time,
                                               action,
                                               buttons,
                                               scroll.x.as_int(),
                                               scroll.y.as_int(),
                                               0.0f,
                                               0.0f);

    if (!sink)
        BOOST_THROW_EXCEPTION(std::runtime_error("Device is not started."));
    sink->handle_input(*button_event);
}

MirPointerAction mtf::FakeInputDeviceImpl::InputDevice::update_buttons(synthesis::EventAction action, MirPointerButton button)
{
    if (action == synthesis::EventAction::Down)
    {
        buttons |= button;
        return mir_pointer_action_button_down;
    }
    else
    {
        buttons &= ~button;
        return mir_pointer_action_button_up;
    }
}

void mtf::FakeInputDeviceImpl::InputDevice::synthesize_events(synthesis::MotionParameters const& pointer)
{
    if (!sink)
        BOOST_THROW_EXCEPTION(std::runtime_error("Device is not started."));

    auto event_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
    // constant scaling is used here to simplify checking for the
    // expected results. Default settings of the device lead to no
    // scaling at all.
    auto const acceleration = settings.cursor_acceleration_bias + 1.0;
    auto const rel_x = pointer.rel_x * acceleration;
    auto const rel_y = pointer.rel_y * acceleration;

    auto pointer_event = builder->pointer_event(event_time,
                                                mir_pointer_action_motion,
                                                buttons,
                                                scroll.x.as_int(),
                                                scroll.y.as_int(),
                                                rel_x,
                                                rel_y);

    sink->handle_input(*pointer_event);
}

void mtf::FakeInputDeviceImpl::InputDevice::synthesize_events(synthesis::TouchParameters const& touch)
{
    if (!sink)
        BOOST_THROW_EXCEPTION(std::runtime_error("Device is not started."));

    auto const event_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch());

    auto touch_action = mir_touch_action_up;
    if (touch.action == synthesis::TouchParameters::Action::Tap)
        touch_action = mir_touch_action_down;
    else if (touch.action == synthesis::TouchParameters::Action::Move)
        touch_action = mir_touch_action_change;

    float abs_x = touch.abs_x;
    float abs_y = touch.abs_y;
    map_touch_coordinates(abs_x, abs_y);
    // those values would need scaling too as soon as they can be controlled by the caller

    auto touch_event = builder->touch_event(
        event_time,
        {{MirTouchId{1}, touch_action, mir_touch_tooltype_finger, abs_x, abs_y, 1.0f, 8.0f, 5.0f, 0.0f}});

    sink->handle_input(*touch_event);
}

mir::optional_value<mi::PointerSettings> mtf::FakeInputDeviceImpl::InputDevice::get_pointer_settings() const
{
    mir::optional_value<mi::PointerSettings> ret;
    if (!contains(info.capabilities, mi::DeviceCapability::pointer))
        return ret;

    ret = settings;
    return ret;
}

void mtf::FakeInputDeviceImpl::InputDevice::apply_settings(mi::PointerSettings const& settings)
{
    if (!contains(info.capabilities, mi::DeviceCapability::pointer))
        return;
    this->settings = settings;
}

mir::optional_value<mi::TouchpadSettings> mtf::FakeInputDeviceImpl::InputDevice::get_touchpad_settings() const
{
    mir::optional_value<mi::TouchpadSettings> ret;
    if (contains(info.capabilities, mi::DeviceCapability::touchpad))
        ret = mi::TouchpadSettings();

    return ret;
}

void mtf::FakeInputDeviceImpl::InputDevice::apply_settings(mi::TouchpadSettings const&)
{
    // Not applicable for configuration since FakeInputDevice just
    // forwards already interpreted events.
}

void mtf::FakeInputDeviceImpl::InputDevice::map_touch_coordinates(float& x, float& y)
{
    // TODO take orientation of input sink into account?
    auto area = sink->bounding_rectangle();
    auto touch_range = FakeInputDevice::maximum_touch_axis_value - FakeInputDevice::minimum_touch_axis_value + 1;
    auto x_scale = area.size.width.as_int() / float(touch_range);
    auto y_scale = area.size.height.as_int() / float(touch_range);
    x = (x - float(FakeInputDevice::minimum_touch_axis_value))*x_scale + area.top_left.x.as_int();
    y = (y - float(FakeInputDevice::minimum_touch_axis_value))*y_scale + area.top_left.y.as_int();
}

void mtf::FakeInputDeviceImpl::InputDevice::start(mi::InputSink* destination, mi::EventBuilder* event_builder)
{
    sink = destination;
    builder = event_builder;
    mtf::StubInputPlatform::register_dispatchable(queue);
}

void mtf::FakeInputDeviceImpl::InputDevice::stop()
{
    sink = nullptr;
    builder = nullptr;
    mtf::StubInputPlatform::unregister_dispatchable(queue);
}
