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

#include "mir/graphics/buffer.h"
#include "mir/frontend/buffer_sink.h"
#include "buffer_map.h"
#include <boost/throw_exception.hpp>
#include <algorithm>

namespace mc = mir::compositor;
namespace mf = mir::frontend;
namespace mg = mir::graphics;

namespace mir
{
namespace compositor
{
enum class BufferMap::Owner
{
    server,
    client
};
}
}

mc::BufferMap::BufferMap(std::shared_ptr<mf::BufferSink> const& sink) :
    sink(sink)
{
}

mg::BufferID mc::BufferMap::add_buffer(std::shared_ptr<mg::Buffer> const& buffer)
{
    try
    {
        std::unique_lock<decltype(mutex)> lk(mutex);
        buffers[buffer->id()] = {buffer, Owner::client};
        if (auto s = sink.lock())
            s->add_buffer(*buffer);
        return buffer->id();
    } catch (std::exception& e)
    {
        if (auto s = sink.lock())
            s->error_buffer(buffer->size(), buffer->pixel_format(), e.what());
        throw;
    }
}

void mc::BufferMap::remove_buffer(mg::BufferID id)
{
    std::unique_lock<decltype(mutex)> lk(mutex);
    auto it = checked_buffers_find(id, lk);
    if (auto s = sink.lock())
        s->remove_buffer(*it->second.buffer);
    buffers.erase(it); 
}

void mc::BufferMap::send_buffer(mg::BufferID id)
{
    std::unique_lock<decltype(mutex)> lk(mutex);
    auto it = buffers.find(id);
    if (it != buffers.end())
    {
        auto buffer = it->second.buffer;
        it->second.owner = Owner::client;
        lk.unlock();
        if (auto s = sink.lock())
            s->update_buffer(*buffer);
    }
}

void mc::BufferMap::receive_buffer(graphics::BufferID id)
{
    std::unique_lock<decltype(mutex)> lk(mutex);
    auto it = buffers.find(id);
    if (it != buffers.end())
        it->second.owner = Owner::server;
}

std::shared_ptr<mg::Buffer>& mc::BufferMap::operator[](mg::BufferID id)
{
    std::unique_lock<decltype(mutex)> lk(mutex);
    return checked_buffers_find(id, lk)->second.buffer;
}

mc::BufferMap::Map::iterator mc::BufferMap::checked_buffers_find(
    mg::BufferID id, std::unique_lock<std::mutex> const&)
{
    auto it = buffers.find(id);
    if (it == buffers.end())
        BOOST_THROW_EXCEPTION(std::logic_error("cannot find buffer by id"));
    return it;
}
