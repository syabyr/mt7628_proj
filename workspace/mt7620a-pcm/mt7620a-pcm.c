/*
 *  Copyright (C) 2010, Lars-Peter Clausen <lars@metafoo.de>
 *  Copyright (C) 2016 Michael Lee <igvtee@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <sound/pcm_params.h>
#include <sound/dmaengine_pcm.h>

#include <asm/mach-ralink/ralink_regs.h>

#define DRV_NAME "ralink-pcm"

#define PCM_GLB_CFG 0x00
#define PCM_GLB_CFG_EN BIT(31)
#define PCM_GLB_CFG_DMA_EN BIT(30)
#define PCM_GLB_CFG_LBK_EN BIT(29)
#define PCM_GLB_CFG_EXT_LBK_EN BIT(28)


#define PCM_GLB_CFG_CH0_TX_EN BIT(8)
#define PCM_GLB_CFG_CH0_RX_EN BIT(0)

#define PCM_GLB_CFG_RFF_THRES 20
#define PCM_GLB_CFG_TFF_THRES 16

#define PCM_GLB_CFG_DFT_THRES	(4 << PCM_GLB_CFG_RFF_THRES) | \
					(4 << PCM_GLB_CFG_TFF_THRES)


#define PCM_PCM_CFG 0x04
#define PCM_PCM_CFG_CLKOUT_EN BIT(30)
#define PCM_PCM_CFG_EXT_FSYNC BIT(27)
#define PCM_PCM_CFG_LONG_FSYNC BIT(26)
#define PCM_PCM_CFG_FSYNC_POL BIT(25)



#define PCM_INT_STATUS 0x08
#define PCM_INT_EN 0x0C
#define PCM_FF_STATUS 0x10
#define PCM_CH0_CFG 0x20
#define PCM_CH1_CFG 0x24
#define PCM_FSYNC_CFG 0x30
#define PCM_CH_CFG2 0x34

#define PCM_DIVCOMP_CFG 0x50
#define PCM_DIVCOMP_CFG_CLK_EN BIT(31)

#define PCM_DIVINT_CFG 0x54
#define PCM_DIGDELAY_CFG 0x60
#define PCM_CH0_FIFO 0x80
#define PCM_CH1_FIFO 0x84
#define PCM_CH2_FIFO 0x88
#define PCM_CH3_FIFO 0x8c

#define PCM_CHA1_FF_STATUS 0x110
#define PCM_CHB1_FF_STATUS 0x113
#define PCM_CHA1_CFG 0x120
#define PCM_CHB1_CFG 0x124
#define PCM_CHA1_CFG2 0x134
#define PCM_CHB1_CFG2 0x138

/////////////////////////////

#define pcm_REG_CFG0		0x00
#define pcm_REG_INT_STATUS	0x04
#define pcm_REG_INT_EN		0x08
#define pcm_REG_FF_STATUS	0x0c
#define pcm_REG_CFG1		0x18
#define pcm_REG_DIVCMP		0x20
#define pcm_REG_DIVINT		0x24

/* pcm_REG_CFG0 */
#define pcm_REG_CFG0_EN		BIT(31)
#define pcm_REG_CFG0_DMA_EN	BIT(30)
#define pcm_REG_CFG0_BYTE_SWAP	BIT(28)
#define pcm_REG_CFG0_TX_EN	BIT(24)
#define pcm_REG_CFG0_RX_EN	BIT(20)
#define pcm_REG_CFG0_SLAVE	BIT(16)
#define pcm_REG_CFG0_RX_THRES	12
#define pcm_REG_CFG0_TX_THRES	4
#define pcm_REG_CFG0_THRES_MASK	(0xf << pcm_REG_CFG0_RX_THRES) | \
	(4 << pcm_REG_CFG0_TX_THRES)
#define pcm_REG_CFG0_DFT_THRES	(4 << pcm_REG_CFG0_RX_THRES) | \
	(4 << pcm_REG_CFG0_TX_THRES)
/* RT305x */
#define pcm_REG_CFG0_CLK_DIS	BIT(8)
#define pcm_REG_CFG0_TXCH_SWAP	BIT(3)
#define pcm_REG_CFG0_TXCH1_OFF	BIT(2)
#define pcm_REG_CFG0_TXCH0_OFF	BIT(1)
#define pcm_REG_CFG0_SLAVE_EN	BIT(0)
/* RT3883 */
#define pcm_REG_CFG0_RXCH_SWAP	BIT(11)
#define pcm_REG_CFG0_RXCH1_OFF	BIT(10)
#define pcm_REG_CFG0_RXCH0_OFF	BIT(9)
#define pcm_REG_CFG0_WS_INV	BIT(0)
/* MT7628 */
#define pcm_REG_CFG0_FMT_LE	BIT(29)
#define pcm_REG_CFG0_SYS_BE	BIT(28)
#define pcm_REG_CFG0_NORM_24	BIT(18)
#define pcm_REG_CFG0_DATA_24	BIT(17)

