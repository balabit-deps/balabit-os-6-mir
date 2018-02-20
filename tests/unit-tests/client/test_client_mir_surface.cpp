/*
 * Copyright © 2012 Canonical Ltd.
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

// TODO: There's a lot to suggest (usage of real connection most prevalent)
// that this is perhaps a set of integration tests. But moving it there conflicts
// with test_mirsurface.cpp. Client MirWindow testing probably needs to be reviewed

#include "mir_protobuf.pb.h"
#include "mir_toolkit/mir_client_library.h"
#include "mir/client_buffer.h"
#include "mir/client_buffer_factory.h"
#include "mir/client_platform.h"
#include "mir/client_platform_factory.h"
#include "src/client/mir_surface.h"
#include "src/client/mir_connection.h"
#include "src/client/default_connection_configuration.h"
#include "src/client/rpc/null_rpc_report.h"
#include "src/client/rpc/mir_display_server.h"
#include "src/client/rpc/mir_basic_rpc_channel.h"
#include "src/client/connection_surface_map.h"
#include "mir/dispatch/dispatchable.h"
#include "mir/dispatch/threaded_dispatcher.h"
#include "mir/events/event_builders.h"

#include "mir/frontend/connector.h"
#include "mir/input/input_platform.h"

#include "mir/test/test_protobuf_server.h"
#include "mir/test/stub_server_tool.h"
#include "mir/test/gmock_fixes.h"
#include "mir/test/fake_shared.h"
#include "mir/test/pipe.h"
#include "mir/test/signal.h"
#include "mir/test/doubles/stub_client_buffer.h"
#include "mir/test/test_dispatchable.h"

#include <boost/throw_exception.hpp>
#include "mir/test/doubles/stub_client_buffer_factory.h"
#include "mir/test/doubles/stub_buffer_stream.h"
#include "mir/test/doubles/mock_mir_buffer_stream.h"

#include "mir_test_framework/stub_client_platform_factory.h"

#include <cstring>
#include <map>
#include <atomic>

#include <fcntl.h>

namespace mcl = mir::client;
namespace mclr = mir::client::rpc;
namespace mircv = mir::input::receiver;
namespace mg = mir::graphics;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd= mir::test::doubles;

namespace
{

struct MockServerPackageGenerator : public mt::StubServerTool
{
    MockServerPackageGenerator()
        : server_package(),
          width_sent(891), height_sent(458),
          pf_sent(mir_pixel_format_abgr_8888),
          stride_sent(66),
          input_fd(open("/dev/null", O_APPEND)),
          global_buffer_id(0)
    {
    }

    ~MockServerPackageGenerator()
    {
        close(input_fd);
        close_server_package_fds();
    }

    void create_surface(
        mir::protobuf::SurfaceParameters const* request,
        mir::protobuf::Surface* response,
        google::protobuf::Closure* done) override
    {
        create_surface_response(response);
        {
            std::lock_guard<std::mutex> lock(guard);
            surf_name = request->surface_name();
        }

        done->Run();
    }

    void release_surface(
        const mir::protobuf::SurfaceId*,
        mir::protobuf::Void*,
        google::protobuf::Closure* done) override
    {
        done->Run();
    }

    void modify_surface(
        const mir::protobuf::SurfaceModifications*, 
        mir::protobuf::Void*,
        google::protobuf::Closure* done) override
    {
        done->Run();
    }

    MirBufferPackage server_package;
    int width_sent;
    int height_sent;
    int pf_sent;
    int stride_sent;

    static std::map<int, int> sent_surface_attributes;

    void generate_unique_buffer()
    {
        global_buffer_id++;

        close_server_package_fds();

        int num_fd = 2, num_data = 8;

        server_package.fd_items = num_fd;
        for (auto i = 0; i < num_fd; i++)
        {
            server_package.fd[i] = open("/dev/null", O_APPEND);
        }

        server_package.data_items = num_data;
        for (auto i = 0; i < num_data; i++)
        {
            server_package.data[i] = (global_buffer_id + i) * 2;
        }

        server_package.stride = stride_sent;
        server_package.width = width_sent;
        server_package.height = height_sent;
    }

    void create_buffer_response(mir::protobuf::Buffer* response)
    {
        generate_unique_buffer();

        response->set_buffer_id(global_buffer_id);

        /* assemble buffers */
        response->set_fds_on_side_channel(server_package.fd_items);
        for (int i=0; i< server_package.data_items; i++)
        {
            response->add_data(server_package.data[i]);
        }
        for (int i=0; i< server_package.fd_items; i++)
        {
            response->add_fd(server_package.fd[i]);
        }

        response->set_stride(server_package.stride);
        response->set_width(server_package.width);
        response->set_height(server_package.height);
    }
    
    void create_surface_response(mir::protobuf::Surface* response)
    {
        unsigned int const id = 2;

        response->set_fds_on_side_channel(1);

        response->mutable_id()->set_value(id);
        response->set_width(width_sent);
        response->set_height(height_sent);
        response->set_pixel_format(pf_sent);
        response->add_fd(input_fd);
        
        for (auto const& kv : sent_surface_attributes)
        {
            auto setting = response->add_attributes();
            setting->mutable_surfaceid()->set_value(id);
            setting->set_attrib(kv.first);
            setting->set_ivalue(kv.second);            
        }

        create_buffer_response(response->mutable_buffer());
    }

    void close_server_package_fds()
    {
        for (int i = 0; i < server_package.fd_items; i++)
            close(server_package.fd[i]);
    }

    int input_fd;
    int global_buffer_id;
};

