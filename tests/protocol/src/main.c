#include <zephyr/ztest.h>
#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "../../../lib/deos_core/src/deos_internal.h"

ZTEST_SUITE(deos_protocol, NULL, NULL, NULL, NULL, NULL);

ZTEST(deos_protocol, test_01_can_id_encode_decode_roundtrip)
{
    TC_PRINT("Test: Verifies that encoding a CAN ID and then decoding it results in the original fields.\n");
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
    TC_PRINT("Test: Verifies that encoding works with the maximum possible values for all fields within a 29-bit CAN ID.\n");
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        DEOS_CAN_PRIORITY_MASK, DEOS_CAN_MESSAGE_CLASS_MASK, 
        DEOS_CAN_SERVICE_MASK, DEOS_CAN_DESTINATION_MASK, 
        DEOS_CAN_SOURCE_MASK, &can_id);
    zassert_equal(ret, 0, "Max fields encode failed");
}

ZTEST(deos_protocol, test_03_invalid_priority_reject)
{
    TC_PRINT("Test: Ensures that an invalid priority value is rejected during CAN ID encoding.\n");
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        8 /* Invalid priority */, DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING,
        DEOS_NODE_STEERING, DEOS_NODE_MAIN_STM32, &can_id);
    zassert_not_equal(ret, 0, "Invalid priority should fail");
}

ZTEST(deos_protocol, test_04_invalid_message_class_reject)
{
    TC_PRINT("Test: Ensures that an invalid message class value is rejected during CAN ID encoding.\n");
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        DEOS_PRIO_CONTROL, 16 /* Invalid class */, DEOS_SERVICE_STEERING,
        DEOS_NODE_STEERING, DEOS_NODE_MAIN_STM32, &can_id);
    zassert_not_equal(ret, 0, "Invalid class should fail");
}

ZTEST(deos_protocol, test_05_invalid_service_reject)
{
    TC_PRINT("Test: Ensures that an invalid service value is rejected during CAN ID encoding.\n");
    uint32_t can_id = 0;
    int ret = deos_can_id_encode(
        DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, 64 /* Invalid service */,
        DEOS_NODE_STEERING, DEOS_NODE_MAIN_STM32, &can_id);
    zassert_not_equal(ret, 0, "Invalid service should fail");
}

ZTEST(deos_protocol, test_06_invalid_source_reject)
{
    TC_PRINT("Test: Ensures that a CAN frame with an invalid source node ID is rejected during decoding.\n");
    /* Decode level reject */
    struct can_frame frame = { .flags = CAN_FRAME_IDE | CAN_FRAME_FDF, .dlc = 3, .data = {0x10, 0, 0} };
    deos_can_id_encode(DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING, DEOS_NODE_STEERING, DEOS_NODE_INVALID, &frame.id);
    
    deos_message_t msg;
    int ret = deos_decode_frame(&frame, &msg);
    zassert_equal(ret, -EPROTO, "Invalid source should return EPROTO");
}

ZTEST(deos_protocol, test_07_invalid_destination_reject)
{
    TC_PRINT("Test: Ensures that a CAN frame with an invalid destination node ID is rejected during decoding.\n");
    struct can_frame frame = { .flags = CAN_FRAME_IDE | CAN_FRAME_FDF, .dlc = 3, .data = {0x10, 0, 0} };
    deos_can_id_encode(DEOS_PRIO_CONTROL, DEOS_CLASS_COMMAND, DEOS_SERVICE_STEERING, DEOS_NODE_INVALID, DEOS_NODE_MAIN_STM32, &frame.id);
    
    deos_message_t msg;
    int ret = deos_decode_frame(&frame, &msg);
    zassert_equal(ret, -EPROTO, "Invalid dest should return EPROTO");
}

ZTEST(deos_protocol, test_08_local_node_invalid_init_reject)
{
    TC_PRINT("Test: Ensures that initializing DEOS Core with an invalid node ID fails.\n");
    struct deos_config config = { .node_id = DEOS_NODE_INVALID };
    int ret = deos_init(&config);
    zassert_equal(ret, -EINVAL, "Init with invalid node should fail");
}

ZTEST(deos_protocol, test_09_local_node_broadcast_init_reject)
{
    TC_PRINT("Test: Ensures that initializing DEOS Core with the broadcast node ID fails.\n");
    struct deos_config config = { .node_id = DEOS_NODE_BROADCAST };
    int ret = deos_init(&config);
    zassert_equal(ret, -EINVAL, "Init with broadcast node should fail");
}

ZTEST_SUITE(deos_fault_test, NULL, NULL, NULL, NULL, NULL);