/* pcm_REG_INT_STATUS */
#define pcm_REG_INT_RX_FAULT	BIT(7)
#define pcm_REG_INT_RX_OVRUN	BIT(6)
#define pcm_REG_INT_RX_UNRUN	BIT(5)
#define pcm_REG_INT_RX_THRES	BIT(4)
#define pcm_REG_INT_TX_FAULT	BIT(3)
#define pcm_REG_INT_TX_OVRUN	BIT(2)
#define pcm_REG_INT_TX_UNRUN	BIT(1)
#define pcm_REG_INT_TX_THRES	BIT(0)
#define pcm_REG_INT_TX_MASK	0xf
#define pcm_REG_INT_RX_MASK	0xf0

/* pcm_REG_INT_STATUS */
#define pcm_RX_AVCNT(x)		((x >> 4) & 0xf)
#define pcm_TX_AVCNT(x)		(x & 0xf)
/* MT7628 */
#define MT7628_pcm_RX_AVCNT(x)	((x >> 8) & 0x1f)
#define MT7628_pcm_TX_AVCNT(x)	(x & 0x1f)

/* pcm_REG_CFG1 */
#define pcm_REG_CFG1_LBK	BIT(31)
#define pcm_REG_CFG1_EXTLBK	BIT(30)
/* RT3883 */
#define pcm_REG_CFG1_LEFT_J	BIT(0)
#define pcm_REG_CFG1_RIGHT_J	BIT(1)
#define pcm_REG_CFG1_FMT_MASK	0x3

/* pcm_REG_DIVCMP */
#define pcm_REG_DIVCMP_CLKEN	BIT(31)
#define pcm_REG_DIVCMP_DIVCOMP_MASK	0x1ff

/* pcm_REG_DIVINT */
#define pcm_REG_DIVINT_MASK	0x3ff

/* BCLK dividers */
#define RALINK_pcm_DIVCMP	0
#define RALINK_pcm_DIVINT	1

/* FIFO */
#define RALINK_pcm_FIFO_SIZE	32

/* feature flags */
#define RALINK_FLAGS_TXONLY	BIT(0)
#define RALINK_FLAGS_LEFT_J	BIT(1)
#define RALINK_FLAGS_RIGHT_J	BIT(2)
#define RALINK_FLAGS_ENDIAN	BIT(3)
#define RALINK_FLAGS_24BIT	BIT(4)

#define RALINK_pcm_INT_EN	0

struct ralink_pcm_stats {
	u32 dmafault;
	u32 overrun;
	u32 underrun;
	u32 belowthres;
};

struct ralink_pcm {
	struct device *dev;
	void __iomem *regs;
	struct clk *clk;
	struct regmap *regmap;
	u32 flags;
	unsigned int fmt;
	u16 txdma_req;
	u16 rxdma_req;

	dma_addr_t phys_base;

	struct snd_dmaengine_dai_dma_data playback_dma_data;
	struct snd_dmaengine_dai_dma_data capture_dma_data;


	struct dentry *dbg_dir;
        struct dentry *dbg_stats;
	struct ralink_pcm_stats txstats;
	struct ralink_pcm_stats rxstats;
};


static inline void ralink_pcm_write(const struct ralink_pcm *pcm,
	unsigned int reg, uint32_t value)
{
	//writel(value, pcm->regs + reg);
	regmap_write(pcm->regmap,reg,value);
}


static void ralink_pcm_dump_regs(struct ralink_pcm *pcm)
{
	u32 buf[32];
	int ret;

	ret = regmap_bulk_read(pcm->regmap, PCM_GLB_CFG,
			buf, ARRAY_SIZE(buf));

	printk(KERN_ALERT "GLB_CFG: %08x, PCM_CFG: %08x, INT_STATUS: %08x, " \
			"INT_EN: %08x, CHA0_FF_STATUS: %08x, CHB0_FF_STATUS: %08x, " \
			"CHA0_CFG: %08x, CHB0_CFG: %08x, FSYNC_CFG: %08x, " \
			"CHA0_CFG2: %08x, CHB0_CFG2: %08x, IP_INFO: %08x\n",
			buf[0], buf[1], buf[2], buf[3], buf[4],
			buf[5], buf[8], buf[9], buf[12], buf[13], buf[14], buf[16]);
}

