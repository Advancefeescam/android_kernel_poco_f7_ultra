#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "hl7603x.h"

#define HL7603_CONFIG0					0x00

#define HL7603_DEV_ID_MASK        		(BIT(7) |BIT(6) |BIT(5) |BIT(4))
#define HL7603_DEV_ID_SHIFT       		4
#define HL7603_DEV_REV_MASK        		(BIT(3) |BIT(2) |BIT(1) |BIT(0))
#define HL7603_DEV_REV_SHIFT       		0

#define HL7603_CONFIG1					0x01

#define HL7603_RESET_MASK               		BIT(7)
#define HL7603_RESET_SHIFT              		7
#define HL7603_DEV_EN_MASK             	BIT(6)
#define HL7603_DEV_EN_SHIFT             	6
#define BYP_DEV_SET_EN             	1
#define HL7603_MODE_CFG_MASK        	(BIT(5) |BIT(4))
#define HL7603_MODE_CFG_SHIFT       		4
#define BYP_DEV_AUTO_BY_PASS_MODE       		2
#define HL7603_VOUT_DISCHG_MASK          BIT(3)
#define HL7603_VOUT_DISCHG_SHIFT         3
#define HL7603_EN_OOA_MASK             	BIT(2)
#define HL7603_EN_OOA_SHIFT             	2
#define HL7603_FPWM_CFG_MASK              BIT(1)
#define HL7603_FPWM_CFG_SHIFT             1

#define HL7603_VOUT_VSEL				0x02
#define HL7603_VOUT_REG_MASK               (BIT(5)|BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0))
#define HL7603_VOUT_REG_SHIFT              	0

#define HL7603_ILIMSET1					0x03

#define HL7603_ILIN1_SET_MASK        		(BIT(7) |BIT(6))
#define HL7603_ILIN1_SET_SHIFT       		6
#define HL7603_ILIM_OFF_MASK    		BIT(5)
#define HL7603_ILIM_OFF_SHIFT   		5
#define HL7603_SOFT_START_MASK   		BIT(4)
#define HL7603_SOFT_START_SHIFT   		4
#define HL7603_ILIM_MASK        			(BIT(3) |BIT(2)|BIT(0))
#define HL7603_ILIM_SHIFT       			0

#define HL7603_ILIMSET2					0x04

#define HL7603_T_ILIM_H_MASK        		(BIT(1) |BIT(0))
#define HL7603_T_ILIM_H_SHIFT       		0

#define HL7603_STATUS					0x05

#define HL7603_TSD_MASK               		BIT(7)
#define HL7603_TSD_SHIFT              		7
#define HL7603_HOTDIE_MASK             	BIT(6)
#define HL7603_HOTDIE_SHIFT             	6
#define HL7603_DCDCMODE_MASK        	BIT(5)
#define HL7603_DCDCMODE_SHIFT       	5
#define HL7603_OPMODE_MASK        		BIT(4)
#define HL7603_OPMODE_SHIFT       		4
#define HL7603_VIN_OVP_MASK         	 	BIT(3)
#define HL7603_VIN_OVP_SHIFT         		3
#define HL7603_VOUT_OVP_MASK             	BIT(2)
#define HL7603_VOUT_OVP_SHIFT             	2
#define HL7603_FAULT_MASK              		BIT(1)
#define HL7603_FAULT_SHIFT             		1
#define HL7603_PGOOD_MASK              	BIT(0)
#define HL7603_PGOOD_SHIFT             		0

// P6 code for HQFEAT-181600 by p-gaobowei1 at 2025/09/10 start
#define CHIP_ID_HL7603 0xB4
#define CHIP_ID_SC8315C 0x05

#define HL7603_STATUS1					0x06
//HL7603
#define HL7603_SPECIAL1					0xA7
#define HL7603_SPECIAL2					0x22
//SC8315C
// P6 code for HQFEAT-186209 by p-gaobowei1 at 2025/09/12 start
#define SC8315C_SPECIAL1				0x7F
#define SC8315C_SPECIAL2				0x82
#define SC8315C_SPECIAL3				0x83
// P6 code for HQFEAT-186209 by p-gaobowei1 at 2025/09/12 end
// P6 code for HQFEAT-181600 by p-gaobowei1 at 2025/09/10 end

struct hl7603_device_info {
	kal_uint8 id;
	char *name;
	kal_uint16 i2c_channel;
	kal_uint8 slave_addr;
	kal_uint8 buck_ctrl;
	kal_uint8 mode_shift;
	kal_uint8 en_shift;
	kal_uint8 chip_id;
	u8  voltage_value;
	struct i2c_client *client;
	struct device *dev;
	struct work_struct irq_work;
	struct mutex i2c_rw_lock;
	int device_id;
	int rev_id;
	int dc_ibus_ucp_happened;
	u32 ic_role;
	int get_id_time;
	int get_rev_time;
	int init_finish_flag;
	int int_notify_enable_flag;
	int switching_frequency;
	int sense_r_actual;
	int sense_r_config;
	int chip_already_init;
};