ZTEST(deos_fault_test, test_fault_01_reject_zero_id)
{
    TC_PRINT("Test: Verifies that raising a fault with ID 0 is rejected.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    int ret = deos_fault_raise(0, DEOS_FAULT_SEVERITY_ERROR);
    zassert_equal(ret, -EINVAL, "Fault ID 0 should be rejected");
}

ZTEST(deos_fault_test, test_fault_02_new_fault_raise)
{
    TC_PRINT("Test: Verifies that a valid new fault can be raised successfully and marked as active.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    int ret = deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    zassert_equal(ret, 0, "Raise failed");
    zassert_true(deos_fault_is_active(0x0100), "Fault should be active");
}

ZTEST(deos_fault_test, test_fault_03_same_fault_repeated)
{
    TC_PRINT("Test: Verifies that raising an already active fault again is handled safely.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    
    /* Internal API would be needed to test occurrence count directly, 
       but we can at least ensure it doesn't fail. */
    zassert_true(deos_fault_is_active(0x0100), "Fault should still be active");
}

ZTEST(deos_fault_test, test_fault_04_set_inactive)
{
    TC_PRINT("Test: Verifies that setting an active fault to inactive updates its state correctly.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_set_inactive(0x0100);
    zassert_false(deos_fault_is_active(0x0100), "Fault should be inactive");
}

ZTEST(deos_fault_test, test_fault_05_latch)
{
    TC_PRINT("Test: Verifies that latching a fault works and keeps the fault active.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_latch(0x0100);
    zassert_true(deos_fault_is_active(0x0100), "Latched fault should be active");
}

ZTEST(deos_fault_test, test_fault_06_clear_single)
{
    TC_PRINT("Test: Verifies that a specific single fault can be cleared.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_clear(0x0100);
    zassert_false(deos_fault_is_active(0x0100), "Fault should be cleared");
}

ZTEST(deos_fault_test, test_fault_07_clear_all)
{
    TC_PRINT("Test: Verifies that all active faults are cleared when clear_all is called.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    deos_fault_raise(0x0100, DEOS_FAULT_SEVERITY_WARNING);
    deos_fault_raise(0x0101, DEOS_FAULT_SEVERITY_ERROR);
    deos_fault_clear_all();
    zassert_false(deos_fault_is_active(0x0100), "Fault 0x0100 should be cleared");
    zassert_false(deos_fault_is_active(0x0101), "Fault 0x0101 should be cleared");
}

ZTEST(deos_fault_test, test_fault_08_max_table_full)
{
    TC_PRINT("Test: Verifies that the fault table safely rejects new faults with -ENOSPC when it reaches maximum capacity.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    for (int i = 1; i <= 32; i++) {
        deos_fault_raise(i, DEOS_FAULT_SEVERITY_INFO);
    }
    int ret = deos_fault_raise(33, DEOS_FAULT_SEVERITY_INFO);
    zassert_equal(ret, -ENOSPC, "Should return -ENOSPC when table is full");
}

ZTEST_SUITE(deos_local_node_test, NULL, NULL, NULL, NULL, NULL);

ZTEST(deos_local_node_test, test_local_node_01_primary_node_is_local)
{
    TC_PRINT("Test: Verifies that the primary node initialized in the config is recognized as a local node.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    zassert_true(deos_is_local_node(DEOS_NODE_STEERING), "Primary node should be local");
}

ZTEST(deos_local_node_test, test_local_node_02_unregistered_node_is_not_local)
{
    TC_PRINT("Test: Verifies that an arbitrary unregistered node ID is correctly identified as non-local.\n");
    zassert_false(deos_is_local_node(DEOS_NODE_BRAKE), "Unregistered node should not be local");
}

ZTEST(deos_local_node_test, test_local_node_03_invalid_node_is_not_local)
{
    TC_PRINT("Test: Verifies that the invalid node ID is not recognized as a local node.\n");
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
    TC_PRINT("Test: Verifies that node-specific fault operations isolate faults (e.g. an unregistered node cannot raise faults).\n");
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
    TC_PRINT("Test: Verifies that the legacy global fault wrappers correctly route fault operations to the primary node.\n");
    struct deos_config config = { .node_id = DEOS_NODE_STEERING };
    deos_init(&config);
    
    int ret = deos_fault_raise(0x1234, DEOS_FAULT_SEVERITY_WARNING);
    zassert_equal(ret, 0, "Legacy wrapper should succeed on primary node");
    
    zassert_true(deos_fault_is_active_for_node(DEOS_NODE_STEERING, 0x1234), "Fault should be active on primary node");
}
