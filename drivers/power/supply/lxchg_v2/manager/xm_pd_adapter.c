#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
#include <linux/power_supply.h>
#include <linux/stat.h>
#include <linux/ctype.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/vmalloc.h>
#include <linux/preempt.h>
#include "../../../misc/lx_typec/tcpc/inc/tcpm.h"
#include "xm_adapter_class.h"
#include "lxchg_printk.h"
#include "lxchg_manager.h"

#ifdef TAG
#undef TAG
#define TAG "[XM_PD_ADAPTER]"
#endif

struct xm_pd_adapter {
	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	struct adapter_device *adapter_dev;
	struct task_struct *adapter_task;
	const char *adapter_dev_name;
	bool enable_kpoc_shdn;
	struct tcpm_svid_list *adapter_svid_list;
};

static int get_apdo_regain;

static void usbpd_mi_vdm_received(struct xm_pd_adapter *padapter, struct tcp_ny_uvdm uvdm)
{
	int i, cmd;

	if (uvdm.uvdm_svid != USB_PD_MI_SVID)
		return;

	cmd = UVDM_HDR_CMD(uvdm.uvdm_data[0]);
	lx_info("cmd = %d\n", cmd);

	lx_info("uvdm.ack: %d, uvdm.uvdm_cnt: %d, uvdm.uvdm_svid: 0x%04x\n",
			uvdm.ack, uvdm.uvdm_cnt, uvdm.uvdm_svid);

	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
		padapter->adapter_dev->vdm_data.ta_version = uvdm.uvdm_data[1];
		lx_info("ta_version:%x\n", padapter->adapter_dev->vdm_data.ta_version);
		break;
	case USBPD_UVDM_CHARGER_TEMP:
		padapter->adapter_dev->vdm_data.ta_temp = (uvdm.uvdm_data[1] & 0xFFFF) * 10;
		lx_info("padapter->adapter_dev->vdm_data.ta_temp:%d\n", padapter->adapter_dev->vdm_data.ta_temp);
		break;
	case USBPD_UVDM_CHARGER_VOLTAGE:
		padapter->adapter_dev->vdm_data.ta_voltage = (uvdm.uvdm_data[1] & 0xFFFF) * 10;
		padapter->adapter_dev->vdm_data.ta_voltage *= 1000; /*V->mV*/
		lx_info("ta_voltage:%d\n", padapter->adapter_dev->vdm_data.ta_voltage);
		break;
	case USBPD_UVDM_SESSION_SEED:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			padapter->adapter_dev->vdm_data.s_secert[i] = uvdm.uvdm_data[i+1];
			lx_info("usbpd s_secert uvdm.uvdm_data[%d]=0x%x", i+1, uvdm.uvdm_data[i+1]);
		}
		break;
	case USBPD_UVDM_AUTHENTICATION:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			padapter->adapter_dev->vdm_data.digest[i] = uvdm.uvdm_data[i+1];
			lx_info("usbpd digest[%d]=0x%x", i+1, uvdm.uvdm_data[i+1]);
		}
		break;
	case USBPD_UVDM_REVERSE_AUTHEN:
		padapter->adapter_dev->vdm_data.reauth = (uvdm.uvdm_data[1] & 0xFFFF);
		break;
	default:
		break;
	}
	padapter->adapter_dev->uvdm_state = cmd;
}


static int pd_tcp_notifier_call(struct notifier_block *pnb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct xm_pd_adapter *padapter;

	padapter = container_of(pnb, struct xm_pd_adapter, pd_nb);

	lx_err("PD charger event:%d %d\n", (int)event,
		(int)noti->pd_state.connected);
	switch (event) {
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case  PD_CONNECT_NONE:
		case  PD_CONNECT_HARD_RESET:
			padapter->adapter_dev->adapter_id = 0;
			padapter->adapter_dev->adapter_svid = 0;
			padapter->adapter_dev->uvdm_state = USBPD_UVDM_DISCONNECT;
			padapter->adapter_dev->verifed = 0;
			padapter->adapter_dev->verify_process = 0;
			break;
		case PD_CONNECT_PE_READY_SNK_PD30:
			padapter->adapter_dev->uvdm_state = USBPD_UVDM_CONNECT;
			break;
		case PD_CONNECT_PE_READY_SNK_APDO:
			get_apdo_regain = 1;
			padapter->adapter_dev->uvdm_state = USBPD_UVDM_CONNECT;
			break;
		};
		break;
	case TCP_NOTIFY_UVDM:
		lx_info("tcpc received uvdm message.\n");
		usbpd_mi_vdm_received(padapter, noti->uvdm_msg);
		break;
	}
	return NOTIFY_OK;
}

