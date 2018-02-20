/*
 * Copyright © 2015-2016 Canonical Ltd.
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

#ifndef MIR_BASIC_SEAT_H_
#define MIR_BASIC_SEAT_H_

#include "mir/input/seat.h"
#include "mir/frontend/event_sink.h"
#include "seat_input_device_tracker.h"

#include <mutex>

namespace mir
{
namespace time
{
class Clock;
}
namespace input
{
class TouchVisualizer;
class CursorListener;
class InputRegion;
class InputDispatcher;
class KeyMapper;
class SeatObserver;

class BasicSeat : public Seat
{
public:
    BasicSeat(std::shared_ptr<InputDispatcher> const& dispatcher,
              std::shared_ptr<TouchVisualizer> const& touch_visualizer,
              std::shared_ptr<CursorListener> const& cursor_listener,
              std::shared_ptr<InputRegion> const& input_region,
              std::shared_ptr<KeyMapper> const& key_mapper,
              std::shared_ptr<time::Clock> const& clock,
              std::shared_ptr<SeatObserver> const& observer);
    // Seat methods:
    void add_device(Device const& device) override;
    void remove_device(Device const& device) override;
    void dispatch_event(MirEvent& event) override;
    geometry::Rectangle get_rectangle_for(Device const& dev) override;
    virtual EventUPtr create_device_state() override;
    virtual void set_confinement_regions(geometry::Rectangles const& regions) override;
    virtual void reset_confinement_regions() override;

    void set_key_state(Device const& dev, std::vector<uint32_t> const& scan_codes) override;
    void set_pointer_state(Device const& dev, MirPointerButtons buttons) override;
    void set_cursor_position(float cursor_x, float cursor_y) override;
private:
    SeatInputDeviceTracker input_state_tracker;
    std::shared_ptr<InputRegion> const input_region;
};
}
}

#endif