std::map<int, int> MockServerPackageGenerator::sent_surface_attributes = {
    { mir_window_attrib_type, mir_window_type_normal },
    { mir_window_attrib_state, mir_window_state_restored },
    { mir_window_attrib_swapinterval, 1 },
    { mir_window_attrib_focus, mir_window_focus_state_focused },
    { mir_window_attrib_dpi, 19 },
    { mir_window_attrib_visibility, mir_window_visibility_exposed },
    { mir_window_attrib_preferred_orientation, mir_orientation_mode_any }
};

struct StubClientInputPlatform : public mircv::InputPlatform
{
    std::shared_ptr<mir::dispatch::Dispatchable> create_input_receiver(int /* fd */, std::shared_ptr<mircv::XKBMapper> const&, std::function<void(MirEvent*)> const& /* callback */)
    {
        return std::shared_ptr<mir::dispatch::Dispatchable>();
    }
};

struct MockClientInputPlatform : public mircv::InputPlatform
{
    MOCK_METHOD3(create_input_receiver, std::shared_ptr<mir::dispatch::Dispatchable>(int, std::shared_ptr<mircv::XKBMapper> const&, std::function<void(MirEvent*)> const&));
};

class TestConnectionConfiguration : public mcl::DefaultConnectionConfiguration
{
public:
    TestConnectionConfiguration()
        : DefaultConnectionConfiguration("./test_socket_surface")
    {
    }

    std::shared_ptr<mcl::rpc::RpcReport> the_rpc_report() override
    {
        return std::make_shared<mcl::rpc::NullRpcReport>();
    }

    std::shared_ptr<mcl::ClientPlatformFactory> the_client_platform_factory() override
    {
        return std::make_shared<mir_test_framework::StubClientPlatformFactory>();
    }
};

struct FakeRpcChannel : public mir::client::rpc::MirBasicRpcChannel
{
    void call_method(
        std::string const&,
        google::protobuf::MessageLite const*,
        google::protobuf::MessageLite*,
        google::protobuf::Closure* closure) override
    {
        delete closure;
    }
    void discard_future_calls() override
    {
    }
    void wait_for_outstanding_calls() override
    {
    }
};

void null_connected_callback(MirConnection* /*connection*/, void * /*client_context*/)
{
}

void null_event_callback(MirWindow*, MirEvent const*, void*)
{
}

void null_lifecycle_callback(MirConnection*, MirLifecycleState, void*)
{
}

struct MirClientSurfaceTest : public testing::Test
{
    MirClientSurfaceTest()
    {
        mock_server_tool->create_surface_response(&surface_proto);
        ON_CALL(*stub_buffer_stream, swap_interval()).WillByDefault(testing::Return(1));
        start_test_server();
        connect_to_test_server();
    }

    ~MirClientSurfaceTest()
    {
        // Clear the lifecycle callback in order not to get SIGHUP by the
        // default lifecycle handler during connection teardown
        connection->register_lifecycle_event_callback(null_lifecycle_callback, nullptr);
    }

    void start_test_server()
    {
        // In case an earlier test left a stray file
        std::remove("./test_socket_surface");
        test_server = std::make_shared<mt::TestProtobufServer>("./test_socket_surface", mock_server_tool);
        test_server->comm->start();
    }

