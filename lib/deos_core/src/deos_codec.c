#include "deos_internal.h"
#include <zephyr/sys/byteorder.h>
#include <errno.h>

void deos_put_u16_le(uint8_t *dst, uint16_t value)
{
    sys_put_le16(value, dst);
}

void deos_put_i16_le(uint8_t *dst, int16_t value)
{
    sys_put_le16((uint16_t)value, dst);
}

void deos_put_u32_le(uint8_t *dst, uint32_t value)
{
    sys_put_le32(value, dst);
}

uint16_t deos_get_u16_le(const uint8_t *src)
{
    return sys_get_le16(src);
}

int16_t deos_get_i16_le(const uint8_t *src)
{
    return (int16_t)sys_get_le16(src);
}

uint32_t deos_get_u32_le(const uint8_t *src)
{
    return sys_get_le32(src);
}

int deos_can_id_encode(
    deos_priority_t priority,
    deos_message_class_t message_class,
    deos_service_id_t service,
    deos_node_id_t destination,
    deos_node_id_t source,
    uint32_t *can_id)
{
    if (!can_id) {
        return -EINVAL;
    }

    /* Validations based on mask width */
    if (priority > DEOS_CAN_PRIORITY_MASK ||
        message_class > DEOS_CAN_MESSAGE_CLASS_MASK ||
        service > DEOS_CAN_SERVICE_MASK) {
        return -EINVAL;
    }

    *can_id = ((uint32_t)priority << DEOS_CAN_PRIORITY_SHIFT) |
              ((uint32_t)message_class << DEOS_CAN_MESSAGE_CLASS_SHIFT) |
              ((uint32_t)service << DEOS_CAN_SERVICE_SHIFT) |
              ((uint32_t)destination << DEOS_CAN_DESTINATION_SHIFT) |
              ((uint32_t)source << DEOS_CAN_SOURCE_SHIFT);

    return 0;
}

int deos_can_id_decode(
    uint32_t can_id,
    deos_message_t *message)
{
    if (!message) {
        return -EINVAL;
    }

    message->priority      = (deos_priority_t)((can_id >> DEOS_CAN_PRIORITY_SHIFT) & DEOS_CAN_PRIORITY_MASK);
    message->message_class = (deos_message_class_t)((can_id >> DEOS_CAN_MESSAGE_CLASS_SHIFT) & DEOS_CAN_MESSAGE_CLASS_MASK);
    message->service       = (deos_service_id_t)((can_id >> DEOS_CAN_SERVICE_SHIFT) & DEOS_CAN_SERVICE_MASK);
    message->destination   = (deos_node_id_t)((can_id >> DEOS_CAN_DESTINATION_SHIFT) & DEOS_CAN_DESTINATION_MASK);
    message->source        = (deos_node_id_t)((can_id >> DEOS_CAN_SOURCE_SHIFT) & DEOS_CAN_SOURCE_MASK);

    return 0;
}

int deos_encode_frame(
    const deos_message_t *msg,
    struct can_frame *frame)
{
    if (!msg || !frame) {
        return -EINVAL;
    }

    if (msg->payload_len > DEOS_MAX_PAYLOAD_LEN) {
        return -EMSGSIZE;
    }

    int ret = deos_can_id_encode(
        msg->priority,
        msg->message_class,
        msg->service,
        msg->destination,
        msg->source,
        &frame->id);

    if (ret != 0) {
        return ret;
    }

    /* Set as Extended and CAN-FD */
    frame->flags = CAN_FRAME_IDE | CAN_FRAME_FDF;

    /* Frame length = version(1) + sequence(1) + command(1) + payload_len */
    frame->dlc = can_bytes_to_dlc(3 + msg->payload_len); 
    /* NOTE: Zephyr's can_bytes_to_dlc takes length, but can_frame->dlc stores DLC.
       Wait, for CAN-FD, it is best to set actual byte length in frame->dlc if using recent Zephyr?
       Actually, modern Zephyr (>= 3.x) uses frame->dlc as length directly or uses DLC. Let's check struct can_frame */
    frame->dlc = 3 + msg->payload_len; // we will pad if needed, but Zephyr CAN API uses `dlc` to store byte length actually.

    frame->data[0] = msg->version;
    frame->data[1] = msg->sequence;
    frame->data[2] = msg->command;

    if (msg->payload_len > 0) {
        memcpy(&frame->data[3], msg->payload, msg->payload_len);
    }

    /* Zero out the rest of the CAN-FD frame payload to avoid dirty bytes if it rounds up DLC */
    uint8_t actual_len = can_dlc_to_bytes(can_bytes_to_dlc(frame->dlc));
    if (actual_len > frame->dlc) {
        memset(&frame->data[frame->dlc], 0, actual_len - frame->dlc);
    }

    return 0;
}

int deos_decode_frame(
    const struct can_frame *frame,
    deos_message_t *msg)
{
    if (!frame || !msg) {
        return -EINVAL;
    }

    if ((frame->flags & CAN_FRAME_IDE) == 0) {
        return -EPROTO; // Only extended ID supported
    }

    if ((frame->flags & CAN_FRAME_FDF) == 0) {
        return -EPROTO; // Only CAN-FD supported
    }

    /* Determine actual received length */
    uint8_t rx_len = can_dlc_to_bytes(can_bytes_to_dlc(frame->dlc));
    
    if (rx_len < 3) {
        return -EMSGSIZE; // Too small
    }
    
    if (rx_len > 64) {
        return -EMSGSIZE; // Too large
    }

    int ret = deos_can_id_decode(frame->id, msg);
    if (ret != 0) {
        return ret;
    }

    if (msg->source == DEOS_NODE_INVALID || msg->destination == DEOS_NODE_INVALID) {
        return -EPROTO;
    }

    msg->version  = frame->data[0];
    msg->sequence = frame->data[1];
    msg->command  = frame->data[2];

    msg->payload_len = rx_len - 3;
    if (msg->payload_len > DEOS_MAX_PAYLOAD_LEN) {
        return -EMSGSIZE;
    }

    /* Validate protocol version */
    if ((msg->version & 0xF0) != (DEOS_PROTOCOL_VERSION & 0xF0)) {
        return -EPROTONOSUPPORT;
    }

    if (msg->payload_len > 0) {
        memcpy(msg->payload, &frame->data[3], msg->payload_len);
    }

    return 0;
}
