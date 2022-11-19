#ifndef __PCM_H__
#define __PCM_H__

#include <linux/regmap.h>

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

void ralink_pcm_dump_regs(struct ralink_pcm *pcm);
#endif