    void connect_to_test_server()
    {
        mir::protobuf::ConnectParameters connect_parameters;
        connect_parameters.set_application_name("test");

        TestConnectionConfiguration conf;
        surface_map = conf.the_surface_map();
        connection = std::make_shared<MirConnection>(conf);
        MirWaitHandle* wait_handle = connection->connect("MirClientSurfaceTest",
                                                         null_connected_callback, 0);
        wait_handle->wait_for_all();
        client_comm_channel = std::make_shared<mclr::DisplayServer>(conf.the_rpc_channel());
    }

    std::shared_ptr<MirWindow> create_surface_with(mclr::DisplayServer& server_stub)
    {
        return std::make_shared<MirWindow>(
            connection.get(),
            server_stub,
            nullptr,
            stub_buffer_stream,
            input_platform,
            spec,
            surface_proto,
            wh);
    }

    std::shared_ptr<MirWindow> create_surface_with(
        mclr::DisplayServer& server_stub,
        std::shared_ptr<MirBufferStream> const& buffer_stream)
    {
        return std::make_shared<MirWindow>(
            connection.get(),
            server_stub,
            nullptr,
            buffer_stream,
            input_platform,
            spec,
            surface_proto,
            wh);
    }

    std::shared_ptr<MirWindow> create_and_wait_for_surface_with(
        mclr::DisplayServer& server_stub)
    {
        auto surface = create_surface_with(server_stub);
        return surface;
    }

    std::shared_ptr<MirWindow> create_and_wait_for_surface_with(
        mclr::DisplayServer& server_stub,
        std::shared_ptr<MirBufferStream> const& buffer_stream)
    {
        auto surface = create_surface_with(server_stub, buffer_stream);
        return surface;
    }

    std::shared_ptr<MirConnection> connection;

    MirWindowSpec const spec{nullptr, 33, 45, mir_pixel_format_abgr_8888};
    std::shared_ptr<mtd::MockMirBufferStream> stub_buffer_stream{std::make_shared<mtd::MockMirBufferStream>()};
    std::shared_ptr<StubClientInputPlatform> const input_platform =
        std::make_shared<StubClientInputPlatform>();
    std::shared_ptr<MockServerPackageGenerator> const mock_server_tool =
        std::make_shared<MockServerPackageGenerator>();

    std::shared_ptr<mcl::ConnectionSurfaceMap> surface_map;
    std::shared_ptr<mt::TestProtobufServer> test_server;
    std::shared_ptr<mclr::DisplayServer> client_comm_channel;
    mir::protobuf::Surface surface_proto;
    std::shared_ptr<MirWaitHandle> const wh{std::make_shared<MirWaitHandle>()};

    std::chrono::milliseconds const pause_time{10};
};

}

TEST_F(MirClientSurfaceTest, attributes_set_on_surface_creation)
{
    using namespace testing;

    auto surface = create_and_wait_for_surface_with(*client_comm_channel);
    
    for (int i = 0; i < mir_window_attribs; i++)
    {
        EXPECT_EQ(MockServerPackageGenerator::sent_surface_attributes[i], surface->attrib(static_cast<MirWindowAttrib>(i)));
    }
}

TEST_F(MirClientSurfaceTest, creates_input_thread_with_input_dispatcher_when_delegate_specified)
{
    using namespace ::testing;

    auto dispatched = std::make_shared<mt::Signal>();

    auto mock_input_dispatcher = std::make_shared<mt::TestDispatchable>([dispatched]() { dispatched->raise(); });
    auto mock_input_platform = std::make_shared<MockClientInputPlatform>();

    EXPECT_CALL(*mock_input_platform, create_input_receiver(_, _, _)).Times(1)
        .WillOnce(Return(mock_input_dispatcher));

    MirWindow surface{connection.get(), *client_comm_channel, nullptr,
        stub_buffer_stream, mock_input_platform, spec, surface_proto, wh};
    surface.set_event_handler(null_event_callback, nullptr);

    mock_input_dispatcher->trigger();

    EXPECT_TRUE(dispatched->wait_for(std::chrono::seconds{5}));
}

