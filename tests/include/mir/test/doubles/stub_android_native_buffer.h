/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan.griffiths@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_STUB_ANDROID_NATIVE_BUFFER_H_
#define MIR_TEST_DOUBLES_STUB_ANDROID_NATIVE_BUFFER_H_

#include "src/platforms/android/include/native_buffer.h"
#include "mir/geometry/size.h"

namespace mir
{
namespace test
{
namespace doubles
{
struct StubAndroidNativeBuffer : public graphics::android::NativeBuffer
{
    StubAndroidNativeBuffer()
    {
    }

    StubAndroidNativeBuffer(geometry::Size sz)
    {
        stub_anwb.width = sz.width.as_int();
        stub_anwb.height = sz.height.as_int();
    }

    auto anwb() const -> ANativeWindowBuffer* override { return const_cast<ANativeWindowBuffer*>(&stub_anwb); }
    auto handle() const -> buffer_handle_t override { return native_handle.get(); }
    auto copy_fence() const -> graphics::android::NativeFence override { return -1; }
    auto fence() const -> graphics::android::NativeFence override { return -1; }

    void ensure_available_for(graphics::android::BufferAccess) {}
    bool ensure_available_for(graphics::android::BufferAccess, std::chrono::milliseconds) { return true; }
    void update_usage(graphics::android::NativeFence&, graphics::android::BufferAccess) {}
    void reset_fence() {}

    void lock_for_gpu() {};
    void wait_for_unlock_by_gpu() {};

    ANativeWindowBuffer stub_anwb;
    std::unique_ptr<native_handle_t> native_handle =
        std::make_unique<native_handle_t>();
};
}
}
}

#endif /* MIR_TEST_DOUBLES_STUB_ANDROID_NATIVE_BUFFER_H_ */