#define EXTBUCK_hl7603	1
/**********************************************************
  *   Global Variable
  *********************************************************/


#define hl7603_print(fmt, args...)	dev_info(chip->dev, "[HL7603] " fmt, ##args)

/**********************************************************
  *
  *   [I2C Function For Read/Write hl7603]
  *
  *********************************************************/
 static int __byp_read_reg(struct hl7603_device_info *chip, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(chip->client, reg);
	if (ret < 0) {
		msleep(500);
		ret = i2c_smbus_read_byte_data(chip->client, reg);
		if (ret < 0) {
			pr_err("%s:%d ,i2c read fail: can't read from reg 0x%02x: %d\n",
				__func__, __LINE__, reg, ret);
			return ret;
		}
		*data = (u8)ret;

		return 0;
	}
	*data = (u8)ret;

	return 0;
}

static int __byp_write_reg(struct hl7603_device_info *chip, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		pr_err("%s:%d ,i2c write fail: can't write 0x%02x to reg 0x%02x: %d\n",
			__func__, __LINE__, val, reg, ret);
		return ret;
	}

	return 0;
}

static int byp_read_byte(struct hl7603_device_info *chip, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&chip->i2c_rw_lock);
	ret = __byp_read_reg(chip, reg, data);
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

static int byp_write_byte(struct hl7603_device_info *chip, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&chip->i2c_rw_lock);
	ret = __byp_write_reg(chip, reg, data);
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

/**********************************************************
  *
  *   [Read / Write Function]
  *
  *********************************************************/
// P6 code for HQFEAT-181600 by p-gaobowei1 at 2025/09/10 start
void byp_dump_register(struct hl7603_device_info *chip, u8 RegNum, u8 *val)
{
	int ret = 0;

	ret = byp_read_byte(chip, RegNum, val);
	if (ret < 0){
		pr_err("%s:%d ,dump register fail,RegNum=0x%02x\n", __func__, __LINE__, RegNum);
	} else {
		pr_info("%s : RegNum=0x%02x:0x%02x\n", __func__, RegNum, *val);
	}
}
// P6 code for HQFEAT-181600 by p-gaobowei1 at 2025/09/10 end

int byp_config_interface(struct hl7603_device_info *chip, u8 RegNum,
				u8 val, u8 MASK, u8 SHIFT)
{
	u8 hl7603_reg = 0;
	int ret = 0;

	ret = byp_read_byte(chip, RegNum, &hl7603_reg);

	hl7603_reg &= ~(MASK << SHIFT);
	hl7603_reg |= (val << SHIFT);

	ret = byp_write_byte(chip, RegNum, hl7603_reg);

	return ret;
}

int byp_enable(struct hl7603_device_info *chip, unsigned char en)
{
	int ret = 1;
	hl7603_print("%s ++\n", __func__);
	ret = byp_config_interface(chip, HL7603_CONFIG1, en,
                                     HL7603_DEV_EN_MASK, HL7603_DEV_EN_SHIFT);

	hl7603_print("%s --, en=%d, ret=%d\n", __func__, en, ret);
	return ret;
}

int byp_set_mode(struct hl7603_device_info *chip, unsigned char mode)
{
	int ret;
	pr_info("%s ++\n", __func__);
	if (mode != 0 && mode != 2 ) {
		pr_err("%s:%d error mode = %d only 0 or 3\n", __func__, __LINE__, mode);
		return -1;
	}

	ret = byp_config_interface(chip, HL7603_CONFIG1, mode, HL7603_MODE_CFG_MASK,HL7603_MODE_CFG_SHIFT);
	pr_info("%s --,ret=%d\n", __func__, ret);
	return ret;
}

static int byp_reg_init(struct hl7603_device_info *chip)
{
	kal_uint32 ret = 0;
	pr_info("%s ++\n", __func__);
	ret = byp_config_interface(chip, HL7603_VOUT_VSEL, chip->voltage_value,
                                      HL7603_VOUT_REG_MASK, HL7603_VOUT_REG_SHIFT);
	if (ret != 0) {
		pr_err("%s:%d byp_config_interface fail,ret=%d\n", __func__, ret);
		return ret;
	}

	ret = byp_set_mode(chip, BYP_DEV_AUTO_BY_PASS_MODE);
	if (ret != 0) {
		pr_err("%s:%d byp_set_mode fail,ret=%d\n", __func__, ret);
		return ret;
	}

	ret = byp_enable(chip, BYP_DEV_SET_EN);
	if (ret != 0) {
		pr_err("%s:%d byp_enable fail,ret=%d\n", __func__, ret);
		return ret;
	}

	pr_info("%s --,chip_id=0x%02x\n", __func__, chip->chip_id);

  	return 0;
}

