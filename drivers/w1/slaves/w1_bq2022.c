/*
 * drivers/w1/slaves/w1_bq2022.c
 *
 * Copyright (C) 2014 Texas Instruments, Inc.
 * Copyright (C) 2015 Balázs Triszka <balika011@protonmail.ch>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 */
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>

#include "../w1.h"
#include "../w1_int.h"
#include "../w1_family.h"
#include <linux/delay.h>


#define HDQ_CMD_SKIP_ROM (0xCC)
#define HDQ_CMD_READ_FIELD (0xF0)

#define CRYPT_COMMON_HEADER	(0xE54C21ED)

#define BQ2022_ID_SAMSUNG_XWD		0x10139461
#define BQ2022_ID_GUANGYU			0x10139462
#define BQ2022_ID_SONY_XWD			0x10139463
#define BQ2022_ID_SAMSUNG_XWD_CD	0x10139464
#define BQ2022_ID_LG_DESA			0x10139465
#define BQ2022_ID_SONY_FMT			0x10139466
#define BQ2022_ID_RUISHENG			0x10139467
#define BQ2022_ID_DELSA				0x8412E562
#define BQ2022_ID_AAC				0xAACAACAA
#define BQ2022_ID_COSLIGHT			0xDF0C7A62
#define BQ2022_ID_SAMSUNG_FMT		0xF40E9762

#define GEN_PSEUDO_INFO(ptr) (((*((unsigned int *)&ptr[60]))&0xFFFFFF00)|((unsigned int)ptr[8]))
#define GEN_PSEUDO_HEADER(ptr) (*((unsigned int *)&ptr[0]))

static char batt_crypt_info[128];
static struct w1_slave *bq2022_slave = NULL;

int w1_bq2022_has_slave(void)
{
	return bq2022_slave != NULL;
}

int w1_bq2022_battery_id(void)
{
	unsigned int pseduo_info;
	int ret = 0;

	if (GEN_PSEUDO_HEADER(batt_crypt_info) != CRYPT_COMMON_HEADER)
	{
		pr_err("%s: cannot read batt id through one-wire\n", __func__);
		return ret;
	}
	else
	{
		pseduo_info = GEN_PSEUDO_INFO(batt_crypt_info);
		pr_info("%s: pseduo_info:0x%08x\n", __func__, pseduo_info);
	}

	switch(pseduo_info) {
	case BQ2022_ID_LG_DESA:
	case BQ2022_ID_COSLIGHT:
		ret = 0x30000; // batt_id_kohm = 12
		break;

	case BQ2022_ID_SAMSUNG_XWD:
	case BQ2022_ID_AAC:
	case BQ2022_ID_SAMSUNG_FMT:
		ret = 0x40000; // batt_id_kohm = 17
		break;

	case BQ2022_ID_SONY_XWD:
	case BQ2022_ID_SONY_FMT:
	case BQ2022_ID_DELSA:
		ret = 0x50000; // batt_id_kohm = 22
		break;

	case BQ2022_ID_GUANGYU:
		ret = 0x60000; // batt_id_kohm = 28
		break;

	case BQ2022_ID_RUISHENG:
		ret = 0x70000; // batt_id_kohm = ??
		break;

	case BQ2022_ID_SAMSUNG_XWD_CD:
		ret = 0x80000; // batt_id_kohm = ??
		break;
	}
	return ret;
}

static int w1_bq2022_read(void)
{
	struct w1_slave *sl = bq2022_slave;
	char cmd[4];
	u8 crc, calc_crc;
	int retries = 5;

	if (!sl) {
		pr_err("%s: No w1 device\n", __func__);
		return -1;
	}

retry:
	/* Initialization, master's mutex should be hold */
	if (!(retries--)) {
		pr_err("%s: fatal error\n", __func__);
		return -1;
	}

	if (w1_reset_bus(sl->master)) {
		pr_warn("%s: reset bus failed, just retry!\n", __func__);
		goto retry;
	}

	/* rom comm byte + read comm byte + addr 2 bytes */
	cmd[0] = HDQ_CMD_SKIP_ROM;
	cmd[1] = HDQ_CMD_READ_FIELD;
	cmd[2] = 0x0;
	cmd[3] = 0x0;

	/* send command */
	w1_write_block(sl->master, cmd, 4);

	/* crc verified for read comm byte and addr 2 bytes*/
	crc = w1_read_8(sl->master);
	calc_crc = w1_calc_crc8(&cmd[1], 3);
	if (calc_crc != crc) {
		pr_err("%s: com crc err\n", __func__);
		goto retry;
	}

	/* read the whole memory, 1024-bit */
	w1_read_block(sl->master, batt_crypt_info, 128);

	/* crc verified for data */
	crc = w1_read_8(sl->master);
	calc_crc = w1_calc_crc8(batt_crypt_info, 128);
	if (calc_crc != crc) {
		pr_err("%s: w1_bq2022 data crc err\n", __func__);
		goto retry;
	}

	return 0;
}

static int w1_bq2022_add_slave(struct w1_slave *sl)
{
	bq2022_slave = sl;
	return w1_bq2022_read();
}

static void w1_bq2022_remove_slave(struct w1_slave *sl)
{
	bq2022_slave = NULL;
}

static struct w1_family_ops w1_bq2022_fops = {
	.add_slave = w1_bq2022_add_slave,
	.remove_slave = w1_bq2022_remove_slave,
};

static struct w1_family w1_bq2022_family = {
	.fid = 0x09,
	.fops = &w1_bq2022_fops,
};

static int __init w1_bq2022_init(void)
{
	return w1_register_family(&w1_bq2022_family);
}
module_init(w1_bq2022_init);

static void __exit w1_bq2022_exit(void)
{
	w1_unregister_family(&w1_bq2022_family);
}
module_exit(w1_bq2022_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Balázs Triszka <balika011@protonmail.ch>");
MODULE_DESCRIPTION("TI BQ2022 Battery Chip Driver");
