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

#include "pcm-reg.h"


#define DRV_NAME "ralink-pcm"


static int slave_mode=0;
module_param(slave_mode,int,S_IRUGO);
MODULE_PARM_DESC(slave_mode, "enable slave test mode.");


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
		struct dentry *dbg_000glbcfg;
		struct dentry *dbg_004pcmcfg;
		struct dentry *dbg_008intstatus;
		struct dentry *dbg_00cinten;
		struct dentry *dbg_010cha0ffstatus;
		struct dentry *dbg_014chb0ffstatus;
		struct dentry *dbg_020cha0cfg;
		struct dentry *dbg_024chb0cfg;
		struct dentry *dbg_030fsynccfg;
		struct dentry *dbg_034cha0cfg2;
		struct dentry *dbg_038chb0cfg2;
		struct dentry *dbg_040rsvreg16;
		struct dentry *dbg_050divcompcfg;
		struct dentry *dbg_054divintcfg;
		struct dentry *dbg_060digdelaycfg;
		struct dentry *dbg_080ch0fifo;
		struct dentry *dbg_084ch1fifo;
		struct dentry *dbg_088ch2fifo;
		struct dentry *dbg_08cch3fifo;
		struct dentry *dbg_110cha1ffstatus;
		struct dentry *dbg_114chb1ffstatus;
		struct dentry *dbg_120cha1cfg;
		struct dentry *dbg_124chb1cfg;
		struct dentry *dbg_134cha1cfg2;
		struct dentry *dbg_138chb1cfg2;
	struct ralink_pcm_stats txstats;
	struct ralink_pcm_stats rxstats;
};

struct ralink_pcm *p_gpcm = NULL;

/////DMA
/*
#include <linux/dmaengine.h>

static const struct snd_soc_component_driver dmaengine_pcm_component_process = {
	.name		= SND_DMAENGINE_PCM_DRV_NAME,
	.probe_order	= SND_SOC_COMP_ORDER_LATE,
	.ops		= &dmaengine_pcm_process_ops,
	.pcm_new	= dmaengine_pcm_new,
};

int my_snd_dmaengine_pcm_register(struct device *dev,
	const struct snd_dmaengine_pcm_config *config, unsigned int flags)
{
	struct dmaengine_pcm *pcm;
	int ret;

	pcm = kzalloc(sizeof(*pcm), GFP_KERNEL);
	if (!pcm)
		return -ENOMEM;

#ifdef CONFIG_DEBUG_FS
	pcm->component.debugfs_prefix = "dma";
#endif
	pcm->config = config;
	pcm->flags = flags;

	ret = dmaengine_pcm_request_chan_of(pcm, dev, config);
	if (ret)
		goto err_free_dma;

	if (config && config->process)
		ret = snd_soc_add_component(dev, &pcm->component,
					    &dmaengine_pcm_component_process,
					    NULL, 0);
	else
		ret = snd_soc_add_component(dev, &pcm->component,
					    &dmaengine_pcm_component, NULL, 0);
	if (ret)
		goto err_free_dma;

	return 0;

err_free_dma:
	dmaengine_pcm_release_chan(pcm);
	kfree(pcm);
	return ret;
}
*/


///// end of dma





static inline uint32_t ralink_pcm_read(const struct ralink_pcm *pcm,
	unsigned int reg)
{
	//writel(value, pcm->regs + reg);
	uint32_t value;
	regmap_read(pcm->regmap, reg, &value);
	return value;
}



static inline void ralink_pcm_write(const struct ralink_pcm *pcm,
	unsigned int reg, uint32_t value)
{
	//writel(value, pcm->regs + reg);
	regmap_write(pcm->regmap,reg,value);
}

