// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 *
 */

#define pr_fmt(fmt) "spi_apb_regops: " fmt

// #define DEBUG

#include <linux/printk.h>
#include <linux/spi/spi.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include "spi_apb_regops.h"
#include "spi_apb_interface.h"
#include <linux/component.h>
#include <linux/delay.h>

static struct spi_apb_dev *local_dev;

#define REGOPS_LOCK() mutex_lock(local_dev->base_lock)
#define REGOPS_UNLOCK() mutex_unlock(local_dev->base_lock)

static inline int __dev_vaild(void)
{
	return (local_dev != NULL &&
		local_dev->base_lock != NULL) ? 0 : -ENODEV;
}

bool spi_apb_ready(void)
{
	return __dev_vaild() == 0 ? true : false;
}
EXPORT_SYMBOL_GPL(spi_apb_ready);

static bool _regops_check(u8 *buf, char *info)
{
	struct spi_transfer tran[2] = { 0 };
	struct spi_message message;
	int ret;
	int retry = RW_CHECK_TIMES;

	while (retry > 0) {
		retry--;
		spi_message_init(&message);
		tran[0].tx_buf = buf;
		tran[0].len = 1;
		tran[1].rx_buf = buf + 1;
		tran[1].len = 2;
		tran[1].bits_per_word = BITS_PER_WORD_10;
		spi_message_add_tail(&tran[0], &message);
		spi_message_add_tail(&tran[1], &message);
		ret = spi_sync(local_dev->spi_dev, &message);

		if (ret != 0) {
			pr_err("SPI get status(%s) sync failed! %d\n", info,
			       ret);
			return false;
		}

		if ((buf[1] & REGOPS_STATUS_ERR) != 0) {
			pr_err("SPI poll status(%s) meet error!\n", info);
			return false;
		} else if ((buf[1] & REGOPS_STATUS_OK) != 0)
			return true;
		else if (buf[1] != 0) {
			pr_err("SPI poll status(%s) error fmt!\n", info);
			return false;
		}
		pr_warn("SPI poll met zero %d times!\n",
			RW_CHECK_TIMES - retry);
	}

	pr_err("SPI poll status(%s) no ready %d times !\n", info,
			RW_CHECK_TIMES);

	return false;
}

__maybe_unused static bool _regops_read_valid_check(void)
{
	u8 r_poll[] = { REGOPS_READ_STATUS_CMD, 0, 0, 0 };

	return _regops_check(r_poll, "read valid");
}

__maybe_unused static bool __regops_write_finish_check(char *s)
{
	u8 w_poll[] = { REGOPS_WRITE_STATUS_CMD, 0, 0, 0 };

	return _regops_check(w_poll, s);
}

__maybe_unused static bool _regops_write_finish_check(void)
{
	return __regops_write_finish_check("write finish");
}

__maybe_unused static bool _regops_read_request(u32 addr)
{
	struct spi_transfer tran = { 0 };
	struct spi_message message;
	struct spi_device *spi_apb = local_dev->spi_dev;
	int ret;
	u8 rq_cmd[] = { REGOPS_READ_REQ_CMD, 0, 0, 0 };

	/* address is aligned by 4 byte */
	addr = addr >> 2;

	rq_cmd[1] = GET_BYTE_2(addr);
	rq_cmd[2] = GET_BYTE_1(addr);
	rq_cmd[3] = GET_BYTE_0(addr);
	tran.tx_buf = rq_cmd;
	tran.len = 4;
	spi_message_init(&message);
	spi_message_add_tail(&tran, &message);

	ret = spi_sync(spi_apb, &message);
	if (ret != 0) {
		pr_err("SPI read request sync failed %d!\n", ret);
		return false;
	}

	return true;
}

