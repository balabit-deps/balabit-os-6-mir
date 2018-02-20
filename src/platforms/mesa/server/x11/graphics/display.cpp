/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Cemil Azizoglu <cemil.azizoglu@canonical.com>
 */

#include "mir/graphics/platform.h"
#include "mir/graphics/display_report.h"
#include "mir/graphics/egl_error.h"
#include "mir/graphics/virtual_output.h"
#include "mir/renderer/gl/context.h"
#include "mir/graphics/gl_config.h"
#include "mir/graphics/atomic_frame.h"
#include "display_configuration.h"
#include "display.h"
#include "display_buffer.h"

#include <boost/throw_exception.hpp>

#include <X11/Xatom.h>

#define MIR_LOG_COMPONENT "x11-display"
#include "mir/log.h"

namespace mg=mir::graphics;
namespace mgx=mg::X;
namespace geom=mir::geometry;

namespace
{
geom::Size clip_to_display(Display *dpy, geom::Size requested_size)
{
    unsigned int screen_width, screen_height, uint_dummy;
    int int_dummy;
    Window window_dummy;
    int const border = 150;

    XGetGeometry(dpy, XDefaultRootWindow(dpy), &window_dummy, &int_dummy, &int_dummy,
        &screen_width, &screen_height, &uint_dummy, &uint_dummy);

    mir::log_info("Screen resolution = %dx%d", screen_width, screen_height);

    if ((screen_width < requested_size.width.as_uint32_t()+border) ||
        (screen_height < requested_size.height.as_uint32_t()+border))
    {
        mir::log_info(" ... is smaller than the requested size (%dx%d) plus border (%d). Clipping to (%dx%d).",
            requested_size.width.as_uint32_t(), requested_size.height.as_uint32_t(), border,
            screen_width-border, screen_height-border);
        return geom::Size{screen_width-border, screen_height-border};
    }

    return requested_size;
}

auto get_pixel_width(Display *dpy)
{
    auto screen = XDefaultScreenOfDisplay(dpy);

    return float(screen->mwidth) / screen->width;
}
auto get_pixel_height(Display *dpy)
{
    auto screen = XDefaultScreenOfDisplay(dpy);

    return float(screen->mheight) / screen->height;
}

class XGLContext : public mir::renderer::gl::Context
{
public:
    XGLContext(::Display* const x_dpy,
               std::shared_ptr<mg::GLConfig> const& gl_config,
               EGLContext const shared_ctx)
        : egl{*gl_config}
    {
        egl.setup(x_dpy, shared_ctx);
    }

    ~XGLContext() = default;

    void make_current() const override
    {
        egl.make_current();
    }

    void release_current() const override
    {
        egl.release_current();
    }

private:
    mgx::helpers::EGLHelper egl;
};
}