static void ralink_pcm_debug_dma(struct ralink_pcm *pcm)
{
	printk(KERN_ALERT "paly_back:\n" \
	"addr: 0x%08x , addr_width: 0x%08x, maxburst: %d, slaveid: %d," \
	"filter_data: 0x%p, chan_name: %s, fifo_size:%d, flags:%d.\n" \
	"capture:\n" \
	"addr: 0x%08x , addr_width: 0x%08x, maxburst: %d, slaveid: %d," \
	"filter_data: 0x%p, chan_name: %s, fifo_size:%d, flags:%d.\n",
	pcm->playback_dma_data.addr,pcm->playback_dma_data.addr_width,pcm->playback_dma_data.maxburst,
	pcm->playback_dma_data.slave_id,pcm->playback_dma_data.filter_data,pcm->playback_dma_data.chan_name,
	pcm->playback_dma_data.fifo_size,pcm->playback_dma_data.flags,
	pcm->capture_dma_data.addr,pcm->capture_dma_data.addr_width,pcm->capture_dma_data.maxburst,
	pcm->capture_dma_data.slave_id,pcm->capture_dma_data.filter_data,pcm->capture_dma_data.chan_name,
	pcm->capture_dma_data.fifo_size,pcm->capture_dma_data.flags
	);
}


static void ralink_pcm_dump_regs(struct ralink_pcm *pcm)
{
	u32 buf[32];
	int ret;

	ret = regmap_bulk_read(pcm->regmap, PCM_GLB_CFG,
			buf, ARRAY_SIZE(buf));

	printk(KERN_ALERT "GLB_CFG: 0x%08x, PCM_CFG: 0x%08x, INT_STATUS: 0x%08x, " \
			"INT_EN: 0x%08x, CHA0_FF_STATUS: 0x%08x, CHB0_FF_STATUS: 0x%08x, " \
			"CHA0_CFG: 0x%08x, CHB0_CFG: 0x%08x, FSYNC_CFG: 0x%08x, " \
			"CHA0_CFG2: 0x%08x, CHB0_CFG2: 0x%08x, IP_INFO: 0x%08x," \
			"DIVCOMP_CFG: 0x%08x, DIVINT_CFG: 0x%08x, DIGDELAY_CFG: 0x%08x\n",
			buf[0], buf[1], buf[2], buf[3], buf[4],
			buf[5], buf[8], buf[9], buf[12], buf[13],
			buf[14], buf[16], buf[20], buf[21], buf[24]);
}

static int ralink_pcm_set_sysclk(struct snd_soc_dai *dai,
                              int clk_id, unsigned int freq, int dir)
{
	return 0;
}


static int ralink_pcm_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	//struct ralink_pcm *pcm = snd_soc_dai_get_drvdata(dai);
	//unsigned int cfg0 = 0, cfg1 = 0;

	/* set master/slave audio interface */
	/*
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
	*/
	/* interface format */
	/*
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
	*/
	/* clock inversion */
	/*
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
	*/
	return 0;
}

static int ralink_pcm_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct ralink_pcm *pcm = snd_soc_dai_get_drvdata(dai);

	if (dai->active)
		return 0;

	/* setup status interrupt */
#if (RALINK_PCM_INT_EN)
	regmap_write(pcm->regmap, PCM_INT_EN, 0xff);
#else
	regmap_write(pcm->regmap, PCM_INT_EN, 0x0);
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

	return 0;
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
	.fifo_size		= 32,
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
	case PCM_INT_STATUS:
	case PCM_CHA0_FF_STATUS:
	case PCM_CHB0_FF_STATUS:
	case PCM_CHA1_FF_STATUS:
	case PCM_CHB1_FF_STATUS:
		return true;
	}
	return false;
}

static bool ralink_pcm_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case PCM_IP_INFO:
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

