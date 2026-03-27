#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include "../../uft_pcie/uft_pcie_ep.h"
#include "../../uft_pcie/xring_pcie_hdma.h"
#include "../../spi_apb/spi_apb_regops.h"
#include "hpc_internal.h"
#include "uft_hpc_cfg.h"
#include "uft_npu_mailbox.h"


/****************************************************************************
 * Version Information
 ****************************************************************************/
#define BELL_NAME                  "KMD BELL_V100"
#define MAJOR_NUMBER               "0"
#define MINOR_NUMBER               "1"
#define PATCH_NUMBER               "1"
#define VERSION_DATE               "2024-12-23"
#define VERSION_NUMBER             "V" MAJOR_NUMBER "." MINOR_NUMBER "." PATCH_NUMBER
#define VERSION_ALPHA              "Alpha" //The earliest version, internal version
#define VERSION_BETA               "Beta"
#define VERSION_RC                 "Release"
#define VERSION_STR                BELL_NAME ":" VERSION_NUMBER "_" VERSION_DATE "_" VERSION_ALPHA

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define F3_UFT_DDR_0_START_ADDR    0x00000000
#define F3_UFT_DDR_0_END_ADDR      0x9FFFFFFF
#define F3_UFT_DDR_1_START_ADDR    0x100000000
#define F3_UFT_DDR_1_END_ADDR      0x1FFFFFFFF
#define SYSCTRL_M85_CPUWAIT_REG    0x0004
#define SYSTEM_NOC_INIT_REG        0xd1f0200c
#define M85_IRAM_GRP_NUM_REG       0x0008
#define M85_AXCACHE_SEL_REG        0x00C8
#define NPU_HARDWARE_RESET_DELAY   10
#define NPU_PROFILE_VOLT_0P80_REG  0x00
#define POLLING_TIMES              6000
#define POLLING_DELAY              10
#define POLLING_DEFAULT_TIME       1440
#define NPU_MCU_BOOT_RETRY_TIMES   10
#define NPU_MCU_BOOT_TEST_ADDR0    0x80000000
#define NPU_MCU_BOOT_TEST_VALUE0   0x12345678
#define NPU_MCU_BOOT_TEST_ADDR1    0x80000004
#define NPU_MCU_BOOT_TEST_VALUE1   0xA5A5A5A5

/****************************************************************************
 * Structure Definitions
 ****************************************************************************/
struct npu_msg_header {
	uint32_t process_id;
	uint32_t thread_id;
	uint32_t cmd;
	uint32_t msg_id;
};

struct payload {
	uint32_t data;
	int ack_status;
};

struct npu_rpmsg_msg {
	struct npu_msg_header header;
	struct payload payload;
};

struct debugfs_priv {
	unsigned long reg_addr;
	unsigned long reg_value[4];
};

/* static function */
static ssize_t powerctrl_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t powerctrl_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
static int npu_load_data(char *filename, u64 address);
static ssize_t load_data_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t load_data_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
static ssize_t access_reg_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t access_reg_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
static ssize_t poll_timeout_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t poll_timeout_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);

/****************************************************************************
 * Private Data
 ****************************************************************************/
extern struct hpc_rpmsg_device *g_hrpdev;
struct uft_pcie_ep *g_ep;
static struct pci_dev *g_pci_dev;
static struct delayed_work uft_hpc_init_work;
static struct dentry *root;
static struct debugfs_priv *access_reg_priv;
static struct file_operations powerctrl_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = powerctrl_read,
	.write = powerctrl_write,
};

static struct file_operations load_data_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = load_data_read,
	.write = load_data_write,
};

static struct file_operations access_reg_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = access_reg_read,
	.write = access_reg_write,
};

const struct file_operations poll_timeout_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = poll_timeout_read,
	.write = poll_timeout_write,
};

static int s_timeout_cnt = POLLING_DEFAULT_TIME;

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static uint32_t read_reg_by_spi(unsigned long address)
{
	int ret;
	uint32_t val;

	ret = spi_apb_regops_read(address, &val);
	if (ret != 0) {
		return ret;
	}

	return val;
}

static int write_reg_by_spi(unsigned long address, unsigned long data)
{
	uint32_t val;

	putreg32(data, address);
	hpcinfo("spi write addr: 0x%lx = 0x%lx\n", address, data);
	val = read_reg_by_spi(address);
	if (val != data) {
		hpcerr("spi read addr: 0x%lx = 0x%x\n", address, val);
		return -1;
	}

	return 0;
}

static void write_reg_by_pcie(u32 reg, u32 val)
{
	if (g_ep == NULL) {
		hpcerr("ep is NULL\n");
		return;
	}

	uft_pcie_ep_reg_write(g_ep, reg, val);
	hpcinfo("pcie write addr: 0x%x = 0x%x\n", reg, val);
	hpcinfo("pcie read addr: 0x%x = 0x%x\n", reg, uft_pcie_ep_reg_read(g_ep, reg));
}

static void write_mask_by_pcie(u32 reg, u32 val, u32 mask)
{
	u32 rdata;

	if (g_ep == NULL) {
		hpcerr("ep is NULL\n");
		return;
	}

	rdata = uft_pcie_ep_reg_read(g_ep, reg);
	uft_pcie_ep_reg_write(g_ep, reg, ((rdata & (~mask)) | (val & mask)));
	hpcinfo("pcie write addr: 0x%x = 0x%x mask = 0x%x rdata = 0x%x\n", reg, val, mask, rdata);
	hpcinfo("pcie read addr: 0x%x = 0x%x\n", reg, uft_pcie_ep_reg_read(g_ep, reg));
}

static u32 read_reg_by_pcie(u32 reg)
{
	if (g_ep == NULL) {
		hpcerr("ep is NULL\n");
		return 0;
	}

	return uft_pcie_ep_reg_read(g_ep, reg);
}

static int mbox_xfer(u32 channel, u32 *msg, u32 msg_len)
{
	u32 data = 0xFF;

	// 配置source寄存器
	uft_pcie_ep_reg_write(g_ep, IPCM_SR(channel), BIT(NS_ID_ACPU0));
	// 配置mode寄存器
	uft_pcie_ep_reg_write(g_ep, IPCM_MODE(channel), BIT(IPCM_MODE_AUTO_ACK));
	// 配置dest寄存器
	uft_pcie_ep_reg_write(g_ep, IPCM_DSET(channel), BIT(NS_ID_MCU));
	// 配置mset寄存器，使能中断
	uft_pcie_ep_reg_write(g_ep, IPCM_MSET(channel), BIT(NS_ID_ACPU0) | BIT(NS_ID_MCU));
	// 配置data寄存器
	uft_pcie_ep_reg_write(g_ep, IPCM_DAT(channel, 0), 1); // data len
	uft_pcie_ep_reg_write(g_ep, IPCM_DAT(channel, 1), data); // data content
	// 配置send寄存器
	uft_pcie_ep_reg_write(g_ep, IPCM_SEND(channel), IPCM_SEND_TO_DEST);

	return 0;
}

