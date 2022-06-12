// SPDX-License-Identifier: GPL-2.0+

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/reset.h>

#include <sound/dmaengine_pcm.h>
#include <sound/pcm_params.h>
#include <sound/simple_card_utils.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#define SUN20I_CODEC_DAC_DPC		0x0000
#define SUN20I_CODEC_DAC_DPC_EN_DA		31
#define SUN20I_CODEC_DAC_DPC_HPF_EN		18
#define SUN20I_CODEC_DAC_DPC_DVOL		12
#define SUN20I_CODEC_DAC_VOL_CTRL	0x0004
#define SUN20I_CODEC_DAC_VOL_CTRL_DAC_VOL_SEL	16
#define SUN20I_CODEC_DAC_VOL_CTRL_DAC_VOL_L	8
#define SUN20I_CODEC_DAC_VOL_CTRL_DAC_VOL_R	0
#define SUN20I_CODEC_DAC_FIFOC		0x0010
#define SUN20I_CODEC_DAC_FIFOC_FS		29
#define SUN20I_CODEC_DAC_FIFOC_FIFO_MODE	24
#define SUN20I_CODEC_DAC_FIFOC_DRQ_CLR_CNT	21
#define SUN20I_CODEC_DAC_FIFOC_TRIG_LEVEL	8
#define SUN20I_CODEC_DAC_FIFOC_MONO_EN		6
#define SUN20I_CODEC_DAC_FIFOC_SAMPLE_BITS	5
#define SUN20I_CODEC_DAC_FIFOC_DRQ_EN		4
#define SUN20I_CODEC_DAC_FIFOC_FIFO_FLUSH	0
#define SUN20I_CODEC_DAC_TXDATA		0x0020
#define SUN20I_CODEC_DAC_DEBUG		0x0028
#define SUN20I_CODEC_DAC_DEBUG_DA_SWP		6
#define SUN20I_CODEC_DAC_ADDA_LOOP_MODE		0

#define SUN20I_CODEC_ADC_FIFOC		0x0030
#define SUN20I_CODEC_ADC_FIFOC_FS		29
#define SUN20I_CODEC_ADC_FIFOC_EN_AD		28
#define SUN20I_CODEC_ADC_FIFOC_FIFO_MODE	24
#define SUN20I_CODEC_ADC_FIFOC_SAMPLE_BITS	16
#define SUN20I_CODEC_ADC_FIFOC_TRIG_LEVEL	4
#define SUN20I_CODEC_ADC_FIFOC_DRQ_EN		3
#define SUN20I_CODEC_ADC_FIFOC_FIFO_FLUSH	0
#define SUN20I_CODEC_ADC_VOL_CTRL	0x0034
#define SUN20I_CODEC_ADC_VOL_CTRL_ADC3_VOL	16
#define SUN20I_CODEC_ADC_VOL_CTRL_ADC2_VOL	8
#define SUN20I_CODEC_ADC_VOL_CTRL_ADC1_VOL	0
#define SUN20I_CODEC_ADC_RXDATA		0x0040
#define SUN20I_CODEC_ADC_DEBUG		0x004c
#define SUN20I_CODEC_ADC_DEBUG_AD_SWP1		24
#define SUN20I_CODEC_ADC_DIG_CTRL	0x0050
#define SUN20I_CODEC_ADC_DIG_CTRL_ADC_VOL_EN	16
#define SUN20I_CODEC_ADC_DIG_CTRL_ADC_EN	0

#define SUN20I_CODEC_DAC_DAP_CTRL	0x00f0
#define SUN20I_CODEC_DAC_DAP_CTRL_DAP_EN	31
#define SUN20I_CODEC_DAC_DAP_CTRL_DAP_DRC_EN	29
#define SUN20I_CODEC_DAC_DAP_CTRL_DAP_HPF_EN	28

#define SUN20I_CODEC_ADC_DAP_CTRL	0x00f8
#define SUN20I_CODEC_ADC_DAP_CTRL_DAP0_EN	31
#define SUN20I_CODEC_ADC_DAP_CTRL_DAP0_DRC_EN	29
#define SUN20I_CODEC_ADC_DAP_CTRL_DAP0_HPF_EN	28
#define SUN20I_CODEC_ADC_DAP_CTRL_DAP1_EN	27
#define SUN20I_CODEC_ADC_DAP_CTRL_DAP1_DRC_EN	25
#define SUN20I_CODEC_ADC_DAP_CTRL_DAP1_HPF_EN	24