static int _regops_read_offset(u32 offset, u32 *valp)
{
	struct spi_transfer tran[3] = { 0 };
	struct spi_message message;
	uint8_t *rdata_buf;
	struct spi_device *spi_apb = local_dev->spi_dev;
	int ret;

	if (valp == NULL)
		return -ENOPARAM;

	if (!_regops_read_request(offset)) {
		pr_err("SPI read request sync failed!\n");
		return -EIO;
	}

	if (!_regops_read_valid_check()) {
		pr_err("SPI read check failed!\n");
		return -EIO;
	}

	rdata_buf = kzalloc(DATA_BUF_SIZE, GFP_KERNEL);

	/* read command 8bit*/
	rdata_buf[0] = REGOPS_READ_CMD;
	tran[0].tx_buf = rdata_buf;
	tran[0].len = 1;

	/* rx frame: [wait cycle 2bit][value 32bit] */
	tran[1].rx_buf = rdata_buf + 1;
	tran[1].bits_per_word = BITS_PER_WORD_10;
	tran[1].len = 2;
	tran[2].rx_buf = rdata_buf + 3;
	tran[2].len = 3;

	memset(&rdata_buf[1], 0, 5);
	spi_message_init(&message);
	spi_message_add_tail(&tran[0], &message);
	spi_message_add_tail(&tran[1], &message);
	spi_message_add_tail(&tran[2], &message);

	ret = spi_sync(spi_apb, &message);
	if (ret != 0) {
		pr_err("SPI read sync failed! %d\n", ret);
		kfree(rdata_buf);
		return ret;
	}

	pr_debug("read buf = [%d-%d-%d-%d-%d]!\n", rdata_buf[1], rdata_buf[2],
	       rdata_buf[3], rdata_buf[4], rdata_buf[5]);

	*valp = 0;
	SET_BYTE_3(*valp, rdata_buf[1]);
	SET_BYTE_2(*valp, rdata_buf[3]);
	SET_BYTE_1(*valp, rdata_buf[4]);
	SET_BYTE_0(*valp, rdata_buf[5]);

	kfree(rdata_buf);

	return 0;
}

static int _regops_dread_offset(u32 offset, u32 *valp)
{
	struct spi_transfer tran[2] = { 0 };
	struct spi_message message;
	struct spi_device *spi_apb = local_dev->spi_dev;

	int ret;
	u8 *dread_buf;

	dread_buf = kzalloc(DATA_BUF_SIZE, GFP_KERNEL);

	if (valp == NULL)
		return -ENOPARAM;

	/* address is aligned by 4 bytes, 24 vaild bits*/
	offset = offset >> 2;
	dread_buf[0] = REGOPS_DREAD_CMD;
	dread_buf[1] = GET_BYTE_2(offset);
	dread_buf[2] = GET_BYTE_1(offset);
	dread_buf[3] = GET_BYTE_0(offset);

	/* tx frame:[cmd 8bit] [offset 24bit] */
	tran[0].tx_buf = dread_buf;
	tran[0].len = 4;

	/* rx frame:[wait cycle 24bit] [value 32bit] */
	tran[1].rx_buf = dread_buf + 4;
	tran[1].len = 7;
	spi_message_init(&message);
	spi_message_add_tail(&tran[0], &message);
	spi_message_add_tail(&tran[1], &message);

	ret = spi_sync(spi_apb, &message);
	if (ret != 0) {
		pr_err("SPI dread sync failed! %d\n", ret);
		return ret;
	}

	*valp = 0;
	SET_BYTE_3(*valp, dread_buf[7]);
	SET_BYTE_2(*valp, dread_buf[8]);
	SET_BYTE_1(*valp, dread_buf[9]);
	SET_BYTE_0(*valp, dread_buf[10]);

	kfree(dread_buf);

	return 0;
}

/**
 * spi_apb_regops_read() - Use spi to access FPGA memory&reg.
 *
 * @addr: The absolute fpga-cpu-addr, Need align to 4Byte
 * @valp: The pointer that holds the read value
 *
 * Will lock spi base register when transfer.
 * NOTICE:Cannot be used in interrupt context
 *
 * Return: zero on success or error code on failure.
 */