static int ralink_pcm_set_sysclk(struct snd_soc_dai *dai,
                              int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int ralink_pcm_set_sys_bclk(struct snd_soc_dai *dai, int width, int rate)
{
	struct ralink_pcm *pcm = snd_soc_dai_get_drvdata(dai);
	unsigned long clk = clk_get_rate(pcm->clk);
	int div;
	uint32_t data;


	/* FREQOUT = FREQIN / (pcm_CLK_DIV + 1) */
	div = (clk / rate ) - 1;

	data = rt_sysc_r32(0x30);
	data &= (0xff << 8);
	data |= (0x1 << 15) | (div << 8);
	rt_sysc_w32(data, 0x30);

	/* enable clock */
	regmap_update_bits(pcm->regmap, pcm_REG_CFG0, pcm_REG_CFG0_CLK_DIS, 0);

	dev_dbg(pcm->dev, "clk: %lu, rate: %u, div: %d\n",
			clk, rate, div);

	return 0;
}

static int ralink_pcm_set_bclk(struct snd_soc_dai *dai, int width, int rate)
{
	struct ralink_pcm *pcm = snd_soc_dai_get_drvdata(dai);
	unsigned long clk = clk_get_rate(pcm->clk);
	int divint, divcomp;

	/* disable clock at slave mode */
	if ((pcm->fmt & SND_SOC_DAIFMT_MASTER_MASK) ==
			SND_SOC_DAIFMT_CBM_CFM) {
		regmap_update_bits(pcm->regmap, pcm_REG_DIVCMP,
				pcm_REG_DIVCMP_CLKEN, 0);
		return 0;
	}

	/* FREQOUT = FREQIN * (1/2) * (1/(DIVINT + DIVCOMP/512)) */
	clk = clk / (2 * 2 * width);
	divint = clk / rate;
	divcomp = ((clk % rate) * 512) / rate;

	if ((divint > pcm_REG_DIVINT_MASK) ||
			(divcomp > pcm_REG_DIVCMP_DIVCOMP_MASK))
		return -EINVAL;

	regmap_update_bits(pcm->regmap, pcm_REG_DIVINT,
			pcm_REG_DIVINT_MASK, divint);
	regmap_update_bits(pcm->regmap, pcm_REG_DIVCMP,
			pcm_REG_DIVCMP_DIVCOMP_MASK, divcomp);

	/* enable clock */
	regmap_update_bits(pcm->regmap, pcm_REG_DIVCMP, pcm_REG_DIVCMP_CLKEN,
			pcm_REG_DIVCMP_CLKEN);

	dev_dbg(pcm->dev, "clk: %lu, rate: %u, int: %d, comp: %d\n",
			clk_get_rate(pcm->clk), rate, divint, divcomp);

	return 0;
}

static int ralink_pcm_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct ralink_pcm *pcm = snd_soc_dai_get_drvdata(dai);
	unsigned int cfg0 = 0, cfg1 = 0;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		if (pcm->flags & RALINK_FLAGS_TXONLY)
			cfg0 |= pcm_REG_CFG0_SLAVE_EN;
		else
			cfg0 |= pcm_REG_CFG0_SLAVE;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		if (pcm->flags & RALINK_FLAGS_RIGHT_J) {
			cfg1 |= pcm_REG_CFG1_RIGHT_J;
			break;
		}
		return -EINVAL;
	case SND_SOC_DAIFMT_LEFT_J:
		if (pcm->flags & RALINK_FLAGS_LEFT_J) {
			cfg1 |= pcm_REG_CFG1_LEFT_J;
			break;
		}
		return -EINVAL;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		return -EINVAL;
	}

	if (pcm->flags & RALINK_FLAGS_TXONLY) {
		regmap_update_bits(pcm->regmap, pcm_REG_CFG0,
				pcm_REG_CFG0_SLAVE_EN, cfg0);
	} else {
		regmap_update_bits(pcm->regmap, pcm_REG_CFG0,
				pcm_REG_CFG0_SLAVE, cfg0);
	}
	regmap_update_bits(pcm->regmap, pcm_REG_CFG1,
			pcm_REG_CFG1_FMT_MASK, cfg1);
	pcm->fmt = fmt;

	return 0;
}

static int ralink_pcm_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct ralink_pcm *pcm = snd_soc_dai_get_drvdata(dai);

	if (dai->active)
		return 0;

	/* setup status interrupt */
#if (RALINK_pcm_INT_EN)
	regmap_write(pcm->regmap, pcm_REG_INT_EN, 0xff);
#else
	regmap_write(pcm->regmap, pcm_REG_INT_EN, 0x0);
#endif

	/* enable */
	regmap_update_bits(pcm->regmap, pcm_REG_CFG0,
			pcm_REG_CFG0_EN | pcm_REG_CFG0_DMA_EN |
			pcm_REG_CFG0_THRES_MASK,
			pcm_REG_CFG0_EN | pcm_REG_CFG0_DMA_EN |
			pcm_REG_CFG0_DFT_THRES);

	return 0;
}

static void ralink_pcm_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct ralink_pcm *pcm = snd_soc_dai_get_drvdata(dai);

	/* If both streams are stopped, disable module and clock */
	if (dai->active)
		return;

	/*
	 * datasheet mention when disable all control regs are cleared
	 * to initial values. need reinit at startup.
	 */
	regmap_update_bits(pcm->regmap, pcm_REG_CFG0, pcm_REG_CFG0_EN, 0);
}