mgx::X11Window::X11Window(::Display* x_dpy,
                          EGLDisplay egl_dpy,
                          geom::Size const size,
                          EGLConfig const egl_cfg)
    : x_dpy{x_dpy}
{
    auto root = XDefaultRootWindow(x_dpy);

    EGLint vid;
    if (!eglGetConfigAttrib(egl_dpy, egl_cfg, EGL_NATIVE_VISUAL_ID, &vid))
        BOOST_THROW_EXCEPTION(mg::egl_error("Cannot get config attrib"));

    XVisualInfo visTemplate;
    std::memset(&visTemplate, 0, sizeof visTemplate);
    int num_visuals = 0;
    visTemplate.visualid = vid;
    auto visInfo = XGetVisualInfo(x_dpy, VisualIDMask, &visTemplate, &num_visuals);
    if (!visInfo || !num_visuals)
        BOOST_THROW_EXCEPTION(mg::egl_error("Cannot get visual info, or no matching visuals"));

    mir::log_info("%d visual(s) found", num_visuals);
    mir::log_info("Using the first one :");
    mir::log_info("ID\t\t:\t%d", visInfo->visualid);
    mir::log_info("screen\t:\t%d", visInfo->screen);
    mir::log_info("depth\t\t:\t%d", visInfo->depth);
    mir::log_info("red_mask\t:\t0x%0X", visInfo->red_mask);
    mir::log_info("green_mask\t:\t0x%0X", visInfo->green_mask);
    mir::log_info("blue_mask\t:\t0x%0X", visInfo->blue_mask);
    mir::log_info("colormap_size\t:\t%d", visInfo->colormap_size);
    mir::log_info("bits_per_rgb\t:\t%d", visInfo->bits_per_rgb);

    r_mask = visInfo->red_mask;

    XSetWindowAttributes attr;
    std::memset(&attr, 0, sizeof(attr));
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(x_dpy, root, visInfo->visual, AllocNone);
    attr.event_mask = StructureNotifyMask |
                      ExposureMask        |
                      KeyPressMask        |
                      KeyReleaseMask      |
                      ButtonPressMask     |
                      ButtonReleaseMask   |
                      FocusChangeMask     |
                      EnterWindowMask     |
                      LeaveWindowMask     |
                      PointerMotionMask;

    auto mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

    win = XCreateWindow(x_dpy, root, 0, 0,
                        size.width.as_int(), size.height.as_int(),
                        0, visInfo->depth, InputOutput,
                        visInfo->visual, mask, &attr);

    XFree(visInfo);

    {
        char const * const title = "Mir On X";
        XSizeHints sizehints;

        // TODO: Due to a bug, resize doesn't work after XGrabKeyboard under Unity.
        //       For now, make window unresizeable.
        //     http://stackoverflow.com/questions/14555703/x11-unable-to-move-window-after-xgrabkeyboard
        sizehints.base_width = size.width.as_int();
        sizehints.base_height = size.height.as_int();
        sizehints.min_width = size.width.as_int();
        sizehints.min_height = size.height.as_int();
        sizehints.max_width = size.width.as_int();
        sizehints.max_height = size.height.as_int();
        sizehints.flags = PSize | PMinSize | PMaxSize;

        XSetNormalHints(x_dpy, win, &sizehints);
        XSetStandardProperties(x_dpy, win, title, title, None, (char **)NULL, 0, &sizehints);

        XWMHints wm_hints = {
            (InputHint|StateHint), // fields in this structure that are defined
            True,                  // does this application rely on the window manager
                                   // to get keyboard input? Yes, if this is False,
                                   // XGrabKeyboard doesn't work reliably.
            NormalState,           // initial_state
            0,                     // icon_pixmap
            0,                     // icon_window
            0, 0,                  // initial position of icon
            0,                     // pixmap to be used as mask for icon_pixmap
            0                      // id of related window_group
        };

        XSetWMHints(x_dpy, win, &wm_hints);

        Atom wmDeleteMessage = XInternAtom(x_dpy, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(x_dpy, win, &wmDeleteMessage, 1);
    }

    XMapWindow(x_dpy, win);

    XEvent xev;
    do 
    {
        XNextEvent(x_dpy, &xev);
    }
    while (xev.type != Expose);
}

mgx::X11Window::~X11Window()
{
    XDestroyWindow(x_dpy, win);
}

mgx::X11Window::operator Window() const
{
    return win;
}

unsigned long mgx::X11Window::red_mask() const
{
    return r_mask;
}

mgx::Display::Display(::Display* x_dpy,
                      geom::Size const requested_size,
                      std::shared_ptr<GLConfig> const& gl_config,
                      std::shared_ptr<DisplayReport> const& report)
    : shared_egl{*gl_config},
      x_dpy{x_dpy},
      actual_size{clip_to_display(x_dpy, requested_size)},
      gl_config{gl_config},
      pixel_width{get_pixel_width(x_dpy)},
      pixel_height{get_pixel_height(x_dpy)},
      scale{1.0f},
      report{report},
      orientation{mir_orientation_normal},
      last_frame{std::make_shared<AtomicFrame>()}
{
    shared_egl.setup(x_dpy);

    win = std::make_unique<X11Window>(x_dpy,
                                      shared_egl.display(),
                                      actual_size,
                                      shared_egl.config());

    auto red_mask = win->red_mask();
    pf = (red_mask == 0xFF0000 ? mir_pixel_format_argb_8888
                               : mir_pixel_format_abgr_8888);

    display_buffer = std::make_unique<mgx::DisplayBuffer>(
                         x_dpy,
                         *win,
                         actual_size,
                         shared_egl.context(),
                         last_frame,
                         report,
                         orientation,
                         *gl_config);

    shared_egl.make_current();

    report->report_successful_display_construction();
}

mgx::Display::~Display() noexcept
{
}

void mgx::Display::for_each_display_sync_group(std::function<void(mg::DisplaySyncGroup&)> const& f)
{
    f(*display_buffer);
}

std::unique_ptr<mg::DisplayConfiguration> mgx::Display::configuration() const
{
    return std::make_unique<mgx::DisplayConfiguration>(
        pf, actual_size, geom::Size{actual_size.width * pixel_width, actual_size.height * pixel_height}, scale, orientation);
}

void mgx::Display::configure(mg::DisplayConfiguration const& new_configuration)
{
    if (!new_configuration.valid())
    {
        BOOST_THROW_EXCEPTION(
            std::logic_error("Invalid or inconsistent display configuration"));
    }

    MirOrientation o = mir_orientation_normal;
    float new_scale = scale;

    new_configuration.for_each_output([&](DisplayConfigurationOutput const& conf_output)
    {
        o = conf_output.orientation;
        new_scale = conf_output.scale;
    });

    orientation = o;
    display_buffer->set_orientation(orientation);
    scale = new_scale;
}

void mgx::Display::register_configuration_change_handler(
    EventHandlerRegister& /* event_handler*/,
    DisplayConfigurationChangeHandler const& /*change_handler*/)
{
}

void mgx::Display::register_pause_resume_handlers(
    EventHandlerRegister& /*handlers*/,
    DisplayPauseHandler const& /*pause_handler*/,
    DisplayResumeHandler const& /*resume_handler*/)
{
}

void mgx::Display::pause()
{
    BOOST_THROW_EXCEPTION(std::runtime_error("'Display::pause()' not yet supported on x11 platform"));
}

void mgx::Display::resume()
{
    BOOST_THROW_EXCEPTION(std::runtime_error("'Display::resume()' not yet supported on x11 platform"));
}

auto mgx::Display::create_hardware_cursor(std::shared_ptr<mg::CursorImage> const& /* initial_image */) -> std::shared_ptr<Cursor>
{
    return nullptr;
}

std::unique_ptr<mg::VirtualOutput> mgx::Display::create_virtual_output(int /*width*/, int /*height*/)
{
    return nullptr;
}

mg::NativeDisplay* mgx::Display::native_display()
{
    return this;
}

std::unique_ptr<mir::renderer::gl::Context> mgx::Display::create_gl_context()
{
    return std::make_unique<XGLContext>(x_dpy, gl_config, shared_egl.context());
}

bool mgx::Display::apply_if_configuration_preserves_display_buffers(
    mg::DisplayConfiguration const& /*conf*/)
{
    return false;
}

mg::Frame mgx::Display::last_frame_on(unsigned) const
{
    return last_frame->load();
}
