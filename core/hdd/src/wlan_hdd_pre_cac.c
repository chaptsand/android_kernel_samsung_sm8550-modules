/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "osif_vdev_sync.h"
#include "wlan_hdd_hostapd.h"
#include "wlan_hdd_pre_cac.h"
#include <qdf_types.h>
#include "osif_pre_cac.h"
#include "wlan_pre_cac_ucfg_api.h"
#include "wlan_ipa_ucfg_api.h"

void hdd_send_conditional_chan_switch_status(struct hdd_context *hdd_ctx,
					     struct wireless_dev *wdev,
					     bool status)
{
	struct sk_buff *event;

	hdd_enter_dev(wdev->netdev);

	if (!hdd_ctx) {
		hdd_err("Invalid HDD context pointer");
		return;
	}

	event = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
		  wdev, sizeof(uint32_t) + NLMSG_HDRLEN,
		  QCA_NL80211_VENDOR_SUBCMD_SAP_CONDITIONAL_CHAN_SWITCH_INDEX,
		  GFP_KERNEL);
	if (!event) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	if (nla_put_u32(event,
			QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_STATUS,
			status)) {
		hdd_err("nla put failed");
		kfree_skb(event);
		return;
	}

	cfg80211_vendor_event(event, GFP_KERNEL);
}

/*
 * Code under PRE_CAC_COMP will be cleaned up
 * once pre cac component is done
 */
#ifndef PRE_CAC_COMP
/**
 * wlan_hdd_set_pre_cac_complete_status() - Set pre cac complete status
 * @ap_adapter: AP adapter
 * @status: Status which can be true or false
 *
 * Sets the status of pre cac i.e., whether it is complete or not
 *
 * Return: Zero on success, non-zero on failure
 */
static int wlan_hdd_set_pre_cac_complete_status(struct hdd_adapter *ap_adapter,
						bool status)
{
	QDF_STATUS ret;

	ret = wlan_sap_set_pre_cac_complete_status(
			WLAN_HDD_GET_SAP_CTX_PTR(ap_adapter), status);
	if (QDF_IS_STATUS_ERROR(ret))
		return -EINVAL;

	return 0;
}
#endif

/**
 * __wlan_hdd_sap_pre_cac_failure() - Process the pre cac failure
 * @adapter: AP adapter
 *
 * Deletes the pre cac adapter
 *
 * Return: None
 */
static void __wlan_hdd_sap_pre_cac_failure(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;

	hdd_enter();

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	hdd_stop_adapter(hdd_ctx, adapter);

	hdd_exit();
}

/**
 * wlan_hdd_sap_pre_cac_failure() - Process the pre cac failure
 * @data: AP adapter
 *
 * Deletes the pre cac adapter
 *
 * Return: None
 */
void wlan_hdd_sap_pre_cac_failure(void *data)
{
	struct hdd_adapter *adapter = data;
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_trans_start_wait(adapter->dev, &vdev_sync);
	if (errno)
		return;

	__wlan_hdd_sap_pre_cac_failure(data);

	osif_vdev_sync_trans_stop(vdev_sync);
}

/*
 * Code under PRE_CAC_COMP will be cleaned up
 * once pre cac component is done
 */
#ifndef PRE_CAC_COMP
/**
 * __wlan_hdd_sap_pre_cac_success() - Process the pre cac result
 * @adapter: AP adapter
 *
 * Stops the pre cac adapter and moves the existing SAP to the pre cac
 * channel
 *
 * Return: None
 */
static void __wlan_hdd_sap_pre_cac_success(struct hdd_adapter *adapter)
#else
static void wlan_hdd_pre_cac_success(struct hdd_adapter *adapter)
#endif
{
	struct hdd_adapter *ap_adapter;
	int i;
	struct hdd_context *hdd_ctx;
	enum phy_ch_width pre_cac_ch_width;
	qdf_freq_t chan_freq;

	hdd_enter();

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("HDD context is null");
		return;
	}

	pre_cac_ch_width = wlansap_get_chan_width(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter));

	hdd_stop_adapter(hdd_ctx, adapter);

	/* Prepare to switch AP from 2.4GHz channel to the pre CAC channel */
	ap_adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	if (!ap_adapter) {
		hdd_err("failed to get SAP adapter, no restart on pre CAC channel");
		return;
	}
