#include <deos/deos.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <errno.h>

LOG_MODULE_REGISTER(deos_rx, LOG_LEVEL_INF);

/*
 * Static RX Queue
 */
#define DEOS_RX_QUEUE_SIZE 16

K_MSGQ_DEFINE(rx_msgq, sizeof(struct can_frame), DEOS_RX_QUEUE_SIZE, 4);

/*
 * RX Worker Thread
 */
#define DEOS_RX_THREAD_STACK_SIZE 1024
#define DEOS_RX_THREAD_PRIORITY   5

static struct k_thread rx_thread_data;
static K_KERNEL_STACK_DEFINE(rx_thread_stack, DEOS_RX_THREAD_STACK_SIZE);
static int rx_filter_id = -1;

/*
 * CAN RX Callback (Runs in interrupt context)
 */
static void deos_can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data)
{
    /* Push to static queue, do not block */
    if (k_msgq_put(&rx_msgq, frame, K_NO_WAIT) != 0) {
        /* Drop frame if queue is full */
        /* Optionally increment a drop counter here */
    }
}

/*
 * DEOS RX Thread Loop
 */
static void deos_rx_thread_func(void *p1, void *p2, void *p3)
{
    struct can_frame frame;
    deos_message_t msg;

    LOG_INF("DEOS RX Thread started");

    while (1) {
        if (k_msgq_get(&rx_msgq, &frame, K_FOREVER) == 0) {
            int ret = deos_decode_frame(&frame, &msg);
            
            if (ret != 0) {
                LOG_WRN("Failed to decode frame: %d", ret);
                continue;
            }

            /* Valid message, pass to dispatcher */
            deos_dispatch(&msg);
        }
    }
}

int deos_rx_init(void)
{
    const struct deos_config *config = deos_get_config();
    if (!config || !config->can_dev) {
        return -ENODEV;
    }

    if (!device_is_ready(config->can_dev)) {
        LOG_ERR("CAN device not ready");
        return -ENODEV;
    }

    /* 
     * CAN controller mode configuration.
     * We depend on the app to configure bitrate in devicetree or beforehand.
     * However, we must ensure CAN-FD mode is active if the driver requires it.
     */
    int ret = can_set_mode(config->can_dev, CAN_MODE_FD);
    if (ret != 0 && ret != -ENOTSUP) {
        /* 
         * Note: Some hardware (like nucleo_f439zi) might not support CAN-FD.
         * The instruction says "Board CAN-FD support etmiyorsa hata dönmesi kabul edilebilir. 
         * Classic CAN'e fallback YAPMA."
         * If the driver doesn't support CAN_MODE_FD, it might return -ENOTSUP.
         * We fail outright if it doesn't support CAN-FD.
         */
        LOG_ERR("Failed to set CAN-FD mode: %d", ret);
        return ret;
    }
    
    if (ret == -ENOTSUP) {
        LOG_ERR("CAN-FD mode not supported by hardware!");
        return ret;
    }

    /* Add CAN filter */
    struct can_filter filter = {
        .flags = CAN_FILTER_IDE, /* Only care about extended frames */
        .id = 0,
        .mask = 0 /* Promiscuous mode for now to support router and normal use */
    };

    rx_filter_id = can_add_rx_filter(config->can_dev, deos_can_rx_callback, NULL, &filter);
    if (rx_filter_id < 0) {
        LOG_ERR("Failed to add RX filter: %d", rx_filter_id);
        return rx_filter_id;
    }

    LOG_DBG("RX infrastructure initialized");
    return 0;
}

int deos_rx_start(void)
{
    const struct deos_config *config = deos_get_config();
    
    /* Start CAN device */
    int ret = can_start(config->can_dev);
    if (ret != 0 && ret != -EALREADY) {
        LOG_ERR("Failed to start CAN device: %d", ret);
        return ret;
    }

    /* Create static thread */
    k_thread_create(
        &rx_thread_data,
        rx_thread_stack,
        K_KERNEL_STACK_SIZEOF(rx_thread_stack),
        deos_rx_thread_func,
        NULL, NULL, NULL,
        DEOS_RX_THREAD_PRIORITY,
        0, K_NO_WAIT);

    return 0;
}
