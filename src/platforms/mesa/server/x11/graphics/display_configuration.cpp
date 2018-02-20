/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Cemil Azizoglu <cemil.azizoglu@canonical.com>
 *
 */

#include "display_configuration.h"
#include <boost/throw_exception.hpp>

namespace mg = mir::graphics;
namespace mgx = mg::X;
namespace geom = mir::geometry;

mg::DisplayConfigurationOutputId const mgx::DisplayConfiguration::the_output_id{1};

mgx::DisplayConfiguration::DisplayConfiguration(MirPixelFormat pf, geom::Size const pixels, geom::Size const size, const float scale, MirOrientation orientation) :
    configuration{
            the_output_id,
            mg::DisplayConfigurationCardId{0},
            mg::DisplayConfigurationOutputType::unknown,
            {pf},
            //TODO: query fps
            {mg::DisplayConfigurationMode{pixels, 60.0}},
            0,
            size,
            true,
            true,
            geom::Point{0, 0},
            0,
            pf,
            mir_power_mode_on,
            orientation,
            scale,
            mir_form_factor_monitor,
            mir_subpixel_arrangement_unknown,
            {},
            mir_output_gamma_unsupported,
            {}},
    card{mg::DisplayConfigurationCardId{0}, 1}
{
}

mgx::DisplayConfiguration::DisplayConfiguration(DisplayConfiguration const& other)
    : mg::DisplayConfiguration(),
      configuration(other.configuration),
      card(other.card)
{
}

void mgx::DisplayConfiguration::for_each_card(std::function<void(mg::DisplayConfigurationCard const&)> f) const
{
    f(card);
}

void mgx::DisplayConfiguration::for_each_output(std::function<void(mg::DisplayConfigurationOutput const&)> f) const
{
    f(configuration);
}

void mgx::DisplayConfiguration::for_each_output(std::function<void(mg::UserDisplayConfigurationOutput&)> f)
{
    mg::UserDisplayConfigurationOutput user(configuration);
    f(user);
}

std::unique_ptr<mg::DisplayConfiguration> mgx::DisplayConfiguration::clone() const
{
    return std::make_unique<mgx::DisplayConfiguration>(*this);
}
