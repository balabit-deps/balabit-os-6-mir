/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include <boost/throw_exception.hpp>
#include "mir/graphics/display_configuration.h"
#include "mir/graphics/display.h"
#include "mir/graphics//default_display_configuration_policy.h"
#include "mir/time/steady_clock.h"
#include "mir/glib_main_loop.h"
#include "src/platforms/mesa/server/kms/platform.h"
#include "src/platforms/mesa/server/kms/kms_display_configuration.h"
#include "src/server/report/null_report_factory.h"

#include "mir/test/signal.h"
#include "mir/test/auto_unblock_thread.h"

#include "mir/test/doubles/mock_egl.h"
#include "mir/test/doubles/mock_gl.h"
#include "mir/test/doubles/mock_drm.h"
#include "mir/test/doubles/mock_gbm.h"
#include "mir/test/doubles/null_emergency_cleanup.h"
#include "mir/test/doubles/null_virtual_terminal.h"
#include "mir/test/doubles/stub_gl_config.h"
#include "mir/test/doubles/stub_gl_program_factory.h"

#include "mir_test_framework/udev_environment.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>

namespace mg = mir::graphics;
namespace mgm = mir::graphics::mesa;
namespace geom = mir::geometry;
namespace mt  = mir::test;
namespace mtd = mir::test::doubles;
namespace mtf = mir_test_framework;

namespace
{

mg::DisplayConfigurationMode conf_mode_from_drm_mode(drmModeModeInfo const& mode)
{
    geom::Size const size{mode.hdisplay, mode.vdisplay};
    double vrefresh_hz{0.0};

    /* Calculate vertical refresh rate from DRM mode information */
    if (mode.htotal != 0.0 && mode.vtotal != 0.0)
    {
        vrefresh_hz = (mode.clock * 100000LL /
                       ((long)mode.htotal * (long)mode.vtotal)) / 100.0;
    }

    return mg::DisplayConfigurationMode{size, vrefresh_hz};
}

struct MainLoop
{
    MainLoop()
    {
        int const owner{0};
        mt::Signal mainloop_started;
        ml.enqueue(&owner, [&] { mainloop_started.raise(); });
        mt::AutoUnblockThread t([this]{ml.stop();}, [this]{ml.run();});
        bool const started = mainloop_started.wait_for(std::chrono::seconds(10));
        if (!started)
            throw std::runtime_error("Failed to start main loop");

        ml_thread = std::move(t);
    }
    mir::GLibMainLoop ml{std::make_shared<mir::time::SteadyClock>()};
    mt::AutoUnblockThread ml_thread;
};

class MesaDisplayConfigurationTest : public ::testing::Test
{
public:
    MesaDisplayConfigurationTest()
    {
        using namespace testing;

        /* Needed for display start-up */
        ON_CALL(mock_egl, eglChooseConfig(_,_,_,1,_))
        .WillByDefault(DoAll(SetArgPointee<2>(mock_egl.fake_configs[0]),
                             SetArgPointee<4>(1),
                             Return(EGL_TRUE)));

        mock_egl.provide_egl_extensions();
        mock_gl.provide_gles_extensions();

        setup_sample_modes();

        fake_devices.add_standard_device("standard-drm-devices");
    }

    std::shared_ptr<mg::Platform> create_platform()
    {
        return std::make_shared<mgm::Platform>(
               mir::report::null_display_report(),
               std::make_shared<mtd::NullVirtualTerminal>(),
               *std::make_shared<mtd::NullEmergencyCleanup>(),
               mgm::BypassOption::allowed);
    }

    std::shared_ptr<mg::Display> create_display(
        std::shared_ptr<mg::Platform> const& platform)
    {
        return platform->create_display(
            std::make_shared<mg::CloneDisplayConfigurationPolicy>(),
            std::make_shared<mtd::StubGLConfig>());
    }