int spi_apb_regops_read(u32 addr, u32 *valp)
{
	int ret = 0;

	ret = __dev_vaild();
	if (ret != 0)
		return ret;

	REGOPS_LOCK();
	ret = _regops_read_offset(addr, valp);
	if (ret != 0)
		goto err;
	REGOPS_UNLOCK();

	return 0;
err:
	pr_err("%s failed %d [0x%x]\n", __func__, ret, addr);
	REGOPS_UNLOCK();
	return ret;
}
EXPORT_SYMBOL_GPL(spi_apb_regops_read);

/**
 * spi_apb_regops_dread() - Use spi to direct access FPGA memory&reg.
 *
 * @addr: The absolute fpga-cpu-addr, Need align to 4Byte
 * @valp: The pointer that holds the read value
 *
 * Will lock spi base register when transfer.
 * NOTICE:Cannot be used in interrupt context
 *
 * Return: zero on success or error code on failure.
 */
int spi_apb_regops_dread(u32 addr, u32 *valp)
{
	int ret = 0;

	ret = __dev_vaild();
	if (ret != 0)
		return ret;

	REGOPS_LOCK();
	ret = _regops_dread_offset(0, valp);
	if (ret != 0)
		goto err;
	REGOPS_UNLOCK();

	return 0;
err:
	pr_err("%s failed %d [0x%x]\n", __func__, ret, addr);
	REGOPS_UNLOCK();
	return ret;
}
EXPORT_SYMBOL_GPL(spi_apb_regops_dread);

static int _regops_write_offset(u32 offset, u32 data)
{
	struct spi_transfer tran = { 0 };
	struct spi_message message;
	uint8_t *wdata_buf;
	struct spi_device *spi_apb = local_dev->spi_dev;
	int ret = 0;

	wdata_buf = kzalloc(DATA_BUF_SIZE, GFP_KERNEL);

	/* tx frame:[cmd 8bit] [offset 24bit]*/
	offset = offset >> 2;
	wdata_buf[0] = REGOPS_WRITE_CMD;
	wdata_buf[1] = GET_BYTE_2(offset);
	wdata_buf[2] = GET_BYTE_1(offset);
	wdata_buf[3] = GET_BYTE_0(offset);

	/*tx frame:[value 32bit]*/
	wdata_buf[4] = GET_BYTE_3(data);
	wdata_buf[5] = GET_BYTE_2(data);
	wdata_buf[6] = GET_BYTE_1(data);
	wdata_buf[7] = GET_BYTE_0(data);
	tran.tx_buf = wdata_buf;
	tran.len = 8;

	spi_message_init(&message);
	spi_message_add_tail(&tran, &message);
	ret = spi_sync(spi_apb, &message);

	if (ret != 0) {
		pr_err("SPI write sync failed! %d\n", ret);
		goto out;
	}

	if (!_regops_write_finish_check()) {
		pr_err("SPI write check failed!");
		ret = -EIO;
		goto out;
	}

	msleep(20);

out:
	kfree(wdata_buf);
	return ret;
}

/**
 * spi_apb_regops_write() - Use spi to access FPGA memory&reg.
 *
 * @addr: The absolute fpga-cpu-addr, Need align to 4Byte
 * @data: value to be write
 *
 * Will lock spi base register when transfer.
 * NOTICE:Cannot be used in interrupt context
 *
 * Return: zero on success or error code on failure.
 */
int spi_apb_regops_write(u32 addr, u32 data)
{
	int ret = 0;

	ret = __dev_vaild();
	if (ret != 0)
		return ret;

	REGOPS_LOCK();
	ret = _regops_write_offset(addr, data);
	if (ret != 0)
		goto err;
	REGOPS_UNLOCK();

	return 0;
err:
	pr_err("%s failed %d [0x%x=0x%x]\n", __func__, ret, addr, data);
	REGOPS_UNLOCK();
	return ret;
}
EXPORT_SYMBOL_GPL(spi_apb_regops_write);

