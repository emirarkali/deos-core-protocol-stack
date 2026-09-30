#include <deos/deos.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(deos_core, LOG_LEVEL_INF);

static struct deos_config core_config;
static atomic_t tx_sequence = ATOMIC_INIT(0);
static bool is_initialized = false;
static bool is_started = false;

extern int deos_rx_init(void);
extern int deos_rx_start(void);

int deos_init(const struct deos_config *config)
{
    if (is_initialized) {
        return -EALREADY;
    }

    if (!config) {
        return -EINVAL;
    }

    if (config->node_id == DEOS_NODE_INVALID || config->node_id == DEOS_NODE_BROADCAST) {
        LOG_ERR("Invalid local node ID: 0x%02X", config->node_id);
        return -EINVAL;
    }

    /* Save configuration */
    core_config = *config;

    atomic_set(&tx_sequence, 0);

    int ret = deos_fault_init();
    if (ret != 0) {
        LOG_ERR("Failed to init fault manager");
        return ret;
    }

    ret = deos_dispatch_init();
    if (ret != 0) {
        LOG_ERR("Failed to init dispatcher");
        return ret;
    }

    if (core_config.router_enabled) {
        ret = deos_router_init();
        if (ret != 0) {
            LOG_ERR("Failed to init router");
            return ret;
        }
    }

    ret = deos_rx_init();
    if (ret != 0) {
        LOG_ERR("Failed to init RX infrastructure");
        return ret;
    }

    is_initialized = true;
    LOG_INF("DEOS Core initialized (Node ID: 0x%02X)", core_config.node_id);

    return 0;
}

int deos_start(void)
{
    if (!is_initialized) {
        return -EPERM;
    }

    if (is_started) {
        return -EALREADY;
    }

    int ret = deos_rx_start();
    if (ret != 0) {
        LOG_ERR("Failed to start RX threads");
        return ret;
    }

    is_started = true;
    LOG_INF("DEOS Core started");
    return 0;
}

const struct deos_config *deos_get_config(void)
{
    return &core_config;
}

uint8_t deos_next_sequence(void)
{
    /* Atomically increment and return the previous value. Wrap at 255. */
    atomic_val_t seq = atomic_inc(&tx_sequence);
    return (uint8_t)(seq & 0xFF);
}
