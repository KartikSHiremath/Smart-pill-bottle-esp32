#include "ble_service.h"

#include <string.h>

#include "esp_log.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "host/ble_hs.h"
#include "host/util/util.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BLE";

static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t notify_handle = 0;

static bool ble_connected = false;

/* Service UUID:
 * 12345678-1234-5678-1234-567812345678
 */
static const ble_uuid128_t service_uuid =
    BLE_UUID128_INIT(
        0x78,0x56,0x34,0x12,
        0x78,0x56,
        0x34,0x12,
        0x78,0x56,
        0x34,0x12,
        0x78,0x56,0x34,0x12);

/* Characteristic UUID:
 * 87654321-4321-8765-4321-876543218765
 */
static const ble_uuid128_t char_uuid =
    BLE_UUID128_INIT(
        0x65,0x87,0x21,0x43,
        0x65,0x87,
        0x21,0x43,
        0x65,0x87,
        0x21,0x43,
        0x21,0x43,0x65,0x87);

static int gatt_access_cb(
    uint16_t conn_handle,
    uint16_t attr_handle,
    struct ble_gatt_access_ctxt *ctxt,
    void *arg)
{
    const char *msg = "Smart Pill Bottle Ready";

    os_mbuf_append(
        ctxt->om,
        msg,
        strlen(msg));

    return 0;
}

static const struct ble_gatt_svc_def gatt_svcs[] =
{
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &service_uuid.u,

        .characteristics =
        (struct ble_gatt_chr_def[])
        {
            {
                .uuid = &char_uuid.u,
                .access_cb = gatt_access_cb,

                .flags =
                    BLE_GATT_CHR_F_READ |
                    BLE_GATT_CHR_F_NOTIFY,

                .val_handle = &notify_handle,
            },

            {0}
        }
    },

    {0}
};

static void start_advertising(void);

static int gap_event(
    struct ble_gap_event *event,
    void *arg)
{
    switch(event->type)
    {
        case BLE_GAP_EVENT_CONNECT:

            if(event->connect.status == 0)
            {
                ble_connected = true;
                conn_handle =
                    event->connect.conn_handle;

                ESP_LOGI(
                    TAG,
                    "Phone Connected");
            }
            else
            {
                start_advertising();
            }

            return 0;

        case BLE_GAP_EVENT_DISCONNECT:

            ESP_LOGI(
                TAG,
                "Phone Disconnected");

            ble_connected = false;

            conn_handle =
                BLE_HS_CONN_HANDLE_NONE;

            start_advertising();

            return 0;

        default:
            return 0;
    }
}

static void start_advertising(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;

    memset(&fields, 0, sizeof(fields));

    fields.flags =
        BLE_HS_ADV_F_DISC_GEN |
        BLE_HS_ADV_F_BREDR_UNSUP;

    const char *name =
        "Smart Pill Bottle";

    fields.name =
        (uint8_t *)name;

    fields.name_len =
        strlen(name);

    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    memset(
        &adv_params,
        0,
        sizeof(adv_params));

    adv_params.conn_mode =
        BLE_GAP_CONN_MODE_UND;

    adv_params.disc_mode =
        BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(
        BLE_OWN_ADDR_PUBLIC,
        NULL,
        BLE_HS_FOREVER,
        &adv_params,
        gap_event,
        NULL);

    ESP_LOGI(
        TAG,
        "Advertising Started");
}

static void on_sync(void)
{
    start_advertising();
}

static void ble_host_task(void *param)
{
    nimble_port_run();

    nimble_port_freertos_deinit();
}

void ble_service_init(void)
{
    nimble_port_init();

    ble_hs_cfg.sync_cb = on_sync;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_svc_gap_device_name_set(
        "Smart Pill Bottle");

    ble_gatts_count_cfg(
        gatt_svcs);

    ble_gatts_add_svcs(
        gatt_svcs);

    nimble_port_freertos_init(
        ble_host_task);

    ESP_LOGI(
        TAG,
        "BLE Initialized");
}

void ble_service_notify(
    const char *message)
{
    if(!ble_connected)
    {
        ESP_LOGW(
            TAG,
            "No Phone Connected");

        return;
    }

    struct os_mbuf *om =
        ble_hs_mbuf_from_flat(
            message,
            strlen(message));

    if(om == NULL)
    {
        return;
    }

    int rc =
        ble_gatts_notify_custom(
            conn_handle,
            notify_handle,
            om);

    if(rc == 0)
    {
        ESP_LOGI(
            TAG,
            "Notification Sent");
    }
    else
    {
        ESP_LOGE(
            TAG,
            "Notify Failed rc=%d",
            rc);
    }
}

bool ble_service_is_connected(void)
{
    return ble_connected;
}