/*
 * Code under PRE_CAC_COMP will be cleaned up
 * once pre cac component is done
 */
#ifndef PRE_CAC_COMP
	/*
	 * Setting of the pre cac complete status will ensure that on channel
	 * switch to the pre CAC DFS channel, there is no CAC again.
	 */
	wlan_hdd_set_pre_cac_complete_status(ap_adapter, true);

	wlan_hdd_set_sap_csa_reason(hdd_ctx->psoc, ap_adapter->vdev_id,
				    CSA_REASON_PRE_CAC_SUCCESS);
	chan_freq = ap_adapter->pre_cac_freq;
	i = hdd_softap_set_channel_change(ap_adapter->dev,
					  chan_freq,
					  pre_cac_ch_width, false);
	if (i) {
		hdd_err("failed to change channel");
		wlan_hdd_set_pre_cac_complete_status(ap_adapter, false);
	}
#else
	/*
	 * Setting of the pre cac complete status will ensure that on channel
	 * switch to the pre CAC DFS channel, there is no CAC again.
	 */
	ucfg_pre_cac_complete_set(ap_adapter->vdev, true);

	wlan_hdd_set_sap_csa_reason(hdd_ctx->psoc, ap_adapter->vdev_id,
				    CSA_REASON_PRE_CAC_SUCCESS);
	chan_freq = ucfg_pre_cac_get_freq(ap_adapter->vdev);
	i = hdd_softap_set_channel_change(ap_adapter->dev,
					  chan_freq,
					  pre_cac_ch_width, false);
	if (i) {
		hdd_err("failed to change channel");
		ucfg_pre_cac_complete_set(ap_adapter->vdev, false);
	}
#endif

	hdd_exit();
}

/*
 * Code under PRE_CAC_COMP will be cleaned up
 * once pre cac component is done
 */
#ifndef PRE_CAC_COMP
void wlan_hdd_sap_pre_cac_success(void *data)
{
	struct hdd_adapter *adapter = data;
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_trans_start_wait(adapter->dev, &vdev_sync);
	if (errno)
		return;

	__wlan_hdd_sap_pre_cac_success(adapter);

	osif_vdev_sync_trans_stop(vdev_sync);
}
#endif /* PRE_CAC_COMP */

void hdd_close_pre_cac_adapter(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *pre_cac_adapter;
	struct osif_vdev_sync *vdev_sync;
	int errno;

	pre_cac_adapter = hdd_get_adapter_by_iface_name(hdd_ctx,
							SAP_PRE_CAC_IFNAME);
	if (!pre_cac_adapter)
		return;

	errno = osif_vdev_sync_trans_start_wait(pre_cac_adapter->dev,
						&vdev_sync);
	if (errno)
		return;

	osif_vdev_sync_unregister(pre_cac_adapter->dev);
	osif_vdev_sync_wait_for_ops(vdev_sync);

	wlan_hdd_release_intf_addr(hdd_ctx, pre_cac_adapter->mac_addr.bytes);
	hdd_close_adapter(hdd_ctx, pre_cac_adapter, true);

	osif_vdev_sync_trans_stop(vdev_sync);
	osif_vdev_sync_destroy(vdev_sync);
}

void hdd_clean_up_pre_cac_interface(struct hdd_context *hdd_ctx)
{
/*
 * Code under PRE_CAC_COMP will be cleaned up
 * once pre cac component is done
 */
#ifndef PRE_CAC_COMP
	uint8_t vdev_id;
	QDF_STATUS status;
	struct hdd_adapter *precac_adapter;

	status = wlan_sap_get_pre_cac_vdev_id(hdd_ctx->mac_handle, &vdev_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed to get pre cac vdev id");
		return;
	}

	precac_adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!precac_adapter) {
		hdd_err("invalid pre cac adapter");
		return;
	}

	qdf_create_work(0, &hdd_ctx->sap_pre_cac_work,
			wlan_hdd_sap_pre_cac_failure,
			(void *)precac_adapter);
	qdf_sched_work(0, &hdd_ctx->sap_pre_cac_work);