static void npu_subchip_power_on(void)
{
	int timeout;
	u32 rval;

	hpcinfo("config start\n");
	//GPC_RST0_CLR set GPC0~3 rstn
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x0014, 0x000f);
	//CFG_NOC IDLE status
	write_mask_by_pcie(NPU_SUBCHIP_CTRL_BASE + 0x200c, (0 << 0),0x00000001);
	for (timeout = 0; timeout < POLLING_TIMES; timeout++) {
		rval = read_reg_by_pcie(NPU_SUBCHIP_CTRL_BASE + 0x2008);
		if ((~rval) & 0x400) {
			break;
		}
		hpcinfo("noc idle 0x%x = 0x%x times = %d\n", NPU_SUBCHIP_CTRL_BASE + 0x2008, rval, timeout);
		msleep(POLLING_DELAY);
	}

	hpcinfo("config end\n");
}

static void npu_subsys_power_on(void)
{
	hpcinfo("config start\n");
	//Set sc_npu_mtcmos_en
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1004, 0x00010001);
	msleep(1);
	//npu_subsys_mem_exit_sd
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x100C, ((0 << 0) | (0x00000001 << 16)));
	msleep(1);
	//npu_subsys_mem_exit_ds
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x100C, ((0 << 1) | (0x00000002 << 16)));
	msleep(1);
	//npu_subsys_iso_deassert
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1000, ((0 << 0) | (0x00000001 << 16)));
	msleep(1);

	//npu_subchip_ip_rst_release
	write_reg_by_pcie(NPU_CRG_BASE + 0x3080, 0x7C11FFFC);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3090, 0x02707FFF);
	msleep(1);
	//npu_subchip_ip_clk_on
	write_reg_by_pcie(NPU_CRG_BASE + 0x3000, 0xF2A0703F);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3010, 0x0C1FF9E0);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3020, 0x0001FFFF);

	hpcinfo("config end\n");
}

static void npu_crg_clk_to_volt(u32 volt)
{
	hpcinfo("config start\n");

	if (volt == NPU_PROFILE_VOLT_0P80_REG) {
		hpcinfo("config npu clk volt to 0.8V\n");
		write_reg_by_pcie(NPU_CRG_BASE + 0x30f4, 0x00030002);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30f4, 0x000c0000);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30f4, 0x00600000);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30f4, 0x01800000);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30f8, 0x00010001);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30f4, 0x20002000);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30f4, 0x02000000);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30f4, 0x40004000);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30f4, 0x04000000);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30f4, 0x18001800);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30f4, 0x80008000);

		write_reg_by_pcie(NPU_CRG_BASE + 0x3404, 0x003F0005);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30d0, 0xffffa041);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30d4, 0xffffa041);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30d8, 0xffff0040);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30e0, 0xffffe040);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30dc, 0xffffffc0);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30e4, 0xffffe040);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30e8, 0xffffe040);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30f0, 0xffffe040);
		write_reg_by_pcie(NPU_CRG_BASE + 0x30ec, 0xffffe040);
	}
	hpcinfo("config end\n");
}

static void npu_mcu_power_on(void)
{
	int timeout;
	u32 rval;

	hpcinfo("config start\n");

	//npu_subchip_cfgbus_reconnect: sysctrl m85 cpu wait set 1
	write_mask_by_pcie(NPU_SYSCTRL_FUN_S_BASE + 0x4, (1 << 0x00), 0x00000001);
	//sysctrl m85 initsvtor set
	write_mask_by_pcie(NPU_SYSCTRL_FUN_S_BASE + 0x8, 0xc00000, 0x01FFFFFF);
	//iprst rst bit 0-4
	write_reg_by_pcie(NPU_CRG_BASE + 0x30b0, 0x1F);
	msleep(1);
	//sc_gt_clk_sys2mcu_func on
	write_reg_by_pcie(NPU_CRG_BASE + 0x30d8, ((1 << 6) | (0x00000040 << 16)));
	//sc_gt_clk_npu_mcu_systick on
	write_reg_by_pcie(NPU_CRG_BASE + 0x30dC, ((1 << 6) | (0x00000040 << 16)));
	//gt clk
	write_reg_by_pcie(NPU_CRG_BASE + 0x3030, 0xFF);
	//npu_mcu_bus_connect
	write_mask_by_pcie(NPU_SUBCHIP_CTRL_BASE + 0x200C, (0 << 3), 0x00000008);
	for (timeout = 0; timeout < POLLING_TIMES; timeout++) {
		rval = read_reg_by_pcie(NPU_SUBCHIP_CTRL_BASE + 0x2000);
		if ((~rval) & 0x20) {
			break;
		}
		hpcinfo("idle ack 0x%x = 0x%x times = %d\n", NPU_SUBCHIP_CTRL_BASE + 0x2000, rval, timeout);
		msleep(POLLING_DELAY);
	}
	//npu_ss_bus_connect vdsp_idlereq set to 0
	write_mask_by_pcie(NPU_SUBCHIP_CTRL_BASE + 0x200C, 0 << 4, 0x00000010);
	for (timeout = 0; timeout < POLLING_TIMES; timeout++) {
		rval = read_reg_by_pcie(NPU_SUBCHIP_CTRL_BASE + 0x2008);
		if ((~rval) & 0x400) {
			break;
		}
		hpcinfo("idle ack 0x%x = 0x%x times = %d\n", NPU_SUBCHIP_CTRL_BASE + 0x2008, rval, timeout);
		msleep(POLLING_DELAY);
	}

	hpcinfo("config end\n");
}

static void npu_vdsp_power_on(void)
{
	int timeout;
	u32 rval;

	hpcinfo("config start\n");

	//sc_npu_vdsp_mtcmos_en set to 1
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1004, ((1 << 1) | (0x00000002 << 16)));
	//sc_npu_top_vdsp_tbu_mem_sd_set_to 0
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x100C, ((0 << 4) | (0x00000010 << 16)));
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x100C, ((0 << 4) | (0x00000010 << 16)));
	msleep(1);
	//sc_npu_vdsp_mem_ds set to 0
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x100C, ((0 << 7) | (0x00000080 << 16)));
	//sc_npu_top_vdsp_tbu_mem_ds_set_to 0
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x100C, ((0 << 5) | (0x00000020 << 16)));
	//npu_vdsp_flushxst
	write_reg_by_pcie(NPU_CRG_BASE + 0x3000, 0xF8280);
	msleep(1);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3004, 0xF8280);
	//npu_vdsp_iso_deassert en off
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1000, ((0 << 1) | (0x00000002 << 16)));
	msleep(1);
	//vdsp_run_stall set to 1
	write_mask_by_pcie(NPU_SYSCTRL_FUN_NS_BASE + 0x4, (1 << 1), 0x00000002);
	//npu_vdsp_rst_release
	write_reg_by_pcie(NPU_CRG_BASE + 0x3080, ((1 << 21) | (1 << 18) | (1 << 17)));
	//npu_vdsp_clk_on
	write_reg_by_pcie(NPU_CRG_BASE + 0x3000, (1 << 19));
	write_reg_by_pcie(NPU_CRG_BASE + 0x3000, ((1 << 18) | (1 << 17) | (1 << 16) | (1 << 15)));
	write_reg_by_pcie(NPU_CRG_BASE + 0x3000, ((1 << 9) | (1 << 7) ));
	write_reg_by_pcie(NPU_CRG_BASE + 0x3010, (1 << 21) );
	msleep(1);
	//npu_vdsp_bus_connect
	write_mask_by_pcie(NPU_SUBCHIP_CTRL_BASE + 0x200C, 0 << 2, 0x00000004);
	for (timeout = 0; timeout < POLLING_TIMES; timeout++) {
		rval = read_reg_by_pcie(NPU_SUBCHIP_CTRL_BASE + 0x2000);
		if ((~rval) & 0x08) {
			break;
		}
		hpcinfo("idle ack 0x%x = 0x%x times = %d\n", NPU_SUBCHIP_CTRL_BASE + 0x2000, rval, timeout);
		msleep(POLLING_DELAY);
	}

	hpcinfo("config end\n");
}

