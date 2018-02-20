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

#ifndef MIR_TEST_FRAMEWORK_FAKE_INPUT_DEVICE_H_
#define MIR_TEST_FRAMEWORK_FAKE_INPUT_DEVICE_H_

#include "mir_toolkit/events/event.h"
#include "mir/test/event_factory.h"
#include <chrono>
#include <functional>

namespace mir_test_framework
{

class FakeInputDevice
{
public:
    /**
     * Valid value range of simulated touch coordinates. The simulated coordinates will be remapped to
     * the coordinates of the given input sink.
     * \{
     */
    static const int maximum_touch_axis_value = 0xFFFF;
    static const int minimum_touch_axis_value = 0;
    /// \}

    FakeInputDevice() = default;
    virtual ~FakeInputDevice() = default;


    virtual void emit_device_removal() = 0;
    virtual void emit_runtime_error() = 0;
    virtual void emit_event(mir::input::synthesis::KeyParameters const& key) = 0;
    virtual void emit_event(mir::input::synthesis::ButtonParameters const& button) = 0;
    virtual void emit_event(mir::input::synthesis::MotionParameters const& motion) = 0;
    virtual void emit_event(mir::input::synthesis::TouchParameters const& touch) = 0;
    virtual void emit_touch_sequence(std::function<mir::input::synthesis::TouchParameters(int)> const& generate_parameters,
                                     int count,
                                     std::chrono::duration<double> delay) = 0;

    FakeInputDevice(FakeInputDevice const&) = delete;
    FakeInputDevice& operator=(FakeInputDevice const&) = delete;
};

}

#endif