#endif
}

/*
 * Code under PRE_CAC_COMP will be cleaned up
 * once pre cac component is done
 */
#ifndef PRE_CAC_COMP
/**
 * wlan_hdd_set_chan_freq_before_pre_cac() - Save the channel before pre cac
 * @ap_adapter: AP adapter
 * @freq_before_pre_cac: Channel
 *
 * Saves the channel frequency which the AP was beaconing on before moving to
 * the pre cac channel. If radar is detected on the pre cac channel, this saved
 * channel will be used for AP operations.
 *
 * Return: Zero on success, non-zero on failure
 */
static int
wlan_hdd_set_chan_freq_before_pre_cac(struct hdd_adapter *ap_adapter,
				      qdf_freq_t freq_before_pre_cac)
{
	QDF_STATUS ret;
	struct sap_context *sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(ap_adapter);

	ret = wlan_sap_set_chan_freq_before_pre_cac(sap_ctx,
						    freq_before_pre_cac);
	if (QDF_IS_STATUS_ERROR(ret))
		return -EINVAL;

	return 0;
}
#endif

static int wlan_set_def_pre_cac_chan(struct hdd_context *hdd_ctx,
				     uint32_t pre_cac_ch_freq,
				     struct cfg80211_chan_def *chandef,
				     enum nl80211_channel_type *chantype,
				     enum phy_ch_width *ch_width)
{
	enum nl80211_channel_type channel_type;
	struct ieee80211_channel *ieee_chan;
	struct ch_params ch_params = {0};

	ieee_chan = ieee80211_get_channel(hdd_ctx->wiphy,
					  pre_cac_ch_freq);
	if (!ieee_chan) {
		hdd_err("channel converion failed %d", pre_cac_ch_freq);
		return -EINVAL;
	}
	ch_params.ch_width = *ch_width;
	wlan_reg_set_channel_params_for_freq(hdd_ctx->pdev,
					     pre_cac_ch_freq, 0,
					     &ch_params);
	switch (ch_params.sec_ch_offset) {
	case HIGH_PRIMARY_CH:
		channel_type = NL80211_CHAN_HT40MINUS;
		break;
	case LOW_PRIMARY_CH:
		channel_type = NL80211_CHAN_HT40PLUS;
		break;
	default:
		channel_type = NL80211_CHAN_HT20;
		break;
	}
	cfg80211_chandef_create(chandef, ieee_chan, channel_type);
	switch (ch_params.ch_width) {
	case CH_WIDTH_80MHZ:
		chandef->width = NL80211_CHAN_WIDTH_80;
		break;
	case CH_WIDTH_80P80MHZ:
		chandef->width = NL80211_CHAN_WIDTH_80P80;
		if (ch_params.mhz_freq_seg1)
			chandef->center_freq2 = ch_params.mhz_freq_seg1;
		break;
	case CH_WIDTH_160MHZ:
		chandef->width = NL80211_CHAN_WIDTH_160;
		break;
	default:
		break;
	}
	if (ch_params.ch_width == CH_WIDTH_80MHZ ||
	    ch_params.ch_width == CH_WIDTH_80P80MHZ ||
	    ch_params.ch_width == CH_WIDTH_160MHZ) {
		if (ch_params.mhz_freq_seg0)
			chandef->center_freq1 = ch_params.mhz_freq_seg0;
	}
	*chantype = channel_type;
	*ch_width = ch_params.ch_width;
	hdd_debug("pre cac ch def: chan:%d width:%d freq1:%d freq2:%d",
		  chandef->chan->center_freq, chandef->width,
		  chandef->center_freq1, chandef->center_freq2);

	return 0;
}

/*
 * Code under PRE_CAC_COMP will be cleaned up
 * once pre cac component is done
 */