    void setup_sample_modes()
    {
        using fake = mtd::FakeDRMResources;

        /* Add DRM modes */
        modes0.push_back(fake::create_mode(1920, 1080, 138500, 2080, 1111, fake::NormalMode));
        modes0.push_back(fake::create_mode(1920, 1080, 148500, 2200, 1125, fake::PreferredMode));
        modes0.push_back(fake::create_mode(1680, 1050, 119000, 1840, 1080, fake::NormalMode));
        modes0.push_back(fake::create_mode(832, 624, 57284, 1152, 667, fake::NormalMode));

        /* Add the DisplayConfiguration modes corresponding to the DRM modes */
        for (auto const& mode : modes0)
            conf_modes0.push_back(conf_mode_from_drm_mode(mode));

        modes1.push_back(fake::create_mode(123, 456, 138500, 2080, 1111, fake::NormalMode));
        modes1.push_back(fake::create_mode(789, 1011, 148500, 2200, 1125, fake::NormalMode));
        modes1.push_back(fake::create_mode(1213, 1415, 119000, 1840, 1080, fake::PreferredMode));

        for (auto const& mode : modes1)
            conf_modes1.push_back(conf_mode_from_drm_mode(mode));
    }

    ::testing::NiceMock<mtd::MockEGL> mock_egl;
    ::testing::NiceMock<mtd::MockGL> mock_gl;
    ::testing::NiceMock<mtd::MockDRM> mock_drm;
    ::testing::NiceMock<mtd::MockGBM> mock_gbm;

    std::vector<drmModeModeInfo> modes0;
    std::vector<mg::DisplayConfigurationMode> conf_modes0;
    std::vector<drmModeModeInfo> modes1;
    std::vector<mg::DisplayConfigurationMode> conf_modes1;
    std::vector<drmModeModeInfo> modes_empty;

    mtf::UdevEnvironment fake_devices;
};

}

TEST_F(MesaDisplayConfigurationTest, configuration_is_read_correctly)
{
    using namespace ::testing;

    /* Set up DRM resources */
    uint32_t const invalid_id{0};
    uint32_t const crtc0_id{10};
    uint32_t const encoder0_id{20};
    uint32_t const encoder1_id{21};
    uint32_t const connector0_id{30};
    uint32_t const connector1_id{31};
    uint32_t const connector2_id{32};
    geom::Size const connector0_physical_size_mm{480, 270};
    geom::Size const connector1_physical_size_mm{};
    geom::Size const connector2_physical_size_mm{};
    std::vector<uint32_t> possible_encoder_ids_empty;
    uint32_t const possible_crtcs_mask_empty{0};

    mtd::FakeDRMResources& resources(mock_drm.fake_drm);

    resources.reset();

    resources.add_crtc(crtc0_id, modes0[1]);

    resources.add_encoder(encoder0_id, crtc0_id, possible_crtcs_mask_empty);
    resources.add_encoder(encoder1_id, invalid_id, possible_crtcs_mask_empty);

    resources.add_connector(connector0_id, DRM_MODE_CONNECTOR_HDMIA,
                            DRM_MODE_CONNECTED, encoder0_id,
                            modes0, possible_encoder_ids_empty,
                            connector0_physical_size_mm);
    resources.add_connector(connector1_id, DRM_MODE_CONNECTOR_Unknown,
                            DRM_MODE_DISCONNECTED, invalid_id,
                            modes_empty, possible_encoder_ids_empty,
                            connector1_physical_size_mm);
    resources.add_connector(connector2_id, DRM_MODE_CONNECTOR_eDP,
                            DRM_MODE_DISCONNECTED, encoder1_id,
                            modes_empty, possible_encoder_ids_empty,
                            connector2_physical_size_mm);

    resources.prepare();

    std::vector<mg::DisplayConfigurationOutput> const expected_outputs =
    {
        {
            mg::DisplayConfigurationOutputId{connector0_id},
            mg::DisplayConfigurationCardId{0},
            mg::DisplayConfigurationOutputType::hdmia,
            {},
            conf_modes0,
            1,
            connector0_physical_size_mm,
            true,
            true,
            geom::Point(),
            1,
            mir_pixel_format_invalid,
            mir_power_mode_on,
            mir_orientation_normal,
            1.0f,
            mir_form_factor_monitor,
            mir_subpixel_arrangement_unknown,
            {},
            mir_output_gamma_unsupported,
            {}
        },
        {
            mg::DisplayConfigurationOutputId{connector1_id},
            mg::DisplayConfigurationCardId{0},
            mg::DisplayConfigurationOutputType::unknown,
            {},
            std::vector<mg::DisplayConfigurationMode>(),
            std::numeric_limits<uint32_t>::max(),
            connector1_physical_size_mm,
            false,
            false,
            geom::Point(),
            std::numeric_limits<uint32_t>::max(),
            mir_pixel_format_invalid,
            mir_power_mode_on,
            mir_orientation_normal,
            1.0f,
            mir_form_factor_monitor,
            mir_subpixel_arrangement_unknown,
            {},
            mir_output_gamma_unsupported,
            {}
        },
        {
            mg::DisplayConfigurationOutputId{connector2_id},
            mg::DisplayConfigurationCardId{0},
            mg::DisplayConfigurationOutputType::edp,
            {},
            std::vector<mg::DisplayConfigurationMode>(),
            std::numeric_limits<uint32_t>::max(),
            connector2_physical_size_mm,
            false,
            false,
            geom::Point(),
            std::numeric_limits<uint32_t>::max(),
            mir_pixel_format_invalid,
            mir_power_mode_on,
            mir_orientation_normal,
            1.0f,
            mir_form_factor_monitor,
            mir_subpixel_arrangement_unknown,
            {},
            mir_output_gamma_unsupported,
            {}
        }
    };

    /* Test body */
    auto display = create_display(create_platform());

    auto conf = display->configuration();

    size_t output_count{0};

    conf->for_each_output([&](mg::DisplayConfigurationOutput const& output)
    {
        ASSERT_LT(output_count, expected_outputs.size());
        EXPECT_EQ(expected_outputs[output_count], output) << "output_count: " << output_count;
        ++output_count;
    });

    EXPECT_EQ(expected_outputs.size(), output_count);
}

