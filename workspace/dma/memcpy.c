#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

struct dma_chan *chan;
unsigned int *txbuf;	//发送buf虚拟地址
unsigned int *rxbuf;	//接收buf虚拟地址
dma_addr_t txaddr;		//保存发送的总线地址
dma_addr_t rxaddr;		//保存接收的总线地址

static void dma_callback(void *data)
{
	int i;
	unsigned int *p = rxbuf;
	printk("dma complete\n");

	for (i = 0; i < PAGE_SIZE / sizeof(unsigned int); i++)
		printk("%d ", *p++);
	printk("\n");
}

static bool filter(struct dma_chan *chan, void *filter_param)
{
	printk("%s\n", dma_chan_name(chan));
	return strcmp(dma_chan_name(chan), filter_param) == 0;
}

static int __init memcpy_init(void)
{
	int i;
	dma_cap_mask_t mask;
	struct dma_async_tx_descriptor *desc;
	char name[] = "dma0chan0";
	unsigned int *p;
	struct dma_slave_config slave_config;
	int ret;

	dma_cap_zero(mask);
	dma_cap_set(DMA_MEMCPY, mask);
	chan = dma_request_channel(mask, filter, name);	//filter和name用来找到指定条件的dma通道
	if (!chan) {
		printk("dma_request_channel failure\n");
		return -ENODEV;
	}
	
	txbuf = dma_alloc_coherent(chan->device->dev, PAGE_SIZE, &txaddr, GFP_KERNEL);
	if (!txbuf) {
		printk("dma_alloc_coherent failure\n");
		dma_release_channel(chan);
		return -ENOMEM;
	}

	rxbuf = dma_alloc_coherent(chan->device->dev, PAGE_SIZE, &rxaddr, GFP_KERNEL);
	if (!rxbuf) {
		printk("dma_alloc_coherent failure\n");
		dma_free_coherent(chan->device->dev, PAGE_SIZE, txbuf, txaddr);
		dma_release_channel(chan);
		return -ENOMEM;
	}

	
	memset(&slave_config, 0, sizeof(slave_config));
	slave_config.direction = DMA_MEM_TO_DEV;
	slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	slave_config.device_fc = false;
	ret = dmaengine_slave_config(chan, &slave_config);
	


	//初始化发送buf
	for (i = 0, p = txbuf; i < PAGE_SIZE / sizeof(unsigned int); i++)
		*p++ = i;
	for (i = 0, p = txbuf; i < PAGE_SIZE / sizeof(unsigned int); i++)
		printk("%d ", *p++);
	printk("\n");
	//初始化接收buf
	memset(rxbuf, 0, PAGE_SIZE);
	for (i = 0, p = rxbuf; i < PAGE_SIZE / sizeof(unsigned int); i++)
		printk("%d ", *p++);
	printk("\n");
	//通道,接收地址,发送地址,大小,标志位
	desc = chan->device->device_prep_dma_memcpy(chan, rxaddr, txaddr, PAGE_SIZE, DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
	desc->callback = dma_callback;
	desc->callback_param = NULL;

	dmaengine_submit(desc);	//提交发送,发送完成后执行dma_callback
	dma_async_issue_pending(chan);

	return 0;
}

static void __exit memcpy_exit(void)
{
	dma_free_coherent(chan->device->dev, PAGE_SIZE, txbuf, txaddr);
	dma_free_coherent(chan->device->dev, PAGE_SIZE, rxbuf, rxaddr);
	dma_release_channel(chan);
}

module_init(memcpy_init);
module_exit(memcpy_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin Jiang <jiangxg@farsight.com.cn>");
MODULE_DESCRIPTION("simple driver using dmaengine");
