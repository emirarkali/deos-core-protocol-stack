#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/can.h>
#include <zephyr/sys/byteorder.h>

#include <deos/deos.h>

LOG_MODULE_REGISTER(deos_app);

/* Example generic application handler */
static int generic_command_handler(
    const deos_message_t *message,
    void *user_data)
{
    LOG_INF("Received generic command from 0x%02X, payload length: %d", 
            message->source, message->payload_len);

    /* Parse payload using sys_get_le16 etc. if needed */

    return 0;
}

int main(void)
{
    LOG_INF("DEOS Core prototype application starting...");

#if DT_HAS_CHOSEN(zephyr_canbus)
    const struct device *can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
#else
    const struct device *can_dev = NULL;
#endif

    if (can_dev && !device_is_ready(can_dev)) {
        LOG_ERR("CAN device not ready");
        /* Depending on board, it might fail here if not configured. We continue for demo. */
    }

    struct deos_config config = {
        /* Define the primary physical node identity of this MCU */
        .node_id = DEOS_NODE_MAIN_STM32,
        .can_dev = can_dev,
        .router_enabled = false
    };

    int ret = deos_init(&config);
    if (ret != 0) {
        LOG_ERR("DEOS Core initialization failed: %d", ret);
        /* 
         * Return early if initialization strictly fails (e.g., node invalid).
         * For F439ZI, CAN-FD setup might fail returning -ENOTSUP.
         */
        if (ret == -ENOTSUP) {
            LOG_WRN("Continuing despite lack of CAN-FD support on this hardware");
        } else {
            return ret;
        }
    }

    /* Example of registering an application handler */
    deos_register_handler(
        DEOS_CLASS_COMMAND,
        DEOS_SERVICE_SYSTEM,
        0x01, /* Example Command ID */
        generic_command_handler,
        NULL);

    ret = deos_start();
    if (ret != 0) {
        LOG_ERR("DEOS Core start failed: %d", ret);
        if (ret != -ENOTSUP) {
            return ret;
        }
    }

#if DEOS_SAMPLE_PING_ENABLED
    /* Compile-time closed ping test as requested */
    /* deos_send_ping(DEOS_NODE_BMS_MAIN, 0x12345678); */
#endif

    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}