TEST_F(MesaDisplayConfigurationTest, reads_subpixel_information_correctly)
{
    using namespace ::testing;

    /* Set up DRM resources */
    uint32_t const crtc0_id{10};
    uint32_t const encoder0_id{20};
    uint32_t const connector0_id{30};
    geom::Size const connector0_physical_size_mm{480, 270};
    std::vector<uint32_t> possible_encoder_ids_empty;
    uint32_t const possible_crtcs_mask_empty{0};

    mtd::FakeDRMResources& resources(mock_drm.fake_drm);

    struct TestData
    {
        drmModeSubPixel drm_subpixel;
        MirSubpixelArrangement mir_subpixel;
    };
    std::vector<TestData> const test_data = {
        { DRM_MODE_SUBPIXEL_UNKNOWN, mir_subpixel_arrangement_unknown },
        { DRM_MODE_SUBPIXEL_HORIZONTAL_RGB, mir_subpixel_arrangement_horizontal_rgb },
        { DRM_MODE_SUBPIXEL_HORIZONTAL_BGR, mir_subpixel_arrangement_horizontal_bgr },
        { DRM_MODE_SUBPIXEL_VERTICAL_RGB, mir_subpixel_arrangement_vertical_rgb },
        { DRM_MODE_SUBPIXEL_VERTICAL_BGR, mir_subpixel_arrangement_vertical_bgr },
        { DRM_MODE_SUBPIXEL_NONE, mir_subpixel_arrangement_none }
    };

    for (auto& data : test_data)
    {

        resources.reset();

        resources.add_crtc(crtc0_id, modes0[1]);

        resources.add_encoder(encoder0_id, crtc0_id, possible_crtcs_mask_empty);

        resources.add_connector(connector0_id, DRM_MODE_CONNECTOR_HDMIA,
                                DRM_MODE_CONNECTED, encoder0_id,
                                modes0, possible_encoder_ids_empty,
                                connector0_physical_size_mm,
                                data.drm_subpixel);

        resources.prepare();

        /* Test body */
        auto display = create_display(create_platform());

        auto conf = display->configuration();


        int output_count{0};

        conf->for_each_output([&](mg::DisplayConfigurationOutput const& output)
                              {
                                  EXPECT_THAT(output.subpixel_arrangement, Eq(data.mir_subpixel));
                                  ++output_count;
                              });

        EXPECT_THAT(output_count, Ge(1));
    }
}

