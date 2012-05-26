/*
 * sdhci-pltfm.c Support for SDHCI platform devices
 * Copyright (c) 2009 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Supports:
 * SDHCI platform devices
 *
 * Inspired by sdhci-pci.c, by Pierre Ossman
 */

#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>

#include <linux/mmc/host.h>

#include <linux/io.h>
#include <linux/mmc/sdhci-pltfm.h>

#include "sdhci.h"
#include "sdhci-pltfm.h"

//HTC_CSP_START
#if defined(CONFIG_MACH_ENDEAVORU) || defined(CONFIG_MACH_ENDEAVORTD)
#include <mach/iomap.h>

struct platform_device *mmci_get_platform_device(void);
struct mmc_host *mmci_get_mmc(void);
typedef struct wlan_sdioDrv{
	struct platform_device *pdev;
	struct mmc_host *mmc;
	int (*wlan_sdioDrv_pm_resume)(void);
	int (*wlan_sdioDrv_pm_suspend)(void);

} wlan_sdioDrv_t;
wlan_sdioDrv_t g_wlan_sdioDrv;
#endif
//HTC_CSP_END

/*****************************************************************************\
 *                                                                           *
 * SDHCI core callbacks                                                      *
 *                                                                           *
\*****************************************************************************/

static struct sdhci_ops sdhci_pltfm_ops = {
};

/*****************************************************************************\
 *                                                                           *
 * Device probing/removal                                                    *
 *                                                                           *
\*****************************************************************************/

static int __devinit sdhci_pltfm_probe(struct platform_device *pdev)
{
//HTC_CSP_START
#if defined(CONFIG_MACH_ENDEAVORU) || defined(CONFIG_MACH_ENDEAVORTD)
    int addr = 0;
#endif
//HTC_CSP_END
	const struct platform_device_id *platid = platform_get_device_id(pdev);
	struct sdhci_pltfm_data *pdata;
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct resource *iomem;
	int ret;

	if (platid && platid->driver_data)
		pdata = (void *)platid->driver_data;
	else
		pdata = pdev->dev.platform_data;

	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iomem) {
		ret = -ENOMEM;
		goto err;
	}

//HTC_CSP_START
#if defined(CONFIG_MACH_ENDEAVORU) || defined(CONFIG_MACH_ENDEAVORTD)
    addr = iomem->start;
    //printk(KERN_INFO "start addr = 0x%x\n", addr);
#endif
//HTC_CSP_END

	if (resource_size(iomem) < 0x100)
		dev_err(&pdev->dev, "Invalid iomem size. You may "
			"experience problems.\n");

	/* Some PCI-based MFD need the parent here */
	if (pdev->dev.parent != &platform_bus)
		host = sdhci_alloc_host(pdev->dev.parent, sizeof(*pltfm_host));
	else
		host = sdhci_alloc_host(&pdev->dev, sizeof(*pltfm_host));

	if (IS_ERR(host)) {
		ret = PTR_ERR(host);
		goto err;
	}

	pltfm_host = sdhci_priv(host);

	host->hw_name = "platform";
	if (pdata && pdata->ops)
		host->ops = pdata->ops;
	else
		host->ops = &sdhci_pltfm_ops;
	if (pdata)
		host->quirks = pdata->quirks;
	host->irq = platform_get_irq(pdev, 0);

	if (!request_mem_region(iomem->start, resource_size(iomem),
		mmc_hostname(host->mmc))) {
		dev_err(&pdev->dev, "cannot request region\n");
		printk(KERN_INFO "%s: cannot request region\n", __func__);
		ret = -EBUSY;
		goto err_request;
	}

	host->ioaddr = ioremap(iomem->start, resource_size(iomem));
	if (!host->ioaddr) {
		dev_err(&pdev->dev, "failed to remap registers\n");
		ret = -ENOMEM;
		goto err_remap;
	}

	if (pdata && pdata->init) {
		ret = pdata->init(host, pdata);
		if (ret)
			goto err_plat_init;
	}

	ret = sdhci_add_host(host);
	if (ret)
		goto err_add_host;

	platform_set_drvdata(pdev, host);

//HTC_CSP_START
#if defined(CONFIG_MACH_ENDEAVORU) || defined(CONFIG_MACH_ENDEAVORTD)
//printk(KERN_INFO "[SD] SdioDrv_probe pdev:0x%x  mmc:0x%x  mmc->index=%d pdev->resource[1].start=%x, addr=%x\n",
//		   (int)pdev, host->mmc, host->mmc->index, pdev->resource[1].start, addr);

	if (addr == TEGRA_SDMMC3_BASE) {
//	printk(KERN_INFO "[SD] Save sdmmc3 dev, mmc\n");
//	printk(KERN_INFO "[SD] SdioDrv_probe pdev:0x%x  mmc:0x%x  mmc->index=%d pdev->resource[1].start=%x, addr=%x\n",
//		   (int)pdev, host->mmc, host->mmc->index, pdev->resource[1].start, addr);
		g_wlan_sdioDrv.pdev = pdev;
		g_wlan_sdioDrv.mmc = host->mmc;
//TODO		wlan_perf_lock = 0;
	}
#endif
//HTC_CSP_END


	return 0;

