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

#include "egl_sync_extensions.h"
#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace mg=mir::graphics;

mg::EGLSyncExtensions::EGLSyncExtensions() :
    eglCreateSyncKHR{
        reinterpret_cast<PFNEGLCREATESYNCKHRPROC>(eglGetProcAddress("eglCreateSyncKHR"))},
    eglDestroySyncKHR{
        reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroySyncKHR"))},
    eglClientWaitSyncKHR{
        reinterpret_cast<PFNEGLCLIENTWAITSYNCKHRPROC>(eglGetProcAddress("eglClientWaitSyncKHR"))}
{
    if (!eglCreateSyncKHR || !eglDestroySyncKHR || !eglClientWaitSyncKHR)
        BOOST_THROW_EXCEPTION(std::runtime_error("EGL doesn't support the KHR_reusable_sync extension"));
}