TEST_F(MesaDisplayConfigurationTest, reads_updated_subpixel_information)
{
    using namespace ::testing;
    using namespace std::chrono_literals;

    /* Set up DRM resources */
    uint32_t const crtc0_id{10};
    uint32_t const encoder0_id{20};
    uint32_t const connector0_id{30};
    geom::Size const connector0_physical_size_mm{480, 270};
    std::vector<uint32_t> possible_encoder_ids_empty;
    uint32_t const possible_crtcs_mask_empty{0};

    mtd::FakeDRMResources& resources(mock_drm.fake_drm);

    resources.reset();

    resources.add_crtc(crtc0_id, modes0[1]);

    resources.add_encoder(encoder0_id, crtc0_id, possible_crtcs_mask_empty);

    resources.add_connector(connector0_id, DRM_MODE_CONNECTOR_HDMIA,
                            DRM_MODE_CONNECTED, encoder0_id,
                            modes0, possible_encoder_ids_empty,
                            connector0_physical_size_mm,
                            DRM_MODE_SUBPIXEL_NONE);

    resources.prepare();

    struct TestData
    {
        drmModeSubPixel drm_subpixel;
        MirSubpixelArrangement mir_subpixel;
    };
    std::vector<TestData> const test_data = {
        { DRM_MODE_SUBPIXEL_UNKNOWN, mir_subpixel_arrangement_unknown },
        { DRM_MODE_SUBPIXEL_HORIZONTAL_RGB, mir_subpixel_arrangement_horizontal_rgb },
        { DRM_MODE_SUBPIXEL_HORIZONTAL_BGR, mir_subpixel_arrangement_horizontal_bgr },
        { DRM_MODE_SUBPIXEL_VERTICAL_RGB, mir_subpixel_arrangement_vertical_rgb },
        { DRM_MODE_SUBPIXEL_VERTICAL_BGR, mir_subpixel_arrangement_vertical_bgr },
        { DRM_MODE_SUBPIXEL_NONE, mir_subpixel_arrangement_none }
    };

    auto display = create_display(create_platform());
    auto const syspath = fake_devices.add_device("drm", "card2", NULL, {}, {"DEVTYPE", "drm_minor"});

    for (auto& data : test_data)
    {

        resources.reset();

        resources.add_crtc(crtc0_id, modes0[1]);

        resources.add_encoder(encoder0_id, crtc0_id, possible_crtcs_mask_empty);

        resources.add_connector(connector0_id, DRM_MODE_CONNECTOR_HDMIA,
                                DRM_MODE_CONNECTED, encoder0_id,
                                modes0, possible_encoder_ids_empty,
                                connector0_physical_size_mm,
                                data.drm_subpixel);

        resources.prepare();

        mt::Signal handler_signal;
        MainLoop ml;
        display->register_configuration_change_handler(ml.ml, [&handler_signal]{handler_signal.raise();});
        fake_devices.emit_device_changed(syspath);
        ASSERT_TRUE(handler_signal.wait_for(10s));


        /* Test body */
        auto conf = display->configuration();


        int output_count{0};

        conf->for_each_output([&](mg::DisplayConfigurationOutput const& output)
                              {
                                  EXPECT_THAT(output.subpixel_arrangement, Eq(data.mir_subpixel));
                                  ++output_count;
                              });

        EXPECT_THAT(output_count, Ge(1));
    }
}