#if (RALINK_PCM_INT_EN)
static irqreturn_t ralink_pcm_irq(int irq, void *devid)
{
	struct ralink_pcm *pcm = devid;
	u32 status;

	regmap_read(pcm->regmap, PCM_INT_STATUS, &status);
	if (unlikely(!status))
		return IRQ_NONE;

	/* tx stats */
	if (status & PCM_REG_INT_TX_MASK) {
		if (status & PCM_INT_STATUS_TX_THRES_INT)
			pcm->txstats.belowthres++;
		if (status & PCM_INT_STATUS_TX_UNRUN_INT)
			pcm->txstats.underrun++;
		if (status & PCM_INT_STATUS_TX_OVF_INT)
		{
			pcm->txstats.overrun++;
		}
		if (status & PCM_INT_STATUS_TX_DMA_FAULT_INT)
			pcm->txstats.dmafault++;
	}

	/* rx stats */
	if (status & PCM_REG_INT_RX_MASK) {
		if (status & PCM_INT_STATUS_RX_THRES_INT)
			pcm->rxstats.belowthres++;
		if (status & PCM_INT_STATUS_RX_UNRUN_INT)
			pcm->rxstats.underrun++;
		if (status & PCM_INT_STATUS_RX_OVF_INT)
			pcm->rxstats.overrun++;
		if (status & PCM_INT_STATUS_RX_DMA_FAULT_INT)
			pcm->rxstats.dmafault++;
	}

	/* clean status bits */
	regmap_write(pcm->regmap, PCM_INT_STATUS, status);

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

//////////////////////////////////////寄存器读写//////////////////////////////////

struct file_disc{
	bool                 is_open;
	size_t               size;
	bool				 is_already;
};

#define RALINK_PCM_OPEN(__attr_name) \
static int ralink_open ## __attr_name(struct inode *inode, struct file *file) \
{  \
	struct file_disc* this = kzalloc(sizeof(*this), GFP_KERNEL);  \
	this->is_open = true;  \
	this->size = 0;  \
	this->is_already = 0;  \
	file->private_data = this;  \
    return 0;  \
}

#define RALINK_PCM_READ(__attr_name, __addr) \
static ssize_t ralink_read ## __attr_name(struct file* file, char __user* buff, size_t count, loff_t* ppos) \
{ \
	uint32_t value; \
	int status; \
	struct file_disc* this = file->private_data; \
	if(1 == this->is_already) return 0; \
	regmap_read(p_gpcm->regmap, __addr, &value); \
	status = sprintf(buff, "0x%08x\r\n", value); \
	ppos+=status; \
	this->is_already=1; \
    return status; \
}

#define RALINK_PCM_WRITE(__attr_name, __addr) \
static int ralink_write ## __attr_name(struct file* file, const char __user* buff, size_t count, loff_t* ppos) \
{ \
	unsigned long value=0; \
	int nret; \
	char tbuf[16]={0}; \
	if(count > 15) return count; \
	memcpy(tbuf, buff, count); \
	tbuf[count] = 0; \
	nret = kstrtoul(tbuf, 0, &value); \
	if ( nret != 0) return count; \
	regmap_write(p_gpcm->regmap,__addr,value); \
	return count; \
}

#define RALINK_PCM_RELEASE(__attr_name) \
static int ralink_release ## __attr_name(struct inode *inode, struct file *file) \
{ \
    struct file_disc* this = file->private_data; \
    if (this != NULL) kfree(this); \
    return 0; \
}

#define RALINK_PCM_FILE_OP(__attr_name) \
static const struct file_operations ralink_pcm_ops ## __attr_name = { \
        .open = ralink_open ## __attr_name, \
        .read = ralink_read ## __attr_name, \
		.write = ralink_write ## __attr_name, \
        .llseek = seq_lseek, \
        .release = ralink_release ## __attr_name, \
};

RALINK_PCM_OPEN(_000glbcfg);
RALINK_PCM_READ(_000glbcfg, 0x0);
RALINK_PCM_WRITE(_000glbcfg, 0x0);
RALINK_PCM_RELEASE(_000glbcfg);
RALINK_PCM_FILE_OP(_000glbcfg);

RALINK_PCM_OPEN(_004pcmcfg);
RALINK_PCM_READ(_004pcmcfg, 0x4);
RALINK_PCM_WRITE(_004pcmcfg, 0x4);
RALINK_PCM_RELEASE(_004pcmcfg);
RALINK_PCM_FILE_OP(_004pcmcfg);

RALINK_PCM_OPEN(_008intstatus);
RALINK_PCM_READ(_008intstatus, 0x8);
RALINK_PCM_WRITE(_008intstatus, 0x8);
RALINK_PCM_RELEASE(_008intstatus);
RALINK_PCM_FILE_OP(_008intstatus);