static void npu_bell_top_power_on(void)
{
	int timeout;
	u32 rval;

	hpcinfo("config start\n");

	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1004, ((1 << 2) | (0x00000004 << 16)));
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x100C, ((0 << 8) | (0x00000100 << 16)));
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x100C, ((0 << 2) | (0x00000004 << 16)));
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1010, ((0 << 0) | (0x00000001 << 16)));
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1010, ((0 << 2) | (0x00000004 << 16)));
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1010, ((0 << 4) | (0x00000010 << 16)));
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1010, ((0 << 6) | (0x00000040 << 16)));
	msleep(1);
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x100C, ((0 << 9) | (0x00000200 << 16)));
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x100C, ((0 << 3) | (0x00000008 << 16)));
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1010, ((0 << 1) | (0x00000002 << 16)));
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1010, ((0 << 3) | (0x00000008 << 16)));
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1010, ((0 << 5) | (0x00000020 << 16)));
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1010, ((0 << 7) | (0x00000080 << 16)));
	//task_npu_bell_crg_rst_release
	write_reg_by_pcie(NPU_CRG_BASE + 0x30A0, (1 << 10));
	msleep(1);
	//task_npu_bell_top_fluhsxst
	write_reg_by_pcie(NPU_CRG_BASE + 0x3020, (1 << 19));
	write_reg_by_pcie(NPU_CRG_BASE + 0x3020, (1 << 20));
	write_reg_by_pcie(NPU_CRG_BASE + 0x3020, (1 << 18));
	write_reg_by_pcie(NPU_CRG_BASE + 0x3020, (1 << 17));
	msleep(1);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3024, (1 << 19));
	write_reg_by_pcie(NPU_CRG_BASE + 0x3024, (1 << 20));
	write_reg_by_pcie(NPU_CRG_BASE + 0x3024, (1 << 18));
	write_reg_by_pcie(NPU_CRG_BASE + 0x3024, (1 << 17));
	msleep(1);
	//task_npu_bell_top_iso_deassert
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1000, ((0 << 2) | (0x00000004 << 16)));
	msleep(1);
	//task_npu_bell_top_ip_release
	write_reg_by_pcie(NPU_CRG_BASE + 0x30A0, (1 << 9));
	msleep(1);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3020, (1 << 19));
	write_reg_by_pcie(NPU_CRG_BASE + 0x3020, (1 << 20));
	write_reg_by_pcie(NPU_CRG_BASE + 0x3020, (1 << 18));
	write_reg_by_pcie(NPU_CRG_BASE + 0x3020, (1 << 17));
	write_reg_by_pcie(NPU_CRG_BASE + 0x30A0, 0x1F);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3080, 0x3C80002);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3090, (1 << 15));
	msleep(1);
	//task_npu_bell_top_ip_clk_on
	write_reg_by_pcie(NPU_CRG_BASE + 0x3040, 0x255);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3050, 0x6A);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3040, 0x1AA);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3050, 0x15);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3000, 0xD500C00);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3010, 0x3C00401);
	msleep(1);
	//task_npu_bell_top_bus_reconnect
	write_mask_by_pcie(NPU_SUBCHIP_CTRL_BASE + 0x200C, 0 << 1, 0x00000002);
	for (timeout = 0; timeout < POLLING_TIMES; timeout++) {
		rval = read_reg_by_pcie(NPU_SUBCHIP_CTRL_BASE + 0x2000);
		if ((~rval) & 0x02) {
			break;
		}
		hpcinfo("idle ack 0x%x = 0x%x times = %d\n", NPU_SUBCHIP_CTRL_BASE + 0x2000, rval, timeout);
		msleep(POLLING_DELAY);
	}
	//cu0123_gpc_bupass
	write_mask_by_pcie(NPU_LPCTRL_BASE + 0x2020, 1 << 1, 0x00000002);
	write_mask_by_pcie(NPU_LPCTRL_BASE + 0x2420, 1 << 1, 0x00000002);
	write_mask_by_pcie(NPU_LPCTRL_BASE + 0x2820, 1 << 1, 0x00000002);
	write_mask_by_pcie(NPU_LPCTRL_BASE + 0x2C20, 1 << 1, 0x00000002);

	hpcinfo("config end\n");
}