TEST_F(MirClientSurfaceTest, adopts_the_default_stream)
{
    using namespace ::testing;

    auto mock_input_platform = std::make_shared<MockClientInputPlatform>();
    auto mock_stream = std::make_shared<mtd::MockMirBufferStream>(); 

    MirWindow* adopted_by = nullptr;
    MirWindow* unadopted_by = nullptr;
    EXPECT_CALL(*mock_stream, adopted_by(_))
        .WillOnce(SaveArg<0>(&adopted_by));
    EXPECT_CALL(*mock_stream, unadopted_by(_))
        .WillOnce(SaveArg<0>(&unadopted_by));

    {
        MirWindow win{connection.get(), *client_comm_channel, nullptr,
            mock_stream, mock_input_platform, spec, surface_proto, wh};
        EXPECT_EQ(&win,    adopted_by);
        EXPECT_EQ(nullptr, unadopted_by);
    }

    EXPECT_NE(nullptr,    unadopted_by);
    EXPECT_EQ(adopted_by, unadopted_by);
}

TEST_F(MirClientSurfaceTest, adopts_custom_streams_set_before_construction)
{   // Regression test for nested server bugs LP: #1661128, LP: #1661072
    using namespace ::testing;

    mir::frontend::BufferStreamId const mock_stream_id(777888);
    auto mock_input_platform = std::make_shared<MockClientInputPlatform>();
    auto mock_stream = std::make_shared<mtd::MockMirBufferStream>(); 

    MirWindow* adopted_by = nullptr;
    MirWindow* unadopted_by = nullptr;
    ON_CALL(*mock_stream, rpc_id())
        .WillByDefault(Return(mock_stream_id));
    EXPECT_CALL(*mock_stream, adopted_by(_))
        .WillOnce(SaveArg<0>(&adopted_by));
    EXPECT_CALL(*mock_stream, unadopted_by(_))
        .WillOnce(SaveArg<0>(&unadopted_by));

    surface_map->insert(mock_stream_id, mock_stream);

    {
        MirWindowSpec spec;
        std::vector<ContentInfo> replacements
        {
            {geom::Displacement{0,0}, mock_stream_id.as_value(), geom::Size{1,1}}
        };
        spec.streams = replacements;
        MirWindow win{connection.get(), *client_comm_channel, nullptr,
            nullptr, mock_input_platform, spec, surface_proto, wh};
        ASSERT_EQ(&win,    adopted_by);
        ASSERT_EQ(nullptr, unadopted_by);
    }

    EXPECT_NE(nullptr,    unadopted_by);
    EXPECT_EQ(adopted_by, unadopted_by);
}

TEST_F(MirClientSurfaceTest, adopts_custom_streams_set_after_construction)
{
    using namespace testing;

    auto mock_input_platform = std::make_shared<MockClientInputPlatform>();

    mir::frontend::BufferStreamId const mock_old_stream_id(11);
    auto mock_old_stream = std::make_shared<mtd::MockMirBufferStream>(); 
    MirWindow* old_adopted_by = nullptr;
    MirWindow* old_unadopted_by = nullptr;
    EXPECT_CALL(*mock_old_stream, adopted_by(_))
        .WillOnce(SaveArg<0>(&old_adopted_by));
    EXPECT_CALL(*mock_old_stream, unadopted_by(_))
        .WillOnce(SaveArg<0>(&old_unadopted_by));
    ON_CALL(*mock_old_stream, rpc_id())
        .WillByDefault(Return(mock_old_stream_id));

    mir::frontend::BufferStreamId const mock_new_stream_id(22);
    auto mock_new_stream = std::make_shared<mtd::MockMirBufferStream>(); 
    MirWindow* new_adopted_by = nullptr;
    MirWindow* new_unadopted_by = nullptr;
    EXPECT_CALL(*mock_new_stream, adopted_by(_))
        .WillOnce(SaveArg<0>(&new_adopted_by));
    EXPECT_CALL(*mock_new_stream, unadopted_by(_))
        .WillOnce(SaveArg<0>(&new_unadopted_by));
    ON_CALL(*mock_new_stream, rpc_id())
        .WillByDefault(Return(mock_new_stream_id));

    surface_map->insert(mock_old_stream_id, mock_old_stream);
    surface_map->insert(mock_new_stream_id, mock_new_stream);
    {
        MirWindow win{connection.get(), *client_comm_channel, nullptr,
            mock_old_stream, mock_input_platform, spec, surface_proto, wh};
    
        EXPECT_EQ(&win,    old_adopted_by);
        EXPECT_EQ(nullptr, old_unadopted_by);
        EXPECT_EQ(nullptr, new_adopted_by);
        EXPECT_EQ(nullptr, new_unadopted_by);

        MirWindowSpec spec;
        std::vector<ContentInfo> replacements
        {
            {geom::Displacement{0,0}, mock_new_stream_id.as_value(), geom::Size{1,1}}
        };
        spec.streams = replacements;
        win.modify(spec)->wait_for_all();

        EXPECT_EQ(&win,    old_unadopted_by);
        EXPECT_EQ(&win,    new_adopted_by);
        EXPECT_EQ(nullptr, new_unadopted_by);
    }
    EXPECT_EQ(new_adopted_by, new_unadopted_by);
    surface_map->erase(mock_old_stream_id);
    surface_map->erase(mock_new_stream_id);
}

