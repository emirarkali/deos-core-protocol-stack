#include <zephyr/ztest.h>
#include <deos/deos.h>
#include "../../lib/deos_core/src/deos_internal.h"

ZTEST_SUITE(deos_protocol, NULL, NULL, NULL, NULL, NULL);

ZTEST(deos_protocol, test_01_can_id_encode_decode_roundtrip)
{
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING,
        DEOS_NODE_STEERING, DEOS_NODE_MAIN_STM32, &can_id);
    
    zassert_equal(ret, 0, "Encode failed");

    deos_message_t msg;
    ret = deos_can_id_decode(can_id, &msg);
    zassert_equal(ret, 0, "Decode failed");
    
    zassert_equal(msg.priority, DEOS_PRIO_CONTROL, "Priority mismatch");
    zassert_equal(msg.message_class, DEOS_CLASS_COMMAND, "Class mismatch");
    zassert_equal(msg.service, DEOS_SERVICE_STEERING, "Service mismatch");
    zassert_equal(msg.destination, DEOS_NODE_STEERING, "Destination mismatch");
    zassert_equal(msg.source, DEOS_NODE_MAIN_STM32, "Source mismatch");
}

ZTEST(deos_protocol, test_02_maximum_valid_29bit_id_fields)
{
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        DEOS_CAN_PRIORITY_MASK, DEOS_CAN_MESSAGE_CLASS_MASK, 
        DEOS_CAN_SERVICE_MASK, DEOS_CAN_DESTINATION_MASK, 
        DEOS_CAN_SOURCE_MASK, &can_id);
    zassert_equal(ret, 0, "Max fields encode failed");
}

ZTEST(deos_protocol, test_03_invalid_priority_reject)
{
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        8 /* Invalid priority */, DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING,
        DEOS_NODE_STEERING, DEOS_NODE_MAIN_STM32, &can_id);
    zassert_not_equal(ret, 0, "Invalid priority should fail");
}

ZTEST(deos_protocol, test_04_invalid_message_class_reject)
{
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        DEOS_PRIO_CONTROL, 16 /* Invalid class */, DEOS_SERVICE_STEERING,
        DEOS_NODE_STEERING, DEOS_NODE_MAIN_STM32, &can_id);
    zassert_not_equal(ret, 0, "Invalid class should fail");
}

ZTEST(deos_protocol, test_05_invalid_service_reject)
{
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, 64 /* Invalid service */,
        DEOS_NODE_STEERING, DEOS_NODE_MAIN_STM32, &can_id);
    zassert_not_equal(ret, 0, "Invalid service should fail");
}

ZTEST(deos_protocol, test_06_invalid_source_reject)
{
    /* Decode level reject */
    struct can_frame frame = { .flags = CAN_FRAME_IDE | CAN_FRAME_FDF, .dlc = 3, .data = {0x10, 0, 0} };
    deos_can_id_encode(DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING, DEOS_NODE_STEERING, DEOS_NODE_INVALID, &frame.id);
    
    deos_message_t msg;
    int ret = deos_decode_frame(&frame, &msg);
    zassert_equal(ret, -EPROTO, "Invalid source should return EPROTO");
}

ZTEST(deos_protocol, test_07_invalid_destination_reject)
{
    struct can_frame frame = { .flags = CAN_FRAME_IDE | CAN_FRAME_FDF, .dlc = 3, .data = {0x10, 0, 0} };
    deos_can_id_encode(DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING, DEOS_NODE_INVALID, DEOS_NODE_MAIN_STM32, &frame.id);
    
    deos_message_t msg;
    int ret = deos_decode_frame(&frame, &msg);
    zassert_equal(ret, -EPROTO, "Invalid dest should return EPROTO");
}

ZTEST(deos_protocol, test_08_local_node_invalid_init_reject)
{
    struct deos_config config = { .node_id = DEOS_NODE_INVALID };
    int ret = deos_init(&config);
    zassert_equal(ret, -EINVAL, "Init with invalid node should fail");
}

ZTEST(deos_protocol, test_09_local_node_broadcast_init_reject)
{
    struct deos_config config = { .node_id = DEOS_NODE_BROADCAST };
    int ret = deos_init(&config);
    zassert_equal(ret, -EINVAL, "Init with broadcast node should fail");
}

/* 
 * NOTE: Other tests for frame length validation, ping encoding, sequence logic 
 * can be added here following the same structure. 
 */
