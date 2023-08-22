/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/net/openthread.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/settings/settings.h>
#include <zephyr/drivers/gpio.h>

#include <openthread/platform/logging.h>
#include <openthread/instance.h>
#include <openthread/thread.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>

LOG_MODULE_REGISTER(main, 4);

static const struct gpio_dt_spec led_yellow = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
void ledHandler(struct k_timer *dummy);
void ledWorkHandler(struct k_work *work);
static K_TIMER_DEFINE(ledTimer, ledHandler, NULL);
static K_WORK_DEFINE(ledWork, ledWorkHandler);

void ledHandler(struct k_timer *dummy)
{
	k_work_submit(&ledWork);
}

void ledWorkHandler(struct k_work *work)
{
	gpio_pin_toggle_dt(&led_yellow);
    k_timer_start(&ledTimer, K_SECONDS(1), K_NO_WAIT);
}


int8_t datahex(char* string, uint8_t *data, int8_t len) 
{
    if(string == NULL) 
       return -1;

    // Count colons
    int colons = 0;
    size_t index = 0;
    for (index = 0, colons=0; string[index] > 0; index++)
        if(string[index] == ':')
          colons++;

    size_t slength = strlen(string);

    if( ((slength-colons) % 2) != 0) // must be even
       return -1;

    if( (slength - colons)/2 > len)
      return -1;

    memset(data, 0, len);

    index = 0;
    size_t dindex = 0;
    while (index < slength) {
        char c = string[index];
        int value = 0;
        if(c >= '0' && c <= '9')
          value = (c - '0');
        else if (c >= 'A' && c <= 'F') 
          value = (10 + (c - 'A'));
        else if (c >= 'a' && c <= 'f')
          value = (10 + (c - 'a'));
        else if (c == ':') {
            index++;
            continue;
        }
        else {
          return -1;
        }

        data[(dindex/2)] += value << (((dindex + 1) % 2) * 4);

        index++;
        dindex++;
    }

    return 1+dindex;
}

struct bt_scan_init_param p_scan_init = {
	.connect_if_match = 1,
	.scan_param = NULL,
	.conn_param = BT_LE_CONN_PARAM_DEFAULT
};

static void scan_init(void)
{
	bt_scan_init(&p_scan_init);
//	bt_scan_cb_register(&scan_cb);

#if 0
	int err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, BT_UUID_LNS);
	if (err) {
		LOG_WRN("Scanning filters cannot be set (err %d)", err);

		return;
	}

	err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
	if (err) {
		LOG_WRN("Filters cannot be turned on (err %d)", err);
	}
#endif
}

int main(void)
{
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_shell_uart), zephyr_cdc_acm_uart)
	int ret;
	const struct device *dev;
	uint32_t dtr = 0U;

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}

	dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	if (dev == NULL) {
		LOG_ERR("Failed to find specific UART device");
		return 0;
	}

#if defined(CONFIG_WAIT_FOR_CLI_CONNECTION)
	LOG_INF("Waiting for host to be ready to communicate");

	/* Data Terminal Ready - check if host is ready to communicate */
	while (!dtr) {
		ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		if (ret) {
			LOG_ERR("Failed to get Data Terminal Ready line state: %d",
				ret);
			continue;
		}
		k_msleep(100);
	}
#endif

	/* Data Carrier Detect Modem - mark connection as established */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DCD, 1);
	/* Data Set Ready - the NCP SoC is ready to communicate */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DSR, 1);
#endif

	LOG_INF("Starting MLE Test");

	otInstance *instance;
    otError error = OT_ERROR_NONE;

	// Setup Thread Mesh
    instance = openthread_get_default_instance();

    otExtendedPanId extendedPanid;
    otNetworkKey masterKey;

	// Set default network settings

    // Set network name
    LOG_INF("Setting Network Name to %s", CONFIG_OPENTHREAD_NETWORK_NAME);
    error = otThreadSetNetworkName(instance, CONFIG_OPENTHREAD_NETWORK_NAME);
    // Set PANID
    LOG_INF("Setting PANID to 0x%04X", (uint16_t)CONFIG_OPENTHREAD_WORKING_PANID);
    error = otLinkSetPanId(instance, (const otPanId)CONFIG_OPENTHREAD_WORKING_PANID);
    // Set extended PANID
    LOG_INF("Setting extended PANID to %s", CONFIG_OPENTHREAD_XPANID);
	datahex(CONFIG_OPENTHREAD_XPANID, &extendedPanid.m8[0], 8);
	error = otThreadSetExtendedPanId(instance, (const otExtendedPanId *)&extendedPanid);
    // Set channel if configured
	if(CONFIG_OPENTHREAD_CHANNEL > 0)
	{
	    LOG_INF("Setting Channel to %d", CONFIG_OPENTHREAD_CHANNEL);
    	error = otLinkSetChannel(instance, CONFIG_OPENTHREAD_CHANNEL);
	}
    // Set masterkey
    LOG_INF("Setting Network Key to %s", CONFIG_OPENTHREAD_NETWORKKEY);
	datahex(CONFIG_OPENTHREAD_NETWORKKEY, &masterKey.m8[0], 16);
    error = otThreadSetNetworkKey(instance, (const otNetworkKey *)&masterKey);

    // Start thread network
#ifdef OPENTHREAD_CONFIG_IP6_SLAAC_ENABLE
    otIp6SetSlaacEnabled(instance, true);
#endif
    error = otIp6SetEnabled(instance, true);
    error = otThreadSetEnabled(instance, true);

	// Start Bluetooth
    int err = bt_enable(NULL);
	if (err) {
		LOG_WRN("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	scan_init();

	LOG_INF("Bluetooth start scan");
	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		LOG_WRN("Scanning failed to start (err %d)", err);
		return 0;
	}

	LOG_INF("Scanning successfully started\n");

	// Toggle yellow LED on dongle so we can see it's working
	gpio_pin_configure_dt(&led_yellow, GPIO_OUTPUT_ACTIVE);
    k_timer_start(&ledTimer, K_SECONDS(1), K_NO_WAIT);

	return 0;
}