#define SUN20I_CODEC_ADC1		0x0300
#define SUN20I_CODEC_ADC1_ADC1_EN		31
#define SUN20I_CODEC_ADC1_MICIN1_PGA_EN		30
#define SUN20I_CODEC_ADC1_ADC1_DITHER_EN	29
#define SUN20I_CODEC_ADC1_MICIN1_SIN_EN		28
#define SUN20I_CODEC_ADC1_FMINL_EN		27
#define SUN20I_CODEC_ADC1_FMINL_GAIN		26
#define SUN20I_CODEC_ADC1_DITHER_LEVEL		24
#define SUN20I_CODEC_ADC1_LINEINL_EN		23
#define SUN20I_CODEC_ADC1_LINEINL_GAIN		22
#define SUN20I_CODEC_ADC1_ADC1_PGA_GAIN		8
#define SUN20I_CODEC_ADC2		0x0304
#define SUN20I_CODEC_ADC2_ADC2_EN		31
#define SUN20I_CODEC_ADC2_MICIN2_PGA_EN		30
#define SUN20I_CODEC_ADC2_ADC2_DITHER_EN	29
#define SUN20I_CODEC_ADC2_MICIN2_SIN_EN		28
#define SUN20I_CODEC_ADC2_FMINR_EN		27
#define SUN20I_CODEC_ADC2_FMINR_GAIN		26
#define SUN20I_CODEC_ADC2_DITHER_LEVEL		24
#define SUN20I_CODEC_ADC2_LINEINR_EN		23
#define SUN20I_CODEC_ADC2_LINEINR_GAIN		22
#define SUN20I_CODEC_ADC2_ADC2_PGA_GAIN		8
#define SUN20I_CODEC_ADC3		0x0308
#define SUN20I_CODEC_ADC3_ADC3_EN		31
#define SUN20I_CODEC_ADC3_MICIN3_PGA_EN		30
#define SUN20I_CODEC_ADC3_ADC3_DITHER_EN	29
#define SUN20I_CODEC_ADC3_MICIN3_SIN_EN		28
#define SUN20I_CODEC_ADC3_DITHER_LEVEL		24
#define SUN20I_CODEC_ADC3_ADC3_PGA_GAIN		8

#define SUN20I_CODEC_DAC		0x0310
#define SUN20I_CODEC_DAC_DACL_EN		15
#define SUN20I_CODEC_DAC_DACR_EN		14
#define SUN20I_CODEC_DAC_LINEOUTL_EN		13
#define SUN20I_CODEC_DAC_LMUTE			12
#define SUN20I_CODEC_DAC_LINEOUTR_EN		11
#define SUN20I_CODEC_DAC_RMUTE			10
#define SUN20I_CODEC_DAC_LINEOUTL_DIFFEN	6
#define SUN20I_CODEC_DAC_LINEOUTR_DIFFEN	5
#define SUN20I_CODEC_DAC_LINEOUT_VOL_CTRL	0

#define SUN20I_CODEC_MICBIAS		0x0318
#define SUN20I_CODEC_MICBIAS_SELDETADCFS	28
#define SUN20I_CODEC_MICBIAS_SELDETADCDB	26
#define SUN20I_CODEC_MICBIAS_SELDETADCBF	24
#define SUN20I_CODEC_MICBIAS_JACKDETEN		23
#define SUN20I_CODEC_MICBIAS_SELDETADCDY	21
#define SUN20I_CODEC_MICBIAS_MICADCEN		20
#define SUN20I_CODEC_MICBIAS_POPFREE		19
#define SUN20I_CODEC_MICBIAS_DET_MODE		18
#define SUN20I_CODEC_MICBIAS_AUTOPLEN		17
#define SUN20I_CODEC_MICBIAS_MICDETPL		16
#define SUN20I_CODEC_MICBIAS_HMICBIASEN		15
#define SUN20I_CODEC_MICBIAS_HMICBIASSEL	13
#define SUN20I_CODEC_MICBIAS_HMIC_CHOPPER_EN	12
#define SUN20I_CODEC_MICBIAS_HMIC_CHOPPER_CLK	10
#define SUN20I_CODEC_MICBIAS_MMICBIASEN		7
#define SUN20I_CODEC_MICBIAS_MMICBIASSEL	5
#define SUN20I_CODEC_MICBIAS_MMIC_CHOPPER_EN	4
#define SUN20I_CODEC_MICBIAS_MMIC_CHOPPER_CLK	2

/* TODO */
#define SUN20I_CODEC_RAMP		0x031c
#define SUN20I_CODEC_RAMP_HP_PULL_OUT_EN	15

#define SUN20I_CODEC_HMIC_CTRL		0x0328
#define SUN20I_CODEC_HMIC_CTRL_SAMPLE_SELECT	21
#define SUN20I_CODEC_HMIC_CTRL_MDATA_THRESHOLD	16
#define SUN20I_CODEC_HMIC_CTRL_SF		14
#define SUN20I_CODEC_HMIC_CTRL_M		10
#define SUN20I_CODEC_HMIC_CTRL_N		6
#define SUN20I_CODEC_HMIC_CTRL_THRESH_DEBOUNCE	3
#define SUN20I_CODEC_HMIC_CTRL_JACK_OUT_IRQ_EN	2
#define SUN20I_CODEC_HMIC_CTRL_JACK_IN_IRQ_EN	1
#define SUN20I_CODEC_HMIC_CTRL_MIC_DET_IRQ_EN	0
#define SUN20I_CODEC_HMIC_STS		0x032c
#define SUN20I_CODEC_HMIC_STS_MDATA_DISCARD	13
#define SUN20I_CODEC_HMIC_STS_HMIC_DATA		8
#define SUN20I_CODEC_HMIC_STS_JACK_OUT_IRQ	4
#define SUN20I_CODEC_HMIC_STS_JACK_IN_IRQ	3
#define SUN20I_CODEC_HMIC_STS_MIC_DET_IRQ	0

#define SUN20I_CODEC_HP2		0x0340
#define SUN20I_CODEC_HP2_HPFB_BUF_EN		31
#define SUN20I_CODEC_HP2_HEADPHONE_GAIN		28
#define SUN20I_CODEC_HP2_HPFB_RES		26
#define SUN20I_CODEC_HP2_HP_DRVEN		21
#define SUN20I_CODEC_HP2_HP_DRVOUTEN		20
#define SUN20I_CODEC_HP2_RSWITCH		19
#define SUN20I_CODEC_HP2_RAMPEN			18
#define SUN20I_CODEC_HP2_HPFB_IN_EN		17
#define SUN20I_CODEC_HP2_RAMP_FINAL_CONTROL	16
#define SUN20I_CODEC_HP2_RAMP_OUT_EN		15
#define SUN20I_CODEC_HP2_RAMP_FINAL_STATE_RES	13

