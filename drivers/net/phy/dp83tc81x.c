// SPDX-License-Identifier: GPL-2.0
/* Driver for the Texas Instruments DP83TC812 PHY
 * Copyright (C) 2021 Texas Instruments Incorporated - http://www.ti.com/
 */

#include <linux/ethtool.h>
#include <linux/ethtool_netlink.h>
#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/mii.h>
#include <linux/mdio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy.h>
#include <linux/netdevice.h>

#define DP83TC812_PHY_ID			0x2000a271
#define DP83TC813_PHY_ID			0x2000a211
#define DP83TC814_PHY_ID			0x2000a261

#define MMD1F						0x1f
#define MMD1						0x1

#define DP83TC81x_STRAP					0x45d
#define MII_DP83TC81x_SGMII_CTRL			0x608
#define SGMII_CONFIG_VAL				0x027B
#define MII_DP83TC81x_RGMII_CTRL			0x600
#define MII_DP83TC81x_INT_STAT1				0x12
#define MII_DP83TC81x_INT_STAT2				0x13
#define MII_DP83TC81x_INT_STAT3				0x18
#define MII_DP83TC81x_RESET_CTRL			0x1f
#define MMD1_PMA_CTRL_2					0x1834
#define DP83TC81x_TDR_CFG5				0x0306
#define DP83TC81x_CDCR					0x1E
#define TDR_DONE					BIT(1)
#define TDR_FAIL					BIT(0)
#define DP83TC81x_TDR_TC1				0x310
#define DP83TC81x_TDR_START_BIT				BIT(15)
#define DP83TC81x_TDR_half_open_det_en			BIT(4)
#define BRK_MS_CFG					BIT(14)
#define HALF_OPEN_DETECT				BIT(8)
#define PEAK_DETECT					BIT(7)
#define PEAK_SIGN					BIT(6)

#define DP83TC81x_HW_RESET				BIT(15)
#define DP83TC81x_SW_RESET				BIT(14)

/* INT_STAT1 bits */
#define DP83TC81x_RX_ERR_CNT_HALF_FULL_INT_EN	BIT(0)
#define DP83TC81x_TX_ERR_CNT_HALF_FULL_INT_EN	BIT(1)
#define DP83TC81x_MS_TRAIN_DONE_INT_EN			BIT(2)
#define DP83TC81x_ESD_EVENT_INT_EN				BIT(3)
#define DP83TC81x_LINK_STAT_INT_EN				BIT(5)
#define DP83TC81x_ENERGY_DET_INT_EN				BIT(6)
#define DP83TC81x_LINK_QUAL_INT_EN				BIT(7)

/* INT_STAT2 bits */
#define DP83TC81x_JABBER_INT_EN			BIT(0)
#define DP83TC81x_POL_INT_EN			BIT(1)
#define DP83TC81x_SLEEP_MODE_INT_EN		BIT(2)
#define DP83TC81x_OVERTEMP_INT_EN		BIT(3)
#define DP83TC81x_FIFO_INT_EN			BIT(4)
#define DP83TC81x_PAGE_RXD_INT_EN		BIT(5)
#define DP83TC81x_OVERVOLTAGE_INT_EN	BIT(6)
#define DP83TC81x_UNDERVOLTAGE_INT_EN	BIT(7)

/* INT_STAT3 bits */
#define DP83TC81x_LPS_INT_EN			BIT(0)
#define DP83TC81x_WUP_INT_EN			BIT(1)
#define DP83TC81x_WAKE_REQ_INT_EN		BIT(2)
#define DP83TC811_NO_FRAME_INT_EN		BIT(3)
#define DP83TC811_POR_DONE_INT_EN		BIT(4)
#define DP83TC81x_SLEEP_FAIL_INT_EN		BIT(5)

/* RGMII_CTRL bits */
#define DP83TC81x_RGMII_EN				BIT(3)

/* SGMII CTRL bits */
#define DP83TC81x_SGMII_AUTO_NEG_EN		BIT(0)
#define DP83TC81x_SGMII_EN				BIT(9)

/* Strap bits */
#define DP83TC81x_MASTER_MODE			BIT(9)
#define DP83TC81x_RGMII_IS_EN			BIT(7)

/* RGMII ID CTRL */
#define DP83TC81x_RGMII_ID_CTRL			0x602
#define DP83TC81x_RX_CLK_SHIFT			BIT(1)
#define DP83TC81x_TX_CLK_SHIFT			BIT(0)

/*SQI Status bits*/
#define DP83TC81x_dsp_reg_71			0x871
#define MAX_SQI_VALUE					0x7

