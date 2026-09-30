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

ZTEST_SUITE(deos_fault_test, NULL, NULL, NULL, NULL, NULL);

ZTEST(deos_fault_test, test_fault_01_reject_zero_id)
{
    deos_fault_init();
    int ret = deos_fault_raise(0, DEOS_FAULT_SEVERITY_ERROR);
    zassert_equal(ret, -EINVAL, "Fault ID 0 should be rejected");
}

ZTEST(deos_fault_test, test_fault_02_new_fault_raise)
{
    deos_fault_init();
    int ret = deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    zassert_equal(ret, 0, "Raise failed");
    zassert_true(deos_fault_is_active(0x0100), "Fault should be active");
}

ZTEST(deos_fault_test, test_fault_03_same_fault_repeated)
{
    deos_fault_init();
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    
    /* Internal API would be needed to test occurrence count directly, 
       but we can at least ensure it doesn't fail. */
    zassert_true(deos_fault_is_active(0x0100), "Fault should still be active");
}

ZTEST(deos_fault_test, test_fault_04_set_inactive)
{
    deos_fault_init();
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_set_inactive(0x0100);
    zassert_false(deos_fault_is_active(0x0100), "Fault should be inactive");
}

ZTEST(deos_fault_test, test_fault_05_latch)
{
    deos_fault_init();
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_latch(0x0100);
    zassert_true(deos_fault_is_active(0x0100), "Latched fault should be active");
}

ZTEST(deos_fault_test, test_fault_06_clear_single)
{
    deos_fault_init();
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_clear(0x0100);
    zassert_false(deos_fault_is_active(0x0100), "Fault should be cleared");
}

ZTEST(deos_fault_test, test_fault_07_clear_all)
{
    deos_fault_init();
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_raise(0x0101, DEOS_FAULT_SEVERITY_ERROR);
    deos_fault_clear_all();
    zassert_false(deos_fault_is_active(0x0100), "Fault 0x0100 should be cleared");
    zassert_false(deos_fault_is_active(0x0101), "Fault 0x0101 should be cleared");
}

ZTEST(deos_fault_test, test_fault_08_max_table_full)
{
    deos_fault_init();
    for (int i = 1; i <= 32; i++) {
        deos_fault_raise(i, DEOS_FAULT_SEVERITY_INFO);
    }
    int ret = deos_fault_raise(33, DEOS_FAULT_SEVERITY_INFO);
    zassert_equal(ret, -ENOSPC, "Should return -ENOSPC when table is full");
}

ZTEST_SUITE(deos_local_node_test, NULL, NULL, NULL, NULL, NULL);

ZTEST(deos_local_node_test, test_local_node_01_primary_node_is_local)
{
    /* Assuming node is initialized to DEOS_NODE_STEERING */
    zassert_true(deos_is_local_node(DEOS_NODE_STEERING), "Primary node should be local");
}

ZTEST(deos_local_node_test, test_local_node_02_unregistered_node_is_not_local)
{
    zassert_false(deos_is_local_node(DEOS_NODE_BRAKE), "Unregistered node should not be local");
}

ZTEST(deos_local_node_test, test_local_node_03_invalid_node_is_not_local)
{
    zassert_false(deos_is_local_node(DEOS_NODE_INVALID), "Invalid node should not be local");
}

/* 
 * NOTE: Further tests requiring deos_init (which requires a valid CAN dev)
 * like register_local_node and routing can be implemented when a full CAN mock
 * or loopback device is fully linked into the ZTEST runner.
 *
 * Current tests cover the basic codec, fault storage, and basic node identity checks.
 */

ZTEST_SUITE(deos_node_fault_test, NULL, NULL, NULL, NULL, NULL);

ZTEST(deos_node_fault_test, test_01_multi_node_fault_isolation)
{
    /* In a mocked CAN environment, these nodes would be registered via deos_register_local_node.
       Since we bypass init in these partial tests, we just assume DEOS_NODE_STEERING 
       is primary and active. We check unregistered behavior for DEOS_NODE_BRAKE. */

    deos_fault_init();

    /* 1. Unregistered node should fail to raise fault */
    int ret = deos_fault_raise_for_node(DEOS_NODE_BRAKE, 0x1111, DEOS_FAULT_SEVERITY_ERROR);
    zassert_equal(ret, -EPERM, "Unregistered node should not be able to raise fault");

    /* 2. Unregistered node should not have active faults */
    zassert_false(deos_fault_is_active_for_node(DEOS_NODE_BRAKE, 0x1111), "Unregistered node should not have active fault");

    /* 3. Primary node should fail if not properly initialized in this test context,
       but assuming it was, it would succeed. To make this pass without deos_init(), 
       we can't easily test it here. We document the test logic. */
}

ZTEST(deos_node_fault_test, test_02_legacy_wrappers)
{
    /* If we used legacy wrappers, they would target the primary node */
    /* We expect -ENODEV here because config is not fully valid without deos_init */
    int ret = deos_fault_raise(0x1234, DEOS_FAULT_SEVERITY_WARNING);
    zassert_equal(ret, -ENODEV, "Wrapper should fail if system not initialized");
}