TEST_F(MesaDisplayConfigurationTest, get_kms_connector_id_returns_correct_id)
{
    uint32_t const crtc0_id{10};
    uint32_t const encoder0_id{20};
    uint32_t const possible_crtcs_mask_empty{0};
    std::vector<uint32_t> const connector_ids{30, 31};
    std::vector<uint32_t> encoder_ids{20};

    /* Set up DRM resources */
    mtd::FakeDRMResources& resources(mock_drm.fake_drm);

    resources.reset();

    resources.add_crtc(crtc0_id, modes0[1]);
    resources.add_encoder(encoder0_id, crtc0_id, possible_crtcs_mask_empty);
    for (auto id : connector_ids)
    {
        resources.add_connector(id, DRM_MODE_CONNECTOR_DVID,
                                DRM_MODE_CONNECTED, encoder0_id,
                                modes0, encoder_ids,
                                geom::Size());
    }

    resources.prepare();

    /* Test body */
    auto display = create_display(create_platform());

    std::shared_ptr<mg::DisplayConfiguration> conf = display->configuration();
    auto const& kms_conf = std::static_pointer_cast<mgm::KMSDisplayConfiguration>(conf);

    size_t output_count{0};

    conf->for_each_output([&](mg::DisplayConfigurationOutput const& output)
    {
        ASSERT_LT(output_count, connector_ids.size());

        EXPECT_EQ(connector_ids[output_count],
                  kms_conf->get_kms_connector_id(output.id));
        ++output_count;
    });
}

TEST_F(MesaDisplayConfigurationTest, get_kms_connector_id_throws_on_invalid_id)
{
    uint32_t const crtc0_id{10};
    uint32_t const encoder0_id{20};
    uint32_t const possible_crtcs_mask_empty{0};
    std::vector<uint32_t> const connector_ids{30, 31};
    std::vector<uint32_t> encoder_ids{20};

    /* Set up DRM resources */
    mtd::FakeDRMResources& resources(mock_drm.fake_drm);

    resources.reset();

    resources.add_crtc(crtc0_id, modes0[1]);
    resources.add_encoder(encoder0_id, crtc0_id, possible_crtcs_mask_empty);
    for (auto id : connector_ids)
    {
        resources.add_connector(id, DRM_MODE_CONNECTOR_VGA,
                                DRM_MODE_CONNECTED, encoder0_id,
                                modes0, encoder_ids,
                                geom::Size());
    }

    resources.prepare();

    /* Test body */
    auto display = create_display(create_platform());

    std::shared_ptr<mg::DisplayConfiguration> conf = display->configuration();
    auto const& kms_conf = std::static_pointer_cast<mgm::KMSDisplayConfiguration>(conf);

    EXPECT_THROW({
        kms_conf->get_kms_connector_id(mg::DisplayConfigurationOutputId{29});
    }, std::runtime_error);
    EXPECT_THROW({
        kms_conf->get_kms_connector_id(mg::DisplayConfigurationOutputId{32});
    }, std::runtime_error);
}