enum dp83tc81x_chip_type {
	DP83TC812_CT,
	DP83TC813_CT,
	DP83TC814_CT,
};

struct dp83tc81x_init_reg {
	int MMD;
	int	reg;
	int	val;
};

static const struct dp83tc81x_init_reg DP83TC812_master_cs2_0_init[] = {
	{0x1F, 0x001F, 0x8000},
	{0x1F, 0x0523, 0x0001},
	{0x1, 0x0834, 0xC001}, //mmd1
	{0x1F, 0x081C, 0x0FE2},
	{0x1F, 0x0872, 0x0300},
	{0x1F, 0x0879, 0x0F00},
	{0x1F, 0x0806, 0x2952},
	{0x1F, 0x0807, 0x3361},
	{0x1F, 0x0808, 0x3D7B},
	{0x1F, 0x083E, 0x045F},
	{0x1F, 0x0834, 0x8000},
	{0x1F, 0x0862, 0x00E8},
	{0x1F, 0x0896, 0x32CB},
	{0x1F, 0x003E, 0x0009},
	{0x1F, 0x001F, 0x4000},
	{0x1F, 0x0523, 0x0000},
};

static const struct dp83tc81x_init_reg DP83TC812_slave_cs2_0_init[] = {
	{0x1F, 0x001F, 0x8000},
	{0x1F, 0x0523, 0x0001},
	{0x1, 0x0834, 0x8001}, //mmd1
	{0x1F, 0x0873, 0x0821},
	{0x1F, 0x0896, 0x22FF},
	{0x1F, 0x089E, 0x0000},
	{0x1F, 0x001F, 0x4000},
	{0x1F, 0x0523, 0x0000},
};

static const struct dp83tc81x_init_reg DP83TC81x_tdr_config_init[] = {
	{0x1F, 0x523, 0x0001},
	{0x1F, 0x827, 0x4800},
	{0x1F, 0x301, 0x1701},
	{0x1F, 0x303, 0x023D},
	{0x1F, 0x305, 0x0015},
	{0x1F, 0x306, 0x001A},
	{0x1F, 0x01f, 0x4000},
	{0x1F, 0x523, 0x0000},
	{0x1F, 0x01f, 0x0000},
};

struct dp83tc81x_private {
	int chip;
	bool is_master;
	bool is_rgmii;
	bool is_sgmii;
};

static int dp83tc81x_read_straps(struct phy_device *phydev)
{
	struct dp83tc81x_private *DP83TC81x = phydev->priv;
	int strap;

	strap = phy_read_mmd(phydev, MMD1F, DP83TC81x_STRAP);
	if (strap < 0)
		return strap;

	if (strap & DP83TC81x_MASTER_MODE)
		DP83TC81x->is_master = true;

	if (strap & DP83TC81x_RGMII_IS_EN)
		DP83TC81x->is_rgmii = true;
	return 0;
};

static int dp83tc81x_reset(struct phy_device *phydev, bool hw_reset)
{
	int ret;

	if (hw_reset)
		ret = phy_write_mmd(phydev, MMD1F,
				    MII_DP83TC81x_RESET_CTRL,
				    DP83TC81x_HW_RESET);
	else
		ret = phy_write_mmd(phydev, MMD1F,
				    MII_DP83TC81x_RESET_CTRL,
				    DP83TC81x_SW_RESET);

	if (ret)
		return ret;

	mdelay(100);

	return 0;
}

static int dp83tc81x_phy_reset(struct phy_device *phydev)
{
	int err;
	int ret;

	err = phy_write_mmd(phydev, MMD1F, MII_DP83TC81x_RESET_CTRL, DP83TC81x_HW_RESET);
	if (err < 0)
		return err;

	ret = dp83tc81x_read_straps(phydev);
	if (ret)
		return ret;

	return 0;
}

static int dp83tc81x_write_seq(struct phy_device *phydev,
			       const struct dp83tc81x_init_reg *init_data, int size)
{
	int ret;
	int i;

	for (i = 0; i < size; i++) {
		ret = phy_write_mmd(phydev,
				    init_data[i].MMD,
				    init_data[i].reg,
				    init_data[i].val);
		if (ret)
			return ret;
	}
	return 0;
}

static int dp83tc81x_read_status(struct phy_device *phydev)
{
	int ret;

	ret = genphy_read_status(phydev);
	if (ret)
		return ret;

	ret = genphy_c45_pma_baset1_read_master_slave(phydev);

	return 0;
}

static int dp83tc81x_sqi(struct phy_device *phydev)
{
	int sqi;

	sqi = phy_read_mmd(phydev, MMD1F, DP83TC81x_dsp_reg_71);

	if (sqi < 0)
		return sqi;

	sqi = (sqi >> 1) & 0x7;

	return sqi;
}