err_add_host:
	if (pdata && pdata->exit)
		pdata->exit(host);
err_plat_init:
	iounmap(host->ioaddr);
err_remap:
	release_mem_region(iomem->start, resource_size(iomem));
err_request:
	sdhci_free_host(host);
err:
	printk(KERN_ERR"Probing of sdhci-pltfm failed: %d\n", ret);
	return ret;
}

static int __devexit sdhci_pltfm_remove(struct platform_device *pdev)
{
	struct sdhci_pltfm_data *pdata = pdev->dev.platform_data;
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct resource *iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	int dead;
	u32 scratch;

	dead = 0;
	scratch = readl(host->ioaddr + SDHCI_INT_STATUS);
	if (scratch == (u32)-1)
		dead = 1;

	sdhci_remove_host(host, dead);
	if (pdata && pdata->exit)
		pdata->exit(host);
	iounmap(host->ioaddr);
	release_mem_region(iomem->start, resource_size(iomem));
	sdhci_free_host(host);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct platform_device_id sdhci_pltfm_ids[] = {
	{ "sdhci", },
#ifdef CONFIG_MMC_SDHCI_CNS3XXX
	{ "sdhci-cns3xxx", (kernel_ulong_t)&sdhci_cns3xxx_pdata },
#endif
#ifdef CONFIG_MMC_SDHCI_ESDHC_IMX
	{ "sdhci-esdhc-imx", (kernel_ulong_t)&sdhci_esdhc_imx_pdata },
#endif
#ifdef CONFIG_MMC_SDHCI_DOVE
	{ "sdhci-dove", (kernel_ulong_t)&sdhci_dove_pdata },
#endif
#ifdef CONFIG_MMC_SDHCI_TEGRA
	{ "sdhci-tegra", (kernel_ulong_t)&sdhci_tegra_pdata },
#endif
	{ },
};
MODULE_DEVICE_TABLE(platform, sdhci_pltfm_ids);

#ifdef CONFIG_PM
static int sdhci_pltfm_suspend(struct platform_device *dev, pm_message_t state)
{
	struct sdhci_host *host = platform_get_drvdata(dev);
	int ret;

	ret = sdhci_suspend_host(host, state);
	if (ret) {
		dev_err(&dev->dev, "suspend failed, error = %d\n", ret);
		return ret;
	}

	if (host->ops && host->ops->suspend)
		ret = host->ops->suspend(host, state);
	if (ret) {
		dev_err(&dev->dev, "suspend hook failed, error = %d\n", ret);
		sdhci_resume_host(host);
	}

	return ret;
}

static int sdhci_pltfm_resume(struct platform_device *dev)
{
	struct sdhci_host *host = platform_get_drvdata(dev);
	int ret = 0;

	if (host->ops && host->ops->resume)
		ret = host->ops->resume(host);
	if (ret) {
		dev_err(&dev->dev, "resume hook failed, error = %d\n", ret);
		return ret;
	}

	ret = sdhci_resume_host(host);
	if (ret)
		dev_err(&dev->dev, "resume failed, error = %d\n", ret);

	return ret;
}
#else
#define sdhci_pltfm_suspend	NULL
#define sdhci_pltfm_resume	NULL
#endif	/* CONFIG_PM */

static struct platform_driver sdhci_pltfm_driver = {
	.driver = {
		.name	= "sdhci",
		.owner	= THIS_MODULE,
	},
	.probe		= sdhci_pltfm_probe,
	.remove		= __devexit_p(sdhci_pltfm_remove),
	.id_table	= sdhci_pltfm_ids,
	.suspend	= sdhci_pltfm_suspend,
	.resume		= sdhci_pltfm_resume,
};

//HTC_CSP_START
#if defined(CONFIG_MACH_ENDEAVORU) || defined(CONFIG_MACH_ENDEAVORTD)
struct platform_device *mmci_get_platform_device(void){
	printk(KERN_INFO "sdhci-tegra.c  g_wlan_sdioDrv.pdev = 0x%x\n", g_wlan_sdioDrv.pdev);
	return g_wlan_sdioDrv.pdev;
}
EXPORT_SYMBOL(mmci_get_platform_device);

struct mmc_host *mmci_get_mmc(void){
	printk(KERN_INFO "sdhci-tegra.c  g_wlan_sdioDrv.mmc = 0x%x\n", g_wlan_sdioDrv.mmc);
	return g_wlan_sdioDrv.mmc;
}
EXPORT_SYMBOL(mmci_get_mmc);
#endif
//HTC_CSP_END


/*****************************************************************************\
 *                                                                           *
 * Driver init/exit                                                          *
 *                                                                           *
\*****************************************************************************/

static int __init sdhci_drv_init(void)
{
	return platform_driver_register(&sdhci_pltfm_driver);
}

static void __exit sdhci_drv_exit(void)
{
	platform_driver_unregister(&sdhci_pltfm_driver);
}

module_init(sdhci_drv_init);
module_exit(sdhci_drv_exit);

MODULE_DESCRIPTION("Secure Digital Host Controller Interface platform driver");
MODULE_AUTHOR("Mocean Laboratories <info@mocean-labs.com>");
MODULE_LICENSE("GPL v2");
