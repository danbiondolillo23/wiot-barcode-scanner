#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/drivers/uart.h> 
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>

#define MAX_BUFFER_SIZE 256 

// Define service and characteristic UUID's
#define BT_UUID_SCANNER_SERVICE BT_UUID_DECLARE_16(0xFF00)
#define BT_UUID_BARCODE_CHARACTERISTIC BT_UUID_DECLARE_16(0xFF01)

static void start_scan(void);

static struct bt_conn *default_conn;

static struct bt_uuid_16 discover_uuid[2] = {BT_UUID_INIT_16(0), BT_UUID_INIT_16(0)};
static struct bt_gatt_discover_params discover_params[2];
static struct bt_gatt_subscribe_params subscribe_params[2];


const struct device *uart;

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	switch (evt->type) {
	
	// Take action once transmission buffer is transmitted
	case UART_TX_DONE:
		printk("Transmission over UART complete!\n");
		break;
		
	default:
		printk("Entered default case\n");
		break;
	}
}

// Initialize uart device from the device tree
int uart_init() {
		int err;

    uart = DEVICE_DT_GET(DT_NODELABEL(uart0));

    if (!device_is_ready(uart)) {
			printk("UART initialization failed\n");
			return -1;
		}

		// Register the callback for UART events
		err = uart_callback_set(uart, uart_cb, NULL);
		if (err) {
			return err;
		}

		return 0;
}

// BLUETOOTH SPECIFIC CODE BELOW

// Callback for notification event
static uint8_t notify_func(struct bt_conn *conn,
                           struct bt_gatt_subscribe_params *params,
                           const void *data, uint16_t length)
{
    if (!data) {
        printk("[UNSUBSCRIBED]\n");
        params->value_handle = 0U;
        return BT_GATT_ITER_STOP;
    }

    const uint8_t* raw_byte_data = (const uint8_t*)data;
		const uint8_t* actual_data = &raw_byte_data[2];

    static uint8_t buffer[MAX_BUFFER_SIZE];
    if (length > MAX_BUFFER_SIZE) {
        printk("Error: Data length exceeds buffer size.\n");
        return BT_GATT_ITER_STOP;
    }

    memset(buffer, 0, MAX_BUFFER_SIZE);
    memcpy(buffer, actual_data, length-4);

    printk("\n[BARCODE READ] data address: %p, length: %u, raw bytes: ", data, length);
    for (int i = 0; i < length; i++) {
        printk("0x%02x ", raw_byte_data[i]);
    }
    printk("\n");

    // Transmit over UART
    int err = uart_tx(uart, actual_data, length-4, SYS_FOREVER_US);
    if (err) {
        printk("UART transmission failed (error code: %d)\n", err);
    }

    return BT_GATT_ITER_CONTINUE;
}

// Callback for service/characteristic/descriptor discovery
static uint8_t discover_func(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params)
{
	int err;

	if (!attr) {
		printk("Discover complete\n");
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	// Log the discovered attribute's handle and UUID declaration (service, characteristic, or descriptor)
	struct bt_uuid_16 *attr_uuid = (struct bt_uuid_16 *)attr->uuid;
	printk("Discovered Attribute Handle: %u, UUID: 0x%04x\n", attr->handle, sys_le16_to_cpu(attr_uuid->val));

	if (!bt_uuid_cmp(discover_params[1].uuid, BT_UUID_SCANNER_SERVICE)) {
		memcpy(&discover_uuid[1], BT_UUID_BARCODE_CHARACTERISTIC, sizeof(discover_uuid[1]));
		discover_params[1].uuid = &discover_uuid[1].uuid;
		discover_params[1].start_handle = attr->handle + 1;
		discover_params[1].type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params[1]);
		if (err) {
			printk("Discover failed (err %d)\n", err);
		}
	} else if (!bt_uuid_cmp(discover_params[1].uuid,
				BT_UUID_BARCODE_CHARACTERISTIC)) {
		memcpy(&discover_uuid[1], BT_UUID_GATT_CCC, sizeof(discover_uuid[1]));
		discover_params[1].uuid = &discover_uuid[1].uuid;
		discover_params[1].start_handle = attr->handle + 2;
		discover_params[1].type = BT_GATT_DISCOVER_DESCRIPTOR;
		subscribe_params[1].value_handle = bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &discover_params[1]);
		if (err) {
			printk("Discover failed (err %d)\n", err);
		}
	} else {
		subscribe_params[1].notify = notify_func;
		subscribe_params[1].value = BT_GATT_CCC_NOTIFY;
		subscribe_params[1].ccc_handle = attr->handle;

		err = bt_gatt_subscribe(conn, &subscribe_params[1]);
		if (err && err != -EALREADY) {
			printk("Subscribe failed (err %d)\n", err);
		} else {
			printk("[SUBSCRIBED]\n");
		}

		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_STOP;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{

	char dev[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, dev, sizeof(dev));

	// Look for name of our device and parse its advertisement data
	if (strcmp(dev, "AB:2B:00:02:C7:9D (public)") == 0) {

		// Print advertisement data
		printk("[DEVICE]: %s, AD evt type %u, AD data len %u, RSSI %i\n", dev, type, ad->len, rssi);
		printk("[DATA]: ");
		for (int i = 0; i < ad->len; i++) {
			printk("%x ", ad->data[i]);
		}
		printk("\n");

		// Stop scan
		int err;
		err = bt_le_scan_stop();
		if (err) {
			printk("Stop LE scan failed (err %d)\n", err);
		}

		// Create connection
		struct bt_le_conn_param *param;
		param = BT_LE_CONN_PARAM_DEFAULT;
		err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
					param, &default_conn);
		if (err) {
			printk("Create conn failed (err %d)\n", err);
			start_scan();
		}
	}
}

static void start_scan(void)
{
	int err;

	struct bt_le_scan_param scan_param = {
		.type       = BT_LE_SCAN_TYPE_ACTIVE,
		.options    = BT_LE_SCAN_OPT_NONE,
		.interval   = BT_GAP_SCAN_FAST_INTERVAL,
		.window     = BT_GAP_SCAN_FAST_WINDOW,
	};

	err = bt_le_scan_start(&scan_param, device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}

	printk("Scanning successfully started\n");
}

// Callback for when connection initiated
static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		printk("Failed to connect to %s (%u)\n", addr, conn_err);

		bt_conn_unref(default_conn);
		default_conn = NULL;

		start_scan();
		return;
	}

	printk("Connected: %s\n", addr);

	// Setup service discovery
	if (conn == default_conn) {

		// Pass UUID of scanner service to be discovered
		memcpy(&discover_uuid[1], BT_UUID_SCANNER_SERVICE, sizeof(discover_uuid[1]));
		discover_params[1].uuid = &discover_uuid[1].uuid;
		discover_params[1].func = discover_func;
		discover_params[1].start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
		discover_params[1].end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
		discover_params[1].type = BT_GATT_DISCOVER_PRIMARY;

		err = bt_gatt_discover(default_conn, &discover_params[1]);
		if (err) {
			printk("Discover failed(err %d)\n", err);
			return;
		}
	}
}

// Callback when disconnection initiated
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	start_scan();
}

// Define callbacks on connect/disconnect initiations
BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

// Run main function
int main(void)
{
	int err;

	err = uart_init();
	if(err) {
		printk("UART initialization failed\n");
	}

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	start_scan();
	return 0;
}