
// SPDX-License-Identifier: GPL-2.0+
#ifndef MPAM_ARCH_H
#define MPAM_ARCH_H

#define MPAM_PARTID_DEFAULT 16
#define MPAM_PORTION_MIN 0
#define MPAM_PORTION_MAX 100
#define MPAM_ZONE_FIRST 0
#define MPAM_ZONE_LAST 5

#define ID_AA64PFR0_MPAM    BIT(40)
/* Sysregs */

/* MPAM0_EL1 */
#define MPAM0_EL1_PARTID_I GENMASK(15, 0)
#define MPAM0_EL1_PARTID_D GENMASK(31, 16)

/* MPAM1_EL1 */
#define MPAM1_EL1_MPAMEN   BIT(63)

/* MPAMIDR_EL1 */
#define MPAMIDR_EL1_PARTID_MAX GENMASK(15, 0)

#define SYS_MPAMIDR_EL1			sys_reg(3, 0, 10, 4, 4)
#define SYS_MPAM1_EL1			sys_reg(3, 0, 10, 5, 0)
#define SYS_MPAM0_EL1			sys_reg(3, 0, 10, 5, 1)

#define FIELD_SET(reg, field, val) (reg = (reg & ~field) | FIELD_PREP(field, val))

extern void mpam_write_partid(unsigned int partid);
extern void mpam_set_task_partid(struct task_struct *p, unsigned int partid);
extern unsigned int mpam_get_task_partid(struct task_struct *p);
extern void mpam_sync_task(struct task_struct *p);

extern int mpam_arch_initd;
#endif