RALINK_PCM_OPEN(_00cinten);
RALINK_PCM_READ(_00cinten, 0xc);
RALINK_PCM_WRITE(_00cinten, 0xc);
RALINK_PCM_RELEASE(_00cinten);
RALINK_PCM_FILE_OP(_00cinten);

RALINK_PCM_OPEN(_010cha0ffstatus);
RALINK_PCM_READ(_010cha0ffstatus, 0x10);
RALINK_PCM_WRITE(_010cha0ffstatus, 0x10);
RALINK_PCM_RELEASE(_010cha0ffstatus);
RALINK_PCM_FILE_OP(_010cha0ffstatus);

RALINK_PCM_OPEN(_014chb0ffstatus);
RALINK_PCM_READ(_014chb0ffstatus, 0x14);
RALINK_PCM_WRITE(_014chb0ffstatus, 0x14);
RALINK_PCM_RELEASE(_014chb0ffstatus);
RALINK_PCM_FILE_OP(_014chb0ffstatus);

RALINK_PCM_OPEN(_020cha0cfg);
RALINK_PCM_READ(_020cha0cfg, 0x20);
RALINK_PCM_WRITE(_020cha0cfg, 0x20);
RALINK_PCM_RELEASE(_020cha0cfg);
RALINK_PCM_FILE_OP(_020cha0cfg);

RALINK_PCM_OPEN(_024chb0cfg);
RALINK_PCM_READ(_024chb0cfg, 0x24);
RALINK_PCM_WRITE(_024chb0cfg, 0x24);
RALINK_PCM_RELEASE(_024chb0cfg);
RALINK_PCM_FILE_OP(_024chb0cfg);

RALINK_PCM_OPEN(_030fsynccfg);
RALINK_PCM_READ(_030fsynccfg, 0x30);
RALINK_PCM_WRITE(_030fsynccfg, 0x30);
RALINK_PCM_RELEASE(_030fsynccfg);
RALINK_PCM_FILE_OP(_030fsynccfg);

RALINK_PCM_OPEN(_034cha0cfg2);
RALINK_PCM_READ(_034cha0cfg2, 0x34);
RALINK_PCM_WRITE(_034cha0cfg2, 0x34);
RALINK_PCM_RELEASE(_034cha0cfg2);
RALINK_PCM_FILE_OP(_034cha0cfg2);

RALINK_PCM_OPEN(_038chb0cfg2);
RALINK_PCM_READ(_038chb0cfg2, 0x38);
RALINK_PCM_WRITE(_038chb0cfg2, 0x38);
RALINK_PCM_RELEASE(_038chb0cfg2);
RALINK_PCM_FILE_OP(_038chb0cfg2);

RALINK_PCM_OPEN(_050divcompcfg);
RALINK_PCM_READ(_050divcompcfg, 0x50);
RALINK_PCM_WRITE(_050divcompcfg, 0x50);
RALINK_PCM_RELEASE(_050divcompcfg);
RALINK_PCM_FILE_OP(_050divcompcfg);

RALINK_PCM_OPEN(_054divintcfg);
RALINK_PCM_READ(_054divintcfg, 0x54);
RALINK_PCM_WRITE(_054divintcfg, 0x54);
RALINK_PCM_RELEASE(_054divintcfg);
RALINK_PCM_FILE_OP(_054divintcfg);

RALINK_PCM_OPEN(_080ch0fifo);
RALINK_PCM_READ(_080ch0fifo, 0x80);
RALINK_PCM_WRITE(_080ch0fifo, 0x80);
RALINK_PCM_RELEASE(_080ch0fifo);
RALINK_PCM_FILE_OP(_080ch0fifo);

RALINK_PCM_OPEN(_084ch1fifo);
RALINK_PCM_READ(_084ch1fifo, 0x84);
RALINK_PCM_WRITE(_084ch1fifo, 0x84);
RALINK_PCM_RELEASE(_084ch1fifo);
RALINK_PCM_FILE_OP(_084ch1fifo);