static int byp_reg_reset(void *dev_data)
{
	int ret = 0;
	u8 reg;
	struct hl7603_device_info *di = dev_data;
	pr_info("%s ++\n", __func__);

	ret = byp_config_interface(di, HL7603_CONFIG1, HL7603_RESET_MASK,HL7603_VOUT_REG_SHIFT, 0x01);
	if (ret != 0) {
		pr_err("%s:%d byp_config_interface fail,ret=%d\n", __func__, __LINE__, ret);
		return ret;
	}

	msleep(10);

	ret = byp_read_byte(di, HL7603_CONFIG1, &reg);
	if (ret != 0) {
		pr_err("%s:%d byp_read_byte fail,ret=%d\n", __func__, __LINE__, ret);
		return ret;
	}

	pr_info("%s --,done,[%x]=0x%x\n", __func__, HL7603_CONFIG1, reg);
	return 0;
}

static int byp_parse_dts(struct hl7603_device_info *chip)
{
	struct device_node *np = chip->dev->of_node;
	int ret = 0;
	u32 voltage;
	pr_info("%s ++\n", __func__);
	ret = of_property_read_u32(np, "halo,hl7603,hl7603_vout_voltage",
				   &voltage);
	if (ret) {
		pr_err("%s:%d failed to read bat-ovp-threshold=%d\n", __func__, __LINE__, ret);
		return ret;
	}

	chip->voltage_value = (u8)voltage;
	pr_info("%s --,chip->voltage_value=0x%x\n", __func__, chip->voltage_value);
	return 0;
}

