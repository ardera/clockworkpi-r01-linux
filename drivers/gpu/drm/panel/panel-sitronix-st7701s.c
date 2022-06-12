// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2017 Free Electrons
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/spi/spi.h>

#include <video/mipi_display.h>

#include <drm/drm_device.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct st7701s {
	struct drm_panel	panel;
	struct gpio_desc	*reset;
	struct spi_device	*spi;
};

enum {
	ST7789V_COMMAND	= 0 << 8,
	ST7789V_DATA	= 1 << 8,
};

#define LCD_WRITE_COMMAND(x)	(ST7789V_COMMAND | (x))
#define LCD_WRITE_DATA(x)	(ST7789V_DATA | (x))

static const u16 st7701s_init_sequence_1[] = {
	LCD_WRITE_COMMAND(0xFF),
	LCD_WRITE_DATA(0x77),
	LCD_WRITE_DATA(0x01),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x10),

	LCD_WRITE_COMMAND(0xC0),
	LCD_WRITE_DATA(0x3B),
	LCD_WRITE_DATA(0x00),

	LCD_WRITE_COMMAND(0xC1),
	LCD_WRITE_DATA(0x0D),
	LCD_WRITE_DATA(0x02),

	LCD_WRITE_COMMAND(0xC2),
	LCD_WRITE_DATA(0x21),
	LCD_WRITE_DATA(0x08),

	// RGB Interface Setting
	// LCD_WRITE_COMMAND(0xC3),
	// LCD_WRITE_DATA(0x02),

	LCD_WRITE_COMMAND(0xCD),
	LCD_WRITE_DATA(0x18),//0F 08-OK  D0-D18

	LCD_WRITE_COMMAND(0xB0),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x11),
	LCD_WRITE_DATA(0x18),
	LCD_WRITE_DATA(0x0E),
	LCD_WRITE_DATA(0x11),
	LCD_WRITE_DATA(0x06),
	LCD_WRITE_DATA(0x07),
	LCD_WRITE_DATA(0x08),
	LCD_WRITE_DATA(0x07),
	LCD_WRITE_DATA(0x22),
	LCD_WRITE_DATA(0x04),
	LCD_WRITE_DATA(0x12),
	LCD_WRITE_DATA(0x0F),
	LCD_WRITE_DATA(0xAA),
	LCD_WRITE_DATA(0x31),
	LCD_WRITE_DATA(0x18),

	LCD_WRITE_COMMAND(0xB1),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x11),
	LCD_WRITE_DATA(0x19),
	LCD_WRITE_DATA(0x0E),
	LCD_WRITE_DATA(0x12),
	LCD_WRITE_DATA(0x07),
	LCD_WRITE_DATA(0x08),
	LCD_WRITE_DATA(0x08),
	LCD_WRITE_DATA(0x08),
	LCD_WRITE_DATA(0x22),
	LCD_WRITE_DATA(0x04),
	LCD_WRITE_DATA(0x11),
	LCD_WRITE_DATA(0x11),
	LCD_WRITE_DATA(0xA9),
	LCD_WRITE_DATA(0x32),
	LCD_WRITE_DATA(0x18),

	LCD_WRITE_COMMAND(0xFF),
	LCD_WRITE_DATA(0x77),
	LCD_WRITE_DATA(0x01),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x11),

	LCD_WRITE_COMMAND(0xB0),
	LCD_WRITE_DATA(0x60),

	LCD_WRITE_COMMAND(0xB1),
	LCD_WRITE_DATA(0x30),

	LCD_WRITE_COMMAND(0xB2),
	LCD_WRITE_DATA(0x87),

	LCD_WRITE_COMMAND(0xB3),
	LCD_WRITE_DATA(0x80),

	LCD_WRITE_COMMAND(0xB5),
	LCD_WRITE_DATA(0x49),

	LCD_WRITE_COMMAND(0xB7),
	LCD_WRITE_DATA(0x85),

	LCD_WRITE_COMMAND(0xB8),
	LCD_WRITE_DATA(0x21),

	LCD_WRITE_COMMAND(0xC1),
	LCD_WRITE_DATA(0x78),

	LCD_WRITE_COMMAND(0xC2),
	LCD_WRITE_DATA(0x78),
};