static int pd_get_svid(struct adapter_device *adapter_dev)
{
	struct xm_pd_adapter *adapter = (struct xm_pd_adapter *)dev_get_drvdata(&adapter_dev->dev);
	struct pd_source_cap_ext cap_ext;
	int ret;
	int i = 0;
	uint32_t pd_vdos[8];

	if (adapter == NULL)
		return -EINVAL;

	lx_info("enter\n");
	if (adapter->adapter_dev->adapter_svid != 0)
		return 0;

	if (adapter->adapter_svid_list == NULL) {
		if (in_interrupt()) {
			adapter->adapter_svid_list = kmalloc(sizeof(struct tcpm_svid_list), GFP_ATOMIC);
		} else {
			adapter->adapter_svid_list = kmalloc(sizeof(struct tcpm_svid_list), GFP_KERNEL);
		}
		if (adapter->adapter_svid_list == NULL)
			lx_err("adapter_svid_list is still NULL!\n");
	}

	ret = tcpm_inquire_pd_partner_inform(adapter->tcpc, pd_vdos);
	if (ret == TCPM_SUCCESS) {
		lx_info("find adapter id success.\n");
		for (i = 0; i < 8; i++)
			lx_info("VDO[%d] : %08x\n", i, pd_vdos[i]);

		adapter->adapter_dev->adapter_svid = pd_vdos[0] & 0x0000FFFF;
		adapter->adapter_dev->adapter_id = pd_vdos[2] & 0x0000FFFF;
		lx_err("adapter_svid = %04x\n", adapter->adapter_dev->adapter_svid);
		lx_err("adapter_id = %08x\n", adapter->adapter_dev->adapter_id);

		ret = tcpm_inquire_pd_partner_svids(adapter->tcpc, adapter->adapter_svid_list);
		lx_info("tcpm_inquire_pd_partner_svids, ret=%d!\n", ret);
		if (ret == TCPM_SUCCESS) {
			lx_info("discover svid number is %d\n", adapter->adapter_svid_list->cnt);
			for (i = 0; i < adapter->adapter_svid_list->cnt; i++) {
				lx_err("SVID[%d] : %04x\n", i, adapter->adapter_svid_list->svids[i]);
				if (adapter->adapter_svid_list->svids[i] == USB_PD_MI_SVID)
					adapter->adapter_dev->adapter_svid = USB_PD_MI_SVID;
			}
		}
	} else {
		ret = tcpm_dpm_pd_get_source_cap_ext(adapter->tcpc,
			NULL, &cap_ext);
		if (ret == TCPM_SUCCESS) {
			adapter->adapter_dev->adapter_svid = cap_ext.vid & 0x0000FFFF;
			adapter->adapter_dev->adapter_id = cap_ext.pid & 0x0000FFFF;
			adapter->adapter_dev->adapter_fw_ver = cap_ext.fw_ver & 0x0000FFFF;
			adapter->adapter_dev->adapter_hw_ver = cap_ext.hw_ver & 0x0000FFFF;
			lx_info("adapter_svid = %04x\n", adapter->adapter_dev->adapter_svid);
			lx_info("adapter_id = %08x\n", adapter->adapter_dev->adapter_id);
			lx_info("adapter_fw_ver = %08x\n", adapter->adapter_dev->adapter_fw_ver);
			lx_info("adapter_hw_ver = %08x\n", adapter->adapter_dev->adapter_hw_ver);
		} else {
			lx_err("get adapter message failed!\n");
			return ret;
		}
	}

	return 0;
}

#define BSWAP_32(x) \
	(u32)((((u32)(x) & 0xff000000) >> 24) | \
			(((u32)(x) & 0x00ff0000) >> 8) | \
			(((u32)(x) & 0x0000ff00) << 8) | \
			(((u32)(x) & 0x000000ff) << 24))

static void usbpd_sha256_bitswap32(unsigned int *array, int len)
{
	int i;

	for (i = 0; i < len; i++)
		array[i] = BSWAP_32(array[i]);
}

