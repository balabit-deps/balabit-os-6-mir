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

#include "mir/test/doubles/stub_buffer.h"
#include "mir/test/doubles/stub_buffer_allocator.h"
#include "mir/test/doubles/mock_event_sink.h"
#include "mir/test/doubles/mock_frame_dropping_policy_factory.h"
#include "mir/test/fake_shared.h"
#include "src/server/compositor/stream.h"
#include "mir/scene/null_surface_observer.h"
#include "mir/frontend/client_buffers.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mir/test/gmock_fixes.h"
using namespace testing;
namespace mf = mir::frontend;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;
namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace geom = mir::geometry;
namespace
{
struct MockSurfaceObserver : mir::scene::NullSurfaceObserver
{
    MOCK_METHOD2(frame_posted, void(int, geom::Size const&));
};

struct StubBufferMap : mf::ClientBuffers
{
    StubBufferMap(mf::EventSink& sink, std::vector<std::shared_ptr<mg::Buffer>>& buffers) :
        buffers{buffers},
        sink{sink}
    {
    }
    mg::BufferID add_buffer(std::shared_ptr<mg::Buffer> const&)
    {
        return mg::BufferID{};
    }
    void remove_buffer(mg::BufferID)
    {
    }
    void with_buffer(mg::BufferID, std::function<void(mg::Buffer&)> const&)
    {
    }
    void receive_buffer(mg::BufferID)
    {
    }
    void send_buffer(mg::BufferID id)
    {
        sink.send_buffer(mf::BufferStreamId{33}, *operator[](id), mg::BufferIpcMsgType::update_msg);
    }
    std::shared_ptr<mg::Buffer>& operator[](mg::BufferID id)
    {
        auto it = std::find_if(buffers.begin(), buffers.end(),
            [id](std::shared_ptr<mg::Buffer> const& b)
            {
                return b->id() == id;
            });
        if (it == buffers.end())
            throw std::logic_error("cannot find buffer in map");
        return *it;
    }
    std::vector<std::shared_ptr<mg::Buffer>>& buffers;
    mf::EventSink& sink;
};

struct Stream : Test
{
    Stream() :
        buffers{
            std::make_shared<mtd::StubBuffer>(initial_size),
            std::make_shared<mtd::StubBuffer>(initial_size),
            std::make_shared<mtd::StubBuffer>(initial_size)}
    {
    }
    
    MOCK_METHOD1(called, void(mg::Buffer&));

    geom::Size initial_size{44,2};
    std::vector<std::shared_ptr<mg::Buffer>> buffers;
    NiceMock<mtd::MockEventSink> mock_sink;
    MirPixelFormat construction_format{mir_pixel_format_rgb_565};
    mtd::MockFrameDroppingPolicyFactory framedrop_factory;
    mc::Stream stream{
        framedrop_factory,
        std::make_unique<StubBufferMap>(mock_sink, buffers), initial_size, construction_format};
};
}

TEST_F(Stream, transitions_from_queuing_to_framedropping)
{
    EXPECT_CALL(mock_sink, send_buffer(_,_,_)).Times(buffers.size() - 1);
    for(auto& buffer : buffers)
        stream.submit_buffer(buffer);
    stream.allow_framedropping(true);

    std::vector<std::shared_ptr<mg::Buffer>> cbuffers;
    while(stream.buffers_ready_for_compositor(this))
        cbuffers.push_back(stream.lock_compositor_buffer(this));
    ASSERT_THAT(cbuffers, SizeIs(1));
    EXPECT_THAT(cbuffers[0]->id(), Eq(buffers.back()->id()));
    Mock::VerifyAndClearExpectations(&mock_sink);
}

TEST_F(Stream, transitions_from_framedropping_to_queuing)
{
    stream.allow_framedropping(true);
    Mock::VerifyAndClearExpectations(&mock_sink);

    EXPECT_CALL(mock_sink, send_buffer(_,_,_)).Times(buffers.size() - 1);
    for(auto& buffer : buffers)
        stream.submit_buffer(buffer);

    stream.allow_framedropping(false);
    for(auto& buffer : buffers)
        stream.submit_buffer(buffer);

    Mock::VerifyAndClearExpectations(&mock_sink);

    std::vector<std::shared_ptr<mg::Buffer>> cbuffers;
    while(stream.buffers_ready_for_compositor(this))
        cbuffers.push_back(stream.lock_compositor_buffer(this));
    EXPECT_THAT(cbuffers, SizeIs(buffers.size()));
}