static int ralink_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct ralink_pcm *pcm = snd_soc_dai_get_drvdata(dai);
	int width;
	int ret;

	width = params_width(params);
	switch (width) {
	case 16:
		if (pcm->flags & RALINK_FLAGS_24BIT)
			regmap_update_bits(pcm->regmap, pcm_REG_CFG0,
					pcm_REG_CFG0_DATA_24, 0);
		break;
	case 24:
		if (pcm->flags & RALINK_FLAGS_24BIT) {
			regmap_update_bits(pcm->regmap, pcm_REG_CFG0,
					pcm_REG_CFG0_DATA_24,
					pcm_REG_CFG0_DATA_24);
			break;
		}
		return -EINVAL;
	default:
		return -EINVAL;
	}

	switch (params_channels(params)) {
	case 2:
		break;
	default:
		return -EINVAL;
	}

	if (pcm->flags & RALINK_FLAGS_ENDIAN) {
		/* system endian */
#ifdef SNDRV_LITTLE_ENDIAN
		regmap_update_bits(pcm->regmap, pcm_REG_CFG0,
				pcm_REG_CFG0_SYS_BE, 0);
#else
		regmap_update_bits(pcm->regmap, pcm_REG_CFG0,
				pcm_REG_CFG0_SYS_BE,
				pcm_REG_CFG0_SYS_BE);
#endif

		/* data endian */
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
		case SNDRV_PCM_FORMAT_S24_LE:
			regmap_update_bits(pcm->regmap, pcm_REG_CFG0,
					pcm_REG_CFG0_FMT_LE,
					pcm_REG_CFG0_FMT_LE);
			break;
		case SNDRV_PCM_FORMAT_S16_BE:
		case SNDRV_PCM_FORMAT_S24_BE:
			regmap_update_bits(pcm->regmap, pcm_REG_CFG0,
					pcm_REG_CFG0_FMT_LE, 0);
			break;
		default:
			return -EINVAL;
		}
	}

	/* setup bclk rate */
	if (pcm->flags & RALINK_FLAGS_TXONLY)
		ret = ralink_pcm_set_sys_bclk(dai, width, params_rate(params));
	else
		ret = ralink_pcm_set_bclk(dai, width, params_rate(params));

	return ret;
}

static int ralink_pcm_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	struct ralink_pcm *pcm = snd_soc_dai_get_drvdata(dai);
	unsigned int mask, val;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		mask = pcm_REG_CFG0_TX_EN;
	else
		mask = pcm_REG_CFG0_RX_EN;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		val = mask;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		val = 0;
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(pcm->regmap, pcm_REG_CFG0, mask, val);

	return 0;
}

//配置dma相关信息
static void ralink_pcm_init_dma_data(struct ralink_pcm *pcm,
		struct resource *res)
{
	struct snd_dmaengine_dai_dma_data *dma_data;

	/* Playback */
	dma_data = &pcm->playback_dma_data;
	dma_data->addr = res->start + PCM_CH0_FIFO;
	dma_data->addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_data->maxburst = 16;
	dma_data->slave_id = pcm->txdma_req;


	/* Capture */
	dma_data = &pcm->capture_dma_data;
	dma_data->addr = res->start + PCM_CH0_FIFO;
	dma_data->addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_data->maxburst = 16;
	dma_data->slave_id = pcm->rxdma_req;
}

static int ralink_pcm_dai_probe(struct snd_soc_dai *dai)
{
	struct ralink_pcm *pcm = snd_soc_dai_get_drvdata(dai);

	snd_soc_dai_init_dma_data(dai, &pcm->playback_dma_data,
			&pcm->capture_dma_data);

	return 0;
}

static int ralink_pcm_dai_remove(struct snd_soc_dai *dai)
{
	return 0;
}

static const struct snd_soc_dai_ops ralink_pcm_dai_ops = {
	.set_sysclk = ralink_pcm_set_sysclk,
	.set_fmt = ralink_pcm_set_fmt,
	.startup = ralink_pcm_startup,
	.shutdown = ralink_pcm_shutdown,
	.hw_params = ralink_pcm_hw_params,
	.trigger = ralink_pcm_trigger,
};

static struct snd_soc_dai_driver ralink_pcm_dai = {
	.name = DRV_NAME,
	.probe = ralink_pcm_dai_probe,
	.remove = ralink_pcm_dai_remove,
	.ops = &ralink_pcm_dai_ops,
	.capture = {
		.stream_name = "pcm Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rate_min = 5512,
		.rate_max = 192000,
		.rates = SNDRV_PCM_RATE_CONTINUOUS,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.playback = {
		.stream_name = "pcm Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rate_min = 5512,
		.rate_max = 192000,
		.rates = SNDRV_PCM_RATE_CONTINUOUS,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.symmetric_rates = 1,
};

static struct snd_pcm_hardware ralink_pcm_hardware = {
	.info = SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER,
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.channels_min		= 2,
	.channels_max		= 2,
	.period_bytes_min	= PAGE_SIZE,
	.period_bytes_max	= PAGE_SIZE * 2,
	.periods_min		= 2,
	.periods_max		= 128,
	.buffer_bytes_max	= 128 * 1024,
	.fifo_size		= RALINK_pcm_FIFO_SIZE,
};

static const struct snd_dmaengine_pcm_config ralink_dmaengine_pcm_config = {
	.prepare_slave_config = snd_dmaengine_pcm_prepare_slave_config,
	.pcm_hardware = &ralink_pcm_hardware,
	.prealloc_buffer_size = 256 * PAGE_SIZE,
};

static const struct snd_soc_component_driver ralink_pcm_component = {
	.name = DRV_NAME,
};

static bool ralink_pcm_readable_reg(struct device *dev, unsigned int reg)
{
	return true;
}

static bool ralink_pcm_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case pcm_REG_INT_STATUS:
	case pcm_REG_FF_STATUS:
		return true;
	}
	return false;
}