static const u16 st7701s_init_sequence_2[] = {
	LCD_WRITE_COMMAND(0xE0),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x1B),
	LCD_WRITE_DATA(0x02),

	LCD_WRITE_COMMAND(0xE1),
	LCD_WRITE_DATA(0x08),
	LCD_WRITE_DATA(0xA0),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x07),
	LCD_WRITE_DATA(0xA0),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x44),
	LCD_WRITE_DATA(0x44),

	LCD_WRITE_COMMAND(0xE2),
	LCD_WRITE_DATA(0x11),
	LCD_WRITE_DATA(0x11),
	LCD_WRITE_DATA(0x44),
	LCD_WRITE_DATA(0x44),
	LCD_WRITE_DATA(0xED),
	LCD_WRITE_DATA(0xA0),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0xEC),
	LCD_WRITE_DATA(0xA0),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x00),

	LCD_WRITE_COMMAND(0xE3),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x11),
	LCD_WRITE_DATA(0x11),

	LCD_WRITE_COMMAND(0xE4),
	LCD_WRITE_DATA(0x44),
	LCD_WRITE_DATA(0x44),

	LCD_WRITE_COMMAND(0xE5),
	LCD_WRITE_DATA(0x0A),
	LCD_WRITE_DATA(0xE9),
	LCD_WRITE_DATA(0xD8),
	LCD_WRITE_DATA(0xA0),
	LCD_WRITE_DATA(0x0C),
	LCD_WRITE_DATA(0xEB),
	LCD_WRITE_DATA(0xD8),
	LCD_WRITE_DATA(0xA0),
	LCD_WRITE_DATA(0x0E),
	LCD_WRITE_DATA(0xED),
	LCD_WRITE_DATA(0xD8),
	LCD_WRITE_DATA(0xA0),
	LCD_WRITE_DATA(0x10),
	LCD_WRITE_DATA(0xEF),
	LCD_WRITE_DATA(0xD8),
	LCD_WRITE_DATA(0xA0),

	LCD_WRITE_COMMAND(0xE6),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x11),
	LCD_WRITE_DATA(0x11),

	LCD_WRITE_COMMAND(0xE7),
	LCD_WRITE_DATA(0x44),
	LCD_WRITE_DATA(0x44),

	LCD_WRITE_COMMAND(0xE8),
	LCD_WRITE_DATA(0x09),
	LCD_WRITE_DATA(0xE8),
	LCD_WRITE_DATA(0xD8),
	LCD_WRITE_DATA(0xA0),
	LCD_WRITE_DATA(0x0B),
	LCD_WRITE_DATA(0xEA),
	LCD_WRITE_DATA(0xD8),
	LCD_WRITE_DATA(0xA0),
	LCD_WRITE_DATA(0x0D),
	LCD_WRITE_DATA(0xEC),
	LCD_WRITE_DATA(0xD8),
	LCD_WRITE_DATA(0xA0),
	LCD_WRITE_DATA(0x0F),
	LCD_WRITE_DATA(0xEE),
	LCD_WRITE_DATA(0xD8),
	LCD_WRITE_DATA(0xA0),

	LCD_WRITE_COMMAND(0xEB),
	LCD_WRITE_DATA(0x02),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0xE4),
	LCD_WRITE_DATA(0xE4),
	LCD_WRITE_DATA(0x88),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x40),

	LCD_WRITE_COMMAND(0xEC),
	LCD_WRITE_DATA(0x3C),
	LCD_WRITE_DATA(0x00),

	LCD_WRITE_COMMAND(0xED),
	LCD_WRITE_DATA(0xAB),
	LCD_WRITE_DATA(0x89),
	LCD_WRITE_DATA(0x76),
	LCD_WRITE_DATA(0x54),
	LCD_WRITE_DATA(0x02),
	LCD_WRITE_DATA(0xFF),
	LCD_WRITE_DATA(0xFF),
	LCD_WRITE_DATA(0xFF),
	LCD_WRITE_DATA(0xFF),
	LCD_WRITE_DATA(0xFF),
	LCD_WRITE_DATA(0xFF),
	LCD_WRITE_DATA(0x20),
	LCD_WRITE_DATA(0x45),
	LCD_WRITE_DATA(0x67),
	LCD_WRITE_DATA(0x98),
	LCD_WRITE_DATA(0xBA),

	LCD_WRITE_COMMAND(0xFF),
	LCD_WRITE_DATA(0x77),
	LCD_WRITE_DATA(0x01),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x00),
	LCD_WRITE_DATA(0x00),

	LCD_WRITE_COMMAND(MIPI_DCS_SET_PIXEL_FORMAT),
	LCD_WRITE_DATA(0x66),

	LCD_WRITE_COMMAND(MIPI_DCS_SET_ADDRESS_MODE),
	LCD_WRITE_DATA(0x00),

	LCD_WRITE_COMMAND(MIPI_DCS_ENTER_INVERT_MODE),

	LCD_WRITE_COMMAND(MIPI_DCS_EXIT_SLEEP_MODE),
};