static int __regops_wxdword_offset(u32 offset, u32 *data, int size, u8 cmd)
{
	struct spi_transfer tran = { 0 };
	struct spi_message message;
	struct spi_device *spi_apb = local_dev->spi_dev;
	uint8_t *wdata_buf;
	int ret = 0;
	int i;
	u32 dw_data = 0;

	wdata_buf = kzalloc(BURST_DATA_BUF_SIZE, GFP_KERNEL);

	/* tx frame:[cmd 1byte] [addr 3bytes]*/
	offset = offset >> 2;
	wdata_buf[0] = cmd;
	wdata_buf[1] = GET_BYTE_2(offset);
	wdata_buf[2] = GET_BYTE_1(offset);
	wdata_buf[3] = GET_BYTE_0(offset);

	/* tx frame: [data 4*size bytes] */
	for (i = 4; i < (size + 1) * 4; i += 4) {
		dw_data = data[(i >> 2) - 1];
		wdata_buf[i + 0] = GET_BYTE_3(dw_data);
		wdata_buf[i + 1] = GET_BYTE_2(dw_data);
		wdata_buf[i + 2] = GET_BYTE_1(dw_data);
		wdata_buf[i + 3] = GET_BYTE_0(dw_data);
	}

	tran.tx_buf = wdata_buf;

	/* cmd + addr 4bytes, data 4*size bytes */
	tran.len = size * 4 + 4;
	spi_message_init(&message);
	spi_message_add_tail(&tran, &message);
	ret = spi_sync(spi_apb, &message);

	if (ret != 0) {
		pr_err("SPI group write failed! %d\n", ret);
		goto out;
	}

	if (!_regops_write_finish_check()) {
		pr_err("SPI group write check failed!");
		ret = -EIO;
	}

out:
	kfree(wdata_buf);
	return ret;
}

static int _regops_w8dword_offset(u32 offset, u32 *data)
{
	return __regops_wxdword_offset(offset, data, 8, REGOPS_WRITE_CMD + 2);
}

static int _regops_w64dword_offset(u8 offset, u32 *data)
{
	return __regops_wxdword_offset(offset, data, 64, REGOPS_WRITE_CMD + 5);
}

/**
 * spi_apb_regops_w8dword() - Use spi to access fpga memory&reg
 * burst access len is 32Byte.
 *
 * @addr: The absolute fpga-addr, Need align to 4Byte
 * @data: pointer to data array.
 *
 * Will lock spi base register when transfer.
 *
 * Return: zero on success or error code on failure.
 */
int spi_apb_regops_w8dword(u32 addr, u32 *data)
{
	int ret = 0;

	if (data == NULL) {
		pr_err("%s: Error: data pointer is NULL\n", __func__);
		return -EFAULT;
	}

	ret = __dev_vaild();
	if (ret != 0)
		return ret;

	REGOPS_LOCK();
	ret = _regops_w8dword_offset(addr, data);
	if (ret != 0)
		goto err;
	REGOPS_UNLOCK();

	return 0;
err:
	pr_err("%s failed %d [0x%x]\n", __func__, ret, addr);
	REGOPS_UNLOCK();
	return ret;
}
EXPORT_SYMBOL_GPL(spi_apb_regops_w8dword);

/**
 * spi_apb_regops_w64dword() - Use spi to access fpga memory&reg
 * burst access len is 32Byte.
 *
 * @addr: The absolute fpga-addr, Need align to 4Byte
 * @data: pointer to data array.
 *
 * Will lock spi base register when transfer.
 *
 * Return: zero on success or error code on failure.
 */
int spi_apb_regops_w64dword(u32 addr, u32 *data)
{
	int ret = 0;

	if (data == NULL) {
		pr_err("%s: Error: data pointer is NULL\n", __func__);
		return -EFAULT;
	}

	ret = __dev_vaild();
	if (ret != 0)
		return ret;

	REGOPS_LOCK();
	ret = _regops_w64dword_offset(0, data);
	if (ret != 0)
		goto err;
	REGOPS_UNLOCK();

	return 0;

err:
	pr_err("%s failed %d [0x%x]\n", __func__, ret, addr);
	REGOPS_UNLOCK();
	return ret;
}
EXPORT_SYMBOL_GPL(spi_apb_regops_w64dword);