void charToint(char *str, int input_len, unsigned int *out, unsigned int *outlen)
{
	int i;

	if (outlen == NULL)
		return;

	*outlen = 0;
	for (i = 0; i < (input_len / 4 + 1); i++) {
		out[i] = ((str[i*4 + 3] * 0x1000000) |
				(str[i*4 + 2] * 0x10000) |
				(str[i*4 + 1] * 0x100) |
				str[i*4]);
		*outlen = *outlen + 1;
	}

	lx_info("outlen = %d\n", *outlen);
	for (i = 0; i < *outlen; i++)
		lx_info("out[%d] = %08x\n", i, out[i]);
	lx_info("char to int done.\n");
}

static int tcp_dpm_event_cb_uvdm(struct tcpc_device *tcpc, int ret,
				 struct tcp_dpm_event *event)
{
	int i;
	struct tcp_dpm_custom_vdm_data vdm_data = event->tcp_dpm_data.vdm_data;

	lx_info("vdm_data.cnt = %d\n", vdm_data.cnt);
	for (i = 0; i < vdm_data.cnt; i++)
		lx_info("vdm_data.vdos[%d] = 0x%08x", i,
			vdm_data.vdos[i]);
	return 0;
}

const struct tcp_dpm_event_cb_data cb_data = {
	.event_cb = tcp_dpm_event_cb_uvdm,
};

static int pd_request_vdm_cmd(struct adapter_device *adapter_dev,
	enum uvdm_state cmd,
	unsigned char *data,
	unsigned int data_len)
{
	u32 vdm_hdr = 0;
	int rc = 0;
	struct tcp_dpm_custom_vdm_data *vdm_data;
	struct xm_pd_adapter *adapter = (struct xm_pd_adapter *)dev_get_drvdata(&adapter_dev->dev);
	unsigned int *int_data;
	unsigned int outlen;
	int i;

	if (in_interrupt()) {
		int_data = kmalloc(40, GFP_ATOMIC);
		vdm_data = kmalloc(sizeof(*vdm_data), GFP_ATOMIC);
		if (int_data == NULL || vdm_data == NULL) {
			lx_err("kmalloc fail\n");
			rc = -ENOMEM;
			goto release_req;
		}
		lx_info("kmalloc atomic ok.\n");
	} else {
		int_data = kmalloc(40, GFP_KERNEL);
		vdm_data = kmalloc(sizeof(*vdm_data), GFP_KERNEL);
		if (int_data == NULL || vdm_data == NULL) {
			lx_err("kmalloc fail\n");
			rc = -ENOMEM;
			goto release_req;
		}
		lx_info("kmalloc kernel ok.\n");
	}
	memset(int_data, 0, 40);

	charToint(data, data_len, int_data, &outlen);

	if (adapter == NULL || adapter->tcpc == NULL) {
		rc = -EINVAL;
		goto release_req;
	}

	vdm_hdr = VDM_HDR(adapter->adapter_dev->adapter_svid, USBPD_VDM_REQUEST, cmd);
	vdm_data->wait_resp = true;
	vdm_data->vdos[0] = vdm_hdr;

	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
	case USBPD_UVDM_CHARGER_TEMP:
	case USBPD_UVDM_CHARGER_VOLTAGE:
		vdm_data->cnt = 1;
		rc = tcpm_dpm_send_custom_vdm(adapter->tcpc, vdm_data, &cb_data);//&tcp_dpm_evt_cb_null
		if (rc < 0) {
			lx_err("failed to send %d\n", cmd);
			goto release_req;
		}
		break;
	case USBPD_UVDM_VERIFIED:
	case USBPD_UVDM_REMOVE_COMPENSATION:
		vdm_data->cnt = 1 + USBPD_UVDM_VERIFIED_LEN;

		for (i = 0; i < USBPD_UVDM_VERIFIED_LEN; i++)
			vdm_data->vdos[i + 1] = int_data[i];
		lx_info("verify-0: %08x\n", vdm_data->vdos[1]);

		rc = tcpm_dpm_send_custom_vdm(adapter->tcpc, vdm_data, &cb_data);//&tcp_dpm_evt_cb_null
		if (rc < 0) {
			lx_err("failed to send %d\n", cmd);
			goto release_req;
		}
		break;
	case USBPD_UVDM_SESSION_SEED:
	case USBPD_UVDM_AUTHENTICATION:
	case USBPD_UVDM_REVERSE_AUTHEN:
		usbpd_sha256_bitswap32(int_data, USBPD_UVDM_SS_LEN);
		vdm_data->cnt = 1 + USBPD_UVDM_SS_LEN;
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++)
			vdm_data->vdos[i + 1] = int_data[i];

		for (i = 0; i < USBPD_UVDM_SS_LEN; i++)
			lx_info("%08x\n", vdm_data->vdos[i+1]);

		rc = tcpm_dpm_send_custom_vdm(adapter->tcpc, vdm_data, &cb_data);//&tcp_dpm_evt_cb_null
		if (rc < 0) {
			lx_err("failed to send %d\n", cmd);
			goto release_req;
		}
		break;
	default:
		lx_err("cmd:%d is not support\n", cmd);
		break;
	}