/* Not affected by codec bus clock/reset */
#define SUN20I_CODEC_POWER		0x0348
#define SUN20I_CODEC_POWER_ALDO_EN_MASK		BIT(31)
#define SUN20I_CODEC_POWER_HPLDO_EN_MASK	BIT(30)
#define SUN20I_CODEC_POWER_ALDO_VOLTAGE_MASK	GENMASK(14, 12)
#define SUN20I_CODEC_POWER_HPLDO_VOLTAGE_MASK	GENMASK(10, 8)

#define SUN20I_CODEC_ADC_CUR		0x034c

#define SUN20I_CODEC_PCM_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE|\
					 SNDRV_PCM_FMTBIT_S20_LE|\
					 SNDRV_PCM_FMTBIT_S32_LE)

#define DRIVER_NAME			"sun20i-codec"
#define PREFIX				"allwinner,"

/* snd_soc_register_card() takes over drvdata, so the card must be first! */
struct sun20i_codec {
	struct snd_soc_card			card;
	struct snd_soc_dai_link			dai_link;
	struct snd_soc_dai_link_component	dlcs[3];
	struct snd_dmaengine_dai_dma_data	dma_data[2];

	struct clk		*bus_clk;
	struct clk		*adc_clk;
	struct clk		*dac_clk;
	struct reset_control	*reset;
};

static int sun20i_codec_dai_probe(struct snd_soc_dai *dai)
{
	struct sun20i_codec *codec = snd_soc_dai_get_drvdata(dai);

	snd_soc_dai_init_dma_data(dai,
				  &codec->dma_data[SNDRV_PCM_STREAM_PLAYBACK],
				  &codec->dma_data[SNDRV_PCM_STREAM_CAPTURE]);

	return 0;
}

static struct clk *sun20i_codec_get_clk(struct snd_pcm_substream *substream,
					struct snd_soc_dai *dai)
{
	struct sun20i_codec *codec = snd_soc_dai_get_drvdata(dai);

	return substream->stream == SNDRV_PCM_STREAM_CAPTURE ?
		codec->adc_clk : codec->dac_clk;
}

static const unsigned int sun20i_codec_rates[] = {
	 7350,   8000,  11025,  12000,  14700,  16000,  22050,  24000,
	29400,  32000,  44100,  48000,  88200,  96000, 176400, 192000,
};

static const struct snd_pcm_hw_constraint_list sun20i_codec_rate_lists[] = {
	[SNDRV_PCM_STREAM_PLAYBACK] = {
		.list	= sun20i_codec_rates,
		.count	= ARRAY_SIZE(sun20i_codec_rates),
	},
	[SNDRV_PCM_STREAM_CAPTURE] = {
		.list	= sun20i_codec_rates,
		.count	= ARRAY_SIZE(sun20i_codec_rates) - 4, /* max 48 kHz */
	},
};

static int sun20i_codec_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	const struct snd_pcm_hw_constraint_list *list;
	int ret;

	list = &sun20i_codec_rate_lists[substream->stream];
	ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
					 SNDRV_PCM_HW_PARAM_RATE, list);
	if (ret)
		return ret;

	ret = clk_prepare_enable(sun20i_codec_get_clk(substream, dai));
	if (ret)
		return ret;

	return 0;
}

static void sun20i_codec_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	clk_disable_unprepare(sun20i_codec_get_clk(substream, dai));
}

static unsigned int sun20i_codec_get_clk_rate(unsigned int sample_rate)
{
	return (sample_rate % 4000) ? 22579200 : 24576000;
}

static const unsigned short sun20i_codec_divisors[] = {
	512, 1024, 2048, 128,
	768, 1536, 3072, 256,
};

static int sun20i_codec_get_fs(unsigned int clk_rate, unsigned int sample_rate)
{
	unsigned int divisor = clk_rate / sample_rate;
	int i;

	for (i = 0; i < ARRAY_SIZE(sun20i_codec_divisors); ++i)
		if (sun20i_codec_divisors[i] == divisor)
			return i;

	return -EINVAL;
}