TEST_F(Stream, indicates_buffers_ready_when_queueing)
{
    for(auto& buffer : buffers)
        stream.submit_buffer(buffer);

    for(auto i = 0u; i < buffers.size(); i++)
    {
        EXPECT_THAT(stream.buffers_ready_for_compositor(this), Eq(1));
        stream.lock_compositor_buffer(this);
    }

    EXPECT_THAT(stream.buffers_ready_for_compositor(this), Eq(0));
}

TEST_F(Stream, indicates_buffers_ready_when_dropping)
{
    stream.allow_framedropping(true);

    for(auto& buffer : buffers)
        stream.submit_buffer(buffer);

    EXPECT_THAT(stream.buffers_ready_for_compositor(this), Eq(1));
    stream.lock_compositor_buffer(this);
    EXPECT_THAT(stream.buffers_ready_for_compositor(this), Eq(0));
}

TEST_F(Stream, tracks_has_buffer)
{
    EXPECT_FALSE(stream.has_submitted_buffer());
    stream.submit_buffer(buffers[0]);
    EXPECT_TRUE(stream.has_submitted_buffer());
}

TEST_F(Stream, calls_observers_after_scheduling_on_submissions)
{
    auto observer = std::make_shared<MockSurfaceObserver>();
    EXPECT_CALL(*observer, frame_posted(1, initial_size));
    stream.add_observer(observer);
    stream.submit_buffer(buffers[0]);
    stream.remove_observer(observer);
    stream.submit_buffer(buffers[0]);
}

TEST_F(Stream, calls_observers_call_doesnt_hold_lock)
{
    auto observer = std::make_shared<MockSurfaceObserver>();
    EXPECT_CALL(*observer, frame_posted(1,_))
        .WillOnce(InvokeWithoutArgs([&]{
            EXPECT_THAT(stream.buffers_ready_for_compositor(this), Eq(1));
            EXPECT_TRUE(stream.has_submitted_buffer());
        }));
    stream.add_observer(observer);
    stream.submit_buffer(buffers[0]);
}

TEST_F(Stream, flattens_queue_out_when_told_to_drop)
{
    for(auto& buffer : buffers)
        stream.submit_buffer(buffer);

    EXPECT_THAT(stream.buffers_ready_for_compositor(this), Eq(1));
    stream.drop_old_buffers();
    stream.lock_compositor_buffer(this);
    EXPECT_THAT(stream.buffers_ready_for_compositor(this), Eq(0));
}

TEST_F(Stream, forces_a_new_buffer_when_told_to_drop_buffers)
{
    int that{0};
    stream.submit_buffer(buffers[0]);
    stream.submit_buffer(buffers[1]);
    stream.submit_buffer(buffers[2]);

    auto a = stream.lock_compositor_buffer(this);
    stream.drop_old_buffers();
    auto b = stream.lock_compositor_buffer(&that);
    auto c = stream.lock_compositor_buffer(this);
    EXPECT_THAT(b->id(), Eq(c->id()));
    EXPECT_THAT(a->id(), Ne(b->id())); 
    EXPECT_THAT(a->id(), Ne(c->id())); 
}

TEST_F(Stream, throws_on_nullptr_submissions)
{
    auto observer = std::make_shared<MockSurfaceObserver>();
    EXPECT_CALL(*observer, frame_posted(_,_)).Times(0);
    stream.add_observer(observer);
    EXPECT_THROW({
        stream.submit_buffer(nullptr);
    }, std::invalid_argument);
    EXPECT_FALSE(stream.has_submitted_buffer());
}

//it doesnt quite make sense that the stream has a size, esp given that there could be different-sized buffers
//in the stream, and the surface has the onscreen size info
TEST_F(Stream, reports_size)
{
    geom::Size new_size{333,139};
    EXPECT_THAT(stream.stream_size(), Eq(initial_size));
    stream.resize(new_size);
    EXPECT_THAT(stream.stream_size(), Eq(new_size));
}

//Likewise, no reason buffers couldn't all be a different pixel format
TEST_F(Stream, reports_format)
{
    EXPECT_THAT(stream.pixel_format(), Eq(construction_format));
}

//confusingly, we have two framedrops. One is swapinterval zero, where old buffers are dropped as quickly as possible.
//In non-framedropping mode, we drop based on a timeout according to a policy, mostly for screen-off scenarios.
//
namespace
{
struct MockPolicy : mc::FrameDroppingPolicy
{
    MOCK_METHOD0(swap_now_blocking, void(void));
    MOCK_METHOD0(swap_unblocked, void(void));
};
}
TEST_F(Stream, timer_starts_when_buffers_run_out_and_framedropping_disabled)
{
    auto policy = std::make_unique<MockPolicy>();
    mtd::FrameDroppingPolicyFactoryMock policy_factory;
    EXPECT_CALL(*policy, swap_now_blocking());
    EXPECT_CALL(policy_factory, create_policy(_))
        .WillOnce(InvokeWithoutArgs([&]{ return std::move(policy); }));
    mc::Stream stream{
        policy_factory,
        std::make_unique<StubBufferMap>(mock_sink, buffers), initial_size, construction_format};
    for (auto const& buffer : buffers)
        stream.associate_buffer(buffer->id());

    for (auto& buffer : buffers)
        stream.submit_buffer(buffer);
}