static int dp83tc81x_sqi_max(struct phy_device *phydev)
{
	return MAX_SQI_VALUE;
}

static int dp83tc81x_cable_test_start(struct phy_device *phydev)
{
	dp83tc81x_write_seq(phydev, DP83TC81x_tdr_config_init,
			    ARRAY_SIZE(DP83TC81x_tdr_config_init));

	phy_write_mmd(phydev, MMD1F, DP83TC81x_CDCR, DP83TC81x_TDR_START_BIT);

	msleep(100);

	return 0;
}

static int dp83tc81x_cable_test_report_trans(struct phy_device *phydev, u32 result)
{
	int length_of_fault;

	if (result == 0) {
		return ETHTOOL_A_CABLE_RESULT_CODE_OK;
	} else if (result & PEAK_DETECT) {
		length_of_fault = (result & 0x3F) * 100;

		// If Cable is Open
		if (result & PEAK_SIGN) {
			
			ethnl_cable_test_fault_length(phydev, ETHTOOL_A_CABLE_PAIR_A, length_of_fault);
			return ETHTOOL_A_CABLE_RESULT_CODE_OPEN;
		}
		// Else it is Short
		ethnl_cable_test_fault_length(phydev, ETHTOOL_A_CABLE_PAIR_A, length_of_fault);
		return ETHTOOL_A_CABLE_RESULT_CODE_SAME_SHORT;
	}

	return ETHTOOL_A_CABLE_RESULT_CODE_UNSPEC;
}

static int dp83tc81x_cable_test_report(struct phy_device *phydev)
{
	int ret;
	ret = phy_read_mmd(phydev, MMD1F, DP83TC81x_TDR_TC1);

	if (ret < 0)
		return ret;
	ethnl_cable_test_result(phydev, ETHTOOL_A_CABLE_PAIR_A,
				dp83tc81x_cable_test_report_trans(phydev, ret));

	return 0;
}

static int dp83tc81x_cable_test_get_status(struct phy_device *phydev, bool *finished)
{
	int ret;
	*finished = false;
	
	ret = phy_read_mmd(phydev, MMD1F, DP83TC81x_CDCR);

	/* Check if TDR test is done*/
	if (!(ret & TDR_DONE))
		return 0;

	/* Check for TDR test failure */
	if (!(ret & TDR_FAIL)) {
		*finished = true;
		return dp83tc81x_cable_test_report(phydev);
	}

	return -EINVAL;
}

static int dp83tc81x_chip_init(struct phy_device *phydev)
{
	struct dp83tc81x_private *DP83TC81x = phydev->priv;
	int ret;

	ret = dp83tc81x_reset(phydev, true);
	if (ret)
		return ret;

	phydev->autoneg = AUTONEG_DISABLE;
	phydev->speed = SPEED_100;
	phydev->duplex = DUPLEX_FULL;
	linkmode_set_bit(ETHTOOL_LINK_MODE_100baseT_Full_BIT,
			 phydev->supported);

	if (DP83TC81x->is_master)
		ret = phy_write_mmd(phydev, MMD1, 0x0834, 0xc001);
	else
		ret = phy_write_mmd(phydev, MMD1, 0x0834, 0x8001);

	switch (DP83TC81x->chip) {
	case DP83TC812_CT:
		if (DP83TC81x->is_master) {
			ret = dp83tc81x_write_seq(phydev, DP83TC812_master_cs2_0_init,
						  ARRAY_SIZE(DP83TC812_master_cs2_0_init));
			phy_set_bits_mmd(phydev, MMD1F, 0x018B, BIT(6));
			//Enables Autonomous Mode
			} else {
				ret = dp83tc81x_write_seq(phydev, DP83TC812_slave_cs2_0_init,
							  ARRAY_SIZE(DP83TC812_slave_cs2_0_init));
				phy_set_bits_mmd(phydev, MMD1F, 0x018B, BIT(6));
				//Enables Autonomous Mode
			}
			break;
	case DP83TC813_CT:
			if (DP83TC81x->is_master) {
				ret = dp83tc81x_write_seq(phydev, DP83TC812_master_cs2_0_init,
							  ARRAY_SIZE(DP83TC812_master_cs2_0_init));
				phy_set_bits_mmd(phydev, MMD1F, 0x018B, BIT(6));
				//Enables Autonomous Mode
			} else {
				ret = dp83tc81x_write_seq(phydev, DP83TC812_slave_cs2_0_init,
							  ARRAY_SIZE(DP83TC812_slave_cs2_0_init));
				phy_set_bits_mmd(phydev, MMD1F, 0x018B, BIT(6));
				//Enables Autonomous Mode
			}
			break;
	case DP83TC814_CT:
			if (DP83TC81x->is_master) {
				ret = dp83tc81x_write_seq(phydev, DP83TC812_master_cs2_0_init,
							  ARRAY_SIZE(DP83TC812_master_cs2_0_init));
				phy_set_bits_mmd(phydev, MMD1F, 0x018B, BIT(6));
				//Enables Autonomous Mode
			} else {
				ret = dp83tc81x_write_seq(phydev, DP83TC812_slave_cs2_0_init,
							  ARRAY_SIZE(DP83TC812_slave_cs2_0_init));
				phy_set_bits_mmd(phydev, MMD1F, 0x018B, BIT(6));
				//Enables Autonomous Mode
			}
			break;
	default:
			return -EINVAL;
	};

	if (ret)
		return ret;

	mdelay(10);

	/* Do a soft reset to restart the PHY with updated values */
	return dp83tc81x_reset(phydev, false);
}

