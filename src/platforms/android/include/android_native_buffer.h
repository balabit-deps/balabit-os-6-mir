/*
 * Copyright © 2013 Canonical Ltd.
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

#ifndef MIR_GRAPHICS_ANDROID_ANDROID_NATIVE_BUFFER_H_
#define MIR_GRAPHICS_ANDROID_ANDROID_NATIVE_BUFFER_H_

#include "native_buffer.h"
#include <memory>
#include <mutex>

namespace mir
{
namespace graphics
{
class CommandStreamSync;
namespace android
{
class Fence;

struct AndroidNativeBuffer : public NativeBuffer
{
    AndroidNativeBuffer(
        std::shared_ptr<ANativeWindowBuffer> const& handle,
        std::shared_ptr<CommandStreamSync> const& cmdstream_sync,
        std::shared_ptr<Fence> const& fence,
        BufferAccess fence_access);

    ANativeWindowBuffer* anwb() const;
    buffer_handle_t handle() const;
    NativeFence copy_fence() const;
    NativeFence fence() const;

    void ensure_available_for(BufferAccess);
    bool ensure_available_for(android::BufferAccess intent, std::chrono::milliseconds timeout);
    void update_usage(NativeFence& merge_fd, BufferAccess);
    void reset_fence();

    void lock_for_gpu();
    void wait_for_unlock_by_gpu();

private:
    std::shared_ptr<CommandStreamSync> cmdstream_sync;
    std::shared_ptr<Fence> fence_;
    BufferAccess access;
    std::shared_ptr<ANativeWindowBuffer> native_window_buffer;
};

struct RefCountedNativeBuffer : public ANativeWindowBuffer
{
    RefCountedNativeBuffer(std::shared_ptr<const native_handle_t> const& handle);

    void driver_reference();
    void driver_dereference();
    void mir_dereference();
private:
    ~RefCountedNativeBuffer() = default;

    std::shared_ptr<const native_handle_t> const handle_resource;

    std::mutex mutex;
    bool mir_reference;
    int driver_references;
};

}
}
}

#endif /* MIR_GRAPHICS_ANDROID_ANDROID_NATIVE_BUFFER_H_ */
