#include <linux/delay.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include "../xiaomi/ufshcd.h"
#include <asm/unaligned.h>
#include "../../../misc/hqsysfs/hqsys_pcba.h"
#include "ufsfeature-check.h"
#define UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE	0x59
#define UFSF_QUERY_DESC_CONFIGURAION_MAX_SIZE	0xE6
#define UFSF_QUERY_DESC_UNIT_MAX_SIZE		0x2D
#define QUERY_ATTR_IDN_SUP_VENDOR_OPTIONS		0xFF
#define UFSF_QUERY_DESC_DEVICE_MAX_SIZE		0x5F
#define LI_EN_32(x)				be32_to_cpu(*(__be32 *)(x))
#define UFS_UPIU_MAX_GENERAL_LUN		8
#define WRITEBOOSTER_TYPE_INDEX			0x54
#define SHARED_BUF_MODE_TRUE			1
#define seq_scan_lu(lun) for (lun = 0; lun < UFS_UPIU_MAX_GENERAL_LUN; lun++)
#define ERR_MSG(msg, args...)		pr_err("%s:%d err: " msg "\n", \
					       __func__, __LINE__, ##args)
void fill_wb_gb(struct ufs_hba *hba, unsigned int segsize, unsigned int unitsize, unsigned int rawval)
{
	int unit;
	if (!hba) {
		pr_err("HBA is null.\n");
		return;
	}
	unit = (segsize * unitsize) >> 1;
	hba->check.wb_gb = (rawval * unit) >> 20;
}
void fill_total_gb(struct ufs_hba *hba,  unsigned long long rawval)
{
	if (hba)
		hba->check.total_gb = rawval >> 21;
}
void ufstw_get_geo_information(struct ufs_hba *hba, u8 *geo_buf)
{
	struct ufstw_check_info *tw_dev_info = &hba->tw_dev_info;
	tw_dev_info->seg_size = LI_EN_32(&geo_buf[GEOMETRY_DESC_PARAM_SEG_SIZE]);
	tw_dev_info->unit_size = geo_buf[GEOMETRY_DESC_PARAM_ALLOC_UNIT_SIZE];
}
static void ufstw_get_lu_info(struct ufs_hba *hba, unsigned int lun, u8 *lu_buf, int flag)
{
	unsigned int tw_lu_buf_size;
	if (flag)
		tw_lu_buf_size = LI_EN_32(&lu_buf[DEVICE_DESC_PARAM_WB_SHARED_ALLOC_UNITS]);
	else
		tw_lu_buf_size = LI_EN_32(&lu_buf[UNIT_DESC_PARAM_WB_BUF_ALLOC_UNITS]);
	fill_wb_gb(hba, hba->tw_dev_info.seg_size, hba->tw_dev_info.unit_size, tw_lu_buf_size);
}
static int ufsf_read_desc(struct ufs_hba *hba, u8 desc_id, u8 desc_index,
			  u8 selector, u8 *desc_buf, u32 size)
{
	int err = 0;
	pm_runtime_get_sync(hba->dev);
	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    desc_id, desc_index,
					    selector,
					    desc_buf, &size);
	if (err)
		ERR_MSG("reading Device Desc failed. err = %d", err);
	pm_runtime_put_sync(hba->dev);
	return err;
}
static int ufsf_read_geo_desc(struct ufs_hba *hba, u8 selector)
{
	u8 geo_buf[UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE];
	u64 total_size;
	int ret;
	ret = ufsf_read_desc(hba, QUERY_DESC_IDN_GEOMETRY, 0, selector,
			     geo_buf, UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE);
	if (ret)
		return ret;
	total_size = get_unaligned_be64(&geo_buf[0x04]);
	fill_total_gb(hba, total_size);
	ufstw_get_geo_information(hba, geo_buf);
	return 0;
}
static void mi_ufshcd_check_provision_config(struct ufs_hba *hba)
{
	struct PCBA_MSG *pcba_msg = get_pcba_msg();
	if (!pcba_msg)
		return;
	if (pcba_msg->pcba_stage < P1)  {
		pr_info("%s: pcba_stage = %d !\n", __func__, pcba_msg->pcba_stage);
	} else {
		pr_info("%s: phone is not enable write booster feature, factory image can not turn on!\n",
			__func__);
		pr_info("%s: If you have any question, please contact memory group!\n", __func__);
		msleep(500);
		BUG_ON(1);
	}
}
int check_wb_size(struct ufs_hba *hba)
{
	int ret = -1;
	int total_gb[] = {64, 128, 256, 512};
	int wb_gb[] = {6, 12, 24, 48};
	int i;
	int total_size_f = 0;
	if (hba->check.total_gb > 512 && hba->check.total_gb <= 1024) {
		total_size_f = 1024;
	} else if (hba->check.total_gb > 256) {
		total_size_f = 512;
	} else if (hba->check.total_gb > 128) {
		total_size_f = 256;
	} else if (hba->check.total_gb > 64) {
		total_size_f = 128;
	} else if (hba->check.total_gb > 32) {
		total_size_f = 64;
	} else if (hba->check.total_gb > 16) {
		total_size_f = 32;
	} else if (hba->check.total_gb > 8) {
		total_size_f = 16;
	} else {
		pr_info("ufs total size unknown:%dGB\n", hba->check.total_gb);
		return ret;
	}
	pr_info("ufs total:%dGB wb:%dGB\n",
		total_size_f, hba->check.wb_gb);
	for (i = 0; i < ARRAY_SIZE(total_gb); i++) {
		if (total_gb[i] == total_size_f) {
			if (wb_gb[i] == hba->check.wb_gb)
				return i;
		}
	}
	return -1;
}
static void check_tw_provision(struct ufs_hba *hba)
{
	if (ufshcd_is_wb_allowed(hba))
		if (check_wb_size(hba) == -1)
			mi_ufshcd_check_provision_config(hba);
}
static void ufsf_read_unit_desc(struct ufs_hba *hba, int lun, u8 selector, int flag)
{
	u8 unit_buf[UFSF_QUERY_DESC_UNIT_MAX_SIZE];
	int ret = 0;
	ret = ufsf_read_desc(hba, QUERY_DESC_IDN_UNIT, lun, selector,
			unit_buf, UFSF_QUERY_DESC_UNIT_MAX_SIZE);
	if (ret) {
		ERR_MSG("read unit desc failed. ret (%d)", ret);
		goto out;
	}
	ufstw_get_lu_info(hba, lun, unit_buf, flag);
	if (lun == 2)
		check_tw_provision(hba);
out:
	return;
}
void ufs_tw_check(struct ufs_hba *hba)
{
	int ret, lun = 0, flag = 0;
	u8 selector = 0;
	u8 device_buf[UFSF_QUERY_DESC_DEVICE_MAX_SIZE];
	ret = ufsf_read_geo_desc(hba, selector);
	if (ret)
		return;
	ret = ufsf_read_desc(hba, QUERY_DESC_IDN_DEVICE, 0, selector,
			     device_buf, UFSF_QUERY_DESC_DEVICE_MAX_SIZE);
	if (ret) {
		ERR_MSG("read device desc failed. ret (%d)", ret);
		goto out;
	}
	if (device_buf[WRITEBOOSTER_TYPE_INDEX]) {
		flag = SHARED_BUF_MODE_TRUE;
		ufstw_get_lu_info(hba, lun, device_buf, flag);
		check_tw_provision(hba);
		goto out;
	}
	seq_scan_lu(lun)
		ufsf_read_unit_desc(hba, lun, selector, flag);
out:
	return;
}