#ifndef PRE_CAC_COMP
/**
 * wlan_hdd_validate_and_get_pre_cac_ch() - Validate and get pre cac channel
 * @hdd_ctx: HDD context
 * @ap_adapter: AP adapter
 * @chan_freq: Channel frequency requested by userspace
 * @pre_cac_chan_freq: Pointer to the pre CAC channel frequency storage
 *
 * Validates the channel provided by userspace. If user provided channel 0,
 * a valid outdoor channel must be selected from the regulatory channel.
 *
 * Return: Zero on success and non zero value on error
 */
static int wlan_hdd_validate_and_get_pre_cac_ch(struct hdd_context *hdd_ctx,
						struct hdd_adapter *ap_adapter,
						uint32_t chan_freq,
						uint32_t *pre_cac_chan_freq)
{
	uint32_t i;
	QDF_STATUS status;
	uint32_t weight_len = 0;
	uint32_t len = CFG_VALID_CHANNEL_LIST_LEN;
	uint32_t freq_list[NUM_CHANNELS] = {0};
	uint8_t pcl_weights[NUM_CHANNELS] = {0};
	mac_handle_t mac_handle;

	if (!chan_freq) {
		/* Channel is not obtained from PCL because PCL may not have
		 * the entire channel list. For example: if SAP is up on
		 * channel 6 and PCL is queried for the next SAP interface,
		 * if SCC is preferred, the PCL will contain only the channel
		 * 6. But, we are in need of a DFS channel. So, going with the
		 * first channel from the valid channel list.
		 */
		status = policy_mgr_get_valid_chans(hdd_ctx->psoc,
						    freq_list, &len);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to get channel list");
			return -EINVAL;
		}
		policy_mgr_update_with_safe_channel_list(hdd_ctx->psoc,
							 freq_list, &len,
							 pcl_weights,
							 weight_len);
		for (i = 0; i < len; i++) {
			if (wlan_reg_is_dfs_for_freq(hdd_ctx->pdev,
						     freq_list[i])) {
				*pre_cac_chan_freq = freq_list[i];
				break;
			}
		}
		if (*pre_cac_chan_freq == 0) {
			hdd_err("unable to find outdoor channel");
			return -EINVAL;
		}
	} else {
		/* Only when driver selects a channel, check is done for
		 * unnsafe and NOL channels. When user provides a fixed channel
		 * the user is expected to take care of this.
		 */
		mac_handle = hdd_ctx->mac_handle;
		if (!sme_is_channel_valid(mac_handle, chan_freq) ||
		    !wlan_reg_is_dfs_for_freq(hdd_ctx->pdev, chan_freq)) {
			hdd_err("Invalid channel for pre cac:%d", chan_freq);
			return -EINVAL;
		}
		*pre_cac_chan_freq = chan_freq;
	}
	hdd_debug("selected pre cac channel:%d", *pre_cac_chan_freq);
	return 0;
}

/**
 * wlan_hdd_set_pre_cac_status() - Set the pre cac status
 * @pre_cac_adapter: AP adapter used for pre cac
 * @status: Status (true or false)
 *
 * Sets the status of pre cac i.e., whether the pre cac is active or not
 *
 * Return: Zero on success, non-zero on failure
 */
static int wlan_hdd_set_pre_cac_status(struct hdd_adapter *pre_cac_adapter,
				       bool status)
{
	QDF_STATUS ret;

	ret = wlan_sap_set_pre_cac_status(
		WLAN_HDD_GET_SAP_CTX_PTR(pre_cac_adapter), status);
	if (QDF_IS_STATUS_ERROR(ret))
		return -EINVAL;

	return 0;
}
#endif

/**
 * __wlan_hdd_request_pre_cac() - Start pre CAC in the driver
 * @hdd_ctx: the HDD context to operate against
 * @chan_freq: Channel frequency option provided by userspace
 * @out_adapter: out parameter for the newly created pre-cac adapter
 *
 * Sets the driver to the required hardware mode and start an adapter for
 * pre CAC which will mimic an AP.
 *
 * Return: Zero on success, non-zero value on error
 */
