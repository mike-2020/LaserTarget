/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp_log.h"
#include "nvs_flash.h"
/* BLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "ble_spp_client.h"
#include "driver/uart.h"
#include "ble_net.h"


static const char *TAG = "BLE_NET";

#define PEER_ADDR_VAL_SIZE      6

static const char *tag = "NimBLE_SPP_BLE_CENT";
static int ble_spp_client_gap_event(struct ble_gap_event *event, void *arg);
void ble_store_config_init(void);
uint16_t attribute_handle[CONFIG_BT_NIMBLE_MAX_CONNECTIONS + 1];
static void ble_spp_client_scan(void);
//static ble_addr_t connected_addr[CONFIG_BT_NIMBLE_MAX_CONNECTIONS + 1];

void BLEMgr_recvData(uint8_t* data, size_t len, uint16_t conn_handle);
int BLEMgr_createDevNode(uint16_t conn_handle, uint8_t *addr, size_t len);
void BLEMgr_delDevNode(uint16_t conn_handle);
int BLEMgr_getDevUID(const uint8_t *addr, size_t len);


static void
ble_spp_client_set_handle(const struct peer *peer)
{
    const struct peer_chr *chr;
    chr = peer_chr_find_uuid(peer,
                             BLE_UUID16_DECLARE(GATT_SPP_SVC_UUID),
                             BLE_UUID16_DECLARE(GATT_SPP_CHR_UUID));
    if(chr==NULL) {
        ESP_LOGE(TAG, "Failed to find char from peer.");
        return;
    }
    attribute_handle[peer->conn_handle] = chr->chr.val_handle;
}

/**
 * Called when service discovery of the specified peer has completed.
 */
static void
ble_spp_client_on_disc_complete(const struct peer *peer, int status, void *arg)
{
    if (status != 0) {
        /* Service discovery failed.  Terminate the connection. */
        ESP_LOGE(TAG, "Error: Service discovery failed; status=%d "
                    "conn_handle=%d\n", status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        BLEMgr_delDevNode(peer->conn_handle);
        peer_delete(peer->conn_handle);
        return;
    }

    /* Service discovery has completed successfully.  Now we have a complete
     * list of services, characteristics, and descriptors that the peer
     * supports.
     */
    ESP_LOGI(TAG, "Service discovery complete; status=%d "
                "conn_handle=%d, CONFIG_BT_NIMBLE_MAX_CONNECTIONS=%d\n", status, peer->conn_handle, CONFIG_BT_NIMBLE_MAX_CONNECTIONS);

    ble_spp_client_set_handle(peer);
#if CONFIG_BT_NIMBLE_MAX_CONNECTIONS > 1
    ble_spp_client_scan();
#endif
}

/**
 * Initiates the GAP general discovery procedure.
 */
static void
ble_spp_client_scan(void)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 1;

    /**
     * Perform a passive scan.  I.e., don't send follow-up scan requests to
     * each advertiser.
     */
    disc_params.passive = 1;

    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                      ble_spp_client_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error initiating GAP discovery procedure; rc=%d\n",
                    rc);
    }
}

/**
 * Indicates whether we should try to connect to the sender of the specified
 * advertisement.  The function returns a positive result if the device
 * advertises connectability and support for the Alert Notification service.
 */
static int
ble_spp_client_should_connect(const struct ble_gap_disc_desc *disc)
{
    struct ble_hs_adv_fields fields;
    int rc;
    int i;

    /* Check if device is already connected or not */
    rc = BLEMgr_getDevUID(disc->addr.val, PEER_ADDR_VAL_SIZE);
    if(rc>0) {
        ESP_LOGW(TAG, "Device already connected");
        return 0;
    }

    /* The device has to be advertising connectability. */
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
            disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {
        //ESP_LOGI(TAG, "The device has no required advertising connectability: disc->event_type = 0x%x.", disc->event_type);
        return 0;
    }

    rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc != 0) {
        ESP_LOGI(TAG, "ble_hs_adv_parse_fields error: %d.", rc);
        return 0;
    }

    /* The device has to advertise support for the SPP
     * service (0xABF0).
     */
    for (i = 0; i < fields.num_uuids16; i++) {
        if (ble_uuid_u16(&fields.uuids16[i].u) == GATT_SPP_SVC_UUID) {
            return 1;
        }else{
            ESP_LOGI(TAG, "Device has no required service: 0x%x.", ble_uuid_u16(&fields.uuids16[i].u));
        }
    }
    return 0;
}

/**
 * Connects to the sender of the specified advertisement of it looks
 * interesting.  A device is "interesting" if it advertises connectability and
 * support for the Alert Notification service.
 */