TEST_F(MesaDisplayConfigurationTest, returns_updated_configuration)
{
    using namespace ::testing;
    using namespace std::chrono_literals;

    uint32_t const invalid_id{0};
    std::vector<uint32_t> const crtc_ids{10, 11};
    std::vector<uint32_t> const encoder_ids{20, 21};
    std::vector<uint32_t> const connector_ids{30, 31};
    std::vector<geom::Size> const connector_physical_sizes_mm_before{
        {480, 270}, {}
    };
    std::vector<geom::Size> const connector_physical_sizes_mm_after{
        {}, {512, 642}
    };
    std::vector<uint32_t> possible_encoder_ids_empty;
    uint32_t const possible_crtcs_mask_empty{0};

    std::vector<mg::DisplayConfigurationOutput> const expected_outputs_before =
    {
        {
            mg::DisplayConfigurationOutputId(connector_ids[0]),
            mg::DisplayConfigurationCardId{0},
            mg::DisplayConfigurationOutputType::composite,
            {},
            conf_modes0,
            1,
            connector_physical_sizes_mm_before[0],
            true,
            true,
            geom::Point(),
            1,
            mir_pixel_format_invalid,
            mir_power_mode_on,
            mir_orientation_normal,
            1.0f,
            mir_form_factor_monitor,
            mir_subpixel_arrangement_unknown,
            {},
            mir_output_gamma_unsupported,
            {}
        },
        {
            mg::DisplayConfigurationOutputId(connector_ids[1]),
            mg::DisplayConfigurationCardId{0},
            mg::DisplayConfigurationOutputType::vga,
            {},
            std::vector<mg::DisplayConfigurationMode>(),
            std::numeric_limits<uint32_t>::max(),
            connector_physical_sizes_mm_before[1],
            false,
            false,
            geom::Point(),
            std::numeric_limits<uint32_t>::max(),
            mir_pixel_format_invalid,
            mir_power_mode_on,
            mir_orientation_normal,
            1.0f,
            mir_form_factor_monitor,
            mir_subpixel_arrangement_unknown,
            {},
            mir_output_gamma_unsupported,
            {}
        },
    };

    std::vector<mg::DisplayConfigurationOutput> const expected_outputs_after =
    {
        {
            mg::DisplayConfigurationOutputId(connector_ids[0]),
            mg::DisplayConfigurationCardId{0},
            mg::DisplayConfigurationOutputType::composite,
            {},
            std::vector<mg::DisplayConfigurationMode>(),
            std::numeric_limits<uint32_t>::max(),
            connector_physical_sizes_mm_after[0],
            false,
            true,
            geom::Point(),
            1,  // Ensure current_mode_index is remembered even still
            mir_pixel_format_invalid,
            mir_power_mode_on,
            mir_orientation_normal,
            1.0f,
            mir_form_factor_monitor,
            mir_subpixel_arrangement_unknown,
            {},
            mir_output_gamma_unsupported,
            {}
        },
        {
            mg::DisplayConfigurationOutputId(connector_ids[1]),
            mg::DisplayConfigurationCardId{0},
            mg::DisplayConfigurationOutputType::vga,
            {},
            conf_modes0,
            1,
            connector_physical_sizes_mm_after[1],
            true,
            false,
            geom::Point(),
            1,
            mir_pixel_format_invalid,
            mir_power_mode_on,
            mir_orientation_normal,
            1.0f,
            mir_form_factor_monitor,
            mir_subpixel_arrangement_unknown,
            {},
            mir_output_gamma_unsupported,
            {}
        },
    };

    /* Set up DRM resources and check */
    mtd::FakeDRMResources& resources(mock_drm.fake_drm);
    auto const syspath = fake_devices.add_device("drm", "card2", NULL, {}, {"DEVTYPE", "drm_minor"});

    resources.reset();

    resources.add_crtc(crtc_ids[0], modes0[1]);

    resources.add_encoder(encoder_ids[0], crtc_ids[0], possible_crtcs_mask_empty);
    resources.add_encoder(encoder_ids[1], invalid_id, possible_crtcs_mask_empty);

    resources.add_connector(connector_ids[0], DRM_MODE_CONNECTOR_Composite,
                            DRM_MODE_CONNECTED, encoder_ids[0],
                            modes0, possible_encoder_ids_empty,
                            connector_physical_sizes_mm_before[0]);
    resources.add_connector(connector_ids[1], DRM_MODE_CONNECTOR_VGA,
                            DRM_MODE_DISCONNECTED, invalid_id,
                            modes_empty, possible_encoder_ids_empty,
                            connector_physical_sizes_mm_before[1]);

    resources.prepare();

    auto display = create_display(create_platform());

    auto conf = display->configuration();

    size_t output_count{0};

    conf->for_each_output([&](mg::DisplayConfigurationOutput const& output)
    {
        ASSERT_LT(output_count, expected_outputs_before.size());
        EXPECT_EQ(expected_outputs_before[output_count], output) << "output_count: " << output_count;
        ++output_count;
    });

    EXPECT_EQ(expected_outputs_before.size(), output_count);

    /* Reset DRM resources and check again */
    resources.reset();

    resources.add_crtc(crtc_ids[1], modes0[1]);

    resources.add_encoder(encoder_ids[0], invalid_id, possible_crtcs_mask_empty);
    resources.add_encoder(encoder_ids[1], crtc_ids[1], possible_crtcs_mask_empty);

    resources.add_connector(connector_ids[0], DRM_MODE_CONNECTOR_Composite,
                            DRM_MODE_DISCONNECTED, invalid_id,
                            modes_empty, possible_encoder_ids_empty,
                            connector_physical_sizes_mm_after[0]);
    resources.add_connector(connector_ids[1], DRM_MODE_CONNECTOR_VGA,
                            DRM_MODE_CONNECTED, encoder_ids[1],
                            modes0, possible_encoder_ids_empty,
                            connector_physical_sizes_mm_after[1]);

    resources.prepare();

    /* Fake a device change so display can fetch updated configuration*/
    MainLoop ml;
    mt::Signal handler_signal;
    display->register_configuration_change_handler(ml.ml, [&handler_signal]{handler_signal.raise();});
    fake_devices.emit_device_changed(syspath);
    ASSERT_TRUE(handler_signal.wait_for(10s));

    conf = display->configuration();

    output_count = 0;

    conf->for_each_output([&](mg::DisplayConfigurationOutput const& output)
    {
        ASSERT_LT(output_count, expected_outputs_after.size());
        EXPECT_EQ(expected_outputs_after[output_count], output) << "output_count: " << output_count;
        ++output_count;
    });

    EXPECT_EQ(expected_outputs_after.size(), output_count);
}

