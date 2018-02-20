/*
 * Copyright © 2016 Canonical Ltd.
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
 *   Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "input_platform.h"
#include "host_connection.h"

#include "mir/input/input_device_registry.h"
#include "mir/input/input_device_info.h"
#include "mir/input/input_device.h"
#include "mir/input/input_sink.h"
#include "mir/input/event_builder.h"
#include "mir/input/pointer_settings.h"
#include "mir/input/touchpad_settings.h"
#include "mir/input/device_capability.h"
#include "mir/dispatch/action_queue.h"
#include "mir/events/event_builders.h"

#include <chrono>

namespace mi = mir::input;
namespace mgn = mir::graphics::nested;

namespace
{

auto filter_pointer_action(MirPointerAction action)
{
    return (action == mir_pointer_action_enter || action == mir_pointer_action_leave)
        ? mir_pointer_action_motion
        : action;
}

mgn::UniqueInputConfig make_empty_config()
{
    return mgn::UniqueInputConfig(nullptr, [](MirInputConfig const*){});
}

}

struct mgn::InputPlatform::InputDevice : mi::InputDevice
{
public:
    InputDevice(MirInputDevice const* dev)
    {
        update(dev);
    }

    MirInputDeviceId id() const
    {
        return device_id;
    }

    void update(MirInputDevice const* dev)
    {
        device_id = mir_input_device_get_id(dev);
        device_info.name = mir_input_device_get_name(dev);
        device_info.unique_id = mir_input_device_get_unique_id(dev);
        device_info.capabilities = mi::DeviceCapabilities(mir_input_device_get_capabilities(dev));
    }

    void start(mi::InputSink* destination, mi::EventBuilder* builder) override
    {
        this->destination = destination;
        this->builder = builder;
    }

    void emit_event(MirInputEvent const* event, mir::geometry::Rectangle const& frame)
    {
        auto const type = mir_input_event_get_type(event);
        auto const event_time = std::chrono::nanoseconds{mir_input_event_get_event_time(event)};

        // TODO move the coordinate transformation over here
        switch(type)
        {
        case mir_input_event_type_touch:
            {
                auto const* touch_event = mir_input_event_get_touch_event(event);
                auto point_count = mir_touch_event_point_count(touch_event);
                std::vector<mir::events::ContactState> contacts(point_count);

                for (int i = 0, point_count = mir_touch_event_point_count(touch_event); i != point_count; ++i)
                {
                    auto & contact = contacts[i];
                    contact.touch_id = mir_touch_event_id(touch_event, i);
                    contact.action = mir_touch_event_action(touch_event, i);
                    contact.tooltype = mir_touch_event_tooltype(touch_event, i);
                    contact.x = mir_touch_event_axis_value(touch_event, i, mir_touch_axis_x);
                    contact.y = mir_touch_event_axis_value(touch_event, i, mir_touch_axis_y);
                    contact.pressure = mir_touch_event_axis_value(touch_event, i, mir_touch_axis_pressure);
                    contact.touch_major = mir_touch_event_axis_value(touch_event, i, mir_touch_axis_touch_major);
                    contact.touch_minor = mir_touch_event_axis_value(touch_event, i, mir_touch_axis_touch_minor);
                }
                destination->handle_input(*builder->touch_event(event_time, contacts));
                break;
            }
        case mir_input_event_type_key:
            {
                auto const* key_event = mir_input_event_get_keyboard_event(event);

                auto new_kev = builder->key_event(
                        event_time,
                        mir_keyboard_event_action(key_event),
                        mir_keyboard_event_key_code(key_event),
                        mir_keyboard_event_scan_code(key_event)
                        );
                destination->handle_input(*new_kev);
                break;
            }
        case mir_input_event_type_pointer:
            {
                auto const* pointer_event = mir_input_event_get_pointer_event(event);
                auto x = mir_pointer_event_axis_value(pointer_event, mir_pointer_axis_x);
                auto y = mir_pointer_event_axis_value(pointer_event, mir_pointer_axis_y);
                auto new_event = builder->pointer_event(
                        event_time,
                        filter_pointer_action(mir_pointer_event_action(pointer_event)),
                        mir_pointer_event_buttons(pointer_event),
                        x + frame.top_left.x.as_int(),
                        y + frame.top_left.y.as_int(),
                        mir_pointer_event_axis_value(pointer_event, mir_pointer_axis_hscroll),
                        mir_pointer_event_axis_value(pointer_event, mir_pointer_axis_vscroll),
                        mir_pointer_event_axis_value(pointer_event, mir_pointer_axis_relative_x),
                        mir_pointer_event_axis_value(pointer_event, mir_pointer_axis_relative_y)
                        );

                destination->handle_input(*new_event);
                break;
            }
        default:
            break;
        }
    }

    void stop() override
    {
        this->destination = nullptr;
        this->builder = nullptr;
    }

    mi::InputDeviceInfo get_device_info() override
    {
        return device_info;
    }

    mir::optional_value<mi::PointerSettings> get_pointer_settings() const override
    {
        return pointer_settings;
    }

    void apply_settings(mi::PointerSettings const&) override
    {
        // TODO no C API for that 
    }

    mir::optional_value<mi::TouchpadSettings> get_touchpad_settings() const override
    {
        return touchpad_settings;
    }

    void apply_settings(mi::TouchpadSettings const&) override
    {
        // no c api for that
    }

    MirInputDeviceId device_id;
    mi::InputSink* destination{nullptr};
    mi::EventBuilder* builder{nullptr};
    mi::InputDeviceInfo device_info;
    mir::optional_value<mi::PointerSettings> pointer_settings;
    mir::optional_value<mi::TouchpadSettings> touchpad_settings;
};


mgn::InputPlatform::InputPlatform(std::shared_ptr<HostConnection> const& connection,
                                  std::shared_ptr<input::InputDeviceRegistry> const& input_device_registry,
                                  std::shared_ptr<input::InputReport> const& report)
    : connection{connection}, input_device_registry{input_device_registry}, report{report}, action_queue{std::make_shared<dispatch::ActionQueue>()}, input_config{make_empty_config()}
{
}

std::shared_ptr<mir::dispatch::Dispatchable> mgn::InputPlatform::dispatchable()
{
    return action_queue;
}

void mgn::InputPlatform::start()
{
    {
        std::lock_guard<std::mutex> lock(devices_guard);
        input_config = connection->create_input_device_config();
        update_devices_locked();
    }

    connection->set_input_device_change_callback(
        [this](mgn::UniqueInputConfig new_config)
        {
            std::lock_guard<std::mutex> lock(devices_guard);
            input_config = std::move(new_config);
            action_queue->enqueue([this]{update_devices();});
        });

    connection->set_input_event_callback(
        [this](MirEvent const& event, mir::geometry::Rectangle const& area)
        {
            auto const event_type = mir_event_get_type(&event);

            if (event_type == mir_event_type_input)
            {
                std::lock_guard<std::mutex> lock(devices_guard);
                auto const* input_ev = mir_event_get_input_event(&event);
                auto const id = mir_input_event_get_device_id(input_ev);
                auto it = devices.find(id);
                if (it != end(devices))
                {
                    it->second->emit_event(input_ev, area);
                }
                else // device was not advertised to us yet.
                {
                    unknown_device_events[id].emplace_back(
                        std::piecewise_construct,
                        std::forward_as_tuple(mir::events::clone_event(event)),
                        std::forward_as_tuple(area));
                }
            }
            else if (event_type == mir_event_type_input_device_state)
            {
                std::lock_guard<std::mutex> lock(devices_guard);
                if (!devices.empty())
                {
                    auto const* device_state = mir_event_get_input_device_state_event(&event);
                    for (size_t index = 0, end_index = mir_input_device_state_event_device_count(device_state);
                         index != end_index; ++index)
                    {
                        auto it = devices.find(mir_input_device_state_event_device_id(device_state, index));
                        if (it != end(devices) && it->second->destination)
                        {
                            auto dest = it->second->destination;
                            auto key_count = mir_input_device_state_event_device_pressed_keys_count(device_state, index);
                            std::vector<uint32_t> scan_codes;
                            for (uint32_t i = 0; i < key_count; i++)
                            {
                                scan_codes.push_back(mir_input_device_state_event_device_pressed_keys_for_index(device_state, index, i));
                            }

                            dest->key_state(scan_codes);
                            dest->pointer_state(
                                mir_input_device_state_event_device_pointer_buttons(device_state, index));
                        }
                    }

                    auto& front = begin(devices)->second;
                    auto device_state_event = front->builder->device_state_event(
                        mir_input_device_state_event_pointer_axis(device_state, mir_pointer_axis_x),
                        mir_input_device_state_event_pointer_axis(device_state, mir_pointer_axis_y));
                    front->destination->handle_input(*device_state_event);
                }
            }
        });
}

void mgn::InputPlatform::stop()
{
    std::function<void(mgn::UniqueInputConfig)> reset;
    connection->set_input_device_change_callback(reset);
    std::function<void(MirEvent const&, mir::geometry::Rectangle const&)> empty_event_callback;
    connection->set_input_event_callback(empty_event_callback);

    std::lock_guard<std::mutex> lock(devices_guard);
    for(auto const& device : devices)
        input_device_registry->remove_device(device.second);

    devices.clear();
}

void mgn::InputPlatform::update_devices()
{
    std::lock_guard<std::mutex> lock(devices_guard);
    update_devices_locked();
}

void mgn::InputPlatform::update_devices_locked()
{
    auto deleted = std::move(devices);
    std::vector<std::pair<std::shared_ptr<mgn::InputPlatform::InputDevice>, MirInputDeviceId>> new_devs;
    auto config_ptr = input_config.get();
    for (size_t i = 0, e = mir_input_config_device_count(config_ptr); i!=e; ++i)
    {
        auto dev = mir_input_config_get_device(config_ptr, i);
        auto const id = mir_input_device_get_id(dev);
        auto it = deleted.find(id);
        if (it != end(deleted))
        {
            it->second->update(dev);
            devices[id] = it->second;
            deleted.erase(it);
        }
        else
        {
            new_devs.emplace_back(std::make_shared<InputDevice>(dev), id);
            devices[id] = new_devs.back().first;
        }
    }

    for (auto const& deleted_dev : deleted)
        input_device_registry->remove_device(deleted_dev.second);
    for (auto new_dev : new_devs)
    {
        input_device_registry->add_device(new_dev.first);

        auto early_event_queue = unknown_device_events.find(new_dev.second);
        if (early_event_queue != end(unknown_device_events))
        {
            for (auto const& stored_event : early_event_queue->second)
            {
                auto const* input_ev = mir_event_get_input_event(stored_event.first.get());
                new_dev.first->emit_event(input_ev, stored_event.second);
            }
        }
    }
    unknown_device_events.clear();
}