RALINK_PCM_OPEN(_088ch2fifo);
RALINK_PCM_READ(_088ch2fifo, 0x88);
RALINK_PCM_WRITE(_088ch2fifo, 0x88);
RALINK_PCM_RELEASE(_088ch2fifo);
RALINK_PCM_FILE_OP(_088ch2fifo);

RALINK_PCM_OPEN(_08cch3fifo);
RALINK_PCM_READ(_08cch3fifo, 0x8c);
RALINK_PCM_WRITE(_08cch3fifo, 0x8c);
RALINK_PCM_RELEASE(_08cch3fifo);
RALINK_PCM_FILE_OP(_08cch3fifo);

RALINK_PCM_OPEN(_110cha1ffstatus);
RALINK_PCM_READ(_110cha1ffstatus, 0x110);
RALINK_PCM_WRITE(_110cha1ffstatus, 0x110);
RALINK_PCM_RELEASE(_110cha1ffstatus);
RALINK_PCM_FILE_OP(_110cha1ffstatus);

RALINK_PCM_OPEN(_114chb1ffstatus);
RALINK_PCM_READ(_114chb1ffstatus, 0x114);
RALINK_PCM_WRITE(_114chb1ffstatus, 0x114);
RALINK_PCM_RELEASE(_114chb1ffstatus);
RALINK_PCM_FILE_OP(_114chb1ffstatus);

RALINK_PCM_OPEN(_120cha1cfg);
RALINK_PCM_READ(_120cha1cfg, 0x120);
RALINK_PCM_WRITE(_120cha1cfg, 0x120);
RALINK_PCM_RELEASE(_120cha1cfg);
RALINK_PCM_FILE_OP(_120cha1cfg);

RALINK_PCM_OPEN(_124chb1cfg);
RALINK_PCM_READ(_124chb1cfg, 0x124);
RALINK_PCM_WRITE(_124chb1cfg, 0x124);
RALINK_PCM_RELEASE(_124chb1cfg);
RALINK_PCM_FILE_OP(_124chb1cfg);

RALINK_PCM_OPEN(_134cha1cfg2);
RALINK_PCM_READ(_134cha1cfg2, 0x134);
RALINK_PCM_WRITE(_134cha1cfg2, 0x134);
RALINK_PCM_RELEASE(_134cha1cfg2);
RALINK_PCM_FILE_OP(_134cha1cfg2);

RALINK_PCM_OPEN(_138chb1cfg2);
RALINK_PCM_READ(_138chb1cfg2, 0x138);
RALINK_PCM_WRITE(_138chb1cfg2, 0x138);
RALINK_PCM_RELEASE(_138chb1cfg2);
RALINK_PCM_FILE_OP(_138chb1cfg2);

#define RALINK_PCM_CREATE(__attr_name , __name) \
pcm->dbg ##__attr_name = debugfs_create_file(__name,S_IRUGO, \
	pcm->dbg_dir, pcm, &ralink_pcm_ops ##__attr_name); \
