/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2018, 2020 The Linux Foundation. All rights reserved.
 */

#ifndef _WCD937X_H
#define _WCD937X_H

#include <dt-bindings/sound/audio-codec-port-types.h>

#define WCD937X_MAX_SLAVE_CH_TYPES 10
#define ZERO 0

struct swr_slave_ch_map {
	u8 ch_type;
	u8 index;
};

static const struct swr_slave_ch_map swr_slv_tx_ch_idx[] = {
	{ADC1, 0},
	{ADC2, 1},
	{ADC3, 2},
	{DMIC0, 3},
	{DMIC1, 4},
	{MBHC, 5},
	{DMIC2, 6},
	{DMIC3, 7},
	{DMIC4, 8},
	{DMIC5, 9},
};

static int swr_master_ch_map[] = {
	ZERO,
	SWRM_TX1_CH1,
	SWRM_TX1_CH2,
	SWRM_TX1_CH3,
	SWRM_TX1_CH4,
	SWRM_TX2_CH1,
	SWRM_TX2_CH2,
	SWRM_TX2_CH3,
	SWRM_TX2_CH4,
	SWRM_TX3_CH1,
	SWRM_TX3_CH2,
	SWRM_TX3_CH3,
	SWRM_TX3_CH4,
	SWRM_PCM_IN,
};

#ifdef CONFIG_SND_SOC_WCD937X
extern int wcd937x_info_create_codec_entry(struct snd_info_entry *codec_root,
				    struct snd_soc_component *component);

static inline int wcd937x_slave_get_master_ch_val(int ch)
{
	int i;

	for (i = 0; i < WCD937X_MAX_SLAVE_CH_TYPES; i++)
		if (ch == swr_master_ch_map[i])
			return i;
	return 0;
}

static inline int wcd937x_slave_get_master_ch(int idx)
{
	return swr_master_ch_map[idx];
}

static inline int wcd937x_slave_get_slave_ch_val(int ch)
{
	int i;

	for (i = 0; i < WCD937X_MAX_SLAVE_CH_TYPES; i++)
		if (ch == swr_slv_tx_ch_idx[i].ch_type)
			return swr_slv_tx_ch_idx[i].index;

	return -EINVAL;
}
#else
extern int wcd937x_info_create_codec_entry(struct snd_info_entry *codec_root,
				    struct snd_soc_component *component)
{
	return 0;
}
static inline int wcd937x_slave_get_master_ch_val(int ch)
{
	return 0;
}
static inline int wcd937x_slave_get_master_ch(int idx)
{
	return 0;
}
static inline int wcd937x_slave_get_slave_ch_val(int ch)
{
	return 0;
}
#endif /* CONFIG_SND_SOC_WCD937X */

#endif