static void npu_bell_cu_power_on(int cu_id)
{
	hpcinfo("config start\n");

	//task_npu_bell_cu_mtcmos on
	if (cu_id == 0) {
		write_mask_by_pcie(NPU_LPCTRL_BASE + 0x2020, 1 << 1, 0x00000002);
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x202C, ((1 << 0) | (0x00000001 << 16)));
	} else if (cu_id == 1) {
		write_mask_by_pcie(NPU_LPCTRL_BASE + 0x2420, 1 << 1, 0x00000002);
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x242C, ((1 << 0) | (0x00000001 << 16)));
	} else if (cu_id == 2) {
		write_mask_by_pcie(NPU_LPCTRL_BASE + 0x2820, 1 << 1, 0x00000002);
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x282C, ((1 << 0) | (0x00000001 << 16)));
	} else if (cu_id == 3) {
		write_mask_by_pcie(NPU_LPCTRL_BASE + 0x2C20, 1 << 1, 0x00000002);
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x2C2C, ((1 << 0) | (0x00000001 << 16)));
	}
	msleep(1);
	//task_npu_bell_cu_mem_exit_sd
	if (cu_id ==0) {
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x202C, ((0 << 4) | (0x00000010 << 16)));
	} else if (cu_id == 1) {
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x242C, ((0 << 4) | (0x00000010 << 16)));
	} else if (cu_id == 2) {
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x282C, ((0 << 4) | (0x00000010 << 16)));
	} else if (cu_id == 3) {
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x2C2C, ((0 << 4) | (0x00000010 << 16)));
	}
	msleep(1);
	//task_npu_bell_cu_mem_exit_ds
	if (cu_id == 0) {
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1010, ((0 << 8) | (0x00000100 << 16)));
	} else if (cu_id == 1) {
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1010, ((0 << 9) | (0x0000200 << 16)));
	} else if (cu_id == 2) {
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1010, ((0 << 10) | (0x0000400 << 16)));
	} else if (cu_id == 3) {
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1010, ((0 << 11) | (0x0000800 << 16)));
	}
	//task_npu_bell_cu_flushxst
	if (cu_id == 0) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3040, ((1 << 17) | (1 << 14) | (1 << 16) | (1 << 15)));
	} else if (cu_id == 1) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3040, ((1 << 13) | (1 << 10) | (1 << 12) | (1 << 11)));
	} else if (cu_id == 2) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3050, ((1 << 14) | (1 << 11) | (1 << 13) | (1 << 12)));
	} else if (cu_id == 3) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3050, ((1 << 10) | (1 << 7) | (1 << 9) | (1 << 8)));
	}
	msleep(1);
	if (cu_id == 0) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3044, ((1 << 17) | (1 << 14) | (1 << 16) | (1 << 15)));
		write_reg_by_pcie(NPU_CRG_BASE + 0x3014, (1 << 4));
	} else if (cu_id == 1) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3044, ((1 << 13) | (1 << 10) | (1 << 12) | (1 << 11)));
		write_reg_by_pcie(NPU_CRG_BASE + 0x3014, (1 << 3));
	} else if (cu_id == 2) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3054, ((1 << 14) | (1 << 11) | (1 << 13) | (1 << 12)));
		write_reg_by_pcie(NPU_CRG_BASE + 0x3014, (1 << 2));
	} else if (cu_id == 3) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3054, ((1 << 10) | (1 << 7) | (1 << 9) | (1 << 8)));
		write_reg_by_pcie(NPU_CRG_BASE + 0x3014, (1 << 1));
	}
	msleep(1);
	//task_npu_bell_cu_iso_deassert
	if (cu_id == 0) {
		write_mask_by_pcie(NPU_LPCTRL_BASE + 0x2020, (0x1 << 1), 0x00000002);
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x202C,  ((0 << 1) | (0x00000002 << 16)));
	} else if (cu_id == 1) {
		write_mask_by_pcie(NPU_LPCTRL_BASE + 0x2420, (0x1 << 1), 0x00000002);
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x242C,  ((0 << 1) | (0x00000002 << 16)));
	} else if (cu_id == 2) {
		write_mask_by_pcie(NPU_LPCTRL_BASE + 0x2820, (0x1 << 1), 0x00000002);
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x282C,  ((0 << 1) | (0x00000002 << 16)));
	} else if (cu_id == 3) {
		write_mask_by_pcie(NPU_LPCTRL_BASE + 0x2C20, (0x1 << 1), 0x00000002);
		write_reg_by_pcie(NPU_LPCTRL_BASE + 0x2C2C,  ((0 << 1) | (0x00000002 << 16)));
	}
	msleep(1);
	//task_npu_bell_cu_rst_release
	if (cu_id == 0) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3090, (1 << 19));
		write_reg_by_pcie(NPU_CRG_BASE + 0x30A0, (1 << 8));
	} else if (cu_id == 1) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3090, (1 << 18));
		write_reg_by_pcie(NPU_CRG_BASE + 0x30A0, (1 << 7));
	} else if (cu_id == 2) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3090, (1 << 17));
		write_reg_by_pcie(NPU_CRG_BASE + 0x30A0, (1 << 6));
	} else if (cu_id == 3) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3090, (1 << 16));
		write_reg_by_pcie(NPU_CRG_BASE + 0x30A0, (1 << 5));
	}
	//task_npu_bell_cu_clk_on
	if (cu_id == 0) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3040, ((1 << 17) | (1 << 14) | (1 << 16) | (1 << 15)));
		write_reg_by_pcie(NPU_CRG_BASE + 0x3010, (1 << 4));
	} else if (cu_id == 1) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3040, ((1 << 13) | (1 << 10) | (1 << 12) | (1 << 11)));
		write_reg_by_pcie(NPU_CRG_BASE + 0x3010, (1 << 3));
	} else if (cu_id == 2) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3050, ((1 << 14) | (1 << 11) | (1 << 13) | (1 << 12)));
		write_reg_by_pcie(NPU_CRG_BASE + 0x3010, (1 << 2));
	} else if (cu_id == 3) {
		write_reg_by_pcie(NPU_CRG_BASE + 0x3050, ((1 << 10) | (1 << 7) | (1 << 9) | (1 << 8)));
		write_reg_by_pcie(NPU_CRG_BASE + 0x3010, (1 << 1));
	}

	hpcinfo("config end\n");
}

static void npu_disreset_fpga(void)
{
	hpcinfo("start\n");

	// 1. assert mcu cpuwait
	uft_pcie_ep_reg_write(g_ep, NPU_SYSCTRL_FUN_S_BASE + SYSCTRL_M85_CPUWAIT_REG, 0x1);

	// 2. config crg
	npu_subchip_power_on();
	npu_subsys_power_on();
	npu_crg_clk_to_volt(NPU_PROFILE_VOLT_0P80_REG);
	npu_mcu_power_on();
	npu_vdsp_power_on();
	npu_bell_top_power_on();
	npu_bell_cu_power_on(0);

	// 3. config mcu iram_grp_num
	uft_pcie_ep_reg_write(g_ep, NPU_M85_CTRL_BASE + M85_IRAM_GRP_NUM_REG, 0xC);

	// 4. set axcache = 1
	uft_pcie_ep_reg_write(g_ep, NPU_M85_CTRL_BASE + M85_AXCACHE_SEL_REG, 0x3);

	// 5. de-assert mcu cpuwait
	uft_pcie_ep_reg_write(g_ep, NPU_SYSCTRL_FUN_S_BASE + SYSCTRL_M85_CPUWAIT_REG, 0x0);

	hpcinfo("end\n");
}

static void npu_mcu_deinit(void)
{
	hpcinfo("start\n");
	/* assert mcu cpuwait */
	write_reg_by_pcie(NPU_SYSCTRL_FUN_S_BASE + SYSCTRL_M85_CPUWAIT_REG, 0x1);
	/* m85 rst */
	write_reg_by_pcie(NPU_CRG_BASE + 0x3034, 0xFF);
	udelay(10);
	/* m85 rst */
	write_reg_by_pcie(NPU_CRG_BASE + 0x30B4, 0x1F);
	udelay(10);
}

static void npu_subsys_power_off(void)
{
	hpcinfo("start\n");

	write_reg_by_pcie(NPU_CRG_BASE + 0x3004, 0xF2A0703F);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3014, 0xC1EF9E0);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3024, 0x1FFFF);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3054, (0x01 << 15));
	udelay(10);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3084, 0x7FFFFFFE);
	write_reg_by_pcie(NPU_CRG_BASE + 0x3094, 0xFFFFFFF);
	udelay(10);
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1000, ((0x1) | (0x1 << 16)));
	msleep(1);
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x100C, ((0x1 << 1) | (0x2 << 16)));
	udelay(10);
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x100C, ((0x1) | (0x1 << 16)));
	udelay(10);
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x1004, ((0x0) | (0x1 << 16)));
	udelay(10);

	hpcinfo("end\n");
}

