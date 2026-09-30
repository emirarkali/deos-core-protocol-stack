#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <errno.h>

LOG_MODULE_REGISTER(deos_fault, LOG_LEVEL_INF);

#define DEOS_MAX_FAULT_RECORDS 32

static deos_fault_record_t fault_table[DEOS_MAX_FAULT_RECORDS];
static K_MUTEX_DEFINE(fault_mutex);

/*
 * Fault Transfer Context for GET_FAULTS
 */
struct deos_fault_transfer {
    bool active;
    deos_node_id_t destination;
    size_t current_index;
    struct k_work_delayable work;
};

static struct deos_fault_transfer transfer_ctx;

static void send_fault_response(const deos_fault_record_t *record, deos_node_id_t destination)
{
    uint8_t payload[10] = {0}; // 10 byte semantic data

    deos_put_u16_le(&payload[0], record->fault_id);
    payload[2] = record->severity;
    payload[3] = record->state;
    deos_put_u16_le(&payload[4], record->occurrence_count);
    deos_put_u32_le(&payload[6], record->last_occurrence_ms);

    int ret = deos_send(
        destination,
        DEOS_PRIO_STATUS, /* Provisional internal choice */
        DEOS_CLASS_RESPONSE,
        DEOS_SERVICE_DIAGNOSTIC,
        DEOS_CMD_DIAG_GET_FAULTS,
        payload,
        10
    );

    if (ret != 0) {
        LOG_WRN("Failed to send fault response to 0x%02X: %d", destination, ret);
    }
}

static void fault_transfer_work_handler(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct deos_fault_transfer *ctx = CONTAINER_OF(dwork, struct deos_fault_transfer, work);

    k_mutex_lock(&fault_mutex, K_FOREVER);

    if (!ctx->active) {
        k_mutex_unlock(&fault_mutex);
        return;
    }

    /* Find next used entry */
    bool found = false;
    deos_fault_record_t record;

    while (ctx->current_index < DEOS_MAX_FAULT_RECORDS) {
        if (fault_table[ctx->current_index].fault_id != DEOS_FAULT_ID_END_OF_LIST) {
            /* Copy record to avoid holding lock during CAN transmission */
            record = fault_table[ctx->current_index];
            ctx->current_index++;
            found = true;
            break;
        }
        ctx->current_index++;
    }

    k_mutex_unlock(&fault_mutex);

    if (found) {
        send_fault_response(&record, ctx->destination);
        /* Schedule next fault in 100 ms */
        k_work_schedule(&ctx->work, K_MSEC(100));
    } else {
        /* End of list */
        deos_fault_record_t end_record = {
            .fault_id = DEOS_FAULT_ID_END_OF_LIST,
            .severity = 0,
            .state = 0,
            .occurrence_count = 0,
            .last_occurrence_ms = 0
        };
        send_fault_response(&end_record, ctx->destination);
        
        k_mutex_lock(&fault_mutex, K_FOREVER);
        ctx->active = false;
        k_mutex_unlock(&fault_mutex);
    }
}

int deos_fault_init(void)
{
    k_mutex_lock(&fault_mutex, K_FOREVER);
    
    for (int i = 0; i < DEOS_MAX_FAULT_RECORDS; i++) {
        fault_table[i].fault_id = DEOS_FAULT_ID_END_OF_LIST; // 0 indicates empty
    }

    transfer_ctx.active = false;
    k_work_init_delayable(&transfer_ctx.work, fault_transfer_work_handler);

    k_mutex_unlock(&fault_mutex);
    LOG_DBG("Fault management initialized");
    return 0;
}

int deos_fault_raise(uint16_t fault_id, deos_fault_severity_t severity)
{
    if (fault_id == DEOS_FAULT_ID_END_OF_LIST) {
        return -EINVAL;
    }

    k_mutex_lock(&fault_mutex, K_FOREVER);

    int free_slot = -1;
    bool found = false;
    uint32_t now = k_uptime_get_32();

    for (int i = 0; i < DEOS_MAX_FAULT_RECORDS; i++) {
        if (fault_table[i].fault_id == fault_id) {
            /* Update existing fault */
            fault_table[i].state = DEOS_FAULT_STATE_ACTIVE;
            fault_table[i].severity = severity;
            if (fault_table[i].occurrence_count < UINT16_MAX) {
                fault_table[i].occurrence_count++;
            }
            fault_table[i].last_occurrence_ms = now;
            found = true;
            break;
        } else if (fault_table[i].fault_id == DEOS_FAULT_ID_END_OF_LIST && free_slot == -1) {
            free_slot = i;
        }
    }

    if (!found) {
        if (free_slot == -1) {
            k_mutex_unlock(&fault_mutex);
            return -ENOSPC;
        }
        fault_table[free_slot].fault_id = fault_id;
        fault_table[free_slot].state = DEOS_FAULT_STATE_ACTIVE;
        fault_table[free_slot].severity = severity;
        fault_table[free_slot].occurrence_count = 1;
        fault_table[free_slot].last_occurrence_ms = now;
    }

    k_mutex_unlock(&fault_mutex);
    LOG_WRN("Fault Raised: 0x%04X (Severity: %d)", fault_id, severity);
    return 0;
}