TEST_F(MirClientSurfaceTest, replacing_delegate_with_nullptr_prevents_further_dispatch)
{
    using namespace ::testing;

    auto dispatched = std::make_shared<mt::Signal>();

    auto mock_input_dispatcher = std::make_shared<mt::TestDispatchable>([dispatched]() { dispatched->raise(); });
    auto mock_input_platform = std::make_shared<MockClientInputPlatform>();

    EXPECT_CALL(*mock_input_platform, create_input_receiver(_, _, _)).Times(1)
        .WillOnce(Return(mock_input_dispatcher));

    MirWindow surface{connection.get(), *client_comm_channel, nullptr,
        stub_buffer_stream, mock_input_platform, spec, surface_proto, wh};
    surface.set_event_handler(null_event_callback, nullptr);

    // Should now not get dispatched.
    surface.set_event_handler(nullptr, nullptr);

    mock_input_dispatcher->trigger();

    EXPECT_FALSE(dispatched->wait_for(std::chrono::seconds{1}));
}


TEST_F(MirClientSurfaceTest, does_not_create_input_dispatcher_when_no_delegate_specified)
{
    using namespace ::testing;

    auto mock_input_platform = std::make_shared<MockClientInputPlatform>();

    EXPECT_CALL(*mock_input_platform, create_input_receiver(_, _, _)).Times(0);

    MirWindow surface{connection.get(), *client_comm_channel, nullptr,
        stub_buffer_stream, mock_input_platform, spec, surface_proto, wh};
}

TEST_F(MirClientSurfaceTest, valid_surface_is_valid)
{
    auto const surface = create_and_wait_for_surface_with(*client_comm_channel);

    EXPECT_TRUE(MirWindow::is_valid(surface.get()));
}

TEST_F(MirClientSurfaceTest, configure_cursor_wait_handle_really_blocks)
{
    using namespace testing;

    FakeRpcChannel fake_channel;
    mclr::DisplayServer unresponsive_server{mt::fake_shared(fake_channel)};

    auto const surface = create_surface_with(unresponsive_server);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    auto cursor_config = mir_cursor_configuration_from_name(mir_default_cursor_name);
#pragma GCC diagnostic pop
    auto cursor_wait_handle = surface->configure_cursor(cursor_config);

    auto expected_end = std::chrono::steady_clock::now() + pause_time;
    cursor_wait_handle->wait_for_pending(pause_time);

    EXPECT_GE(std::chrono::steady_clock::now(), expected_end);

    mir_cursor_configuration_destroy(cursor_config);
}

TEST_F(MirClientSurfaceTest, configure_wait_handle_really_blocks)
{
    using namespace testing;

    FakeRpcChannel fake_channel;
    mclr::DisplayServer unresponsive_server{mt::fake_shared(fake_channel)};

    auto const surface = create_surface_with(unresponsive_server);

    auto configure_wait_handle = surface->configure(mir_window_attrib_dpi, 100);

    auto expected_end = std::chrono::steady_clock::now() + pause_time;
    configure_wait_handle->wait_for_pending(pause_time);

    EXPECT_GE(std::chrono::steady_clock::now(), expected_end);
}

TEST_F(MirClientSurfaceTest, resizes_streams_and_calls_callback_if_no_customized_streams)
{
    using namespace testing;
    auto mock_stream = std::make_shared<mtd::MockMirBufferStream>(); 
    mir::frontend::BufferStreamId const mock_stream_id(2);
    auto mock_input_platform = std::make_shared<NiceMock<MockClientInputPlatform>>();
    ON_CALL(*mock_input_platform, create_input_receiver(_,_,_))
        .WillByDefault(Return(std::make_shared<mt::TestDispatchable>([]{})));
    ON_CALL(*mock_stream, rpc_id()).WillByDefault(Return(mock_stream_id));

    geom::Size size(120, 124);
    EXPECT_CALL(*mock_stream, set_size(size));
    auto ev = mir::events::make_event(mir::frontend::SurfaceId(2), size);

    MirWindow surface{connection.get(), *client_comm_channel, nullptr,
        mock_stream, mock_input_platform, spec, surface_proto, wh};
    surface_map->insert(mock_stream_id, mock_stream);
    surface.handle_event(*ev);
    surface_map->erase(mock_stream_id);
}