static void npu_subchip_power_off(void)
{
	int timeout;
	u32 rval;

	hpcinfo("start\n");
	/* noc bus idle */
	write_mask_by_pcie(NPU_SUBCHIP_CTRL_BASE + 0x200c, (0x1 << 0),0x00000001);
	for (timeout = 0; timeout < POLLING_TIMES; timeout++) {
		rval = read_reg_by_pcie(NPU_SUBCHIP_CTRL_BASE + 0x2008);
		if ((rval) & 0x400) {
			break;
		}
		hpcinfo("noc idle 0x%x = 0x%x times = %d\n", NPU_SUBCHIP_CTRL_BASE + 0x2008, rval, timeout);
		msleep(POLLING_DELAY);
	}
	/* clk off */
	//write_reg_by_pcie(NPU_PERI_CRG_BASE + 0x3054, ((0x1 << 14) | (0x1 << 15) | (0x1 << 13)));
	/* shbchip ip rst assert */
	//write_reg_by_pcie(NPU_PERI_CRG_BASE + 0x30A4, ((0x1 << 5) | (0x1 << 4)));
	write_reg_by_pcie(NPU_LPCTRL_BASE + 0x18, 0xF);
	/* cfgbus_clk off */
	//write_reg_by_pcie(NPU_PERI_CRG_BASE + 0x3054, ((0x1 << 16)));
	//udelay(10);
	///* soc rs clk off */
	//write_reg_by_pcie(NPU_PERI_CRG_BASE + 0x54, ((0x1 << 14) | (0x1 << 15) | (0x1 << 13)));
	//write_reg_by_pcie(NPU_PERI_CRG_BASE + 0x64, ((0x1 << 2)));
	///* rst assert */
	//write_reg_by_pcie(NPU_PERI_CRG_BASE + 0x30c4, ((0x1 << 16)));
	///* crg rst assert */
	//write_reg_by_pcie(NPU_PERI_CRG_BASE + 0x30d4, ((0x1 << 26)));

	hpcinfo("end\n");
}

static void npu_reset_fpga(void)
{
	hpcinfo("start\n");

	npu_mcu_deinit();

	npu_subsys_power_off();

	npu_subchip_power_off();
	///* subsys rst */
	//write_reg_by_pcie(NPU_CRG_BASE + 0x3094, 0xFFFFFFF);
	///* npu bell rst */
	//write_reg_by_pcie(NPU_CRG_BASE + 0x30A4, 0x7FF);

	hpcinfo("end\n");
}

static void uft_reset_mailbox(void)
{
	/* clear mailbox irq to avoid legacy irqs */
	uft_pcie_ep_reg_write(g_ep, IPCM_MCLR(ACPU02MCU), BIT(NS_ID_MCU));
	uft_pcie_ep_reg_write(g_ep, IPCM_SR(ACPU02MCU), 0);
	uft_pcie_ep_reg_write(g_ep, IPCM_SEND(ACPU02MCU), IPCM_SEND_NO_AFFECT);
	uft_pcie_ep_reg_write(g_ep, IPCM_DCLR(MCU2ACPU0), BIT(NS_ID_ACPU0));
	uft_pcie_ep_reg_write(g_ep, IPCM_SEND(MCU2ACPU0), IPCM_SEND_TO_SRC);
}

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
static void npu_load_firmware(struct hpc_device *hdev)
{
	int ret;
	struct file *img_file = NULL;
	int img_file_size;
	loff_t pos = 0;
	void *va;
	u64   pa;
	struct device *dev = &hdev->pdev->dev;

	if (dev == NULL) {
		return;
	}

	img_file = filp_open(NPUFW_IMG_FILE, O_RDONLY, 0);
	if (IS_ERR(img_file)) {
		hpcerr("open file failed\n");
		return;
	}
	img_file_size = img_file->f_inode->i_size;

	va = dma_alloc_coherent(dev, img_file_size, &pa, GFP_KERNEL);
	if (!va) {
		hpcerr("dma_alloc_coherent failed\n");
		goto release;
	}

	ret = kernel_read(img_file, va, img_file_size, &pos);
	if (ret < 0) {
		hpcerr("read file failed\n");
		goto release;
	}

	hpcinfo("load %s to 0x%x\n", NPUFW_IMG_FILE, NPUFW_IMG_ADDR);
	memcpy_to_fpga_ddr(va, img_file_size, NPUFW_IMG_ADDR);

release:
	if (va) {
		dma_free_coherent(dev, img_file_size, va, pa);
	}

	filp_close(img_file, NULL);

	return;
}

static inline void dump_rpmsg(unsigned long addr)
{
	hpcinfo("process_id = %u thread_id = %u cmd = %u msg_id = 0x%x, data = 0x%x, ack_status = %u\n",
			uft_pcie_ep_ddr_read(g_ep, addr),
			uft_pcie_ep_ddr_read(g_ep, addr + 0x4),
			uft_pcie_ep_ddr_read(g_ep, addr + 0x8),
			uft_pcie_ep_ddr_read(g_ep, addr + 0xC),
			uft_pcie_ep_ddr_read(g_ep, addr + 0x10),
			uft_pcie_ep_ddr_read(g_ep, addr + 0x14));
}

static ssize_t powerctrl_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	hpcinfo("enter\n");

	char output[50];

	int ret;
	ret = scnprintf(output, 50, "powerctrl_read pass\n");

	return simple_read_from_buffer(buf, count, ppos, output, ret);
}

static ssize_t powerctrl_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	char cmd[32];
	char *p;
	u32 cmd_val;

	ret = copy_from_user(cmd, buf, count);
	if (ret < 0) {
		hpcerr("copy_from_user failed\n");
		return -EFAULT;
	}
	cmd[count] = '\0';
	p = cmd;
	cmd_val = simple_strtoul(p, &p, 16);
	hpcinfo("cmd = %s\n", cmd);

	// echo 1 or 0 > /sys/kernel/debug/powerctrl
	if (cmd_val == 1) {
		npu_disreset_fpga();
	} else if (cmd_val == 0) {
		npu_reset_fpga();
	} else {
		hpcerr("cmd error\n");
	}

	return count;
}

static int npu_load_data(char *filename, u64 address)
{
	int ret = 0;
	struct file *data_file = NULL;
	int data_file_size;
	unsigned char *data = NULL;
	loff_t pos = 0;

	data_file = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(data_file)) {
		ret = PTR_ERR(data_file);
		hpcerr("open file %s failed, ret = %d\n", filename, ret);
		goto release;
	}
	data_file_size = data_file->f_inode->i_size;
	hpcinfo("file size %d\n", data_file_size);

	data = vmalloc(data_file_size);
	if (data == NULL) {
		hpcerr("vmalloc failed\n");
		goto release;
	}

	ret = kernel_read(data_file, data, data_file_size, &pos);
	if (ret < 0) {
		hpcerr("read file failed\n");
		goto release;
	}

	memcpy_to_fpga_ddr(data, data_file_size, (u64)address);

	hpcinfo("end\n");
release:
	if (data)
		vfree(data);
	if (!IS_ERR(data_file))
		filp_close(data_file, NULL);

	return ret;
}

static ssize_t load_data_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	hpcinfo("enter\n");

	char output[50];

	int ret;
	ret = scnprintf(output, 50, "load_data_read pass\n");

	return simple_read_from_buffer(buf, count, ppos, output, ret);
}

static ssize_t load_data_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	char cmd[128];
	char filename[128];
	char *delim = " ";
	char *token;
	char *p;

	u64 addr;

	ret = copy_from_user(cmd, buf, count);
	if (ret < 0) {
		hpcerr("copy_from_user failed\n");
		return -EFAULT;
	}
	cmd[count] = '\0';
	hpcinfo("cmd = %s", cmd);
	p = cmd;
	// echo filename address > /sys/kernel/debug/loaddata
	token = strsep(&p, delim);
	strcpy(filename, token);
	token = strsep(&p, delim);
	addr = simple_strtoul(token, NULL, 16);

	hpcinfo("filename = %s, addr = 0x%llx\n", filename, addr);

	npu_load_data(filename, addr);

	return count;
}