static int __wlan_hdd_request_pre_cac(struct hdd_context *hdd_ctx,
				      uint32_t chan_freq,
				      struct hdd_adapter **out_adapter)
{
	uint8_t *mac_addr = NULL;
	uint32_t pre_cac_chan_freq = 0;
	int ret;
	struct hdd_adapter *ap_adapter, *pre_cac_adapter;
	struct hdd_ap_ctx *hdd_ap_ctx;
	QDF_STATUS status;
	struct wiphy *wiphy;
	struct net_device *dev;
	struct cfg80211_chan_def chandef;
	enum nl80211_channel_type channel_type;
	mac_handle_t mac_handle;
/*
 * Code under PRE_CAC_COMP will be cleaned up
 * once pre cac component is done
 */
#ifndef PRE_CAC_COMP
	bool val;
	qdf_freq_t freq;
#endif
	enum phy_ch_width cac_ch_width;
	struct hdd_adapter_create_param params = {0};

	if (!policy_mgr_is_hw_dbs_capable(hdd_ctx->psoc)) {
		hdd_debug("Pre CAC is not supported on non-dbs platforms");
		return -EINVAL;
	}

	pre_cac_adapter = hdd_get_adapter_by_iface_name(hdd_ctx,
							SAP_PRE_CAC_IFNAME);
/*
 * Code under PRE_CAC_COMP will be cleaned up
 * once pre cac component is done
 */
#ifndef PRE_CAC_COMP
	if (pre_cac_adapter) {
		/* Flush existing pre_cac work */
		if (hdd_ctx->sap_pre_cac_work.fn)
			cds_flush_work(&hdd_ctx->sap_pre_cac_work);
	} else {
		if (policy_mgr_get_connection_count(hdd_ctx->psoc) > 1) {
			hdd_err("pre cac not allowed in concurrency");
			return -EINVAL;
		}
	}
#else
	if (!pre_cac_adapter &&
	    (policy_mgr_get_connection_count(hdd_ctx->psoc) > 1)) {
		hdd_err("pre cac not allowed in concurrency");
		return -EINVAL;
	}
#endif

	ap_adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	if (!ap_adapter) {
		hdd_err("unable to get SAP adapter");
		return -EINVAL;
	}

	if (qdf_atomic_read(&ap_adapter->ch_switch_in_progress)) {
		hdd_err("pre cac not allowed during CSA");
		return -EINVAL;
	}

	mac_handle = hdd_ctx->mac_handle;
/*
 * Code under PRE_CAC_COMP will be cleaned up
 * once pre cac component is done
 */
#ifndef PRE_CAC_COMP
	val = wlan_sap_is_pre_cac_active(mac_handle);
	if (val) {
		hdd_err("pre cac is already in progress");
		return -EINVAL;
	}
#endif

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	if (!hdd_ap_ctx) {
		hdd_err("SAP context is NULL");
		return -EINVAL;
	}

	if (wlan_reg_is_dfs_for_freq(hdd_ctx->pdev,
				     hdd_ap_ctx->operating_chan_freq)) {
		hdd_err("SAP is already on DFS channel:%d",
			hdd_ap_ctx->operating_chan_freq);
		return -EINVAL;
	}

	if (!WLAN_REG_IS_24GHZ_CH_FREQ(hdd_ap_ctx->operating_chan_freq)) {
		hdd_err("pre CAC alllowed only when SAP is in 2.4GHz:%d",
			hdd_ap_ctx->operating_chan_freq);
		return -EINVAL;
	}

	hdd_debug("channel: %d", chan_freq);

/*
 * Code under PRE_CAC_COMP will be cleaned up
 * once pre cac component is done
 */
#ifndef PRE_CAC_COMP
	ret = wlan_hdd_validate_and_get_pre_cac_ch(
		hdd_ctx, ap_adapter, chan_freq, &pre_cac_chan_freq);
#else
	ret = ucfg_pre_cac_validate_and_get_freq(hdd_ctx->pdev, chan_freq,
						 &pre_cac_chan_freq);
#endif
	if (ret != 0) {
		hdd_err("can't validate pre-cac channel");
		goto release_intf_addr_and_return_failure;
	}