int deos_fault_set_inactive(uint16_t fault_id)
{
    if (fault_id == DEOS_FAULT_ID_END_OF_LIST) {
        return -EINVAL;
    }

    int ret = -ENOENT;
    k_mutex_lock(&fault_mutex, K_FOREVER);

    for (int i = 0; i < DEOS_MAX_FAULT_RECORDS; i++) {
        if (fault_table[i].fault_id == fault_id) {
            /* Only set INACTIVE if not LATCHED */
            if (fault_table[i].state != DEOS_FAULT_STATE_LATCHED) {
                fault_table[i].state = DEOS_FAULT_STATE_INACTIVE;
            }
            ret = 0;
            break;
        }
    }

    k_mutex_unlock(&fault_mutex);
    if (ret == 0) {
        LOG_INF("Fault Inactive: 0x%04X", fault_id);
    }
    return ret;
}

int deos_fault_latch(uint16_t fault_id)
{
    if (fault_id == DEOS_FAULT_ID_END_OF_LIST) {
        return -EINVAL;
    }

    int ret = -ENOENT;
    k_mutex_lock(&fault_mutex, K_FOREVER);

    for (int i = 0; i < DEOS_MAX_FAULT_RECORDS; i++) {
        if (fault_table[i].fault_id == fault_id) {
            fault_table[i].state = DEOS_FAULT_STATE_LATCHED;
            ret = 0;
            break;
        }
    }

    k_mutex_unlock(&fault_mutex);
    return ret;
}

int deos_fault_clear(uint16_t fault_id)
{
    if (fault_id == DEOS_FAULT_ID_END_OF_LIST) {
        return -EINVAL;
    }

    int ret = -ENOENT;
    k_mutex_lock(&fault_mutex, K_FOREVER);

    for (int i = 0; i < DEOS_MAX_FAULT_RECORDS; i++) {
        if (fault_table[i].fault_id == fault_id) {
            fault_table[i].fault_id = DEOS_FAULT_ID_END_OF_LIST;
            ret = 0;
            break;
        }
    }

    k_mutex_unlock(&fault_mutex);
    return ret;
}

int deos_fault_clear_all(void)
{
    k_mutex_lock(&fault_mutex, K_FOREVER);
    
    for (int i = 0; i < DEOS_MAX_FAULT_RECORDS; i++) {
        fault_table[i].fault_id = DEOS_FAULT_ID_END_OF_LIST;
    }

    k_mutex_unlock(&fault_mutex);
    LOG_INF("All faults cleared");
    return 0;
}

bool deos_fault_is_active(uint16_t fault_id)
{
    if (fault_id == DEOS_FAULT_ID_END_OF_LIST) {
        return false;
    }

    bool active = false;
    k_mutex_lock(&fault_mutex, K_FOREVER);

    for (int i = 0; i < DEOS_MAX_FAULT_RECORDS; i++) {
        if (fault_table[i].fault_id == fault_id) {
            if (fault_table[i].state == DEOS_FAULT_STATE_ACTIVE ||
                fault_table[i].state == DEOS_FAULT_STATE_LATCHED) {
                active = true;
            }
            break;
        }
    }

    k_mutex_unlock(&fault_mutex);
    return active;
}

void deos_fault_handle_get_faults(const deos_message_t *msg)
{
    k_mutex_lock(&fault_mutex, K_FOREVER);

    if (transfer_ctx.active) {
        LOG_WRN("GET_FAULTS requested but transfer already active. Ignoring.");
        k_mutex_unlock(&fault_mutex);
        return;
    }

    transfer_ctx.active = true;
    transfer_ctx.destination = msg->source;
    transfer_ctx.current_index = 0;

    /* Schedule immediately for the first fault (or END if none) */
    k_work_schedule(&transfer_ctx.work, K_NO_WAIT);

    k_mutex_unlock(&fault_mutex);
}

void deos_fault_handle_clear_faults(const deos_message_t *msg)
{
    LOG_INF("CLEAR_FAULTS requested from 0x%02X", msg->source);
    deos_fault_clear_all();
}
