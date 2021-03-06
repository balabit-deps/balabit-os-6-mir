/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
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

#include "sync_fence.h"

#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/sync.h>  // This will come from /usr/include/android{,-19}/

namespace mga = mir::graphics::android;

mga::SyncFence::SyncFence(std::shared_ptr<mga::SyncFileOps> const& ops, Fd fd)
   : fence_fd(std::move(fd)),
     ops(ops)
{
}

void mga::SyncFence::wait()
{
    if (fence_fd > 0)
    {
        int timeout = infinite_timeout;
        ops->ioctl(fence_fd, SYNC_IOC_WAIT, &timeout);
        fence_fd = mir::Fd(Fd::invalid);
    }
}

bool mga::SyncFence::wait_for(std::chrono::milliseconds ms)
{
    int timed_out = 0;
    if (fence_fd > 0)
    {
        int timeout = ms.count();
        timed_out = ops->ioctl(fence_fd, SYNC_IOC_WAIT, &timeout);
        fence_fd = mir::Fd(Fd::invalid);
    }
    return timed_out >= 0;
}

void mga::SyncFence::reset_fence()
{
   fence_fd = mir::Fd(mir::Fd::invalid);
}

void mga::SyncFence::merge_with(NativeFence& merge_fd)
{
    if (merge_fd < 0)
    {
        return;
    }

    if (fence_fd < 0)
    {
        //our fence was invalid, adopt the other fence
        fence_fd = mir::Fd(merge_fd);
    }
    else
    {
        //both fences were valid, must merge
        struct sync_merge_data data { merge_fd, "mirfence", infinite_timeout };
        ops->ioctl(fence_fd, static_cast<int>(SYNC_IOC_MERGE), &data);
        ops->close(merge_fd);
        fence_fd = mir::Fd(data.fence);
    }

    merge_fd = -1;
}

mga::NativeFence mga::SyncFence::copy_native_handle() const
{
    return ops->dup(fence_fd);
}

mga::NativeFence mga::SyncFence::native_handle() const
{
    return fence_fd;
}

int mga::RealSyncFileOps::ioctl(int fd, int req, void* dat)
{
    return ::ioctl(fd, req, dat);
}

int mga::RealSyncFileOps::dup(int fd)
{
    return ::dup(fd);
}

int mga::RealSyncFileOps::close(int fd)
{
    return ::close(fd);
}