release_req:
	if (int_data != NULL)
		kfree(int_data);
	if (vdm_data != NULL)
		kfree(vdm_data);
	return rc;
}

static int pd_get_power_role(struct adapter_device *adapter_dev)
{
	struct xm_pd_adapter *adapter = (struct xm_pd_adapter *)dev_get_drvdata(&adapter_dev->dev);
	if (adapter == NULL || adapter->tcpc == NULL)
		return -EINVAL;

	adapter->adapter_dev->role = tcpm_inquire_pd_power_role(adapter->tcpc);
	lx_err("power role is %d\n", adapter->adapter_dev->role);
	return 0;
}

static int pd_get_current_state(struct adapter_device *adapter_dev)
{
	struct xm_pd_adapter *adapter = (struct xm_pd_adapter *)dev_get_drvdata(&adapter_dev->dev);

	if (adapter == NULL || adapter->tcpc == NULL)
		return -EINVAL;

	adapter->adapter_dev->current_state = tcpm_inquire_pd_state_curr(adapter->tcpc);
	lx_err("current state is %d\n", adapter->adapter_dev->current_state);
	return 0;
}

static int pd_get_pdos(struct adapter_device *adapter_dev)
{
	struct xm_pd_adapter *adapter = (struct xm_pd_adapter *)dev_get_drvdata(&adapter_dev->dev);
	struct tcpm_power_cap cap;
	int ret, i;
	int pd_wait_cnt = 0;

	if (adapter == NULL || adapter->tcpc == NULL)
		return -EINVAL;

	ret = tcpm_inquire_pd_source_cap(adapter->tcpc, &cap);
	lx_info("tcpm_inquire_pd_source_cap is %d.\n", ret);
	if (ret < 0) {
		for(pd_wait_cnt = 0; pd_wait_cnt < 25; pd_wait_cnt++){
			msleep(20);
			lx_info("retry tcpm_inquire_pd_source_cap is %d. cnt =%d\n", ret, pd_wait_cnt);
			ret = tcpm_inquire_pd_source_cap(adapter->tcpc, &cap);
			if(ret == 0)
				break;
		}
	}
	for (i = 0; i < 7; i++) {
		adapter->adapter_dev->received_pdos[i] = cap.pdos[i];
		lx_info("pdo[%d] { received_pdos is %08x, cap.pdos is %08x}\n",
			i, adapter->adapter_dev->received_pdos[i], cap.pdos[i]);
	}

	return 0;
}

static int pd_set_pd_verify_process(struct adapter_device *dev, int verify_in_process)
{
	int ret = 0;
	//union power_supply_propval val = {0,};
	//struct power_supply *usb_psy = NULL;

	lx_err("pd verify in process:%d\n", verify_in_process);
/*
	usb_psy = power_supply_get_by_name("usb");

	if (usb_psy) {
		val.intval = verify_in_process;
		ret = power_supply_set_property(usb_psy,
			POWER_SUPPLY_PROP_PD_VERIFY_IN_PROCESS, &val);
	} else {
		lx_err("[%s] usb psy not found!\n", __func__);
	}
*/
	return ret;
}

static int pd_get_cap(struct adapter_device *adapter_dev,
	enum xm_adapter_cap_type type,
	struct adapter_power_cap *tacap)
{
	int ret;
	int i;
	int timeout = 0;
	struct xm_pd_adapter *adapter = (struct xm_pd_adapter *)dev_get_drvdata(&adapter_dev->dev);
	struct tcpm_remote_power_cap pd_cap;

	if (adapter == NULL || adapter->tcpc == NULL)
		return -EINVAL;

	if (adapter->adapter_dev->verify_process)
		return -1;

