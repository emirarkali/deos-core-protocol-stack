#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/can.h>
#include <zephyr/sys/byteorder.h>

#include <deos/deos.h>

LOG_MODULE_REGISTER(deos_app);

/* Example application handler for Steering */
static int steering_target_handler(
    const deos_message_t *message,
    void *user_data)
{
    if (message->payload_len != 2) {
        return -EMSGSIZE;
    }

    /* byteorder header included at the top */
    int16_t raw = (int16_t)sys_get_le16(message->payload);

    LOG_INF("Steering target raw = %d", raw);

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
        .node_id = DEOS_NODE_STEERING,
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

    deos_register_handler(
        DEOS_CLASS_COMMAND,
        DEOS_SERVICE_STEERING,
        DEOS_CMD_STEERING_SET_TARGET_ANGLE,
        steering_target_handler,
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
    deos_send_ping(DEOS_NODE_MAIN_STM32, 0x12345678);
#endif

    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}