static bool ralink_pcm_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case pcm_REG_FF_STATUS:
		return false;
	}
	return true;
}

static const struct regmap_config ralink_pcm_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.writeable_reg = ralink_pcm_writeable_reg,
	.readable_reg = ralink_pcm_readable_reg,
	.volatile_reg = ralink_pcm_volatile_reg,
	.max_register = PCM_CHB1_CFG2,
};

#if (RALINK_pcm_INT_EN)
static irqreturn_t ralink_pcm_irq(int irq, void *devid)
{
	struct ralink_pcm *pcm = devid;
	u32 status;

	regmap_read(pcm->regmap, pcm_REG_INT_STATUS, &status);
	if (unlikely(!status))
		return IRQ_NONE;

	/* tx stats */
	if (status & pcm_REG_INT_TX_MASK) {
		if (status & pcm_REG_INT_TX_THRES)
			pcm->txstats.belowthres++;
		if (status & pcm_REG_INT_TX_UNRUN)
			pcm->txstats.underrun++;
		if (status & pcm_REG_INT_TX_OVRUN)
			pcm->txstats.overrun++;
		if (status & pcm_REG_INT_TX_FAULT)
			pcm->txstats.dmafault++;
	}

	/* rx stats */
	if (status & pcm_REG_INT_RX_MASK) {
		if (status & pcm_REG_INT_RX_THRES)
			pcm->rxstats.belowthres++;
		if (status & pcm_REG_INT_RX_UNRUN)
			pcm->rxstats.underrun++;
		if (status & pcm_REG_INT_RX_OVRUN)
			pcm->rxstats.overrun++;
		if (status & pcm_REG_INT_RX_FAULT)
			pcm->rxstats.dmafault++;
	}

	/* clean status bits */
	regmap_write(pcm->regmap, pcm_REG_INT_STATUS, status);

	return IRQ_HANDLED;
}
#endif

//#if IS_ENABLED(CONFIG_DEBUG_FS)
#if 1
static int ralink_pcm_stats_show(struct seq_file *s, void *unused)
{
        struct ralink_pcm *pcm = s->private;

	seq_printf(s, "tx stats\n");
	seq_printf(s, "\tbelow threshold\t%u\n", pcm->txstats.belowthres);
	seq_printf(s, "\tunder run\t%u\n", pcm->txstats.underrun);
	seq_printf(s, "\tover run\t%u\n", pcm->txstats.overrun);
	seq_printf(s, "\tdma fault\t%u\n", pcm->txstats.dmafault);

	seq_printf(s, "rx stats\n");
	seq_printf(s, "\tbelow threshold\t%u\n", pcm->rxstats.belowthres);
	seq_printf(s, "\tunder run\t%u\n", pcm->rxstats.underrun);
	seq_printf(s, "\tover run\t%u\n", pcm->rxstats.overrun);
	seq_printf(s, "\tdma fault\t%u\n", pcm->rxstats.dmafault);

	ralink_pcm_dump_regs(pcm);

	return 0;
}

static int ralink_pcm_stats_open(struct inode *inode, struct file *file)
{
        return single_open(file, ralink_pcm_stats_show, inode->i_private);
}

static const struct file_operations ralink_pcm_stats_ops = {
        .open = ralink_pcm_stats_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = single_release,
};

static inline int ralink_pcm_debugfs_create(struct ralink_pcm *pcm)
{
        pcm->dbg_dir = debugfs_create_dir(dev_name(pcm->dev), NULL);
        if (!pcm->dbg_dir)
                return -ENOMEM;

        pcm->dbg_stats = debugfs_create_file("stats", S_IRUGO,
                        pcm->dbg_dir, pcm, &ralink_pcm_stats_ops);
        if (!pcm->dbg_stats) {
                debugfs_remove(pcm->dbg_dir);
                return -ENOMEM;
        }

        return 0;
}

static inline void ralink_pcm_debugfs_remove(struct ralink_pcm *pcm)
{
	debugfs_remove(pcm->dbg_stats);
	debugfs_remove(pcm->dbg_dir);
}
#else
static inline int ralink_pcm_debugfs_create(struct ralink_pcm *pcm)
{
	return 0;
}