static int sun20i_codec_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct sun20i_codec *codec = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_component *component = dai->component;
	unsigned int channels = params_channels(params);
	unsigned int sample_bits = params_width(params);
	unsigned int sample_rate = params_rate(params);
	unsigned int clk_rate = sun20i_codec_get_clk_rate(sample_rate);
	enum dma_slave_buswidth dma_width;
	unsigned int reg;
	int ret, val;

	switch (params_physical_width(params)) {
	case 16:
		dma_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		break;
	case 32:
		dma_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		break;
	default:
		dev_err(dai->dev, "Unsupported physical sample width: %d\n",
			params_physical_width(params));
		return -EINVAL;
	}
	codec->dma_data[substream->stream].addr_width = dma_width;

	ret = clk_set_rate(sun20i_codec_get_clk(substream, dai),
			   sun20i_codec_get_clk_rate(sample_rate));
	if (ret)
		return ret;

	reg = substream->stream == SNDRV_PCM_STREAM_CAPTURE ?
		SUN20I_CODEC_ADC_FIFOC : SUN20I_CODEC_DAC_FIFOC;

	val = sun20i_codec_get_fs(clk_rate, sample_rate);
	if (val < 0)
		return val;
	snd_soc_component_update_bits(component, reg,
				      0x7 << SUN20I_CODEC_DAC_FIFOC_FS,
				      val << SUN20I_CODEC_DAC_FIFOC_FS);

	/* Data is at MSB for full 4-byte samples, otherwise at LSB. */
	val = sample_bits != 32;
	snd_soc_component_update_bits(component, reg,
				      0x1 << SUN20I_CODEC_DAC_FIFOC_FIFO_MODE,
				      val << SUN20I_CODEC_DAC_FIFOC_FIFO_MODE);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		val = sample_bits > 16;
		snd_soc_component_update_bits(component, reg,
					      0x1 << SUN20I_CODEC_ADC_FIFOC_SAMPLE_BITS,
					      val << SUN20I_CODEC_ADC_FIFOC_SAMPLE_BITS);

		val = BIT(channels) - 1;
		snd_soc_component_update_bits(component, SUN20I_CODEC_ADC_DIG_CTRL,
					      0xf << SUN20I_CODEC_ADC_DIG_CTRL_ADC_EN,
					      val << SUN20I_CODEC_ADC_DIG_CTRL_ADC_EN);
	} else {
		val = sample_bits > 16;
		snd_soc_component_update_bits(component, reg,
					      0x1 << SUN20I_CODEC_DAC_FIFOC_SAMPLE_BITS,
					      val << SUN20I_CODEC_DAC_FIFOC_SAMPLE_BITS);

		val = channels == 1;
		snd_soc_component_update_bits(component, reg,
					      0x1 << SUN20I_CODEC_DAC_FIFOC_MONO_EN,
					      val << SUN20I_CODEC_DAC_FIFOC_MONO_EN);
	}

	return 0;
}

