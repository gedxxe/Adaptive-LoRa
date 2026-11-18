#include <unity.h>
#include <initializer_list>
#include "common/protocol/sequence_tracker.h"
#include "project_config.h"
using namespace adaptive_lora;
namespace {
void test_duplicate_identity() {
    SequenceTracker t;
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,3,4})));
    TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(t.observe({1,2,3,4})));
}
void test_all_identity_fields_participate() {
    SequenceTracker t;
    for (const PacketIdentity id : {PacketIdentity{1,2,3,4}, PacketIdentity{2,2,3,4},
                                   PacketIdentity{1,3,3,4}, PacketIdentity{1,2,4,4}, PacketIdentity{1,2,3,5}})
        TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe(id)));
    TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(t.observe({1,2,3,4})));
}
void test_out_of_order_window() {
    SequenceTracker t;
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,3,100})));
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,3,37})));
    TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(t.observe({1,2,3,37})));
    TEST_ASSERT_EQUAL(int(SequenceResult::STALE), int(t.observe({1,2,3,36})));
}
void test_large_jump_and_no_wrap_in_same_session() {
    SequenceTracker t;
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,3,0})));
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,3,65535})));
    TEST_ASSERT_EQUAL(int(SequenceResult::STALE), int(t.observe({1,2,3,0})));
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,4,0})));
}
void test_sequence_exhaustion_requires_new_session() {
    SequenceCounter c; uint16_t sequence = 100;
    TEST_ASSERT_FALSE(c.take(sequence)); c.begin(123);
    for (uint32_t i = 0; i <= 65535; ++i) {
        TEST_ASSERT_TRUE(c.take(sequence)); TEST_ASSERT_EQUAL_UINT16(i, sequence);
    }
    TEST_ASSERT_FALSE(c.take(sequence)); TEST_ASSERT_FALSE(c.take(sequence));
    TEST_ASSERT_EQUAL_UINT32(123, c.bootId());
    c.begin(124); TEST_ASSERT_TRUE(c.take(sequence)); TEST_ASSERT_EQUAL_UINT16(0, sequence);
}
void test_bounded_session_replacement() {
    SequenceTracker t;
    for (uint32_t boot = 1; boot <= 9; ++boot)
        TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,boot,0})));
    TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(t.observe({1,2,9,0})));
    // Evicted sessions have no retained replay state; this is a bounded M2 cache.
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(t.observe({1,2,1,0})));
}
void test_wrap_safe_deadline() {
    TEST_ASSERT_FALSE(deadlineReached(0xFFFFFFF0, 0x10));
    TEST_ASSERT_TRUE(deadlineReached(0x10, 0x10));
    TEST_ASSERT_TRUE(deadlineReached(0x11, 0x10));
    TEST_ASSERT_FALSE(deadlineReached(0xFFFFFF00, 0xFFFFFF01));
}
void test_one_five_eight_clients_retain_replay_windows() {
    for (const unsigned count : {1u, 5u, 8u}) {
        SequenceTracker tracker;
        for (unsigned i = 0; i < count; ++i)
            TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(tracker.observe({1, uint16_t(i + 2), 1, 100})));
        for (unsigned i = 0; i < count; ++i) {
            const uint16_t source = uint16_t(i + 2);
            TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(tracker.observe({1, source, 1, 100})));
            TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(tracker.observe({1, source, 1, 99})));
            TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(tracker.observe({1, source, 1, 99})));
        }
    }
}
void test_nine_ten_seventy_sessions_eviction_and_replay_thrash() {
    for (const unsigned count : {9u, 10u, 70u}) {
        SequenceTracker tracker;
        for (unsigned i = 0; i < count; ++i)
            TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(tracker.observe({1, uint16_t(i + 2), 1, 7})));
        for (unsigned i = count - 8; i < count; ++i)
            TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(tracker.observe({1, uint16_t(i + 2), 1, 7})));
        // Replaying in insertion order evicts the next old key before it arrives.
        // This characterizes the M2 cache; it is no multi-client delivery guarantee.
        for (unsigned i = 0; i < count; ++i)
            TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(tracker.observe({1, uint16_t(i + 2), 1, 7})));
    }
}
void test_boot_churn_competes_with_other_clients_for_capacity() {
    SequenceTracker tracker;
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(tracker.observe({1, 3, 1, 9})));
    for (uint32_t boot = 1; boot <= 7; ++boot)
        TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(tracker.observe({1, 2, boot, 0})));
    TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(tracker.observe({1, 3, 1, 9})));
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(tracker.observe({1, 2, 8, 0})));
    TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(tracker.observe({1, 2, 8, 0})));
    TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(tracker.observe({1, 3, 1, 9})));
}
void test_multiple_clients_keep_old_and_renewed_sessions_distinct() {
    SequenceTracker tracker;
    for (uint16_t source = 2; source <= 4; ++source) {
        TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(tracker.observe({1, source, 10, 65535})));
        TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(tracker.observe({1, source, 11, 0})));
    }
    for (uint16_t source = 2; source <= 4; ++source) {
        TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(tracker.observe({1, source, 10, 65535})));
        TEST_ASSERT_EQUAL(int(SequenceResult::STALE), int(tracker.observe({1, source, 10, 0})));
        TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(tracker.observe({1, source, 11, 0})));
    }
}
void test_gateway_recreation_loses_all_volatile_session_history() {
    SequenceTracker before;
    for (uint16_t source = 2; source <= 6; ++source) {
        TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(before.observe({1, source, 10, 123})));
        TEST_ASSERT_EQUAL(int(SequenceResult::DUPLICATE), int(before.observe({1, source, 10, 123})));
    }
    SequenceTracker after;
    for (uint16_t source = 2; source <= 6; ++source)
        TEST_ASSERT_EQUAL(int(SequenceResult::NEW), int(after.observe({1, source, 10, 123})));
}
}
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_duplicate_identity); RUN_TEST(test_all_identity_fields_participate);
    RUN_TEST(test_out_of_order_window); RUN_TEST(test_large_jump_and_no_wrap_in_same_session);
    RUN_TEST(test_sequence_exhaustion_requires_new_session); RUN_TEST(test_bounded_session_replacement);
    RUN_TEST(test_wrap_safe_deadline);
    RUN_TEST(test_one_five_eight_clients_retain_replay_windows);
    RUN_TEST(test_nine_ten_seventy_sessions_eviction_and_replay_thrash);
    RUN_TEST(test_boot_churn_competes_with_other_clients_for_capacity);
    RUN_TEST(test_multiple_clients_keep_old_and_renewed_sessions_distinct);
    RUN_TEST(test_gateway_recreation_loses_all_volatile_session_history);
    return UNITY_END();
}