if (!pcm->dbg ##__attr_name) { \
    debugfs_remove(pcm->dbg_dir); \
    return -ENOMEM; \
}

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
		RALINK_PCM_CREATE(_000glbcfg,"000_glb_cfg");
		RALINK_PCM_CREATE(_004pcmcfg,"004_pcm_cfg");
		RALINK_PCM_CREATE(_008intstatus,"008_int_status");
		RALINK_PCM_CREATE(_00cinten,"00c_int_en");
		RALINK_PCM_CREATE(_010cha0ffstatus,"010_cha0_ff_status");
		RALINK_PCM_CREATE(_014chb0ffstatus,"014_chb0_ff_status");
		RALINK_PCM_CREATE(_020cha0cfg,"020_cha0_cfg");
		RALINK_PCM_CREATE(_024chb0cfg,"024_chb0_cfg");
		RALINK_PCM_CREATE(_030fsynccfg,"030_fsync_cfg");
		RALINK_PCM_CREATE(_034cha0cfg2,"034_cha0_cfg2");
		RALINK_PCM_CREATE(_038chb0cfg2,"038_chb0_cfg2");
		RALINK_PCM_CREATE(_050divcompcfg,"050_divcomp_cfg");
		RALINK_PCM_CREATE(_054divintcfg,"054_divint_cfg");
		RALINK_PCM_CREATE(_080ch0fifo,"080_ch0_fifo");
		RALINK_PCM_CREATE(_084ch1fifo,"084_ch1_fifo");
		RALINK_PCM_CREATE(_088ch2fifo,"088_ch2_fifo");
		RALINK_PCM_CREATE(_08cch3fifo,"08c_ch3_fifo");
		RALINK_PCM_CREATE(_110cha1ffstatus,"110_cha1_ff_status");
		RALINK_PCM_CREATE(_114chb1ffstatus,"114_chb1_ff_status");
		RALINK_PCM_CREATE(_120cha1cfg,"120_cha1_cfg");
		RALINK_PCM_CREATE(_124chb1cfg,"124_chb1_cfg");
		RALINK_PCM_CREATE(_134cha1cfg2,"134_cha1_cfg2");
		RALINK_PCM_CREATE(_138chb1cfg2,"138_chb1_cfg2");

        return 0;
}

static inline void ralink_pcm_debugfs_remove(struct ralink_pcm *pcm)
{
	debugfs_remove(pcm->dbg_stats);
	debugfs_remove(pcm->dbg_000glbcfg);
	debugfs_remove(pcm->dbg_004pcmcfg);
	debugfs_remove(pcm->dbg_008intstatus);
	debugfs_remove(pcm->dbg_00cinten);
	debugfs_remove(pcm->dbg_010cha0ffstatus);
	debugfs_remove(pcm->dbg_014chb0ffstatus);
	debugfs_remove(pcm->dbg_020cha0cfg);
	debugfs_remove(pcm->dbg_024chb0cfg);
	debugfs_remove(pcm->dbg_030fsynccfg);
	debugfs_remove(pcm->dbg_034cha0cfg2);
	debugfs_remove(pcm->dbg_038chb0cfg2);
	debugfs_remove(pcm->dbg_050divcompcfg);
	debugfs_remove(pcm->dbg_054divintcfg);
	debugfs_remove(pcm->dbg_080ch0fifo);
	debugfs_remove(pcm->dbg_084ch1fifo);
	debugfs_remove(pcm->dbg_088ch2fifo);
	debugfs_remove(pcm->dbg_08cch3fifo);
	debugfs_remove(pcm->dbg_110cha1ffstatus);
	debugfs_remove(pcm->dbg_114chb1ffstatus);
	debugfs_remove(pcm->dbg_120cha1cfg);
	debugfs_remove(pcm->dbg_124chb1cfg);
	debugfs_remove(pcm->dbg_134cha1cfg2);
	debugfs_remove(pcm->dbg_138chb1cfg2);

	debugfs_remove(pcm->dbg_dir);
}

//////////////////////////////////////寄存器读写结束//////////////////////////////////
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

#define PCMCLOCK_OUT 7 // 256KHz

unsigned long i2sMaster_inclk_int[11] = {
	78,     56,     52,     39,     28,     26,     19,     14,     13,     9,      6};
unsigned long i2sMaster_inclk_comp[11] = {
	64,     352,    42,     32,     176,    21,     272,    88,     10,     512,    261};