	hdd_debug("starting pre cac SAP  adapter");

	if (!pre_cac_adapter) {
		mac_addr = wlan_hdd_get_intf_addr(hdd_ctx, QDF_SAP_MODE);
		if (!mac_addr) {
			hdd_err("can't add virtual intf: Not getting valid mac addr");
			return -EINVAL;
		}

		/**
		 * Starting a SAP adapter:
		 * Instead of opening an adapter, we could just do a SME open
		 * session for AP type. But, start BSS would still need an
		 * adapter. So, this option is not taken.
		 *
		 * hdd open adapter is going to register this precac interface
		 * with user space. This interface though exposed to user space
		 * will be in DOWN state. Consideration was done to avoid this
		 * registration to the user space. But, as part of SAP
		 * operations multiple events are sent to user space. Some of
		 * these events received from unregistered interface was
		 * causing crashes. So, retaining the registration.
		 *
		 * So, this interface would remain registered and will remain
		 * in DOWN state for the CAC duration. We will add notes in the
		 * feature announcement to not use this temporary interface for
		 * any activity from user space.
		 */
		params.is_add_virtual_iface = 1;
		pre_cac_adapter = hdd_open_adapter(hdd_ctx, QDF_SAP_MODE,
						   SAP_PRE_CAC_IFNAME, mac_addr,
						   NET_NAME_UNKNOWN, true,
						   &params);

		if (!pre_cac_adapter) {
			hdd_err("error opening the pre cac adapter");
			goto release_intf_addr_and_return_failure;
		}
	}

	sap_clear_global_dfs_param(mac_handle,
				   WLAN_HDD_GET_SAP_CTX_PTR(pre_cac_adapter));

	/*
	 * This interface is internally created by the driver. So, no interface
	 * up comes for this interface from user space and hence starting
	 * the adapter internally.
	 */
	if (hdd_start_adapter(pre_cac_adapter)) {
		hdd_err("error starting the pre cac adapter");
		goto close_pre_cac_adapter;
	}

	hdd_debug("preparing for start ap/bss on the pre cac adapter");

	wiphy = hdd_ctx->wiphy;
	dev = pre_cac_adapter->dev;

	/* Since this is only a dummy interface lets us use the IEs from the
	 * other active SAP interface. In regular scenarios, these IEs would
	 * come from the user space entity
	 */
	pre_cac_adapter->session.ap.beacon = qdf_mem_malloc(
			sizeof(*ap_adapter->session.ap.beacon));
	if (!pre_cac_adapter->session.ap.beacon)
		goto stop_close_pre_cac_adapter;

	qdf_mem_copy(pre_cac_adapter->session.ap.beacon,
		     ap_adapter->session.ap.beacon,
		     sizeof(*pre_cac_adapter->session.ap.beacon));
	pre_cac_adapter->session.ap.sap_config.ch_width_orig =
			ap_adapter->session.ap.sap_config.ch_width_orig;
	pre_cac_adapter->session.ap.sap_config.authType =
			ap_adapter->session.ap.sap_config.authType;

	/* The orginal premise is that on moving from 2.4GHz to 5GHz, the SAP
	 * will continue to operate on the same bandwidth as that of the 2.4GHz
	 * operations. Only bandwidths 20MHz/40MHz are possible on 2.4GHz band.
	 * Now some customer request to start AP on higher BW such as 80Mhz.
	 * Hence use max possible supported BW based on phymode configurated
	 * on SAP.
	 */
	cac_ch_width = wlansap_get_max_bw_by_phymode(
			WLAN_HDD_GET_SAP_CTX_PTR(ap_adapter));
	if (cac_ch_width > DEFAULT_PRE_CAC_BANDWIDTH)
		cac_ch_width = DEFAULT_PRE_CAC_BANDWIDTH;

