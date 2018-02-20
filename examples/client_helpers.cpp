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

#include "client_helpers.h"
#include "mir_toolkit/mir_client_library.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <signal.h>

namespace me = mir::examples;

me::Connection::Connection(char const* socket_file, char const* name)
    : connection{mir_connect_sync(socket_file, name)}
{
    if (!mir_connection_is_valid(connection))
        throw std::runtime_error(
            std::string("could not connect to server: ") +
            mir_connection_get_error_message(connection));
}

me::Connection::Connection(char const* socket_file) :
    Connection(socket_file, __PRETTY_FUNCTION__)
{
}

me::Connection::~Connection()
{
    mir_connection_release(connection);
}

me::Connection::operator MirConnection*()
{
    return connection;
}

me::BufferStream::BufferStream(
    Connection& connection,
    unsigned int width,
    unsigned int height,
    bool prefer_alpha,
    bool hardware)
    : stream{create_stream(connection, width, height, prefer_alpha, hardware),
             &mir_buffer_stream_release_sync}
{
    if (!mir_buffer_stream_is_valid(stream.get()))
    {
        // TODO: Huh. There's no mir_buffer_stream_get_error?
        throw std::runtime_error("Could not create buffer stream.");
    }
}

me::BufferStream::operator MirBufferStream*() const
{
    return stream.get();
}

MirBufferStream* me::BufferStream::create_stream(
    MirConnection *connection,
    unsigned int width,
    unsigned int height,
    bool prefer_alpha,
    bool hardware)
{
    MirPixelFormat selected_format;
    unsigned int valid_formats{0};
    MirPixelFormat pixel_formats[mir_pixel_formats];
    mir_connection_get_available_surface_formats(connection, pixel_formats, mir_pixel_formats, &valid_formats);
    if (valid_formats == 0)
        throw std::runtime_error("no pixel formats for buffer stream");
    selected_format = pixel_formats[0];
    //select an 8 bit opaque format if we can
    if (!prefer_alpha)
    {
        for(auto i = 0u; i < valid_formats; i++)
        {
            if (pixel_formats[i] == mir_pixel_format_xbgr_8888 ||
                pixel_formats[i] == mir_pixel_format_xrgb_8888)
            {
                selected_format = pixel_formats[i];
                break;
            }
        }
    }

    return mir_connection_create_buffer_stream_sync(
        connection,
        width,
        height,
        selected_format,
        hardware ? mir_buffer_usage_hardware : mir_buffer_usage_software);
}

me::NormalWindow::NormalWindow(me::Connection& connection, unsigned int width, unsigned int height, bool prefers_alpha, bool hardware) :
    window{create_window(connection, width, height, prefers_alpha, hardware), window_deleter}
{
}

me::NormalWindow::operator MirWindow*() const
{
    return window.get();
}

MirWindow* me::NormalWindow::create_window(
    MirConnection* connection,
    unsigned int width,
    unsigned int height,
    bool prefers_alpha,
    bool hardware)
{
    MirPixelFormat selected_format;
    unsigned int valid_formats{0};
    MirPixelFormat pixel_formats[mir_pixel_formats];
    mir_connection_get_available_surface_formats(connection, pixel_formats, mir_pixel_formats, &valid_formats);
    if (valid_formats == 0)
        throw std::runtime_error("no pixel formats for surface");
    selected_format = pixel_formats[0]; 
    //select an 8 bit opaque format if we can
    if (!prefers_alpha)
    {
        for(auto i = 0u; i < valid_formats; i++)
        {
            if (pixel_formats[i] == mir_pixel_format_xbgr_8888 ||
                pixel_formats[i] == mir_pixel_format_xrgb_8888)
            {
                selected_format = pixel_formats[i];
                break;
            }
        }
    }
    
    auto deleter = [](MirWindowSpec *spec) { mir_window_spec_release(spec); };
    std::unique_ptr<MirWindowSpec, decltype(deleter)> spec{
        mir_create_normal_window_spec(connection, width, height),
        deleter
    };

    mir_window_spec_set_pixel_format(spec.get(), selected_format);
    mir_window_spec_set_name(spec.get(), __PRETTY_FUNCTION__);
    mir_window_spec_set_buffer_usage(spec.get(), hardware ? mir_buffer_usage_hardware : mir_buffer_usage_software);
    auto window = mir_create_window_sync(spec.get());
    return window;
}