static int sun20i_codec_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	unsigned int reg, mask;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		reg  = SUN20I_CODEC_ADC_FIFOC;
		mask = BIT(SUN20I_CODEC_ADC_FIFOC_DRQ_EN);
	} else {
		reg  = SUN20I_CODEC_DAC_FIFOC;
		mask = BIT(SUN20I_CODEC_DAC_FIFOC_DRQ_EN);
	}

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		mask |= BIT(SUN20I_CODEC_DAC_FIFOC_FIFO_FLUSH);
		snd_soc_component_update_bits(component, reg, mask, mask);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		snd_soc_component_update_bits(component, reg, mask, 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct snd_soc_dai_ops sun20i_codec_dai_ops = {
	.startup	= sun20i_codec_startup,
	.shutdown	= sun20i_codec_shutdown,
	.hw_params	= sun20i_codec_hw_params,
	.trigger	= sun20i_codec_trigger,
};

static struct snd_soc_dai_driver sun20i_codec_dai = {
	.name = DRIVER_NAME,
	.probe = sun20i_codec_dai_probe,
	.ops = &sun20i_codec_dai_ops,
	.capture = {
		.stream_name	= "Capture",
		.channels_min	= 1,
		.channels_max	= 3, /* ??? */
		.rates		= SNDRV_PCM_RATE_CONTINUOUS,
		.formats	= SUN20I_CODEC_PCM_FORMATS,
		.sig_bits	= 20,
	},
	.playback = {
		.stream_name	= "Playback",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates		= SNDRV_PCM_RATE_CONTINUOUS,
		.formats	= SUN20I_CODEC_PCM_FORMATS,
		.sig_bits	= 20,
	},
};

static const DECLARE_TLV_DB_SCALE(sun20i_codec_boost_vol_scale, 0, 600, 0);
static const DECLARE_TLV_DB_SCALE(sun20i_codec_digital_vol_scale, -12000, 75, 1);
static const DECLARE_TLV_DB_SCALE(sun20i_codec_headphone_vol_scale, -4200, 600, 0);
/* FIXME */
static const DECLARE_TLV_DB_SCALE(sun20i_codec_line_out_vol_scale, -4650, 150, 1);
/* FIXME */
static const DECLARE_TLV_DB_SCALE(sun20i_codec_pga_vol_scale, 500, 100, 0);

static const char *const sun20i_codec_line_out_mode_enum_text[] = {
	"Single-Ended", "Differential"
};

static const SOC_ENUM_DOUBLE_DECL(sun20i_codec_line_out_mode_enum,
				  SUN20I_CODEC_DAC,
				  SUN20I_CODEC_DAC_LINEOUTL_DIFFEN,
				  SUN20I_CODEC_DAC_LINEOUTR_DIFFEN,
				  sun20i_codec_line_out_mode_enum_text);

static const struct snd_kcontrol_new sun20i_codec_controls[] = {
	/* Digital Controls */
	SOC_DOUBLE_TLV("DAC Playback Volume",
		       SUN20I_CODEC_DAC_VOL_CTRL,
		       SUN20I_CODEC_DAC_VOL_CTRL_DAC_VOL_L,
		       SUN20I_CODEC_DAC_VOL_CTRL_DAC_VOL_R,
		       0xc0, 0, sun20i_codec_digital_vol_scale),
	SOC_SINGLE_TLV("ADC3 Capture Volume",
		       SUN20I_CODEC_ADC_VOL_CTRL,
		       SUN20I_CODEC_ADC_VOL_CTRL_ADC3_VOL,
		       0xc0, 0, sun20i_codec_digital_vol_scale),
	SOC_SINGLE_TLV("ADC2 Capture Volume",
		       SUN20I_CODEC_ADC_VOL_CTRL,
		       SUN20I_CODEC_ADC_VOL_CTRL_ADC2_VOL,
		       0xc0, 0, sun20i_codec_digital_vol_scale),
	SOC_SINGLE_TLV("ADC1 Capture Volume",
		       SUN20I_CODEC_ADC_VOL_CTRL,
		       SUN20I_CODEC_ADC_VOL_CTRL_ADC1_VOL,
		       0xc0, 0, sun20i_codec_digital_vol_scale),

	/* Analog Controls */
	SOC_DOUBLE_R_TLV("FM Capture Volume",
			 SUN20I_CODEC_ADC1,
			 SUN20I_CODEC_ADC2,
			 SUN20I_CODEC_ADC1_FMINL_GAIN,
			 0x1, 0, sun20i_codec_boost_vol_scale),
	SOC_DOUBLE_R_TLV("Line In Capture Volume",
			 SUN20I_CODEC_ADC1,
			 SUN20I_CODEC_ADC2,
			 SUN20I_CODEC_ADC1_LINEINL_GAIN,
			 0x1, 0, sun20i_codec_boost_vol_scale),
	SOC_ENUM("Line Out Mode Playback Enum",
		 sun20i_codec_line_out_mode_enum),
	SOC_SINGLE_TLV("Line Out Playback Volume",
		       SUN20I_CODEC_DAC,
		       SUN20I_CODEC_DAC_LINEOUT_VOL_CTRL,
		       0x1f, 0, sun20i_codec_line_out_vol_scale),
	SOC_SINGLE_TLV("Headphone Playback Volume",
		       SUN20I_CODEC_HP2,
		       SUN20I_CODEC_HP2_HEADPHONE_GAIN,
		       0x7, 1, sun20i_codec_headphone_vol_scale),
};

static const struct snd_kcontrol_new sun20i_codec_line_out_switch =
	SOC_DAPM_DOUBLE("Line Out Playback Switch",
			SUN20I_CODEC_DAC,
			SUN20I_CODEC_DAC_LMUTE,
			SUN20I_CODEC_DAC_RMUTE, 1, 1);

static const struct snd_kcontrol_new sun20i_codec_hp_switch =
	SOC_DAPM_SINGLE("Headphone Playback Switch",
			SUN20I_CODEC_HP2,
			SUN20I_CODEC_HP2_HP_DRVOUTEN, 1, 0);

static const struct snd_kcontrol_new sun20i_codec_adc12_mixer_controls[] = {
	/* ADC1 Only */
	SOC_DAPM_SINGLE("Mic1 Capture Switch",
			SUN20I_CODEC_ADC1,
			SUN20I_CODEC_ADC1_MICIN1_SIN_EN, 1, 0),
	/* Shared */
	SOC_DAPM_DOUBLE_R("FM Capture Switch",
			  SUN20I_CODEC_ADC1,
			  SUN20I_CODEC_ADC2,
			  SUN20I_CODEC_ADC1_FMINL_EN, 1, 0),
	/* Shared */
	SOC_DAPM_DOUBLE_R("Line In Capture Switch",
			  SUN20I_CODEC_ADC1,
			  SUN20I_CODEC_ADC2,
			  SUN20I_CODEC_ADC1_LINEINL_EN, 1, 0),
	/* ADC2 Only */
	SOC_DAPM_SINGLE("Mic2 Capture Switch",
			SUN20I_CODEC_ADC2,
			SUN20I_CODEC_ADC2_MICIN2_SIN_EN, 1, 0),
};

static const struct snd_kcontrol_new sun20i_codec_adc3_mixer_controls[] = {
	SOC_DAPM_SINGLE("Mic3 Capture Switch",
			SUN20I_CODEC_ADC3,
			SUN20I_CODEC_ADC3_MICIN3_SIN_EN, 1, 0),
};

static const struct snd_kcontrol_new sun20i_codec_mic1_volume =
	SOC_DAPM_SINGLE_TLV("Capture Volume",
			    SUN20I_CODEC_ADC1,
			    SUN20I_CODEC_ADC1_ADC1_PGA_GAIN,
			    0x1f, 0, sun20i_codec_pga_vol_scale);

static const struct snd_kcontrol_new sun20i_codec_mic2_volume =
	SOC_DAPM_SINGLE_TLV("Capture Volume",
			    SUN20I_CODEC_ADC2,
			    SUN20I_CODEC_ADC2_ADC2_PGA_GAIN,
			    0x1f, 0, sun20i_codec_pga_vol_scale);

static const struct snd_kcontrol_new sun20i_codec_mic3_volume =
	SOC_DAPM_SINGLE_TLV("Capture Volume",
			    SUN20I_CODEC_ADC3,
			    SUN20I_CODEC_ADC3_ADC3_PGA_GAIN,
			    0x1f, 0, sun20i_codec_pga_vol_scale);

static const struct snd_soc_dapm_widget sun20i_codec_widgets[] = {
	/* Playback */
	SND_SOC_DAPM_OUTPUT("LINEOUTL"),
	SND_SOC_DAPM_OUTPUT("LINEOUTR"),

	SND_SOC_DAPM_SWITCH("LINEOUTL Switch",
			    SUN20I_CODEC_DAC,
			    SUN20I_CODEC_DAC_LINEOUTL_EN, 0,
			    &sun20i_codec_line_out_switch),
	SND_SOC_DAPM_SWITCH("LINEOUTR Switch",
			    SUN20I_CODEC_DAC,
			    SUN20I_CODEC_DAC_LINEOUTR_EN, 0,
			    &sun20i_codec_line_out_switch),

	SND_SOC_DAPM_OUTPUT("HPOUTL"),
	SND_SOC_DAPM_OUTPUT("HPOUTR"),

	SND_SOC_DAPM_SWITCH("HPOUTL Switch",
			    SND_SOC_NOPM, 0, 0, &sun20i_codec_hp_switch),
	SND_SOC_DAPM_SWITCH("HPOUTR Switch",
			    SND_SOC_NOPM, 0, 0, &sun20i_codec_hp_switch),
	SND_SOC_DAPM_SUPPLY("Headphone Driver",
			    SUN20I_CODEC_HP2,
			    SUN20I_CODEC_HP2_HP_DRVEN, 0, NULL, 0),

	SND_SOC_DAPM_DAC("DACL", NULL,
			 SUN20I_CODEC_DAC,
			 SUN20I_CODEC_DAC_DACL_EN, 0),
	SND_SOC_DAPM_DAC("DACR", NULL,
			 SUN20I_CODEC_DAC,
			 SUN20I_CODEC_DAC_DACR_EN, 0),
	SND_SOC_DAPM_SUPPLY("DAC",
			    SUN20I_CODEC_DAC_DPC,
			    SUN20I_CODEC_DAC_DPC_EN_DA, 0, NULL, 0),

	SND_SOC_DAPM_AIF_IN("DACL FIFO", "Playback", 0,
			    SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("DACR FIFO", "Playback", 1,
			    SND_SOC_NOPM, 0, 0),

	/* Capture */
	SND_SOC_DAPM_AIF_OUT("ADC1 FIFO", "Capture", 0,
			     SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("ADC2 FIFO", "Capture", 1,
			     SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("ADC3 FIFO", "Capture", 2,
			     SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("ADC1", NULL,
			 SUN20I_CODEC_ADC1,
			 SUN20I_CODEC_ADC1_ADC1_EN, 0),
	SND_SOC_DAPM_ADC("ADC2", NULL,
			 SUN20I_CODEC_ADC2,
			 SUN20I_CODEC_ADC2_ADC2_EN, 0),
	SND_SOC_DAPM_ADC("ADC3", NULL,
			 SUN20I_CODEC_ADC3,
			 SUN20I_CODEC_ADC3_ADC3_EN, 0),
	SND_SOC_DAPM_SUPPLY("ADC",
			    SUN20I_CODEC_ADC_FIFOC,
			    SUN20I_CODEC_ADC_FIFOC_EN_AD, 0, NULL, 0),

	SND_SOC_DAPM_MIXER_NAMED_CTL("ADC1 Mixer", SND_SOC_NOPM, 0, 0,
				     sun20i_codec_adc12_mixer_controls, 3),
	SND_SOC_DAPM_MIXER_NAMED_CTL("ADC2 Mixer", SND_SOC_NOPM, 0, 0,
				     sun20i_codec_adc12_mixer_controls + 1, 3),
	SND_SOC_DAPM_MIXER_NAMED_CTL("ADC3 Mixer", SND_SOC_NOPM, 0, 0,
				     sun20i_codec_adc3_mixer_controls,
				     ARRAY_SIZE(sun20i_codec_adc3_mixer_controls)),

	SND_SOC_DAPM_PGA("Mic1",
			 SUN20I_CODEC_ADC1,
			 SUN20I_CODEC_ADC1_MICIN1_PGA_EN, 0,
			 &sun20i_codec_mic1_volume, 1),
	SND_SOC_DAPM_PGA("Mic2",
			 SUN20I_CODEC_ADC2,
			 SUN20I_CODEC_ADC2_MICIN2_PGA_EN, 0,
			 &sun20i_codec_mic2_volume, 1),
	SND_SOC_DAPM_PGA("Mic3",
			 SUN20I_CODEC_ADC3,
			 SUN20I_CODEC_ADC3_MICIN3_PGA_EN, 0,
			 &sun20i_codec_mic3_volume, 1),

	SND_SOC_DAPM_INPUT("MICIN1"),
	SND_SOC_DAPM_INPUT("MICIN2"),
	SND_SOC_DAPM_INPUT("MICIN3"),

	SND_SOC_DAPM_INPUT("FMINL"),
	SND_SOC_DAPM_INPUT("FMINR"),

	SND_SOC_DAPM_INPUT("LINEINL"),
	SND_SOC_DAPM_INPUT("LINEINR"),

	SND_SOC_DAPM_SUPPLY("HBIAS",
			    SUN20I_CODEC_MICBIAS,
			    SUN20I_CODEC_MICBIAS_HMICBIASEN, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MBIAS",
			    SUN20I_CODEC_MICBIAS,
			    SUN20I_CODEC_MICBIAS_MMICBIASEN, 0, NULL, 0),

	SND_SOC_DAPM_REGULATOR_SUPPLY("avcc", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("hpvcc", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("vdd33", 0, 0),
};

static const struct snd_soc_dapm_route sun20i_codec_routes[] = {
	/* Playback */
	{ "LINEOUTL", NULL, "LINEOUTL Switch" },
	{ "LINEOUTR", NULL, "LINEOUTR Switch" },

	{ "LINEOUTL Switch", "Line Out Playback Switch", "DACL" },
	{ "LINEOUTR Switch", "Line Out Playback Switch", "DACR" },

	{ "HPOUTL", NULL, "HPOUTL Switch" },
	{ "HPOUTR", NULL, "HPOUTR Switch" },

	{ "HPOUTL Switch", "Headphone Playback Switch", "DACL" },
	{ "HPOUTR Switch", "Headphone Playback Switch", "DACR" },
	{ "HPOUTL Switch", NULL, "Headphone Driver" },
	{ "HPOUTR Switch", NULL, "Headphone Driver" },
	{ "Headphone Driver", NULL, "hpvcc" },

	{ "DACL", NULL, "DACL FIFO" },
	{ "DACR", NULL, "DACR FIFO" },
	{ "DACL", NULL, "DAC" },
	{ "DACR", NULL, "DAC" },
	{ "DACL", NULL, "avcc" },
	{ "DACR", NULL, "avcc" },

	/* Capture */
	{ "ADC1 FIFO", NULL, "ADC1" },
	{ "ADC2 FIFO", NULL, "ADC2" },
	{ "ADC3 FIFO", NULL, "ADC3" },

	{ "ADC1", NULL, "ADC1 Mixer" },
	{ "ADC2", NULL, "ADC2 Mixer" },
	{ "ADC3", NULL, "ADC3 Mixer" },
	{ "ADC1", NULL, "ADC" },
	{ "ADC2", NULL, "ADC" },
	{ "ADC3", NULL, "ADC" },
	{ "ADC1", NULL, "avcc" },
	{ "ADC2", NULL, "avcc" },
	{ "ADC3", NULL, "avcc" },

	{ "ADC1 Mixer", "Mic1 Capture Switch", "Mic1" },
	{ "ADC2 Mixer", "Mic2 Capture Switch", "Mic2" },
	{ "ADC3 Mixer", "Mic3 Capture Switch", "Mic3" },
	{ "ADC1 Mixer", "FM Capture Switch", "FMINL" },
	{ "ADC2 Mixer", "FM Capture Switch", "FMINR" },
	{ "ADC1 Mixer", "Line In Capture Switch", "LINEINL" },
	{ "ADC2 Mixer", "Line In Capture Switch", "LINEINR" },

	{ "Mic1", NULL, "MICIN1" },
	{ "Mic2", NULL, "MICIN2" },
	{ "Mic3", NULL, "MICIN3" },

	{ "HBIAS", NULL, "vdd33" },
	{ "MBIAS", NULL, "vdd33" },
};

static int sun20i_codec_component_probe(struct snd_soc_component *component)
{
	struct sun20i_codec *codec = snd_soc_component_get_drvdata(component);
	int ret;

	ret = reset_control_deassert(codec->reset);
	if (ret)
		return ret;

	ret = clk_prepare_enable(codec->bus_clk);
	if (ret)
		goto err_assert_reset;

	/* Enable digital volume control. */
	snd_soc_component_update_bits(component, SUN20I_CODEC_DAC_VOL_CTRL,
				      0x1 << SUN20I_CODEC_DAC_VOL_CTRL_DAC_VOL_SEL,
				      0x1 << SUN20I_CODEC_DAC_VOL_CTRL_DAC_VOL_SEL);
	snd_soc_component_update_bits(component, SUN20I_CODEC_ADC_DIG_CTRL,
				      0x3 << SUN20I_CODEC_ADC_DIG_CTRL_ADC_VOL_EN,
				      0x3 << SUN20I_CODEC_ADC_DIG_CTRL_ADC_VOL_EN);

	/* Maaagic... */
	snd_soc_component_update_bits(component, SUN20I_CODEC_RAMP,
				      BIT(1) | BIT(0), BIT(0));

	return 0;

err_assert_reset:
	reset_control_assert(codec->reset);

	return ret;
}

static void sun20i_codec_component_remove(struct snd_soc_component *component)
{
	struct sun20i_codec *codec = snd_soc_component_get_drvdata(component);

	clk_disable_unprepare(codec->bus_clk);
	reset_control_assert(codec->reset);
}

static const struct snd_soc_component_driver sun20i_codec_component = {
	.controls		= sun20i_codec_controls,
	.num_controls		= ARRAY_SIZE(sun20i_codec_controls),
	.dapm_widgets		= sun20i_codec_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(sun20i_codec_widgets),
	.dapm_routes		= sun20i_codec_routes,
	.num_dapm_routes	= ARRAY_SIZE(sun20i_codec_routes),
	.probe			= sun20i_codec_component_probe,
	.remove			= sun20i_codec_component_remove,
};

static int sun20i_codec_init_card(struct device *dev,
				  struct sun20i_codec *codec)
{
	struct snd_soc_dai_link *dai_link = &codec->dai_link;
	struct snd_soc_card *card = &codec->card;
	int ret;

	codec->dlcs[0].of_node	= dev->of_node;
	codec->dlcs[0].dai_name	= DRIVER_NAME;
	codec->dlcs[1].name	= "snd-soc-dummy";
	codec->dlcs[1].dai_name	= "snd-soc-dummy-dai";
	codec->dlcs[2].of_node	= dev->of_node;

	dai_link->name		= DRIVER_NAME;
	dai_link->stream_name	= DRIVER_NAME;
	dai_link->cpus		= &codec->dlcs[0];
	dai_link->num_cpus	= 1;
	dai_link->codecs	= &codec->dlcs[1];
	dai_link->num_codecs	= 1;
	dai_link->platforms	= &codec->dlcs[2];
	dai_link->num_platforms	= 1;

	card->name		= DRIVER_NAME;
	card->dev		= dev;
	card->owner		= THIS_MODULE;
	card->dai_link		= dai_link;
	card->num_links		= 1;
	card->fully_routed	= true;

	ret = snd_soc_of_parse_audio_simple_widgets(card, PREFIX "widgets");
	if (ret)
		return ret;

	ret = snd_soc_of_parse_audio_routing(card, PREFIX "routing");
	if (ret)
		return ret;

	ret = snd_soc_of_parse_aux_devs(card, PREFIX "aux-devs");
	if (ret)
		return ret;

	return 0;
}

static const struct regmap_config sun20i_codec_regmap_config = {
	.reg_bits		= 32,
	.reg_stride		= 4,
	.val_bits		= 32,
	.max_register		= SUN20I_CODEC_ADC_CUR,
};

static const struct regulator_ops sun20i_codec_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
};

static const struct regulator_desc sun20i_codec_ldos[] = {
	{
		.name		= "aldo",
		.supply_name	= "vdd33",
		.of_match	= "aldo",
		.regulators_node = "regulators",
		.ops		= &sun20i_codec_ldo_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
		.n_voltages	= BIT(3),
		.min_uV		= 1650000,
		.uV_step	= 50000,
		.vsel_reg	= SUN20I_CODEC_POWER,
		.vsel_mask	= SUN20I_CODEC_POWER_ALDO_VOLTAGE_MASK,
		.enable_reg	= SUN20I_CODEC_POWER,
		.enable_mask	= SUN20I_CODEC_POWER_ALDO_EN_MASK,
	},
	{
		.name		= "hpldo",
		.supply_name	= "hpldoin",
		.of_match	= "hpldo",
		.regulators_node = "regulators",
		.ops		= &sun20i_codec_ldo_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
		.n_voltages	= BIT(3),
		.min_uV		= 1650000,
		.uV_step	= 50000,
		.vsel_reg	= SUN20I_CODEC_POWER,
		.vsel_mask	= SUN20I_CODEC_POWER_HPLDO_VOLTAGE_MASK,
		.enable_reg	= SUN20I_CODEC_POWER,
		.enable_mask	= SUN20I_CODEC_POWER_HPLDO_EN_MASK,
	},
};

static int sun20i_codec_probe(struct platform_device *pdev)
{
	struct regulator_config config = { .dev = &pdev->dev };
	struct device *dev = &pdev->dev;
	struct sun20i_codec *codec;
	struct regulator_dev *rdev;
	struct regmap *regmap;
	struct resource *res;
	void __iomem *base;
	int i, ret;

	codec = devm_kzalloc(dev, sizeof(*codec), GFP_KERNEL);
	if (!codec)
		return -ENOMEM;

	dev_set_drvdata(dev, codec);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return dev_err_probe(dev, PTR_ERR(base),
				     "Failed to map registers\n");

	regmap = devm_regmap_init_mmio(dev, base,
				       &sun20i_codec_regmap_config);
	if (IS_ERR(regmap))
		return dev_err_probe(dev, PTR_ERR(regmap),
				     "Failed to create regmap\n");

	codec->bus_clk = devm_clk_get(dev, "bus");
	if (IS_ERR(codec->bus_clk))
		return dev_err_probe(dev, PTR_ERR(codec->bus_clk),
				     "Failed to get bus clock\n");

	codec->adc_clk = devm_clk_get(dev, "adc");
	if (IS_ERR(codec->adc_clk))
		return dev_err_probe(dev, PTR_ERR(codec->adc_clk),
				     "Failed to get ADC clock\n");

	codec->dac_clk = devm_clk_get(dev, "dac");
	if (IS_ERR(codec->dac_clk))
		return dev_err_probe(dev, PTR_ERR(codec->dac_clk),
				     "Failed to get DAC clock\n");

	codec->reset = devm_reset_control_get_exclusive(dev, NULL);
	if (IS_ERR(codec->reset))
		return dev_err_probe(dev, PTR_ERR(codec->reset),
				     "Failed to get reset\n");

	for (i = 0; i < ARRAY_SIZE(sun20i_codec_ldos); ++i) {
		const struct regulator_desc *desc = &sun20i_codec_ldos[i];

		rdev = devm_regulator_register(dev, desc, &config);
		if (IS_ERR(rdev))
			return PTR_ERR(rdev);
	}

	ret = devm_snd_soc_register_component(dev, &sun20i_codec_component,
					      &sun20i_codec_dai, 1);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to register component\n");

	codec->dma_data[SNDRV_PCM_STREAM_PLAYBACK].addr =
			res->start + SUN20I_CODEC_DAC_TXDATA;
	codec->dma_data[SNDRV_PCM_STREAM_PLAYBACK].maxburst = 8;
	codec->dma_data[SNDRV_PCM_STREAM_CAPTURE].addr =
			res->start + SUN20I_CODEC_ADC_RXDATA;
	codec->dma_data[SNDRV_PCM_STREAM_CAPTURE].maxburst = 8;

	ret = devm_snd_dmaengine_pcm_register(dev, NULL, 0);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to register PCM\n");

	ret = sun20i_codec_init_card(dev, codec);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to initialize card\n");

	ret = devm_snd_soc_register_card(dev, &codec->card);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to register card\n");

	return 0;
}

static const struct of_device_id sun20i_codec_of_match[] = {
	{ .compatible = "allwinner,sun20i-d1-audio-codec" },
	{}
};
MODULE_DEVICE_TABLE(of, sun20i_codec_of_match);

static struct platform_driver sun20i_codec_driver = {
	.driver	= {
		.name		= DRIVER_NAME,
		.of_match_table	= sun20i_codec_of_match,
	},
	.probe	= sun20i_codec_probe,
};
module_platform_driver(sun20i_codec_driver);

MODULE_DESCRIPTION("Allwinner D1 (sun20i) codec driver");
MODULE_AUTHOR("Samuel Holland <samuel@sholland.org>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sun20i-codec");