static inline void ralink_pcm_debugfs_remove(struct fsl_ssi_dbg *ssi_dbg)
{
}
#endif

/*
 * TODO: these refclk setup functions should use
 * clock framework instead. hardcode it now.
 */
static void rt3350_refclk_setup(void)
{
	uint32_t data;

	/* set refclk output 12Mhz clock */
	data = rt_sysc_r32(0x2c);
	data |= (0x1 << 8);
	rt_sysc_w32(data, 0x2c);
}

static void rt3883_refclk_setup(void)
{
	uint32_t data;

	/* set refclk output 12Mhz clock */
	data = rt_sysc_r32(0x2c);
	data &= ~(0x3 << 13);
	data |= (0x1 << 13);
	rt_sysc_w32(data, 0x2c);
}

static void rt3552_refclk_setup(void)
{
	uint32_t data;

	/* set refclk output 12Mhz clock */
	data = rt_sysc_r32(0x2c);
	data &= ~(0xf << 8);
	data |= (0x3 << 8);
	rt_sysc_w32(data, 0x2c);
}

static void mt7620_refclk_setup(void)
{
	uint32_t data;

	/* set refclk output 12Mhz clock */
	data = rt_sysc_r32(0x2c);
	data &= ~(0x7 << 9);
	data |= 0x1 << 9;
	rt_sysc_w32(data, 0x2c);
}

static void mt7621_refclk_setup(void)
{
	uint32_t data;

	/* set refclk output 12Mhz clock */
	data = rt_sysc_r32(0x2c);
	data &= ~(0x1f << 18);
	data |= (0x19 << 18);
	data &= ~(0x1f << 12);
	data |= (0x1 << 12);
	data &= ~(0x7 << 9);
	data |= (0x5 << 9);
	rt_sysc_w32(data, 0x2c);
}

static void mt7628_refclk_setup(void)
{
	uint32_t data;

	/* set pcm and refclk digital pad */
	data = rt_sysc_r32(0x3c);
	data |= 0x1f;
	rt_sysc_w32(data, 0x3c);

	/* Adjust REFCLK0's driving strength */
	data = rt_sysc_r32(0x1354);
	data &= ~(0x1 << 5);
	rt_sysc_w32(data, 0x1354);
	data = rt_sysc_r32(0x1364);
	data |= ~(0x1 << 5);
	rt_sysc_w32(data, 0x1364);

	/* set refclk output 12Mhz clock */
	data = rt_sysc_r32(0x2c);
	data &= ~(0x7 << 9);
	data |= 0x1 << 9;
	rt_sysc_w32(data, 0x2c);
}

#define PCMCLOCK_OUT 0 // 256KHz

unsigned long i2sMaster_inclk_int[11] = {
	78,     56,     52,     39,     28,     26,     19,     14,     13,     9,      6};
unsigned long i2sMaster_inclk_comp[11] = {
	64,     352,    42,     32,     176,    21,     272,    88,     10,     455,    261};

static int ralink_pcm_setup(struct ralink_pcm *pcm)
{
	printk(KERN_ALERT "ralink_pcm_setup\n");

	uint32_t cfg;

	//和snd不同的是,这里的寄存器尽量一次性配置完成

	//1.Set PCM_CFG
	regmap_read(pcm->regmap, PCM_PCM_CFG, &cfg);	//默认情况下,应该是0x03000000
	
	cfg = 0x03000000;

	//默认使用内部时钟源
	uint8_t clock_external = 0;
	if(0 == clock_external)
	{
		cfg |= PCM_PCM_CFG_CLKOUT_EN; // pcm clock from internal
        cfg &= ~PCM_PCM_CFG_EXT_FSYNC; // pcm sync from internal
	}
	else
	{
		cfg &= ~PCM_PCM_CFG_CLKOUT_EN; // pcm clock from external
        cfg |= PCM_PCM_CFG_EXT_FSYNC; // pcm sync from external
	}
	cfg |= PCM_PCM_CFG_LONG_FSYNC; //long sync mode
    cfg |= PCM_PCM_CFG_FSYNC_POL; // sync high active

	//SLOT模式 先默认4槽位,后面再修改
	regmap_write(pcm->regmap, PCM_PCM_CFG, cfg);

	//2.
	regmap_write(pcm->regmap, PCM_INT_EN, 0);



	//设置时钟
	regmap_write(pcm->regmap, PCM_DIVINT_CFG, i2sMaster_inclk_int[PCMCLOCK_OUT]);
	regmap_write(pcm->regmap, PCM_DIVCOMP_CFG, i2sMaster_inclk_comp[PCMCLOCK_OUT] | PCM_DIVCOMP_CFG_CLK_EN);
	
	

	//使能PCM
	//PCM全局寄存器 default 0x00440000
	regmap_read(pcm->regmap, PCM_GLB_CFG, &cfg);

	cfg = 0x00440000;
	cfg |= PCM_GLB_CFG_DFT_THRES;
	cfg |= PCM_GLB_CFG_EN;
	cfg |= PCM_GLB_CFG_DMA_EN;

	regmap_write(pcm->regmap,PCM_GLB_CFG,cfg);


	return 0;
}