static int dp83tc81x_config_init(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	s32 rx_int_delay;
	s32 tx_int_delay;
	int rgmii_delay;
	int value, ret;

	ret = dp83tc81x_chip_init(phydev);
	if (ret)
		return ret;

	if (phy_interface_is_rgmii(phydev)) {
		rx_int_delay = phy_get_internal_delay(phydev, dev, NULL, 0,
						      true);

		if (rx_int_delay <= 0)
			rgmii_delay = 0;
		else
			rgmii_delay = DP83TC81x_RX_CLK_SHIFT;

		tx_int_delay = phy_get_internal_delay(phydev, dev, NULL, 0,
						      false);
		if (tx_int_delay <= 0)
			rgmii_delay &= ~DP83TC81x_TX_CLK_SHIFT;
		else
			rgmii_delay |= DP83TC81x_TX_CLK_SHIFT;

		if (rgmii_delay) {
			ret = phy_set_bits_mmd(phydev, MMD1,
					       DP83TC81x_RGMII_ID_CTRL,
					       rgmii_delay);
			if (ret)
				return ret;
		}
	}

	if (phydev->interface == PHY_INTERFACE_MODE_SGMII) {
		value = phy_read(phydev, MII_DP83TC81x_SGMII_CTRL);
		ret = phy_write_mmd(phydev, MMD1F, MII_DP83TC81x_SGMII_CTRL,
				    SGMII_CONFIG_VAL);
	if (ret < 0)
		return ret;
	}

	return 0;
}

/* static int dp83tc812_ack_interrupt(struct phy_device *phydev)
 *{
 *	int err;
 *
 *	err = phy_read(phydev, MII_DP83TC812_INT_STAT1);
 *	if (err < 0)
 *		return err;
 *
 *	err = phy_read(phydev, MII_DP83TC812_INT_STAT2);
 *	if (err < 0)
 *		return err;
 *
 *	err = phy_read(phydev, MII_DP83TC812_INT_STAT3);
 *	if (err < 0)
 *		return err;
 *
 *	return 0;
 *}
 */

static int dp83tc81x_config_intr(struct phy_device *phydev)
{
	int misr_status, err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED) {
		misr_status = phy_read(phydev, MII_DP83TC81x_INT_STAT1);
		if (misr_status < 0)
			return misr_status;

		misr_status |= (DP83TC81x_ESD_EVENT_INT_EN |
				DP83TC81x_LINK_STAT_INT_EN |
				DP83TC81x_ENERGY_DET_INT_EN |
				DP83TC81x_LINK_QUAL_INT_EN);

		err = phy_write(phydev, MII_DP83TC81x_INT_STAT1, misr_status);
		if (err < 0)
			return err;

		misr_status = phy_read(phydev, MII_DP83TC81x_INT_STAT2);
		if (misr_status < 0)
			return misr_status;

		misr_status |= (DP83TC81x_SLEEP_MODE_INT_EN |
				DP83TC81x_OVERTEMP_INT_EN |
				DP83TC81x_OVERVOLTAGE_INT_EN |
				DP83TC81x_UNDERVOLTAGE_INT_EN);

		err = phy_write(phydev, MII_DP83TC81x_INT_STAT2, misr_status);
		if (err < 0)
			return err;

		misr_status = phy_read(phydev, MII_DP83TC81x_INT_STAT3);
		if (misr_status < 0)
			return misr_status;

		misr_status |= (DP83TC81x_LPS_INT_EN |
				DP83TC81x_WAKE_REQ_INT_EN |
				DP83TC811_NO_FRAME_INT_EN |
				DP83TC811_POR_DONE_INT_EN);

		err = phy_write(phydev, MII_DP83TC81x_INT_STAT3, misr_status);

	} else {
		err = phy_write(phydev, MII_DP83TC81x_INT_STAT1, 0);
		if (err < 0)
			return err;

		err = phy_write(phydev, MII_DP83TC81x_INT_STAT2, 0);
		if (err < 0)
			return err;

		err = phy_write(phydev, MII_DP83TC81x_INT_STAT3, 0);
	}

	return err;
}