static ssize_t access_reg_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	hpcinfo("enter\n");

	struct debugfs_priv *priv = (struct debugfs_priv *)file->private_data;
	char output[128];
	int ret;

	if (priv->reg_addr == 0xFFFFFFFF) {
		ret = scnprintf(output, sizeof(output), "no reg addr assigned yet\n");
	} else {
		ret = scnprintf(output, sizeof(output), "0x%lx: 0x%08lx 0x%08lx 0x%08lx 0x%08lx\n", priv->reg_addr,
			priv->reg_value[0], priv->reg_value[1], priv->reg_value[2], priv->reg_value[3]);
	}

	return simple_read_from_buffer(buf, count, ppos, output, ret);
}

static ssize_t access_reg_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	// write: echo addr val > /sys/kernel/debug/access_reg
	// read : echo addr > /sys/kernel/debug/access_reg
	int ret;
	char cmd[128];
	char *delim = " ";
	char *token;
	char params[2][20];
	int param_num = 0;
	int is_write = 0;
	char *p;

	struct debugfs_priv *priv = (struct debugfs_priv *)file->private_data;

	unsigned long addr = 0;
	unsigned long data = 0;

	ret = copy_from_user(cmd, buf, count);
	if (ret < 0) {
		hpcerr("copy_from_user failed\n");
		return -EFAULT;
	}
	cmd[count] = '\0';
	hpcinfo("cmd = %s\n", cmd);
	p = cmd;
	token = strsep(&p, delim);
	while (token != NULL) {
		param_num++;
		hpcinfo("token = %s\n", token);
		strcpy(params[param_num - 1], token);
		if (p == NULL) {
			break;
		}
		token = strsep(&p, delim);
	}

	if (param_num == 1) {
		addr = simple_strtoul(token, NULL, 16);
	} else if (param_num == 2) {
		addr = simple_strtoul(params[0], NULL, 16);
		data = simple_strtoul(params[1], NULL, 16);
		is_write = 1;
	} else {
		hpcerr("invalid param num\n");
		return -EINVAL;
	}

	if (addr >= 0 && addr < 0xA0000000) {
		if (is_write) {
			uft_pcie_ep_ddr_write(g_ep, addr, data);
		} else {
			priv->reg_addr     = ALIGN_DOWN(addr, 0x10);
			priv->reg_value[0] = uft_pcie_ep_ddr_read(g_ep, priv->reg_addr);
			priv->reg_value[1] = uft_pcie_ep_ddr_read(g_ep, priv->reg_addr + 0x4);
			priv->reg_value[2] = uft_pcie_ep_ddr_read(g_ep, priv->reg_addr + 0x8);
			priv->reg_value[3] = uft_pcie_ep_ddr_read(g_ep, priv->reg_addr + 0xc);
		}
	} else if (addr >= NPU_SYS_BASE && addr < NPU_SYS_BASE + NPU_SYS_SIZE) {
		if (is_write) {
			uft_pcie_ep_reg_write(g_ep, addr, data);
		} else {
			priv->reg_addr     = ALIGN_DOWN(addr, 0x10);
			priv->reg_value[0] = uft_pcie_ep_reg_read(g_ep, priv->reg_addr);
			priv->reg_value[1] = uft_pcie_ep_reg_read(g_ep, priv->reg_addr + 0x4);
			priv->reg_value[2] = uft_pcie_ep_reg_read(g_ep, priv->reg_addr + 0x8);
			priv->reg_value[3] = uft_pcie_ep_reg_read(g_ep, priv->reg_addr + 0xc);
		}
	} else {
		hpcerr("not expected addr: 0x%lx\n", addr);
		return -EINVAL;
	}

	return count;
}

static ssize_t poll_timeout_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	char output[50];
	int ret;

	hpcinfo("cur s_timeout_cnt = %d\n", s_timeout_cnt);
	ret = scnprintf(output, ARRAY_SIZE(output), "cur s_timeout_cnt = %d\n", s_timeout_cnt);

	return simple_read_from_buffer(buf, count, ppos, output, ret);
}

static ssize_t poll_timeout_write(struct file *file, const char __user *buf, size_t count,
									loff_t *ppos)
{
	int ret;
	int value;
	char cmd[32];
	char *p;

	ret = copy_from_user(cmd, buf, count);
	if (ret < 0) {
		hpcerr("copy_from_user failed\n");
		return -EFAULT;
	}
	cmd[count] = '\0';
	p = cmd;
	ret = kstrtoint(p, 10, &value);
	if (ret != 0) {
		hpcerr("Get value failed\n");
		return -EFAULT;
	}
	s_timeout_cnt = value;
	hpcinfo("cmd = %s s_timeout_cnt= %d\n", cmd, s_timeout_cnt);

	return count;
}

static int uft_pcie_add_npu_debugfs(void)
{
	struct dentry *pwctrl_dfile;
	struct dentry *load_data_dfile;
	struct dentry *access_reg_dfile;
	struct dentry *poll_timeout_dfile;
	int ret = 0;
	root = debugfs_create_dir("npu_test", NULL);
	if (!root) {
		hpcinfo("debugfs_create_dir failed\n");
		return -1;
	}
	pwctrl_dfile = debugfs_create_file("powerctrl", 0666, root, NULL, &powerctrl_fops);
	if (IS_ERR_OR_NULL(pwctrl_dfile)) {
		hpcinfo("debugfs_create_file failed\n");
		ret = -1;
		goto err;
	}
	load_data_dfile = debugfs_create_file("loaddata", 0666, root, NULL, &load_data_fops);
	if (IS_ERR_OR_NULL(load_data_dfile)) {
		hpcinfo("debugfs_create_file failed\n");
		ret = -1;
		goto err;
	}

	access_reg_priv = kzalloc(sizeof(struct debugfs_priv), GFP_KERNEL);
	if (!access_reg_priv) {
		hpcinfo("kzalloc failed\n");
		ret = -1;
		goto err;
	}
	access_reg_priv->reg_addr  = 0xFFFFFFFF;
	access_reg_dfile = debugfs_create_file("access_reg", 0666, root, access_reg_priv, &access_reg_fops);
	if (IS_ERR_OR_NULL(access_reg_dfile)) {
		hpcinfo("debugfs_create_file failed\n");
		ret = -1;
		goto err;
	}

	poll_timeout_dfile = debugfs_create_file("poll_timeout", 0666, root, NULL, &poll_timeout_fops);
	if (IS_ERR_OR_NULL(poll_timeout_dfile)) {
		hpcinfo("debugfs_create_file failed\n");
		ret = -1;
		goto err;
	}

	hpcinfo("create npu_debugfs success\n");
	return ret;
err:
	kfree(access_reg_priv);
	access_reg_priv = NULL;
	debugfs_remove_recursive(root);
	return ret;
}

// static void uft_pcie_remove_npu_debugfs(void)
// {
// 	kfree(access_reg_priv);
// 	access_reg_priv = NULL;
// 	debugfs_remove_recursive(root);
// 	root = NULL;
// }