static int hl7603_driver_probe(struct i2c_client *client,
                       const struct i2c_device_id *id)
{
      	int ret;
	kal_uint8 byp_chip_id = 0;
	// P6 code for HQFEAT-181600 by p-gaobowei1 at 2025/09/10 start
	//add dump register
	u8 reg_01 = 0;
	u8 reg_02 = 0;
	u8 reg_03 = 0;
	u8 reg_04 = 0;
	u8 reg_05 = 0;
	u8 reg_06 = 0;
	// P6 code for HQFEAT-186209 by p-gaobowei1 at 2025/09/12 start
	u8 reg_82 = 0;
	u8 reg_83 = 0;
	// P6 code for HQFEAT-186209 by p-gaobowei1 at 2025/09/12 end
	u8 reg_22 = 0;
	u8 reg_A7 = 0;
	// P6 code for HQFEAT-181600 by p-gaobowei1 at 2025/09/10 end
	struct hl7603_device_info *di = NULL;
	struct device_node *np = NULL;
	pr_info("%s enter\n", __func__);
	if (!client || !client->dev.of_node || !id)
		return -ENODEV;

	di = devm_kzalloc(&client->dev, sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	di->dev = &client->dev;
	np = di->dev->of_node;
	di->client = client;

	ret = byp_read_byte(di, HL7603_CONFIG0, &byp_chip_id);
	if (ret != 0) {
		return ret;
	} else {
		di->chip_id = byp_chip_id;
		pr_info("%s byp_chip_id=0x%02x\n", __func__, di->chip_id);
	}

	mutex_init(&di->i2c_rw_lock);

	ret = byp_parse_dts(di);
	if (ret != 0)
		goto cleanup;

	ret = byp_reg_reset(di);
	if (ret != 0)
		goto cleanup;

	ret = byp_reg_init(di);
	if (ret != 0)
		goto cleanup;
	// P6 code for HQFEAT-186209 by p-gaobowei1 at 2025/09/12 start
	// P6 code for HQFEAT-181600 by p-gaobowei1 at 2025/09/10 start
	if( di->chip_id == CHIP_ID_HL7603 ) {
		//1st hl7603 -- 0xb4
		//write 0x03 -> 0x9
		//write 0xA7 -> 0xF9
		//read 0x22 -> 0x40, if 0x40 write 0x00
		pr_info("%s ,is hl7603 IC.\n", __func__);
		ret = byp_config_interface(di, HL7603_ILIMSET1, 0x09,
						0xff, 0);
		ret |= byp_config_interface(di, HL7603_SPECIAL1, 0xF9,
						0xff, 0);
		ret |= byp_read_byte(di, HL7603_SPECIAL2, &reg_22);
		if ((ret == 0) && (reg_22 == 0x40)) {
			pr_info("%s ,reg 0x22=0x%02x,start write 0x00\n", __func__, reg_22);
			ret |= byp_config_interface(di, HL7603_SPECIAL2, 0x00,
							0xff, 0);
		}
		if (ret != 0) {
			goto cleanup;
		}
		byp_dump_register(di, HL7603_SPECIAL1, &reg_A7);
		byp_dump_register(di, HL7603_SPECIAL2, &reg_22);
	} else if ( di->chip_id == CHIP_ID_SC8315C ) {
		//2nd sc8315c -- 0x05
		//write 0x03 -> 0x0A
		//write 0x04 -> 0x4B
		//write 0x7F -> 0x55 / 0xAA / 0x69 / 0x96
		//write 0x82 -> 0x09
		//write 0x83 -> 0x20
		pr_info("%s ,is sc8315c IC.\n", __func__);
		ret = byp_config_interface(di, HL7603_ILIMSET1, 0x0A,
						0xff, 0);
		ret = byp_config_interface(di, HL7603_ILIMSET2, 0x4B,
						0xff, 0);
		ret |= byp_config_interface(di, SC8315C_SPECIAL1, 0x55,
						0xff, 0);
		ret |= byp_config_interface(di, SC8315C_SPECIAL1, 0xAA,
						0xff, 0);
		ret |= byp_config_interface(di, SC8315C_SPECIAL1, 0x69,
						0xff, 0);
		ret |= byp_config_interface(di, SC8315C_SPECIAL1, 0x96,
						0xff, 0);
		ret |= byp_config_interface(di, SC8315C_SPECIAL2, 0x09,
						0xff, 0);
		ret |= byp_config_interface(di, SC8315C_SPECIAL3, 0x20,
						0xff, 0);
		if (ret != 0) {
			goto cleanup;
		}
		byp_dump_register(di, SC8315C_SPECIAL2, &reg_82);
		byp_dump_register(di, SC8315C_SPECIAL3, &reg_83);
	}
	//add dump register
	byp_dump_register(di, HL7603_CONFIG0, &byp_chip_id);
	byp_dump_register(di, HL7603_CONFIG1, &reg_01);
	byp_dump_register(di, HL7603_VOUT_VSEL, &reg_02);
	byp_dump_register(di, HL7603_ILIMSET1, &reg_03);
	byp_dump_register(di, HL7603_ILIMSET2, &reg_04);
	byp_dump_register(di, HL7603_STATUS, &reg_05);
	byp_dump_register(di, HL7603_STATUS1, &reg_06);
	// P6 code for HQFEAT-181600 by p-gaobowei1 at 2025/09/10 end
	// P6 code for HQFEAT-186209 by p-gaobowei1 at 2025/09/12 end

	i2c_set_clientdata(client, di);

	pr_info("%s probe success exit\n", __func__);
	return 0;

cleanup:
	pr_err("%s:%d probe fail!!!\n", __func__, __LINE__);
	mutex_destroy(&di->i2c_rw_lock);
	devm_kfree(&client->dev, di);
	return ret;
}

static int hl7603_remove(struct i2c_client *client)
{
	struct hl7603_device_info *di = i2c_get_clientdata(client);

	if (!di)
		return -ENODEV;

	byp_reg_reset(di);
	mutex_destroy(&di->i2c_rw_lock);

	return 0;
}

static void hl7603_shutdown(struct i2c_client *client)
{
	struct hl7603_device_info *di = i2c_get_clientdata(client);

	if (!di)
		return;

	byp_reg_reset(di);
}

static const struct of_device_id hl7603_of_match[] = {
	{
		.compatible = "hl7603x",
		.data = NULL,
	},
	{},
};
MODULE_DEVICE_TABLE(of, hl7603_of_match);

static const struct i2c_device_id hl7603_i2c_id[] = {
	{ "hl7603x", 0 }, {}
};
MODULE_DEVICE_TABLE(i2c, hl7603_i2c_id);

static struct i2c_driver hl7603_driver = {
	.probe = hl7603_driver_probe,
	.remove = hl7603_remove,
	.shutdown = hl7603_shutdown,
	.id_table = hl7603_i2c_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = "hl7603x",
		.of_match_table = of_match_ptr(hl7603_of_match),
	},
};

static int __init hl7603_init(void)
{
	return i2c_add_driver(&hl7603_driver);
}

static void __exit hl7603_exit(void)
{
	i2c_del_driver(&hl7603_driver);
}

module_init(hl7603_init);
module_exit(hl7603_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("hl7603x module driver");
MODULE_AUTHOR("Halo Technologies Co., Ltd.");