static int dp83tc81x_config_aneg(struct phy_device *phydev)
{
	bool changed = false;
	int err, value, ret;

	if (phydev->interface == PHY_INTERFACE_MODE_SGMII) {
		value = phy_read(phydev, MII_DP83TC81x_SGMII_CTRL);
		ret = phy_write_mmd(phydev, MMD1F, MII_DP83TC81x_SGMII_CTRL,
				    SGMII_CONFIG_VAL);
		if (ret < 0)
			return ret;
	}

	err = genphy_c45_pma_baset1_setup_master_slave(phydev);
	if (err < 0)
		return err;
	else if (err)
		changed = true;

	if (phydev->autoneg != AUTONEG_ENABLE)
		return genphy_setup_forced(phydev);

	return genphy_config_aneg(phydev);
}

static int dp83tc81x_probe(struct phy_device *phydev)
{
	struct dp83tc81x_private *DP83TC81x;
	int ret;

	DP83TC81x = devm_kzalloc(&phydev->mdio.dev, sizeof(*DP83TC81x), GFP_KERNEL);
	if (!DP83TC81x)
		return -ENOMEM;

	phydev->priv = DP83TC81x;
	ret = dp83tc81x_read_straps(phydev);
	if (ret)
		return ret;

	switch (phydev->phy_id) {
	case DP83TC812_PHY_ID:
		DP83TC81x->chip = DP83TC812_CT;
		break;
	case DP83TC813_PHY_ID:
		DP83TC81x->chip = DP83TC813_CT;
		break;
	case DP83TC814_PHY_ID:
		DP83TC81x->chip = DP83TC814_CT;
		break;
	default:
		return -EINVAL;
	};

	return dp83tc81x_config_init(phydev);
}

#define DP83TC81x_PHY_DRIVER(_id, _name)					\
{										\
	PHY_ID_MATCH_EXACT(_id),						\
	.name				= (_name),				\
	.probe				= dp83tc81x_probe,			\
	/* PHY_BASIC_FEATURES */						\
	.soft_reset			= dp83tc81x_phy_reset,			\
	.config_init			= dp83tc81x_config_init,		\
	.config_aneg			= dp83tc81x_config_aneg,		\
	.config_intr			= dp83tc81x_config_intr,		\
	.suspend			= genphy_suspend,			\
	.resume				= genphy_resume,			\
	.get_sqi			= dp83tc81x_sqi,			\
	.get_sqi_max			= dp83tc81x_sqi_max,			\
	.cable_test_start		= dp83tc81x_cable_test_start, 		\
	.cable_test_get_status		= dp83tc81x_cable_test_get_status,	\
	.read_status			= dp83tc81x_read_status,		\
}

/*		.ack_interrupt 		= dp83tc812_ack_interrupt,\
 *if 0								\
 *		.handle_interrupt = dp83812_handle_interrupt,	\
 * #endif
 */

static struct phy_driver DP83TC81x_driver[] = {
	DP83TC81x_PHY_DRIVER(DP83TC812_PHY_ID, "TI DP83TC812"),
	DP83TC81x_PHY_DRIVER(DP83TC813_PHY_ID, "TI DP83TC813"),
	DP83TC81x_PHY_DRIVER(DP83TC814_PHY_ID, "TI DP83TC814"),
	};
module_phy_driver(DP83TC81x_driver);

static struct mdio_device_id __maybe_unused DP83TC81x_tbl[] = {
	// { PHY_ID_MATCH_EXACT(DP83TC812_CS1_0_PHY_ID) },
	{ PHY_ID_MATCH_EXACT(DP83TC812_PHY_ID) },
	{ PHY_ID_MATCH_EXACT(DP83TC813_PHY_ID) },
	{ PHY_ID_MATCH_EXACT(DP83TC814_PHY_ID) },
	{ },
};
MODULE_DEVICE_TABLE(mdio, DP83TC81x_tbl);

MODULE_DESCRIPTION("Texas Instruments DP83TC812 PHY driver");
MODULE_AUTHOR("Hari Nagalla <hnagalla@ti.com");
MODULE_LICENSE("GPL");