me::Context::Context(Connection& connection, MirWindow* window, int swap_interval) :
    native_display(reinterpret_cast<EGLNativeDisplayType>(
        mir_connection_get_egl_native_display(connection))),
    native_window(reinterpret_cast<EGLNativeWindowType>(
        mir_buffer_stream_get_egl_native_window(mir_window_get_buffer_stream(window)))),
    display(native_display),
    config(chooseconfig(display.disp)),
    surface(display.disp, config, native_window),
    context(display.disp, config)
{
    make_current();
    eglSwapInterval(display.disp, swap_interval);
}

void me::Context::make_current()
{
    if (eglMakeCurrent(display.disp, surface.surface, surface.surface, context.context) == EGL_FALSE)
        throw(std::runtime_error("could not makecurrent"));
}

void me::Context::release_current()
{
    if (eglMakeCurrent(display.disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE)
        throw(std::runtime_error("could not makecurrent"));
}
void me::Context::swapbuffers()
{
    if (eglSwapBuffers(display.disp, surface.surface) == EGL_FALSE)
        throw(std::runtime_error("could not swapbuffers"));
}

EGLConfig me::Context::chooseconfig(EGLDisplay disp)
{
    int n{0};
    EGLConfig egl_config;
    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE };
    if (eglChooseConfig(disp, attribs, &egl_config, 1, &n) != EGL_TRUE || n != 1)
        throw std::runtime_error("could not find egl config");
    return egl_config;
}

me::Context::Display::Display(EGLNativeDisplayType native) :
    disp(eglGetDisplay(native))
{
    int major{0}, minor{0};
    if (disp == EGL_NO_DISPLAY)
        throw std::runtime_error("no egl display");
    if (eglInitialize(disp, &major, &minor) != EGL_TRUE || major != 1 || minor != 4)
        throw std::runtime_error("could not init egl");
}

me::Context::Display::~Display()
{
    eglTerminate(disp);
}

me::Context::Surface::Surface(EGLDisplay display, EGLConfig config, EGLNativeWindowType native_window) :
    disp(display),
    surface(eglCreateWindowSurface(disp, config, native_window, NULL))
{
    if (surface == EGL_NO_SURFACE)
        throw std::runtime_error("could not create egl surface");
}

me::Context::Surface::~Surface()
{
    if (eglGetCurrentSurface(EGL_DRAW) == surface)
        eglMakeCurrent(disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(disp, surface);
}

me::Context::EglContext::EglContext(EGLDisplay disp, EGLConfig config) :
    disp(disp),
    context(eglCreateContext(disp, config, EGL_NO_CONTEXT, context_attribs))
{
    if (context == EGL_NO_CONTEXT)
        throw std::runtime_error("could not create egl context");
}

me::Context::EglContext::~EglContext()
{
    eglDestroyContext(disp, context);
}

me::Shader::Shader(GLchar const* const* src, GLuint type) :
    shader(glCreateShader(type))
{
    glShaderSource(shader, 1, src, 0);
    glCompileShader(shader);
}

me::Shader::~Shader()
{
    glDeleteShader(shader);
}

me::Program::Program(Shader& vertex, Shader& fragment) :
    program(glCreateProgram())
{
    glAttachShader(program, vertex.shader);
    glAttachShader(program, fragment.shader);
    glLinkProgram(program);
}

me::Program::~Program()
{
    glDeleteProgram(program);
}
