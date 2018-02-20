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
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_TEST_DOUBLES_STUB_SESSION_H
#define MIR_TEST_DOUBLES_STUB_SESSION_H

#include <mir/scene/session.h>

namespace mir
{
namespace test
{
namespace doubles
{

struct StubSession : scene::Session, frontend::SessionExtensions
{
    StubSession(pid_t pid = -1);

    std::shared_ptr<frontend::Surface> get_surface(
        frontend::SurfaceId surface) const override;

    std::string name() const override;

    void drop_outstanding_requests() override;

    pid_t process_id() const override;

    void take_snapshot(scene::SnapshotCallback const& snapshot_taken) override;

    std::shared_ptr<scene::Surface> default_surface() const override;

    void set_lifecycle_state(MirLifecycleState state) override;

    void send_display_config(graphics::DisplayConfiguration const&) override;

    void send_error(ClientVisibleError const&) override;

    void hide() override;

    void show() override;

    void start_prompt_session() override;

    void stop_prompt_session() override;

    void suspend_prompt_session() override;

    void resume_prompt_session() override;

    frontend::SurfaceId create_surface(
        scene::SurfaceCreationParameters const& params,
        std::shared_ptr<frontend::EventSink> const& sink) override;

    void destroy_surface(frontend::SurfaceId surface) override;

    std::shared_ptr<scene::Surface> surface(
        frontend::SurfaceId surface) const override;

    std::shared_ptr<scene::Surface> surface_after(
        std::shared_ptr<scene::Surface> const&) const override;

    std::shared_ptr<frontend::BufferStream> get_buffer_stream(
        frontend::BufferStreamId stream) const override;

    frontend::BufferStreamId create_buffer_stream(
        graphics::BufferProperties const& props) override;
    graphics::BufferID create_buffer(geometry::Size, uint32_t, uint32_t) override;
    graphics::BufferID create_buffer(geometry::Size, MirPixelFormat) override;

    void destroy_buffer_stream(frontend::BufferStreamId stream) override;

    void configure_streams(
        scene::Surface& surface,
        std::vector<shell::StreamSpecification> const& config) override;

    void destroy_surface(std::weak_ptr<scene::Surface> const& surface) override;

    void send_input_config(MirInputConfig const& config) override;

    graphics::BufferID create_buffer(graphics::BufferProperties const& properties) override;
    void destroy_buffer(graphics::BufferID) override;
    std::shared_ptr<graphics::Buffer> get_buffer(graphics::BufferID) override;

    pid_t pid;
};
}
}
}

#endif //MIR_TEST_DOUBLES_STUB_SESSION_H