static int f3_npu_pcie_ep_init(void)
{
	struct uft_pcie_bar_ctrl *ddr_bar;
	struct uft_pcie_bar_ctrl *npu_ctrl_bar;
	// int ret;
	hpcinfo("enter\n");
	struct uft_pcie_ep *ep = NULL;

	if (g_pci_dev == NULL) {
		hpcerr("failed to get pci device\n");
		return -1;
	}

	if (g_ep == NULL) {
		hpcerr("failed to get ep\n");
		return -1;
	}

	ep = g_ep;
	ddr_bar      = ep->uft_bars[DDR_ACCESS_BAR];
	npu_ctrl_bar = ep->uft_bars[REG_ACCESS_BAR];

	hpcinfo("     ddr bar[%d]: iova = %p, phys = 0x%x, target_addr = 0x%x, size = 0x%x\n",
			DDR_ACCESS_BAR,
			ddr_bar->bar_virt,
			ddr_bar->bar_phys,
			ddr_bar->ib_target_addr_phys,
			ddr_bar->bar_size);

	hpcinfo("npu ctrl bar[%d]: iova = %p, phys = 0x%x, target_addr = 0x%x, size = 0x%x\n",
			REG_ACCESS_BAR,
			npu_ctrl_bar->bar_virt,
			npu_ctrl_bar->bar_phys,
			npu_ctrl_bar->ib_target_addr_phys,
			npu_ctrl_bar->bar_size);

	uft_pcie_add_npu_debugfs();

	/* config npu domain offset */
	write_reg_by_spi(UFT_SPI_REG_BASE + REG_DUT_OFFSET_CFG, 0xD6000000);

	/* init noc */
	uft_pcie_ep_reg_write(g_ep, SYSTEM_NOC_INIT_REG, 0x0);

	// struct hpc_mem_info *mem = &g_hrpdev->uft_share_mem;
	// xring_pcie_alloc_ob_atu(g_ep, PCIE_SPACE_BASE, mem->pa, mem->size);

	return 0;
}