static void
ble_spp_client_connect_if_interesting(const struct ble_gap_disc_desc *disc)
{
    uint8_t own_addr_type;
    int rc;

    /* Don't do anything if we don't care about this advertiser. */
    if (!ble_spp_client_should_connect(disc)) {
        //ESP_LOGI(TAG, "ble_spp_client_should_connect return false.");
        return;
    }

    /* Scanning must be stopped before a connection can be initiated. */
    rc = ble_gap_disc_cancel();
    if (rc != 0) {
        MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
        return;
    }

    /* Figure out address to use for connect (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for
     * timeout.
     */

    rc = ble_gap_connect(own_addr_type, &disc->addr, 30000, NULL,
                         ble_spp_client_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to connect to device; addr_type=%d "
                    "addr=%s; rc=%d\n",
                    disc->addr.type, addr_str(disc->addr.val), rc);
        return;
    }else{
        ESP_LOGI(TAG, "ble_gap_connect success.");
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.  ble_spp_client uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  ble_spp_client.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
ble_spp_client_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        //ESP_LOGI(TAG, "Got event BLE_GAP_EVENT_DISC.");
        rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                     event->disc.length_data);
        if (rc != 0) {
            ESP_LOGW(TAG, "Failed to parse fields: rc= %d.", rc);
            return 0;
        }

        if(fields.name!=NULL) ESP_LOGW(TAG, "fields.name=%s", fields.name);
        /* An advertisment report was received during GAP discovery. */
        print_adv_fields(&fields);

        /* Try to connect to the advertiser if it looks interesting. */
        ble_spp_client_connect_if_interesting(&event->disc);
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        if (event->connect.status == 0) {
            /* Connection successfully established. */
            MODLOG_DFLT(INFO, "Connection established ");
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            //memcpy(&connected_addr[event->connect.conn_handle].val, desc.peer_id_addr.val,
            //       PEER_ADDR_VAL_SIZE);
            print_conn_desc(&desc);
            MODLOG_DFLT(INFO, "\n");

            /* Remember peer. */
            rc = peer_add(event->connect.conn_handle);
            if (rc != 0) {
                MODLOG_DFLT(ERROR, "Failed to add peer; rc=%d\n", rc);
                return 0;
            }

            /* Perform service discovery. */
            rc = peer_disc_all(event->connect.conn_handle,
                               ble_spp_client_on_disc_complete, NULL);
            if (rc != 0) {
                MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n", rc);
                return 0;
            }

            BLEMgr_createDevNode(event->connect.conn_handle, desc.peer_id_addr.val, PEER_ADDR_VAL_SIZE);

        } else {
            /* Connection attempt failed; resume scanning. */
            MODLOG_DFLT(ERROR, "Error: Connection failed; status=%d\n",
                        event->connect.status);
            ble_spp_client_scan();
        }

        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated. */
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        BLEMgr_delDevNode(event->disconnect.conn.conn_handle);

        /* Forget about peer. */
        //memset(&connected_addr[event->disconnect.conn.conn_handle].val, 0, PEER_ADDR_VAL_SIZE);
        attribute_handle[event->disconnect.conn.conn_handle] = 0;
        peer_delete(event->disconnect.conn.conn_handle);

        /* Resume scanning. */
        ble_spp_client_scan();
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        MODLOG_DFLT(INFO, "discovery complete; reason=%d\n",
                    event->disc_complete.reason);
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        /* Peer sent us a notification or indication. */
        MODLOG_DFLT(INFO, "received %s; conn_handle=%d attr_handle=%d "
                    "attr_len=%d\n",
                    event->notify_rx.indication ?
                    "indication" :
                    "notification",
                    event->notify_rx.conn_handle,
                    event->notify_rx.attr_handle,
                    OS_MBUF_PKTLEN(event->notify_rx.om));

        /* Attribute data is contained in event->notify_rx.om. Use
         * `os_mbuf_copydata` to copy the data received in notification mbuf */
        size_t len = OS_MBUF_PKTLEN(event->notify_rx.om);
        uint8_t recvBuff[16];
        if(len > sizeof(recvBuff)) {
            ESP_LOGW(TAG, "Received data length exceeds data buffer size.");
            return -1;
        }
        os_mbuf_copydata(event->notify_rx.om, 0, len, recvBuff);
        BLEMgr_recvData(recvBuff, len, event->notify_rx.conn_handle);
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    default:
        return 0;
    }
}

static void
ble_spp_client_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
ble_spp_client_on_sync(void)
{
    int rc;

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Begin scanning for a peripheral to connect to. */
    ble_spp_client_scan();
}

void ble_spp_client_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}


void ble_send_data(uint16_t conn_handle, uint8_t *data, size_t len)
{
    int rc = 0;
    rc = ble_gattc_write_flat(conn_handle, attribute_handle[conn_handle], data, len, NULL, NULL);
    if (rc == 0) {
        ESP_LOGI(TAG, "Write BLE success!");
    } else {
        ESP_LOGE(TAG, "Error in writing BLE characteristic rc=%d", rc);
    }
}


int ble_net_init(void)
{
    int rc;
    rc = nimble_port_init();
    if (rc != ESP_OK) {
        MODLOG_DFLT(ERROR, "Failed to init nimble %d \n", rc);
        return ESP_FAIL;
    }

    /* Configure the host. */
    ble_hs_cfg.reset_cb = ble_spp_client_on_reset;
    ble_hs_cfg.sync_cb = ble_spp_client_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Initialize data structures to track connected peers. */
    rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("ble-spp-client");
    assert(rc == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(ble_spp_client_host_task);

    return 0;
}