int spi_apb_regops_get_speed(void)
{
	int sp = 0;
	struct spi_device *spi_apb = local_dev->spi_dev;

	sp = __dev_vaild();
	if (sp != 0)
		return -ENODEV;

	sp = spi_apb->max_speed_hz;

	pr_crit("%s [%d]\n", __func__, sp);
	return sp;
}
EXPORT_SYMBOL_GPL(spi_apb_regops_get_speed);

int spi_apb_regops_set_speed(u32 sp)
{
	int ret = 0;
	struct spi_device *spi_apb = local_dev->spi_dev;

	if (sp < SPI_APB_SPI_LOW_SP || sp > SPI_APB_SPI_HIGH_SP) {
		pr_err("Error: SPI speed %u Hz is out of valid range (%u Hz - %u Hz)\n", sp, SPI_APB_SPI_LOW_SP, SPI_APB_SPI_HIGH_SP);
		return -EINVAL;
	}

	ret = __dev_vaild();
	if (ret != 0)
		return ret;

	spi_apb->max_speed_hz = sp;
	spi_setup(spi_apb);

	pr_info("%s [%d]\n", __func__, sp);
	return 0;
}
EXPORT_SYMBOL_GPL(spi_apb_regops_set_speed);

static long spi_apb_regops_unlocked_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	int ret = 0;
	struct spi_apb_ioc_args k_args;

	if (_IOC_TYPE(cmd) != SPI_APB_IOC_MAGIC) {
		pr_err("Ioc type error!");
		return -EINVAL;
	}

	pr_info("got cmd nr=%d\t ", _IOC_NR(cmd));

	if (copy_from_user(&k_args, (void __user *)arg, sizeof(struct spi_apb_ioc_args))) {
		pr_err("Failed to get args from user space");
		return -EFAULT;
	}

	switch (cmd) {
	case CMD_SPI_APB_ADDR_READ:
		ret = spi_apb_regops_read(k_args.addr, &k_args.value);
		if (ret) {
			pr_err("Regops read 0x%x failed(%d)! %s:%d\n",
				       k_args.addr, ret, __func__, __LINE__);
			break;
		} else if (copy_to_user((void __user *)arg, &k_args, sizeof(struct spi_apb_ioc_args))) {
			pr_err("Failed to copy args to user");
			ret = -EFAULT;
		}
		break;
	case CMD_SPI_APB_ADDR_WRITE:
		ret = spi_apb_regops_write(k_args.addr, k_args.value);
		if (ret) {
			pr_err("Regops write 0x%x failed(%d)! %s:%d\n",
			       k_args.addr, ret, __func__, __LINE__);
		}
		break;
	default:
		pr_err("cmd not found");
		return -EINVAL;
	}

	return ret;
}

static const struct file_operations spi_apb_regops_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = spi_apb_regops_unlocked_ioctl,
};

static struct miscdevice spi_apb_regops_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "spi_apb_regops",
	.fops = &spi_apb_regops_fops,
};

int spi_apb_regops_init(struct spi_device *sdev)
{
	int ret;

	pr_info("Into %s\n", __func__);
	pr_info("spi speed = %d\n", sdev->max_speed_hz);

	local_dev = spi_get_drvdata(sdev);
	if (!local_dev) {
		pr_err("spi_apb: no drvdata found!");
		return -ENODEV;
	}

	/*init regops lock*/
	local_dev->base_lock = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	mutex_init(local_dev->base_lock);

	/* register misc dev for user space*/
	ret = misc_register(&spi_apb_regops_miscdev);
	if (ret != 0) {
		pr_info("misc register failed %d!\n", ret);
		kfree(local_dev->base_lock);
	}

	pr_info("reg ops init finish!\n");

	return 0;
}

int spi_apb_regops_deinit(struct spi_device *sdev)
{
	if (local_dev->base_lock != NULL)
		kfree(local_dev->base_lock);

	local_dev = NULL;
	misc_deregister(&spi_apb_regops_miscdev);

	return 0;
}
