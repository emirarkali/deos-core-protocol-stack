#include <deos/deos.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(deos_dispatch, LOG_LEVEL_INF);

#define DEOS_MAX_HANDLERS 32

struct deos_handler_entry {
    bool in_use;
    deos_message_class_t message_class;
    deos_service_id_t service;
    uint8_t command;
    deos_message_handler_t handler;
    void *user_data;
};

static struct deos_handler_entry handler_registry[DEOS_MAX_HANDLERS];

int deos_dispatch_init(void)
{
    memset(handler_registry, 0, sizeof(handler_registry));
    LOG_DBG("Dispatcher initialized");
    return 0;
}

int deos_register_handler(
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data)
{
    if (!handler) {
        return -EINVAL;
    }

    int free_slot = -1;

    for (int i = 0; i < DEOS_MAX_HANDLERS; i++) {
        if (!handler_registry[i].in_use) {
            if (free_slot == -1) {
                free_slot = i;
            }
        } else {
            if (handler_registry[i].message_class == message_class &&
                handler_registry[i].service == service &&
                handler_registry[i].command == command) {
                return -EEXIST; /* Duplicate */
            }
        }
    }

    if (free_slot == -1) {
        return -ENOSPC; /* Full */
    }

    handler_registry[free_slot].message_class = message_class;
    handler_registry[free_slot].service       = service;
    handler_registry[free_slot].command       = command;
    handler_registry[free_slot].handler       = handler;
    handler_registry[free_slot].user_data     = user_data;
    handler_registry[free_slot].in_use        = true;

    LOG_DBG("Registered handler for Class: 0x%X, Svc: 0x%X, Cmd: 0x%X",
            message_class, service, command);

    return 0;
}

void deos_dispatch(const deos_message_t *msg)
{
    const struct deos_config *config = deos_get_config();
    bool is_local = (msg->destination == config->node_id || msg->destination == DEOS_NODE_BROADCAST);

    /* Router Logic */
    if (config->router_enabled && config->node_id == DEOS_NODE_MAIN_STM32) {
        if (msg->destination != config->node_id) {
            /* Message for another node, route it */
            deos_route_message(msg, DEOS_TRANSPORT_CAN_FD);
            
            /* If it is not a broadcast, we stop processing locally */
            if (msg->destination != DEOS_NODE_BROADCAST) {
                return;
            }
        }
    }

    if (!is_local) {
        /* Not for us */
        return;
    }

    /* Common Network Behaviour */
    if (msg->message_class == DEOS_CLASS_NETWORK && msg->service == DEOS_SERVICE_SYSTEM) {
        switch (msg->command) {
            case DEOS_CMD_SYSTEM_PING:
                deos_handle_ping(msg);
                return;
            case DEOS_CMD_SYSTEM_PONG:
                deos_handle_pong(msg);
                return;
            case DEOS_CMD_SYSTEM_HEARTBEAT:
                /* Provisional heartbeat handler */
                return;
            default:
                break; /* Allow app to handle other system network cmds */
        }
    }

    /* Common Diagnostic Behaviour */
    if (msg->service == DEOS_SERVICE_DIAGNOSTIC) {
        if (msg->message_class == DEOS_CLASS_REQUEST && msg->command == DEOS_CMD_DIAG_GET_FAULTS) {
            deos_fault_handle_get_faults(msg);
            return;
        } else if (msg->message_class == DEOS_CLASS_COMMAND && msg->command == DEOS_CMD_DIAG_CLEAR_FAULTS) {
            deos_fault_handle_clear_faults(msg);
            return;
        }
    }

    /* Application Handlers */
    bool handled = false;
    for (int i = 0; i < DEOS_MAX_HANDLERS; i++) {
        if (handler_registry[i].in_use &&
            handler_registry[i].message_class == msg->message_class &&
            handler_registry[i].service == msg->service &&
            handler_registry[i].command == msg->command) {
            
            int ret = handler_registry[i].handler(msg, handler_registry[i].user_data);
            if (ret != 0) {
                LOG_WRN("Handler returned error %d for Cmd: 0x%X", ret, msg->command);
            }
            handled = true;
            break; /* Assume one handler per command */
        }
    }

    if (!handled) {
        LOG_DBG("No handler for Class: 0x%X, Svc: 0x%X, Cmd: 0x%X",
                msg->message_class, msg->service, msg->command);
    }
}
