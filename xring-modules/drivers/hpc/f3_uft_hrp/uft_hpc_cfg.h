#ifndef _F3_HPC_FPGA_CFG_H
#define _F3_HPC_FPGA_CFG_H

/*************pcie space *************/
#define PCIE_SPACE_BASE             0xA0000000

/*************spi reg space*************/
#define UFT_SPI_REG_BASE            0x00408000
#define REG_DUT_OFFSET_CFG          0x2C
#define REG_DDR4_33BIT              0x38

/*************ddr space*************/
#define NPUFW_IMG_ADDR              0x60000000
#define RPMSG_TX_ADDR               0x61C00000
#define RPMSG_RX_ADDR               0x61C03000

/*************npu reg space*************/
#define NPU_SYS_BASE           0xD0000000
#define NPU_SYS_SIZE           0x1000000

#define NPU_NS_IPCM_BASE       (0x00D18000 + NPU_SYS_BASE)
#define NPU_SYSCTRL_FUN_S_BASE (0x00B34000 + NPU_SYS_BASE)
#define NPU_M85_CTRL_BASE      (0x00B04000 + NPU_SYS_BASE)
#define NPU_SUBCHIP_CTRL_BASE  (0x01F00000 + NPU_SYS_BASE)
#define NPU_LPCTRL_BASE        (0x01F04000 + NPU_SYS_BASE)
#define NPU_CRG_BASE           (0x00B00000 + NPU_SYS_BASE)
#define NPU_SYSCTRL_FUN_NS_BASE (0x00B38000 + NPU_SYS_BASE)
/*************util macros*************/
#define SETBIT(value, bit, zero_or_one) \
    (((value) & ~(1 << (bit))) | ((zero_or_one & 1) << (bit)))


/*************path macros*************/
#define NPUFW_IMG_FILE "/data/vendor/npu/npu_fw.bin"

/*************npu operaton api*************/
int npu_boot(struct hpc_device *hdev);
int npu_shutdown(void);
int npu_rpmsg_tx_data(void *data, size_t len);
int npu_mailbox_poll(void);
int npu_rpmsg_rx_data(void *msg_buf, size_t len);
int memcpy_to_fpga_ddr(void *data, size_t size, u64 fpga_ddr_addr);
int memcpy_from_fpga_ddr(void *dst, size_t size, u64 fpga_ddr_addr);
int uft_hpc_driver_init(void *data);

#endif