TEST_F(MirClientSurfaceTest, resizes_streams_and_calls_callback_if_customized_streams)
{
    using namespace testing;
    auto mock_stream = std::make_shared<NiceMock<mtd::MockMirBufferStream>>();
    mir::frontend::BufferStreamId const mock_stream_id(2);
    auto mock_input_platform = std::make_shared<NiceMock<MockClientInputPlatform>>();
    ON_CALL(*mock_stream, rpc_id()).WillByDefault(Return(mock_stream_id));
    ON_CALL(*mock_input_platform, create_input_receiver(_,_,_))
        .WillByDefault(Return(std::make_shared<mt::TestDispatchable>([]{})));

    geom::Size size(120, 124);
    EXPECT_CALL(*mock_stream, set_size(size)).Times(0);
    auto ev = mir::events::make_event(mir::frontend::SurfaceId(2), size);
    MirWindow surface{connection.get(), *client_comm_channel, nullptr,
        mock_stream, mock_input_platform, spec, surface_proto, wh};

    MirWindowSpec spec;
    std::vector<ContentInfo> info =
        { ContentInfo{ geom::Displacement{0,0}, 2, geom::Size{1,1}} };
    spec.streams = info;
    surface_map->insert(mock_stream_id, mock_stream);
    surface.modify(spec)->wait_for_all();
    surface.handle_event(*ev);
    surface_map->erase(mock_stream_id);
}

TEST_F(MirClientSurfaceTest, parameters_are_unhooked_from_stream_sizes)
{
    using namespace testing;
    auto mock_stream = std::make_shared<mtd::MockMirBufferStream>(); 
    auto mock_input_platform = std::make_shared<NiceMock<MockClientInputPlatform>>();
    ON_CALL(*mock_input_platform, create_input_receiver(_,_,_))
        .WillByDefault(Return(std::make_shared<mt::TestDispatchable>([]{})));
    ON_CALL(*mock_stream, rpc_id()).WillByDefault(Return(mir::frontend::BufferStreamId(2)));
    geom::Size size(120, 124);
    EXPECT_CALL(*mock_stream, set_size(size));
    auto ev = mir::events::make_event(mir::frontend::SurfaceId(2), size);

    surface_proto.set_width(size.width.as_int());
    surface_proto.set_height(size.height.as_int());

    MirWindow surface{connection.get(), *client_comm_channel, nullptr,
        mock_stream, mock_input_platform, spec, surface_proto, wh};

    auto params = surface.get_parameters();
    EXPECT_THAT(params.width, Eq(size.width.as_int())); 
    EXPECT_THAT(params.height, Eq(size.height.as_int()));
    surface.handle_event(*ev);
    params = surface.get_parameters();
    EXPECT_THAT(params.width, Eq(size.width.as_int())); 
    EXPECT_THAT(params.height, Eq(size.height.as_int()));
}

//LP: #1612256
TEST_F(MirClientSurfaceTest, initial_sizes_are_from_response_from_server)
{
    using namespace testing;
    auto mock_stream = std::make_shared<mtd::MockMirBufferStream>(); 
    auto mock_input_platform = std::make_shared<NiceMock<MockClientInputPlatform>>();
    ON_CALL(*mock_input_platform, create_input_receiver(_,_,_))
        .WillByDefault(Return(std::make_shared<mt::TestDispatchable>([]{})));
    ON_CALL(*mock_stream, rpc_id()).WillByDefault(Return(mir::frontend::BufferStreamId(2)));
    geom::Size size(120, 124);

    surface_proto.set_width(size.width.as_int());
    surface_proto.set_height(size.height.as_int());
    MirWindow surface{connection.get(), *client_comm_channel, nullptr,
        mock_stream, mock_input_platform, spec, surface_proto, wh};

    auto params = surface.get_parameters();
    EXPECT_THAT(params.width, Eq(size.width.as_int()));
    EXPECT_THAT(params.height, Eq(size.height.as_int()));
}
