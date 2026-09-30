#ifndef DEOS_FAULT_H
#define DEOS_FAULT_H

#include <stdint.h>
#include <stdbool.h>
#include <deos/deos_icd.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Common Fault ID Registry
 * Node-specific fault IDs should start from 0x0100.
 */
typedef enum
{
    DEOS_FAULT_ID_END_OF_LIST             = 0x0000,
    DEOS_FAULT_INTERNAL_SOFTWARE_ERROR    = 0x0001,
    DEOS_FAULT_WATCHDOG_RESET             = 0x0002,
    DEOS_FAULT_INVALID_CONFIGURATION      = 0x0003,
    DEOS_FAULT_COMMUNICATION_TIMEOUT      = 0x0004,
    DEOS_FAULT_RX_QUEUE_OVERFLOW          = 0x0005,
    DEOS_FAULT_TX_FAILURE                 = 0x0006,
    DEOS_FAULT_PROTOCOL_ERROR             = 0x0007,
    DEOS_FAULT_LOCAL_STORAGE_ERROR        = 0x0008
} deos_common_fault_id_t;

/*
 * Fault Severity
 */
typedef enum
{
    DEOS_FAULT_SEVERITY_INFO     = 0x01,
    DEOS_FAULT_SEVERITY_WARNING  = 0x02,
    DEOS_FAULT_SEVERITY_ERROR    = 0x03,
    DEOS_FAULT_SEVERITY_CRITICAL = 0x04
} deos_fault_severity_t;

/*
 * Fault State
 */
typedef enum
{
    DEOS_FAULT_STATE_INACTIVE = 0x00,
    DEOS_FAULT_STATE_ACTIVE   = 0x01,
    DEOS_FAULT_STATE_LATCHED  = 0x02
} deos_fault_state_t;

/*
 * Fault Record Structure
 */
typedef struct
{
    uint16_t fault_id;
    uint8_t severity;
    uint8_t state;
    uint16_t occurrence_count;
    uint32_t last_occurrence_ms;
} deos_fault_record_t;

/**
 * @brief Raise or update a fault condition.
 *
 * @param fault_id Unique identifier for the fault.
 * @param severity Severity of the fault.
 * @return 0 on success, negative error code on failure (e.g. -ENOSPC).
 */
int deos_fault_raise(uint16_t fault_id, deos_fault_severity_t severity);

/**
 * @brief Mark an active fault as inactive.
 *
 * @param fault_id Unique identifier for the fault.
 * @return 0 on success, -ENOENT if fault not found.
 */
int deos_fault_set_inactive(uint16_t fault_id);

/**
 * @brief Mark a fault as latched (must be explicitly cleared).
 *
 * @param fault_id Unique identifier for the fault.
 * @return 0 on success, -ENOENT if fault not found.
 */
int deos_fault_latch(uint16_t fault_id);

/**
 * @brief Clear a specific fault from local storage.
 *
 * @param fault_id Unique identifier for the fault.
 * @return 0 on success, -ENOENT if fault not found.
 */
int deos_fault_clear(uint16_t fault_id);

/**
 * @brief Clear all stored faults.
 *
 * @return 0 on success.
 */
int deos_fault_clear_all(void);

/**
 * @brief Check if a fault is currently active or latched.
 *
 * @param fault_id Unique identifier for the fault.
 * @return true if ACTIVE or LATCHED, false otherwise.
 */
bool deos_fault_is_active(uint16_t fault_id);

#ifdef __cplusplus
}
#endif

#endif /* DEOS_FAULT_H */