//寄存器初始化尽量都在这里完成
static int ralink_pcm_setup(struct ralink_pcm *pcm)
{
	uint32_t cfg;

	printk(KERN_ALERT "ralink_pcm_setup\n");
	//和snd不同的是,这里的寄存器尽量一次性配置完成

	//1.Set PCM_CFG
	//regmap_read(pcm->regmap, PCM_PCM_CFG, &cfg);	//默认情况下,应该是0x03000000
	
	cfg = 0x03000000;

	//默认使用内部时钟源
	if(0 == slave_mode)
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

	cfg|= 1<<0; //使能1bit模式

	//SLOT模式 先默认4槽位,后面再修改
	regmap_write(pcm->regmap, PCM_PCM_CFG, cfg);


	//2.暂时不使用中断
	regmap_write(pcm->regmap, PCM_INT_EN, 0);

	//配置每个槽位的偏移量
	regmap_write(pcm->regmap, PCM_CHA0_CFG, 0);
	regmap_write(pcm->regmap, PCM_CHB0_CFG, 16);
	regmap_write(pcm->regmap, PCM_CHA1_CFG, 32);
	regmap_write(pcm->regmap, PCM_CHB1_CFG, 48);


	//设置同步模式
	//cfg = 0xa8000001;
	//cfg |= PCM_FSYNC_POS_CAP_DT;

	cfg = PCM_FSYNC_EN|PCM_FSYNC_POS_DRV_DT|PCM_FSYNC_POS_CAP_FSYNC|PCM_FSYNC_POS_DRV_FSYNC;
	cfg |= BIT(0);
	regmap_write(pcm->regmap, PCM_FSYNC_CFG, cfg);
	/*
	if(0 == slave_mode) {

	}
	else {

	}
	*/
	//设置时钟
	regmap_write(pcm->regmap, PCM_DIVINT_CFG, i2sMaster_inclk_int[PCMCLOCK_OUT]);
	regmap_write(pcm->regmap, PCM_DIVCOMP_CFG, i2sMaster_inclk_comp[PCMCLOCK_OUT] | PCM_DIVCOMP_CFG_CLK_EN);
	
	

	//使能PCM
	//PCM全局寄存器 default 0x00440000
	cfg = 0x00440000;
	cfg |= PCM_GLB_CFG_DFT_THRES;
	cfg |= PCM_GLB_CFG_EN;
	//cfg |= PCM_GLB_CFG_LBK_EN;
	//cfg |= PCM_GLB_CFG_EXT_LBK_EN;
	cfg |= PCM_GLB_CFG_CH0_EN;
	cfg |= PCM_GLB_CFG_CH1_EN;
	cfg |= PCM_GLB_CFG_CH2_EN;
	cfg |= PCM_GLB_CFG_CH3_EN;
	//cfg |= PCM_GLB_CFG_DMA_EN;

	regmap_write(pcm->regmap,PCM_GLB_CFG,cfg);

	#if (RALINK_PCM_INT_EN)
	regmap_write(pcm->regmap, PCM_INT_EN, 0xff);
#else
	regmap_write(pcm->regmap, PCM_INT_EN, 0x0);
#endif

	return 0;
}

static int ralink_pcm_clean(struct ralink_pcm *pcm)
{
	uint32_t cfg;
	//关闭pcm
	regmap_read(pcm->regmap, PCM_GLB_CFG, &cfg);
	cfg &= ~PCM_GLB_CFG_EN;
	regmap_write(pcm->regmap,PCM_GLB_CFG,cfg);

	//关闭外部时钟
	cfg = 0x03000000;
	regmap_write(pcm->regmap,PCM_PCM_CFG,cfg);

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
	struct ralink_pcm *pcm;
	struct resource *res;
	int irq, ret;
	struct rt_pcm_data *data;
	printk(KERN_ALERT "ralink_pcm_probe\n");


	pcm = devm_kzalloc(&pdev->dev, sizeof(*pcm), GFP_KERNEL);
	if (!pcm)
		return -ENOMEM;
	printk(KERN_ALERT "platform_set_drvdata\n");
	platform_set_drvdata(pdev, pcm);
	pcm->dev = &pdev->dev;
	printk(KERN_ALERT "of_match_device\n");
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
	struct device_node *np = pdev->dev.of_node;
	u32 dma_req;
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
	pcm->rxdma_req = 4;
	pcm->txdma_req = 6;

	printk(KERN_ALERT "platform_get_resource\n");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pcm->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pcm->regs)) {
		printk(KERN_ALERT "platform_get_resource ret:%d\r\n",ret);
		return PTR_ERR(pcm->regs);
	}
	printk(KERN_ALERT "devm_regmap_init_mmio\n");
	pcm->regmap = devm_regmap_init_mmio(&pdev->dev, pcm->regs,
			&ralink_pcm_regmap_config);
	if (IS_ERR(pcm->regmap)) {
		printk(KERN_ALERT "devm_regmap_init_mmio ret:%d\r\n",ret);
		dev_err(&pdev->dev, "regmap init failed\n");
		return PTR_ERR(pcm->regmap);
	}

	printk(KERN_ALERT "ralink_pcm_setup\n");
	ret = ralink_pcm_setup(pcm);
	if(0 != ret)
	{
		printk(KERN_ALERT "ralink_pcm_setup failed,ret:%d\r\n",ret);
	}
	printk(KERN_ALERT "platform_get_irq\n");
    irq = platform_get_irq(pdev, 0);
    if (irq < 0) {
			printk(KERN_ALERT "platform_get_irq ret:%d\r\n",ret);
            dev_err(&pdev->dev, "failed to get irq\n");
            return -EINVAL;
    }