TEST_F(Stream, timer_does_not_start_when_no_associated_buffers)
{
    auto policy = std::make_unique<MockPolicy>();
    mtd::FrameDroppingPolicyFactoryMock policy_factory;
    EXPECT_CALL(*policy, swap_now_blocking())
        .Times(0);
    EXPECT_CALL(policy_factory, create_policy(_))
        .WillOnce(InvokeWithoutArgs([&]{ return std::move(policy); }));
    mc::Stream stream{
        policy_factory,
        std::make_unique<StubBufferMap>(mock_sink, buffers), initial_size, construction_format};
    for (auto& buffer : buffers)
        stream.submit_buffer(buffer);
}

TEST_F(Stream, timer_stops_if_a_buffer_is_available)
{
    auto policy = std::make_unique<MockPolicy>();
    mtd::FrameDroppingPolicyFactoryMock policy_factory;
    EXPECT_CALL(*policy, swap_now_blocking());
    EXPECT_CALL(*policy, swap_unblocked());
    EXPECT_CALL(policy_factory, create_policy(_))
        .WillOnce(InvokeWithoutArgs([&]{ return std::move(policy); }));
    mc::Stream stream{
        policy_factory,
        std::make_unique<StubBufferMap>(mock_sink, buffers), initial_size, construction_format};
    for (auto const& buffer : buffers)
        stream.associate_buffer(buffer->id());
    for (auto& buffer : buffers)
        stream.submit_buffer(buffer);
    stream.lock_compositor_buffer(this);
}

TEST_F(Stream, triggering_policy_gives_a_buffer_back)
{
    for (auto& buffer : buffers)
        stream.submit_buffer(buffer);
    stream.lock_compositor_buffer(this);

    Mock::VerifyAndClearExpectations(&mock_sink);
    EXPECT_CALL(mock_sink, send_buffer(_,_,_));
    framedrop_factory.trigger_policies();
    Mock::VerifyAndClearExpectations(&mock_sink);
}

TEST_F(Stream, doesnt_drop_the_only_frame_when_arbiter_has_none)
{
    stream.submit_buffer(buffers[0]);
    Mock::VerifyAndClearExpectations(&mock_sink);
    EXPECT_CALL(mock_sink, send_buffer(_,_,_))
        .Times(0);
    framedrop_factory.trigger_policies();
    Mock::VerifyAndClearExpectations(&mock_sink);
}

TEST_F(Stream, doesnt_drop_the_latest_frame_with_a_longer_queue)
{
    stream.submit_buffer(buffers[0]);
    stream.lock_compositor_buffer(this);
    stream.submit_buffer(buffers[1]);
    stream.submit_buffer(buffers[2]);

    Mock::VerifyAndClearExpectations(&mock_sink);
    EXPECT_CALL(mock_sink, send_buffer(_,Ref(*buffers[1]),_))
        .Times(1);
    framedrop_factory.trigger_policies();
    Mock::VerifyAndClearExpectations(&mock_sink);
}

TEST_F(Stream, doesnt_drop_the_latest_frame_with_a_2_buffer_queue)
{
    stream.submit_buffer(buffers[0]);
    stream.lock_compositor_buffer(this);
    stream.submit_buffer(buffers[1]);

    Mock::VerifyAndClearExpectations(&mock_sink);
    EXPECT_CALL(mock_sink, send_buffer(_,Ref(*buffers[1]),_))
        .Times(0);
    framedrop_factory.trigger_policies();
    Mock::VerifyAndClearExpectations(&mock_sink);
}

TEST_F(Stream, returns_buffers_to_client_when_told_to_bring_queue_up_to_date)
{
    stream.submit_buffer(buffers[0]);
    stream.submit_buffer(buffers[1]);
    stream.submit_buffer(buffers[2]);

    Mock::VerifyAndClearExpectations(&mock_sink);
    EXPECT_CALL(mock_sink, send_buffer(_,Ref(*buffers[0]),_));
    EXPECT_CALL(mock_sink, send_buffer(_,Ref(*buffers[1]),_));
    stream.drop_old_buffers();
    Mock::VerifyAndClearExpectations(&mock_sink);
}
