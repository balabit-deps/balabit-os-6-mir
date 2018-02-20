/*
 * Copyright © 2016 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "error_chain.h"
#include <boost/throw_exception.hpp>

namespace mcl = mir::client;
namespace geom = mir::geometry;

mcl::ErrorChain::ErrorChain(
    MirConnection* connection,
    int id,
    std::string const& error_msg) :
    connection_(connection),
    stream_id(id),
    error(error_msg)
{
}

char const* mcl::ErrorChain::error_msg() const
{
    return error.c_str();
}

MirConnection* mcl::ErrorChain::connection() const
{
    return connection_;
}

int mcl::ErrorChain::rpc_id() const
{
    return stream_id;
}

void mcl::ErrorChain::submit_buffer(MirBuffer*)
{
    BOOST_THROW_EXCEPTION(std::logic_error("Cannot submit: invalid MirPresentationChain"));
}

void mcl::ErrorChain::set_queueing_mode()
{
    BOOST_THROW_EXCEPTION(std::logic_error("Cannot set mode: invalid MirPresentationChain"));
}

void mcl::ErrorChain::set_dropping_mode()
{
    BOOST_THROW_EXCEPTION(std::logic_error("Cannot set mode: invalid MirPresentationChain"));
}