TEST_F(MesaDisplayConfigurationTest, new_monitor_defaults_to_preferred_mode)
{
    using namespace ::testing;
    using namespace std::chrono_literals;

    uint32_t const crtc_ids[1] = {10};
    uint32_t const encoder_ids[2] = {20, 21};
    uint32_t const connector_ids[1] = {30};
    geom::Size const connector_physical_sizes_mm_before{480, 270};
    geom::Size const connector_physical_sizes_mm_after{512, 642};
    std::vector<uint32_t> possible_encoder_ids_empty;
    uint32_t const possible_crtcs_mask_empty{0};
    int const noutputs = 1;

    mg::DisplayConfigurationOutput const expected_outputs_before[noutputs] =
    {
        {
            mg::DisplayConfigurationOutputId(connector_ids[0]),
            mg::DisplayConfigurationCardId{0},
            mg::DisplayConfigurationOutputType::composite,
            {},
            conf_modes0,
            1,
            connector_physical_sizes_mm_before,
            true,
            true,
            geom::Point(),
            1,
            mir_pixel_format_invalid,
            mir_power_mode_on,
            mir_orientation_normal,
            1.0f,
            mir_form_factor_monitor,
            mir_subpixel_arrangement_unknown,
            {},
            mir_output_gamma_unsupported,
            {}
        },
    };

    mg::DisplayConfigurationOutput const expected_outputs_after[noutputs] =
    {
        {
            mg::DisplayConfigurationOutputId(connector_ids[0]),
            mg::DisplayConfigurationCardId{0},
            mg::DisplayConfigurationOutputType::composite,
            {},
            conf_modes1,
            2,
            connector_physical_sizes_mm_after,
            true,
            true,
            geom::Point(),
            2,  // current_mode_index changed to preferred as the list of
                // available modes has also changed
            mir_pixel_format_invalid,
            mir_power_mode_on,
            mir_orientation_normal,
            1.0f,
            mir_form_factor_monitor,
            mir_subpixel_arrangement_unknown,
            {},
            mir_output_gamma_unsupported,
            {}
        },
    };

    mtd::FakeDRMResources& resources(mock_drm.fake_drm);
    auto const syspath = fake_devices.add_device("drm", "card2", NULL, {}, {"DEVTYPE", "drm_minor"});

    resources.reset();
    resources.add_crtc(crtc_ids[0], modes0[1]);
    resources.add_encoder(encoder_ids[0], crtc_ids[0], possible_crtcs_mask_empty);
    resources.add_connector(connector_ids[0], DRM_MODE_CONNECTOR_Composite,
                            DRM_MODE_CONNECTED, encoder_ids[0],
                            modes0, possible_encoder_ids_empty,
                            connector_physical_sizes_mm_before);
    resources.prepare();

    auto display = create_display(create_platform());
    auto conf = display->configuration();
    int output_count = 0;
    conf->for_each_output([&](mg::DisplayConfigurationOutput const& output)
    {
        ASSERT_LT(output_count, noutputs);
        EXPECT_EQ(expected_outputs_before[output_count], output) << "output_count: " << output_count;
        ++output_count;
    });
    EXPECT_EQ(noutputs, output_count);

    // Now simulate a change of monitor with different capabilities where the
    // old current_mode does not exist. Mir should choose the preferred mode.
    resources.reset();
    resources.add_crtc(crtc_ids[0], modes1[1]);
    resources.add_encoder(encoder_ids[0], crtc_ids[0], possible_crtcs_mask_empty);
    resources.add_encoder(encoder_ids[1], 0, possible_crtcs_mask_empty);
    resources.add_connector(connector_ids[0], DRM_MODE_CONNECTOR_Composite,
                            DRM_MODE_CONNECTED, encoder_ids[1],
                            modes1, possible_encoder_ids_empty,
                            connector_physical_sizes_mm_after);
    resources.prepare();

    MainLoop ml;
    mt::Signal handler_signal;
    display->register_configuration_change_handler(ml.ml, [&handler_signal]{handler_signal.raise();});
    fake_devices.emit_device_changed(syspath);
    ASSERT_TRUE(handler_signal.wait_for(10s));

    conf = display->configuration();
    output_count = 0;
    conf->for_each_output([&](mg::DisplayConfigurationOutput const& output)
    {
        ASSERT_LT(output_count, noutputs);
        EXPECT_EQ(expected_outputs_after[output_count], output) << "output_count: " << output_count;
        ++output_count;
    });
    EXPECT_EQ(noutputs, output_count);
}