#if (RALINK_PCM_INT_EN)
	ret = devm_request_irq(&pdev->dev, irq, ralink_pcm_irq,
			0, dev_name(&pdev->dev), pcm);
	if (ret) {
		printk(KERN_ALERT "devm_request_irq ret:%d\r\n",ret);
		dev_err(&pdev->dev, "failed to request irq\n");
		return ret;
	}
#endif
	printk(KERN_ALERT "devm_clk_get\n");
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

	p_gpcm = pcm;

	//device_reset(&pdev->dev);
	printk(KERN_ALERT "ralink_pcm_debugfs_create\n");
	ret = ralink_pcm_debugfs_create(pcm);
	if (ret) {
		printk(KERN_ALERT "ralink_pcm_debugfs_create ret:%d\r\n",ret);
		dev_err(&pdev->dev, "create debugfs failed\n");
		goto err_clk_disable;
	}
	printk(KERN_ALERT "devm_snd_soc_register_component\n");
	ret = devm_snd_soc_register_component(&pdev->dev, &ralink_pcm_component,
			&ralink_pcm_dai, 1);
	if (ret)
	{
		printk(KERN_ALERT "devm_snd_soc_register_component ret:%d\r\n",ret);
		goto err_debugfs;
	}

	//DMA相关
	///*
	printk(KERN_ALERT "ralink_pcm_init_dma_data\n");
	ralink_pcm_init_dma_data(pcm, res);

	printk(KERN_ALERT "ralink_pcm_init_dma_data ok\r\n");

	//snd_dmaengine_pcm_register(&pdev->dev,
	//				&ralink_dmaengine_pcm_config,
	//				SND_DMAENGINE_PCM_FLAG_COMPAT);
	printk(KERN_ALERT "devm_snd_dmaengine_pcm_register\n");
	ret = devm_snd_dmaengine_pcm_register(&pdev->dev,
			&ralink_dmaengine_pcm_config,
			SND_DMAENGINE_PCM_FLAG_COMPAT);
	if (ret)
	{
		printk(KERN_ALERT "devm_snd_dmaengine_pcm_register ret:%d\r\n",ret);
		//goto err_debugfs;
	}
	printk(KERN_ALERT "devm_snd_dmaengine_pcm_register ok\r\n");

	ralink_pcm_debug_dma(pcm);
	
	//*/

	//dev_info(pcm->dev, "mclk %luMHz\n", clk_get_rate(pcm->clk) / 1000000);

	ralink_pcm_dump_regs(pcm);

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

	ralink_pcm_clean(pcm);

	printk(KERN_ALERT "ralink_pcm_remove\n");
	ralink_pcm_debugfs_remove(pcm);
	clk_disable_unprepare(pcm->clk);

	//snd_soc_unregister_component(&pdev->dev);

	//iounmap(pcm->regs);
	//release_mem_region(pcm->mem->start, resource_size(pcm->mem));

	//kfree(pcm);

	snd_dmaengine_pcm_unregister(&pdev->dev);

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
