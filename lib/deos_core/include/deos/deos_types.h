#ifndef DEOS_TYPES_H
#define DEOS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <deos/deos_icd.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Maximum theoretical command payload for DEOS
 * (64 bytes CAN-FD payload - 3 bytes header)
 */
#define DEOS_MAX_PAYLOAD_LEN 61

/*
 * Core DEOS Message Structure
 * Reusable container for DEOS protocol messages independent of CAN transport.
 */
typedef struct {
    deos_priority_t priority;
    deos_message_class_t message_class;
    deos_service_id_t service;

    deos_node_id_t destination;
    deos_node_id_t source;

    uint8_t version;
    uint8_t sequence;
    uint8_t command;

    uint8_t payload[DEOS_MAX_PAYLOAD_LEN];
    uint8_t payload_len;
} deos_message_t;

struct device;

/*
 * Configuration Struct
 */
struct deos_config {
    deos_node_id_t node_id;
    const struct device *can_dev;
    bool router_enabled;
    bool hosted_nodes_enabled;
};

/*
 * Message Handler Type
 */
typedef int (*deos_message_handler_t)(const deos_message_t *message, void *user_data);

/*
 * Transport Type
 */
typedef enum {
    DEOS_TRANSPORT_LOCAL,
    DEOS_TRANSPORT_CAN_FD,
    DEOS_TRANSPORT_ETHERNET,
    DEOS_TRANSPORT_LORA
} deos_transport_t;

#ifdef __cplusplus
}
#endif

#endif /* DEOS_TYPES_H */
