/*
 * Copyright © 2016 Canonical Ltd.
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

#include "mir/test/doubles/mock_client_buffer.h"
#include "src/client/buffer.h"
#include "mir_protobuf.pb.h"
#include <gtest/gtest.h>

namespace mcl = mir::client;
namespace mtd = mir::test::doubles;
namespace geom = mir::geometry;
using namespace testing;

namespace
{

void buffer_callback(MirBuffer*, void* call_count_context)
{
    if (call_count_context)
        (*static_cast<int*>(call_count_context))++;
}

struct MirBufferTest : Test
{
    MirBufferTest()
    {
        ON_CALL(*mock_client_buffer, size())
            .WillByDefault(Return(geom::Size{width, height}));
        ON_CALL(*mock_client_buffer, pixel_format())
            .WillByDefault(Return(format)); 
    }
    MirGraphicsRegion region;
    int buffer_id { 32 };
    geom::Width width { 190 };
    geom::Height height { 119 };
    geom::Stride stride { 2211 };
    MirBufferUsage usage { mir_buffer_usage_hardware };
    MirPixelFormat format { mir_pixel_format_abgr_8888 };
    std::shared_ptr<char> vaddr { std::make_shared<char>('\0') };
    MirBufferCallback cb { buffer_callback };
    std::shared_ptr<mtd::MockClientBuffer> const mock_client_buffer {
        std::make_shared<NiceMock<mtd::MockClientBuffer>>() };
    std::chrono::nanoseconds timeout { 101 };
    MirBufferPackage update_message;
};

}

TEST_F(MirBufferTest, fills_region_with_correct_info_when_securing)
{
    auto region = std::make_shared<mcl::MemoryRegion>(
        mcl::MemoryRegion{width, height, stride, format, vaddr});
    EXPECT_CALL(*mock_client_buffer, secure_for_cpu_write())
        .WillOnce(Return(region));

    mcl::Buffer buffer(cb, nullptr, buffer_id, mock_client_buffer, nullptr, usage);
    auto out_region = buffer.map_region();

    EXPECT_THAT(out_region.width, Eq(width.as_int()));
    EXPECT_THAT(out_region.height, Eq(height.as_int()));
    EXPECT_THAT(out_region.stride, Eq(stride.as_int()));
    EXPECT_THAT(out_region.pixel_format, Eq(format));
    EXPECT_THAT(out_region.vaddr, Eq(vaddr.get()));
}

TEST_F(MirBufferTest, notifies_client_buffer_when_submitted)
{
    EXPECT_CALL(*mock_client_buffer, mark_as_submitted());
    mcl::Buffer buffer(cb, nullptr, buffer_id, mock_client_buffer, nullptr, usage);
    buffer.received();
    buffer.submitted();
}

TEST_F(MirBufferTest, increases_age)
{
    EXPECT_CALL(*mock_client_buffer, increment_age());
    mcl::Buffer buffer(cb, nullptr, buffer_id, mock_client_buffer, nullptr, usage);
    buffer.increment_age();
}

TEST_F(MirBufferTest, releases_buffer_refcount_implicitly_on_submit)
{
    auto region = std::make_shared<mcl::MemoryRegion>(
        mcl::MemoryRegion{width, height, stride, format, vaddr});
    EXPECT_CALL(*mock_client_buffer, secure_for_cpu_write())
        .WillOnce(Return(region));

    mcl::Buffer buffer(cb, nullptr, buffer_id, mock_client_buffer, nullptr, usage);
    buffer.received();

    auto use_count_before = region.use_count();
    buffer.map_region();

    EXPECT_THAT(use_count_before, Lt(region.use_count()));

    buffer.submitted();

    EXPECT_THAT(use_count_before, Eq(region.use_count()));
}

TEST_F(MirBufferTest, callback_called_when_available_from_creation)
{
    int call_count = 0;
    mcl::Buffer buffer(cb, &call_count, buffer_id, mock_client_buffer, nullptr, usage);
    buffer.received();
    EXPECT_THAT(call_count, Eq(1));
}

TEST_F(MirBufferTest, callback_called_when_available_from_server_return)
{
    int call_count = 0;
    mcl::Buffer buffer(cb, &call_count, buffer_id, mock_client_buffer, nullptr, usage);
    buffer.received(update_message);
    EXPECT_THAT(call_count, Eq(1));
}

TEST_F(MirBufferTest, callback_called_after_change_when_available_from_creation)
{
    int call_count = 0;
    mcl::Buffer buffer(cb, nullptr, buffer_id, mock_client_buffer, nullptr, usage);
    buffer.set_callback(cb, &call_count); 
    buffer.received();
    EXPECT_THAT(call_count, Eq(1));
}

TEST_F(MirBufferTest, callback_called_after_change_when_available_from_server_return)
{
    int call_count = 0;
    mcl::Buffer buffer(cb, nullptr, buffer_id, mock_client_buffer, nullptr, usage);
    buffer.set_callback(cb, &call_count); 
    buffer.received(update_message);
    EXPECT_THAT(call_count, Eq(1));
}

TEST_F(MirBufferTest, updates_package_when_server_returns)
{
    EXPECT_CALL(*mock_client_buffer, update_from(Ref(update_message)));
    mcl::Buffer buffer(cb, nullptr, buffer_id, mock_client_buffer, nullptr, usage);
    buffer.received(update_message);
}

TEST_F(MirBufferTest, submitting_unowned_buffer_throws)
{
    mcl::Buffer buffer(cb, nullptr, buffer_id, mock_client_buffer, nullptr, usage);
    EXPECT_THROW({ 
        buffer.submitted();
    }, std::logic_error);
}

TEST_F(MirBufferTest, returns_correct_format)
{
    mcl::Buffer buffer(cb, nullptr, buffer_id, mock_client_buffer, nullptr, usage);
    EXPECT_THAT(buffer.pixel_format(), Eq(format));
}

TEST_F(MirBufferTest, returns_correct_usage)
{
    mcl::Buffer buffer(cb, nullptr, buffer_id, mock_client_buffer, nullptr, usage);
    EXPECT_THAT(buffer.buffer_usage(), Eq(usage));
}

TEST_F(MirBufferTest, returns_correct_size)
{
    mcl::Buffer buffer(cb, nullptr, buffer_id, mock_client_buffer, nullptr, usage);
    EXPECT_THAT(buffer.size().height, Eq(height));
    EXPECT_THAT(buffer.size().width, Eq(width));
}