struct rt_pcm_data {
	u32 flags;
	void (*refclk_setup)(void);
};

struct rt_pcm_data rt3050_pcm_data = { .flags = RALINK_FLAGS_TXONLY };
struct rt_pcm_data rt3350_pcm_data = { .flags = RALINK_FLAGS_TXONLY,
	.refclk_setup = rt3350_refclk_setup };
struct rt_pcm_data rt3883_pcm_data = {
	.flags = (RALINK_FLAGS_LEFT_J | RALINK_FLAGS_RIGHT_J),
	.refclk_setup = rt3883_refclk_setup };
struct rt_pcm_data rt3352_pcm_data = { .refclk_setup = rt3552_refclk_setup};
struct rt_pcm_data mt7620_pcm_data = { .refclk_setup = mt7620_refclk_setup};
struct rt_pcm_data mt7621_pcm_data = { .refclk_setup = mt7621_refclk_setup};
struct rt_pcm_data mt7628_pcm_data = {
	.flags = (RALINK_FLAGS_ENDIAN | RALINK_FLAGS_24BIT |
			RALINK_FLAGS_LEFT_J),
	.refclk_setup = mt7628_refclk_setup};

static const struct of_device_id ralink_pcm_match_table[] = {
	{ .compatible = "mediatek,mt7621-pcm",
		.data = (void *)&mt7621_pcm_data },
	{ .compatible = "ralink,mt7620a-pcm",
		.data = (void *)&mt7628_pcm_data },
};
MODULE_DEVICE_TABLE(of, ralink_pcm_match_table);

static int ralink_pcm_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device_node *np = pdev->dev.of_node;
	struct ralink_pcm *pcm;
	struct resource *res;
	int irq, ret;
	u32 dma_req;
	struct rt_pcm_data *data;
	printk(KERN_ALERT "ralink_pcm_probe\n");

	pcm = devm_kzalloc(&pdev->dev, sizeof(*pcm), GFP_KERNEL);
	if (!pcm)
		return -ENOMEM;

	platform_set_drvdata(pdev, pcm);
	pcm->dev = &pdev->dev;

	match = of_match_device(ralink_pcm_match_table, &pdev->dev);
	if (!match) {
		printk(KERN_ALERT "of_property_read_u32 ret:%d\r\n",ret);
		return -EINVAL;
	}
	data = (struct rt_pcm_data *)match->data;
	pcm->flags = data->flags;
	/* setup out 12Mhz refclk to codec as mclk */
	if (data->refclk_setup)
		data->refclk_setup();

	/*
	if (of_property_read_u32(np, "txdma-req", &dma_req)) {
		dev_err(&pdev->dev, "no txdma-req define\n");
		printk(KERN_ALERT "of_property_read_u32 ret:%d\r\n",ret);
		//return -EINVAL;
	}
	pcm->txdma_req = (u16)dma_req;
	if (!(pcm->flags & RALINK_FLAGS_TXONLY)) {
		if (of_property_read_u32(np, "rxdma-req", &dma_req)) {
			dev_err(&pdev->dev, "no rxdma-req define\n");
			printk(KERN_ALERT "of_property_read_u32 ret:%d\r\n",ret);
			//return -EINVAL;
		}
		pcm->rxdma_req = (u16)dma_req;
	}
	*/
	//上面是从dts读取dma配置的代码,暂时写死
	pcm->txdma_req = 6;
	pcm->rxdma_req = 4;


	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pcm->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pcm->regs)) {
		printk(KERN_ALERT "platform_get_resource ret:%d\r\n",ret);
		return PTR_ERR(pcm->regs);
	}

	pcm->regmap = devm_regmap_init_mmio(&pdev->dev, pcm->regs,
			&ralink_pcm_regmap_config);
	if (IS_ERR(pcm->regmap)) {
		printk(KERN_ALERT "devm_regmap_init_mmio ret:%d\r\n",ret);
		dev_err(&pdev->dev, "regmap init failed\n");
		return PTR_ERR(pcm->regmap);
	}


	ret = ralink_pcm_setup(pcm);
	if(0 != ret)
	{
		printk(KERN_ALERT "ralink_pcm_setup failed,ret:%d\r\n",ret);
	}

    irq = platform_get_irq(pdev, 0);
    if (irq < 0) {
			printk(KERN_ALERT "platform_get_irq ret:%d\r\n",ret);
            dev_err(&pdev->dev, "failed to get irq\n");
            return -EINVAL;
    }

#if (RALINK_pcm_INT_EN)
	ret = devm_request_irq(&pdev->dev, irq, ralink_pcm_irq,
			0, dev_name(&pdev->dev), pcm);
	if (ret) {
		printk(KERN_ALERT "devm_request_irq ret:%d\r\n",ret);
		dev_err(&pdev->dev, "failed to request irq\n");
		return ret;
	}