static const u16 st7701s_enable_sequence[] = {
	LCD_WRITE_COMMAND(MIPI_DCS_SET_DISPLAY_ON),
};

static const u16 st7701s_disable_sequence[] = {
	LCD_WRITE_COMMAND(MIPI_DCS_SET_DISPLAY_OFF),
};

static inline struct st7701s *panel_to_st7701s(struct drm_panel *panel)
{
	return container_of(panel, struct st7701s, panel);
}

static int st7701s_spi_write(struct st7701s *ctx, const u16 *data, size_t size)
{
	struct spi_transfer xfer = { };
	struct spi_message msg;

	spi_message_init(&msg);

	xfer.tx_buf = data;
	xfer.bits_per_word = 9;
	xfer.len = size;

	spi_message_add_tail(&xfer, &msg);
	return spi_sync(ctx->spi, &msg);
}

static const struct drm_display_mode default_mode = {
	.clock		= 19800,
	.hdisplay	= 480,
	.hsync_start	= 480 + 60,
	.hsync_end	= 480 + 60 + 12,
	.htotal		= 480 + 60 + 12 + 60,
	.vdisplay	= 480,
	.vsync_start	= 480 + 18,
	.vsync_end	= 480 + 18 + 4,
	.vtotal		= 480 + 18 + 4 + 18,
};

static int st7701s_get_modes(struct drm_panel *panel,
			     struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 72;

	return 1;
}

static int st7701s_prepare(struct drm_panel *panel)
{
	struct st7701s *ctx = panel_to_st7701s(panel);

	gpiod_set_value_cansleep(ctx->reset, 1);
	msleep(20);

	gpiod_set_value_cansleep(ctx->reset, 0);
	msleep(20);

	st7701s_spi_write(ctx, st7701s_init_sequence_1,
			  sizeof(st7701s_init_sequence_1));
	msleep(20);

	st7701s_spi_write(ctx, st7701s_init_sequence_2,
			  sizeof(st7701s_init_sequence_2));
	msleep(120);

	return 0;
}

static int st7701s_enable(struct drm_panel *panel)
{
	struct st7701s *ctx = panel_to_st7701s(panel);

	st7701s_spi_write(ctx, st7701s_enable_sequence,
			  sizeof(st7701s_enable_sequence));
	msleep(20);

	return 0;
}

static int st7701s_disable(struct drm_panel *panel)
{
	struct st7701s *ctx = panel_to_st7701s(panel);

	st7701s_spi_write(ctx, st7701s_disable_sequence,
			  sizeof(st7701s_disable_sequence));

	return 0;
}

static int st7701s_unprepare(struct drm_panel *panel)
{
	return 0;
}

static const struct drm_panel_funcs st7701s_drm_funcs = {
	.disable	= st7701s_disable,
	.enable		= st7701s_enable,
	.get_modes	= st7701s_get_modes,
	.prepare	= st7701s_prepare,
	.unprepare	= st7701s_unprepare,
};

static int st7701s_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct st7701s *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	spi_set_drvdata(spi, ctx);
	ctx->spi = spi;

	ctx->reset = devm_gpiod_get(&spi->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset)) {
		dev_err(&spi->dev, "Couldn't get our reset line\n");
		return PTR_ERR(ctx->reset);
	}

	drm_panel_init(&ctx->panel, dev, &st7701s_drm_funcs,
		       DRM_MODE_CONNECTOR_DPI);

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return ret;

	drm_panel_add(&ctx->panel);

	return 0;
}

static void st7701s_remove(struct spi_device *spi)
{
	struct st7701s *ctx = spi_get_drvdata(spi);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id st7701s_of_match[] = {
	{ .compatible = "sitronix,st7701s" },
	{ }
};
MODULE_DEVICE_TABLE(of, st7701s_of_match);

static const struct spi_device_id st7701s_ids[] = {
	{ "st7701s" },
	{ }
};
MODULE_DEVICE_TABLE(spi, st7701s_ids);

static struct spi_driver st7701s_driver = {
	.probe = st7701s_probe,
	.remove = st7701s_remove,
	.driver = {
		.name = "st7701s",
		.of_match_table = st7701s_of_match,
	},
};
module_spi_driver(st7701s_driver);

MODULE_AUTHOR("Maxime Ripard <maxime.ripard@free-electrons.com>");
MODULE_DESCRIPTION("Sitronix ST7701s LCD Driver");
MODULE_LICENSE("GPL v2");