	if (type == XM_PD) {
APDO_REGAIN:
		pd_cap.nr = 0;
		pd_cap.selected_cap_idx = 0;
		tcpm_get_remote_power_cap(adapter->tcpc, &pd_cap);

		tacap->nr = pd_cap.nr;
		tacap->selected_cap_idx = pd_cap.selected_cap_idx - 1;
		lx_info("nr:%d idx:%d\n",
		pd_cap.nr, pd_cap.selected_cap_idx - 1);
		for (i = 0; i < pd_cap.nr; i++) {
			tacap->ma[i] = pd_cap.ma[i];
			tacap->max_mv[i] = pd_cap.max_mv[i];
			tacap->min_mv[i] = pd_cap.min_mv[i];
			tacap->maxwatt[i] = tacap->max_mv[i] * tacap->ma[i];
			tacap->type[i] = pd_cap.type[i];
			lx_info("%d mv:[%d,%d] %d max:%d min:%d type:%d %d\n",
				i, tacap->min_mv[i],
				tacap->max_mv[i], tacap->ma[i],
				tacap->maxwatt[i], tacap->minwatt[i],
				tacap->type[i], pd_cap.type[i]);
		}
	}  else if (type == XM_PD_APDO_REGAIN) {
		get_apdo_regain = 0;
		ret = tcpm_dpm_pd_get_source_cap(adapter->tcpc, NULL);
		if (ret == TCPM_SUCCESS) {
			while (timeout < 10) {
				if (get_apdo_regain) {
					lx_err("ready to get pps adapter!\n");
					goto APDO_REGAIN;
				} else {
					msleep(100);
					timeout++;
				}
			}
			lx_err("ready to get pps adapter - for test!\n");
			goto APDO_REGAIN;
		} else {
			lx_err("tcpm_dpm_pd_get_source_cap failed!\n");
			return -EINVAL;
		}
	}
	lx_err("tacap->nr is %d\n", tacap->nr);

	return 0;
}

static int pd_get_usbpd_verifed(struct adapter_device *adapter_dev, bool *verifed)
{
	struct xm_pd_adapter *adapter = (struct xm_pd_adapter *)dev_get_drvdata(&adapter_dev->dev);
	if (adapter == NULL || adapter->tcpc == NULL)
		return -EINVAL;

	*verifed = adapter->adapter_dev->verifed;
	lx_err("current usbpd_verifed is %d\n", *verifed);
	return 0;
}

static struct xm_adapter_ops xm_adapter_ops = {
	.get_cap = pd_get_cap,
	.get_svid = pd_get_svid,
	.request_vdm_cmd = pd_request_vdm_cmd,
	.get_power_role = pd_get_power_role,
	.get_current_state = pd_get_current_state,
	.get_pdos = pd_get_pdos,
	.set_pd_verify_process = pd_set_pd_verify_process,
	.get_usbpd_verifed = pd_get_usbpd_verifed,
};

static int adapter_parse_dt(struct charger_manager *manager, struct xm_pd_adapter *adapter)
{
	struct device_node *np = of_find_node_by_name(manager->dev->of_node, "xm_pd_adapter");

	lx_err("enter\n");

	if (!np) {
		lx_err("no device node\n");
		return -EINVAL;
	}

	if (of_property_read_string(np, "adapter_name",	&adapter->adapter_dev_name) < 0) {
		lx_err("no charger name\n");
		return -ENOMEM;
	}

	return 0;
}

int xm_pd_adapter_init(struct charger_manager *manager)
{
	struct xm_pd_adapter *adapter = NULL;
	int ret = 0;

	lx_info("start!\n");

	adapter = devm_kzalloc(manager->dev, sizeof(struct xm_pd_adapter), GFP_KERNEL);
	if (!adapter)
		return -ENOMEM;

	ret = adapter_parse_dt(manager, adapter);
	if (ret) {
		lx_err("pd adapter disabled!\n");
		goto err_out;
	}

	adapter->adapter_dev = adapter_device_register(adapter->adapter_dev_name,
		manager->dev, adapter, &xm_adapter_ops, NULL);
	if (IS_ERR_OR_NULL(adapter->adapter_dev)) {
		ret = PTR_ERR(adapter->adapter_dev);
		lx_err("adapter_device_register failed\n");
		goto err_out;
	}
	manager->pd_adapter = adapter->adapter_dev;

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	adapter->tcpc = manager->tcpc;
#endif
	adapter->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(adapter->tcpc, &adapter->pd_nb,
				TCP_NOTIFY_TYPE_USB | TCP_NOTIFY_TYPE_MISC | TCP_NOTIFY_TYPE_MODE);
	if (ret < 0) {
		lx_err("register tcpc notifer fail\n");
		ret = -EINVAL;
		goto err_get_tcpc_dev;
	}
	lx_info("successfully\n");

	return 0;

err_get_tcpc_dev:
	adapter_device_unregister(adapter->adapter_dev);
err_out:
	devm_kfree(manager->dev, adapter);

	return ret;
}