TEST_F(MesaDisplayConfigurationTest, does_not_query_drm_unnecesarily)
{
    using namespace ::testing;
    using namespace std::chrono_literals;

    uint32_t const crtc_id{10};
    uint32_t const encoder_id{20};
    uint32_t const connector_id{30};
    geom::Size const connector_physical_sizes_mm{480, 270};
    std::vector<uint32_t> possible_encoder_ids_empty;

    uint32_t const possible_crtcs_mask_empty{0};
    mtd::FakeDRMResources& resources(mock_drm.fake_drm);
    resources.reset();
    resources.add_crtc(crtc_id, modes0[1]);
    resources.add_encoder(encoder_id, crtc_id, possible_crtcs_mask_empty);
    resources.add_connector(connector_id, DRM_MODE_CONNECTOR_Composite,
                            DRM_MODE_CONNECTED, encoder_id,
                            modes0, possible_encoder_ids_empty,
                            connector_physical_sizes_mm);
    resources.prepare();

    auto const syspath = fake_devices.add_device("drm", "card2", NULL, {}, {"DEVTYPE", "drm_minor"});

    auto display = create_display(create_platform());

    // No display change reported yet so display should not query drm
    EXPECT_CALL(mock_drm, drmModeGetConnector(_,_)).Times(0);
    display->configuration();

    // Now signal a device change
    MainLoop ml;
    mt::Signal handler_signal;
    display->register_configuration_change_handler(ml.ml, [&handler_signal]{handler_signal.raise();});
    fake_devices.emit_device_changed(syspath);
    ASSERT_TRUE(handler_signal.wait_for(10s));

    EXPECT_CALL(mock_drm, drmModeGetConnector(_,_)).Times(1);
    display->configuration();
}