	qdf_mem_zero(&chandef, sizeof(struct cfg80211_chan_def));
	if (wlan_set_def_pre_cac_chan(hdd_ctx, pre_cac_chan_freq,
				      &chandef, &channel_type,
				      &cac_ch_width)) {
		hdd_err("error set pre_cac channel %d", pre_cac_chan_freq);
		goto close_pre_cac_adapter;
	}
	pre_cac_adapter->session.ap.sap_config.ch_width_orig =
					hdd_map_nl_chan_width(chandef.width);

	hdd_debug("existing ap phymode:%d pre cac ch_width:%d freq:%d",
		  ap_adapter->session.ap.sap_config.SapHw_mode,
		  cac_ch_width, pre_cac_chan_freq);
	/*
	 * Doing update after opening and starting pre-cac adapter will make
	 * sure that driver won't do hardware mode change if there are any
	 * initial hick-ups or issues in pre-cac adapter's configuration.
	 * Since current SAP is in 2.4GHz and pre CAC channel is in 5GHz, this
	 * connection update should result in DBS mode
	 */
	status = policy_mgr_update_and_wait_for_connection_update(
			hdd_ctx->psoc, ap_adapter->vdev_id, pre_cac_chan_freq,
			POLICY_MGR_UPDATE_REASON_PRE_CAC);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("error in moving to DBS mode");
		goto stop_close_pre_cac_adapter;
	}

	ret = wlan_hdd_set_channel(wiphy, dev, &chandef, channel_type);
	if (ret != 0) {
		hdd_err("failed to set channel");
		goto stop_close_pre_cac_adapter;
	}

	status = wlan_hdd_cfg80211_start_bss(pre_cac_adapter, NULL,
					     PRE_CAC_SSID,
					     qdf_str_len(PRE_CAC_SSID),
					     NL80211_HIDDEN_SSID_NOT_IN_USE,
					     false);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("start bss failed");
		goto stop_close_pre_cac_adapter;
	}

	/*
	 * The pre cac status is set here. But, it would not be reset explicitly
	 * anywhere, since after the pre cac success/failure, the pre cac
	 * adapter itself would be removed.
	 */
/*
 * Code under PRE_CAC_COMP will be cleaned up
 * once pre cac component is done
 */
#ifndef PRE_CAC_COMP
	ret = wlan_hdd_set_pre_cac_status(pre_cac_adapter, true);
	if (ret != 0) {
		hdd_err("failed to set pre cac status");
		goto stop_close_pre_cac_adapter;
	}

	freq = hdd_ap_ctx->operating_chan_freq;
	ret = wlan_hdd_set_chan_freq_before_pre_cac(ap_adapter,
						    freq);
	if (ret != 0) {
		hdd_err("failed to set channel before pre cac");
		goto stop_close_pre_cac_adapter;
	}

	ap_adapter->pre_cac_freq = pre_cac_chan_freq;
	pre_cac_adapter->is_pre_cac_adapter = true;
#else
	ret = ucfg_pre_cac_set_status(pre_cac_adapter->vdev, true);
	if (ret != 0) {
		hdd_err("failed to set pre cac status");
		goto stop_close_pre_cac_adapter;
	}

	ucfg_pre_cac_set_freq_before_pre_cac(ap_adapter->vdev,
					     hdd_ap_ctx->operating_chan_freq);
	ucfg_pre_cac_set_freq(ap_adapter->vdev, pre_cac_chan_freq);
	ucfg_pre_cac_adapter_set(pre_cac_adapter->vdev, true);
#endif
	*out_adapter = pre_cac_adapter;

	return 0;

stop_close_pre_cac_adapter:
	hdd_stop_adapter(hdd_ctx, pre_cac_adapter);
	qdf_mem_free(pre_cac_adapter->session.ap.beacon);
	pre_cac_adapter->session.ap.beacon = NULL;
close_pre_cac_adapter:
	hdd_close_adapter(hdd_ctx, pre_cac_adapter, false);