#endif

	pcm->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(pcm->clk)) {
		dev_err(&pdev->dev, "no clock defined\n");
		printk(KERN_ALERT "devm_clk_get ret:%d\r\n",ret);
		//return PTR_ERR(pcm->clk);
	}

	/*
	ret = clk_prepare_enable(pcm->clk);
	if (ret)
	{
		printk(KERN_ALERT "clk_prepare_enable ret:%d\r\n",ret);
		//return ret;
	}
	*/

	

	//device_reset(&pdev->dev);

	ret = ralink_pcm_debugfs_create(pcm);
	if (ret) {
		printk(KERN_ALERT "ralink_pcm_debugfs_create ret:%d\r\n",ret);
		dev_err(&pdev->dev, "create debugfs failed\n");
		goto err_clk_disable;
	}

	/* enable 24bits support */
	if (pcm->flags & RALINK_FLAGS_24BIT) {
		ralink_pcm_dai.capture.formats |= SNDRV_PCM_FMTBIT_S24_LE;
		ralink_pcm_dai.playback.formats |= SNDRV_PCM_FMTBIT_S24_LE;
	}

	/* enable big endian support */
	if (pcm->flags & RALINK_FLAGS_ENDIAN) {
		ralink_pcm_dai.capture.formats |= SNDRV_PCM_FMTBIT_S16_BE;
		ralink_pcm_dai.playback.formats |= SNDRV_PCM_FMTBIT_S16_BE;
		ralink_pcm_hardware.formats |= SNDRV_PCM_FMTBIT_S16_BE;
		if (pcm->flags & RALINK_FLAGS_24BIT) {
			ralink_pcm_dai.capture.formats |=
				SNDRV_PCM_FMTBIT_S24_BE;
			ralink_pcm_dai.playback.formats |=
				SNDRV_PCM_FMTBIT_S24_BE;
			ralink_pcm_hardware.formats |=
				SNDRV_PCM_FMTBIT_S24_BE;
		}
	}

	/* disable capture support */
	if (pcm->flags & RALINK_FLAGS_TXONLY)
		memset(&ralink_pcm_dai.capture, sizeof(ralink_pcm_dai.capture),
				0);

	ret = devm_snd_soc_register_component(&pdev->dev, &ralink_pcm_component,
			&ralink_pcm_dai, 1);
	if (ret)
	{
		printk(KERN_ALERT "devm_snd_soc_register_component ret:%d\r\n",ret);
		goto err_debugfs;
	}

	//DMA相关
	ralink_pcm_init_dma_data(pcm, res);

	printk(KERN_ALERT "ralink_pcm_init_dma_data ok\r\n");

	ret = devm_snd_dmaengine_pcm_register(&pdev->dev,
			&ralink_dmaengine_pcm_config,
			SND_DMAENGINE_PCM_FLAG_COMPAT);
	if (ret)
	{
		printk(KERN_ALERT "devm_snd_dmaengine_pcm_register ret:%d\r\n",ret);
		//goto err_debugfs;
	}
	printk(KERN_ALERT "devm_snd_dmaengine_pcm_register ok\r\n");

	//dev_info(pcm->dev, "mclk %luMHz\n", clk_get_rate(pcm->clk) / 1000000);

	u32 buf[32];

	ret = regmap_bulk_read(pcm->regmap, PCM_GLB_CFG,
			buf, ARRAY_SIZE(buf));

	printk(KERN_ALERT "GLB_CFG: %08x, PCM_CFG: %08x, INT_STATUS: %08x, " \
			"INT_EN: %08x, CHA0_FF_STATUS: %08x, CHB0_FF_STATUS: %08x, " \
			"CHA0_CFG: %08x, CHB0_CFG: %08x, FSYNC_CFG: %08x, " \
			"CHA0_CFG2: %08x, CHB0_CFG2: %08x, IP_INFO: %08x\n",
			buf[0], buf[1], buf[2], buf[3], buf[4],
			buf[5], buf[8], buf[9], buf[12], buf[13], buf[14], buf[16]);


	return 0;

err_debugfs:
	ralink_pcm_debugfs_remove(pcm);

err_clk_disable:
	clk_disable_unprepare(pcm->clk);

	printk(KERN_ALERT "ret:%d\r\n",ret);
	return ret;
}

static int ralink_pcm_remove(struct platform_device *pdev)
{
	struct ralink_pcm *pcm = platform_get_drvdata(pdev);
	printk(KERN_ALERT "ralink_pcm_remove\n");
	ralink_pcm_debugfs_remove(pcm);
	clk_disable_unprepare(pcm->clk);

	return 0;
}

static struct platform_driver ralink_pcm_driver = {
	.probe = ralink_pcm_probe,
	.remove = ralink_pcm_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = ralink_pcm_match_table,
	},
};
module_platform_driver(ralink_pcm_driver);

MODULE_AUTHOR("Lars-Peter Clausen, <lars@metafoo.de>");
MODULE_DESCRIPTION("Ralink/MediaTek pcm driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