static void uft_hpc_init_work_handler(struct work_struct *work)
{
	g_pci_dev = pci_get_device(PCI_VENDER_ID_UFT, PCI_DEVICE_ID_UFT, NULL);
	if (g_pci_dev == NULL) {
		hpcinfo("waiting for uft_pcie_boot ready\n");
		mod_delayed_work(system_wq, &uft_hpc_init_work, msecs_to_jiffies(1000));
		return;
	}

	g_ep = pci_get_drvdata(g_pci_dev);
	if (g_ep == NULL) {
		hpcinfo("waiting for uft_pcie_boot ready\n");
		mod_delayed_work(system_wq, &uft_hpc_init_work, msecs_to_jiffies(1000));
		return;
	}

	hpcinfo("uft_pcie_boot ready\n");

	f3_npu_pcie_ep_init();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int npu_boot(struct hpc_device *hdev)
{
	// 0822bit版本，fpga ddr未初始化会导致使用hdma从ep往rc搬数据失败
	int timeout;
	int retry;
	uint32_t rval0;
	uint32_t rval1;
	bool boot_flag;
	struct device *dev = &hdev->pdev->dev;
	void *va;
	u64   pa;
	u32 size = 0x100000;
	va = dma_alloc_coherent(dev, size, &pa, GFP_KERNEL);
	if (!va) {
		hpcerr("dma_alloc_coherent failed\n");
		return -ENOMEM;
	}
	memset(va, 0, size);

	memcpy_to_fpga_ddr(va, 0x100, RPMSG_TX_ADDR);

	memcpy_to_fpga_ddr(va, 0x100, RPMSG_RX_ADDR);

	memcpy_to_fpga_ddr(va, size, NPUFW_IMG_ADDR);

	dma_free_coherent(dev, size, va, pa);

	retry = 0;
	do {
		/* clean mcu boot flag */
		write_reg_by_pcie(NPU_MCU_BOOT_TEST_ADDR0, 0x0);
		write_reg_by_pcie(NPU_MCU_BOOT_TEST_ADDR1, 0x0);
		/* reset mcu & npu sys */
		npu_reset_fpga();
		/* load firmware */
		npu_load_firmware(hdev);
		/* clear mailbox irq to avoid legacy irqs */
		uft_reset_mailbox();
		/* boot mcu & run */
		npu_disreset_fpga();
		/* check mcu boot flag */
		for (timeout = 0; timeout < POLLING_TIMES; timeout++) {
			rval0 = read_reg_by_pcie(NPU_MCU_BOOT_TEST_ADDR0);
			rval1 = read_reg_by_pcie(NPU_MCU_BOOT_TEST_ADDR1);
			if (rval0 == NPU_MCU_BOOT_TEST_VALUE0 && rval1 == NPU_MCU_BOOT_TEST_VALUE1) {
				break;
			}
			msleep(POLLING_DELAY);
		}

		if (timeout >= POLLING_TIMES)
			boot_flag = false;
		else
			boot_flag = true;

		hpcinfo("NPU_MCU_ADDR_READ_%s retry = %d times = %d\n",
						boot_flag ? "SUCCESS" : "FAILED", retry, timeout);
		hpcinfo("NPU_MCU_ADDR_READ addr:0x%x excpt:0x%x read:0x%x\n",
					NPU_MCU_BOOT_TEST_ADDR0, NPU_MCU_BOOT_TEST_VALUE0, rval0);
		hpcinfo("NPU_MCU_ADDR_READ addr:0x%x excpt:0x%x read:0x%x\n",
					NPU_MCU_BOOT_TEST_ADDR1, NPU_MCU_BOOT_TEST_VALUE1, rval1);

	}while((!boot_flag) && (retry++ < NPU_MCU_BOOT_RETRY_TIMES));

	return 0;
}
EXPORT_SYMBOL_GPL(npu_boot);

int npu_shutdown(void)
{
	hpcinfo("npu & m85 shutdown \n");

	npu_reset_fpga();

	return 0;
}
EXPORT_SYMBOL_GPL(npu_shutdown);

int npu_rpmsg_tx_data(void *data, size_t len)
{
	// u32 result = 0;
	// u32 cnt = 50;
	u32 msg;

	memcpy_to_fpga_ddr(data, len, RPMSG_TX_ADDR);

	dump_rpmsg(RPMSG_TX_ADDR);

	mbox_xfer(ACPU02MCU, &msg, 1);

	// IPCM在fpga上，不会有IPCM中断报过来，check DSTATUS寄存器, 如果DSTATUS寄存器为0(MCU接收到消息后会清除dest)，则表示MCU侧收到数据
	// do {
	//     msleep(10);
	//     result = readl(ns_mailbox_base + IPCM_DSTATUS(ACPU02MCU));
	//     cnt --;
	// } while ((result != 0) && (cnt > 0)));

	// if (cnt == 0) {
	//     hpcinfo("mailbox channel %d dstatus: 0x%x\n", ACPU02MCU, result);
	//     hpcinfo("send msg timeout\n");
	//     return 0;
	// } else {
	//     hpcinfo("send msg done\n");
	// }

	return 0;
}
EXPORT_SYMBOL_GPL(npu_rpmsg_tx_data);

int npu_mailbox_poll(void)
{
	u32 i, j;
	u32 irq_status = 0;
	u32 data, data_len;
	bool send_done = false;
	bool receive_done = false;
	u32 time_1min = POLLING_TIMES;

	/* default time = s_timeout_cnt * time_1min ==> one day */
	for (i = 0; i < s_timeout_cnt; i++) {
		/* The following cycle time is one minute */
		for (j = 0; j < time_1min; j++) {
			irq_status = uft_pcie_ep_reg_read(g_ep, IPCM_RIS(NS_ID_ACPU0));
			if (irq_status & BIT(ACPU02MCU)) {
				uft_pcie_ep_reg_write(g_ep, IPCM_MCLR(ACPU02MCU), BIT(NS_ID_MCU));
				uft_pcie_ep_reg_write(g_ep, IPCM_SR(ACPU02MCU), 0);
				uft_pcie_ep_reg_write(g_ep, IPCM_SEND(ACPU02MCU), IPCM_SEND_NO_AFFECT);
				hpcinfo("send msg to mcu done\n");
				send_done = true;
			}
			if (irq_status & BIT(MCU2ACPU0)) {
				hpcinfo("receive msg from mcu\n");
				data_len = uft_pcie_ep_reg_read(g_ep, IPCM_DAT(MCU2ACPU0, 0));
				data     = uft_pcie_ep_reg_read(g_ep, IPCM_DAT(MCU2ACPU0, 1));
				uft_pcie_ep_reg_write(g_ep, IPCM_DCLR(MCU2ACPU0), BIT(NS_ID_ACPU0));
				uft_pcie_ep_reg_write(g_ep, IPCM_SEND(MCU2ACPU0), IPCM_SEND_TO_SRC);
				receive_done = true;
			}

			if (send_done && receive_done)
				return 0;

			msleep(POLLING_DELAY);
		}
		hpcinfo("irq_status = 0x%x time = %u min\n", irq_status, j);
	}

	hpcinfo("mailbox_timeout\n");

	return -1;
}
EXPORT_SYMBOL_GPL(npu_mailbox_poll);

int npu_rpmsg_rx_data(void *msg_buf, size_t len)
{
	struct pcie_hdma *pdma = NULL;
	if (g_ep == NULL) {
		hpcerr("g_ep is NULL\n");
		return 0;
	}
	pdma = g_ep->pdma;
	// dump_rpmsg(rx_buf);

	memcpy_from_fpga_ddr(msg_buf, len, RPMSG_RX_ADDR);

	struct npu_rpmsg_msg *msg = (struct npu_rpmsg_msg *)msg_buf;
	hpcinfo("rx_msg: [%u %u %u %u %u %u]",
				msg->header.process_id,
				msg->header.thread_id,
				msg->header.cmd,
				msg->header.msg_id,
				msg->payload.data,
				msg->payload.ack_status);

	return 0;
}
EXPORT_SYMBOL_GPL(npu_rpmsg_rx_data);

int memcpy_to_fpga_ddr(void *data, size_t size, u64 fpga_ddr_addr)
{
	hpcinfo("size = %ld, fpga_ddr_addr = 0x%llx\n", size, fpga_ddr_addr);
	// return mem_update_by_sliding_win(g_ep, data, size, fpga_ddr_addr);
	struct pcie_hdma *pdma = NULL;
	if (g_ep == NULL) {
		hpcerr("g_ep is NULL\n");
		return 0;
	}
	pdma = g_ep->pdma;

	if (((fpga_ddr_addr >= F3_UFT_DDR_0_START_ADDR) && (fpga_ddr_addr <= F3_UFT_DDR_0_END_ADDR)) &&
		 ((fpga_ddr_addr + size) <= F3_UFT_DDR_0_END_ADDR)) {
		/* When start address space is in low 4GB, and the end address also in low 4GB case */
		write_reg_by_spi(UFT_SPI_REG_BASE + REG_DDR4_33BIT, 0x00);
	} else if (((fpga_ddr_addr >= F3_UFT_DDR_1_START_ADDR) && (fpga_ddr_addr <= F3_UFT_DDR_1_END_ADDR)) &&
				((fpga_ddr_addr + size) <= F3_UFT_DDR_1_END_ADDR)) {
		/* When start address space is in high 4GB, and the end address in high 4GB case */
		write_reg_by_spi(UFT_SPI_REG_BASE + REG_DDR4_33BIT, 0x01);
		fpga_ddr_addr -= F3_UFT_DDR_1_START_ADDR;
	} else {
		hpcerr("DDR copy to fpga address space invalid\n");
	}

	xring_hdma_single_trans_va(pdma, HDMA_FROM_DEVICE, data, size, (u32)fpga_ddr_addr);

	// Recovery to default
	write_reg_by_spi(UFT_SPI_REG_BASE + REG_DDR4_33BIT, 0x00);

	return 0;
}
EXPORT_SYMBOL_GPL(memcpy_to_fpga_ddr);

int memcpy_from_fpga_ddr(void *dst, size_t size, u64 fpga_ddr_addr)
{
	hpcinfo("size = %ld, fpga_ddr_addr = 0x%llx\n", size, fpga_ddr_addr);
	struct pcie_hdma *pdma = NULL;
	if (g_ep == NULL) {
		hpcerr("g_ep is NULL\n");
		return 0;
	}
	pdma = g_ep->pdma;

	if (((fpga_ddr_addr >= F3_UFT_DDR_0_START_ADDR) && (fpga_ddr_addr <= F3_UFT_DDR_0_END_ADDR)) &&
		 ((fpga_ddr_addr + size) <= F3_UFT_DDR_0_END_ADDR)) {
		/* When start address space is in low 4GB, and the end address also in low 4GB case */
		write_reg_by_spi(UFT_SPI_REG_BASE + REG_DDR4_33BIT, 0x00);
	} else if (((fpga_ddr_addr >= F3_UFT_DDR_1_START_ADDR) && (fpga_ddr_addr <= F3_UFT_DDR_1_END_ADDR)) &&
				((fpga_ddr_addr + size) <= F3_UFT_DDR_1_END_ADDR)) {
		/* When start address space is in high 4GB, and the end address in high 4GB case */
		write_reg_by_spi(UFT_SPI_REG_BASE + REG_DDR4_33BIT, 0x01);
		fpga_ddr_addr -= F3_UFT_DDR_1_START_ADDR;
	} else {
		hpcerr("DDR copy from fpga address space invalid\n");
	}

	xring_hdma_single_trans_va(pdma, HDMA_TO_DEVICE, dst, size, (u32)fpga_ddr_addr);

	// Recovery to default
	write_reg_by_spi(UFT_SPI_REG_BASE + REG_DDR4_33BIT, 0x00);

	return 0;
}
EXPORT_SYMBOL_GPL(memcpy_from_fpga_ddr);

int uft_hpc_driver_init(void *data)
{
	hpcinfo("UFT Driver init\n");
	hpcinfo("******************* HPC VERSION Information*********************\n");
	hpcinfo("%s\n", VERSION_STR);

	INIT_DELAYED_WORK(&uft_hpc_init_work, uft_hpc_init_work_handler);

	schedule_delayed_work(&uft_hpc_init_work, msecs_to_jiffies(1000));

	return 0;
}
EXPORT_SYMBOL_GPL(uft_hpc_driver_init);


MODULE_AUTHOR("High Performance Computing Group");
MODULE_DESCRIPTION("HPC UFT Driver");
MODULE_LICENSE("GPL v2");