release_intf_addr_and_return_failure:
	/*
	 * Release the interface address as the adapter
	 * failed to start, if you don't release then next
	 * adapter which is trying to come wouldn't get valid
	 * mac address. Remember we have limited pool of mac addresses
	 */
	if (mac_addr)
		wlan_hdd_release_intf_addr(hdd_ctx, mac_addr);
	return -EINVAL;
}

static int
wlan_hdd_start_pre_cac_trans(struct hdd_context *hdd_ctx,
			     struct osif_vdev_sync **out_vdev_sync,
			     bool *is_vdev_sync_created)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_START_PRE_CAC_TRANS;
	int errno;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (!qdf_str_cmp(adapter->dev->name, SAP_PRE_CAC_IFNAME)) {
			errno = osif_vdev_sync_trans_start(adapter->dev,
							   out_vdev_sync);

			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return errno;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	errno = osif_vdev_sync_create_and_trans(hdd_ctx->parent_dev,
						out_vdev_sync);
	if (errno)
		return errno;

	*is_vdev_sync_created = true;
	return 0;
}

int wlan_hdd_request_pre_cac(struct hdd_context *hdd_ctx, uint32_t chan_freq)
{
	struct hdd_adapter *adapter;
	struct osif_vdev_sync *vdev_sync;
	int errno;
	bool is_vdev_sync_created = false;

	errno = wlan_hdd_start_pre_cac_trans(hdd_ctx, &vdev_sync,
					     &is_vdev_sync_created);
	if (errno)
		return errno;

	errno = __wlan_hdd_request_pre_cac(hdd_ctx, chan_freq, &adapter);
	if (errno)
		goto destroy_sync;

	if (is_vdev_sync_created)
		osif_vdev_sync_register(adapter->dev, vdev_sync);
	osif_vdev_sync_trans_stop(vdev_sync);

	return 0;

destroy_sync:
	osif_vdev_sync_trans_stop(vdev_sync);
	if (is_vdev_sync_created)
		osif_vdev_sync_destroy(vdev_sync);

	return errno;
}

/*
 * Code under PRE_CAC_COMP will be cleaned up
 * once pre cac component is done
 */
#ifdef PRE_CAC_COMP
static void
wlan_hdd_pre_cac_conditional_freq_switch_ind(struct wlan_objmgr_vdev *vdev,
					     bool completed)
{
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	struct hdd_adapter *adapter;
	struct hdd_ap_ctx *ap_ctx;

	if (completed) {
		uint8_t vdev_id = vdev->vdev_objmgr.vdev_id;

		adapter = wlan_hdd_get_adapter_from_vdev(psoc, vdev_id);
		if (!adapter) {
			hdd_err("Invalid adapter");
			return;
		}

		ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);
		ap_ctx->dfs_cac_block_tx = false;
		ucfg_ipa_set_dfs_cac_tx(adapter->hdd_ctx->pdev,
					ap_ctx->dfs_cac_block_tx);
		adapter->hdd_ctx->dev_dfs_cac_status = DFS_CAC_ALREADY_DONE;
	}
}

static void
wlan_hdd_pre_cac_complete(struct wlan_objmgr_vdev *vdev,
			  QDF_STATUS status)
{
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	uint8_t vdev_id = vdev->vdev_objmgr.vdev_id;
	struct hdd_adapter *adapter;

	adapter = wlan_hdd_get_adapter_from_vdev(psoc, vdev_id);
	if (!adapter) {
		hdd_err("Invalid adapter");
		return;
	}

	wlan_hdd_pre_cac_success(adapter);
}

struct osif_pre_cac_legacy_ops pre_cac_legacy_ops = {
	.conditional_csa_ind_legacy_cb =
		wlan_hdd_pre_cac_conditional_freq_switch_ind,
	.pre_cac_complete_legacy_cb = wlan_hdd_pre_cac_complete,
};

QDF_STATUS hdd_pre_cac_register_cb(void)
{
	osif_pre_cac_set_legacy_cb(&pre_cac_legacy_ops);

	return osif_pre_cac_register_cb();
}

void hdd_pre_cac_unregister_cb(void)
{
	osif_pre_cac_reset_legacy_cb();
}
#endif /* PRE_CAC_COMP */
