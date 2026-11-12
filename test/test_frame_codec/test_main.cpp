#include <unity.h>
#include <initializer_list>
#include <cstring>
#include "common/protocol/frame_codec.h"
using namespace adaptive_lora;
namespace {
Frame hello() {
    Frame f;
    f.networkId = 0x1234;
    f.sourceId = 2;
    f.destinationId = 1;
    f.bootId = 0x78563412;
    f.sequence = 0xABCD;
    return f;
}
uint8_t bytes[128];
size_t length;
Frame decoded;
void validBytes() { TEST_ASSERT_EQUAL(int(CodecResult::OK), int(encodeFrame(hello(), bytes, sizeof(bytes), length))); }
void expect(CodecResult result, size_t size = 28) {
    TEST_ASSERT_EQUAL(int(result), int(decodeFrame(bytes, size, 0x1234, 1, decoded)));
}
void test_golden_empty_hello_layout() {
    validBytes();
    const uint8_t golden[] = {0x4C,0x4D,1,1,0,28,0x34,0x12,2,0,1,0,0x12,0x34,0x56,0x78,
                             0,0,0,0,0xCD,0xAB,0,0,0,0,0,0};
    TEST_ASSERT_EQUAL_UINT(28, length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(golden, bytes, 28);
    expect(CodecResult::OK);
    TEST_ASSERT_EQUAL_UINT32(0x78563412, decoded.bootId);
    TEST_ASSERT_EQUAL_UINT16(0xABCD, decoded.sequence);
}
void test_payload_minimum_and_maximum() {
    for (uint8_t size : {uint8_t(0), uint8_t(1), uint8_t(68)}) {
        Frame f = hello(); f.payloadLength = size;
        for (unsigned i = 0; i < size; ++i) f.payload[i] = static_cast<uint8_t>(i);
        TEST_ASSERT_EQUAL(int(CodecResult::OK), int(encodeFrame(f, bytes, sizeof(bytes), length)));
        expect(CodecResult::OK, length);
        TEST_ASSERT_EQUAL_UINT(size, decoded.payloadLength);
        if (size != 0) TEST_ASSERT_EQUAL_UINT8_ARRAY(f.payload, decoded.payload, size);
    }
}
void test_encoder_capacity_and_null() {
    Frame f = hello();
    TEST_ASSERT_EQUAL(int(CodecResult::BUFFER), int(encodeFrame(f, bytes, 27, length)));
    TEST_ASSERT_EQUAL_UINT(0, length);
    TEST_ASSERT_EQUAL(int(CodecResult::BUFFER), int(encodeFrame(f, nullptr, 96, length)));
    TEST_ASSERT_EQUAL(int(CodecResult::BUFFER), int(decodeFrame(nullptr, 28, 1, 1, decoded)));
}
void test_all_header_truncations() { validBytes(); for (size_t i = 0; i < 28; ++i) expect(CodecResult::LENGTH, i); }
void test_payload_truncation_and_excess() {
    Frame f = hello(); f.payloadLength = 68;
    TEST_ASSERT_EQUAL(int(CodecResult::OK), int(encodeFrame(f, bytes, sizeof(bytes), length)));
    for (size_t i = 28; i < 96; ++i) expect(CodecResult::LENGTH, i);
    expect(CodecResult::LENGTH, 97);
    validBytes(); expect(CodecResult::LENGTH, 29);
}
void test_oversize_payload_rejected() {
    Frame f = hello(); f.payloadLength = 69;
    TEST_ASSERT_EQUAL(int(CodecResult::LENGTH), int(encodeFrame(f, bytes, 128, length)));
    validBytes(); bytes[26] = 255; expect(CodecResult::LENGTH);
}
void test_magic() { validBytes(); bytes[0] ^= 1; expect(CodecResult::MAGIC); }
void test_version() { validBytes(); bytes[2] = 2; expect(CodecResult::VERSION); }
void test_type_and_future_types() {
    for (uint8_t type : {uint8_t(0),uint8_t(3),uint8_t(0x10),uint8_t(0x11),uint8_t(0x7F),uint8_t(0xFF)}) {
        validBytes(); bytes[3] = type; expect(CodecResult::TYPE);
    }
}
void test_header_length() { validBytes(); bytes[5] = 27; expect(CodecResult::HEADER); }
void test_reserved_fields() {
    validBytes(); bytes[4] = 1; expect(CodecResult::RESERVED);
    validBytes(); bytes[27] = 1; expect(CodecResult::RESERVED);
}
void test_network_filter() {
    validBytes(); bytes[6] = 0; bytes[7] = 0; expect(CodecResult::NETWORK);
    validBytes(); TEST_ASSERT_EQUAL(int(CodecResult::NETWORK), int(decodeFrame(bytes, 28, 2, 1, decoded)));
}
void test_destination_and_source_ranges() {
    validBytes(); put16(bytes + 10, 65535); expect(CodecResult::DESTINATION);
    validBytes(); put16(bytes + 10, 0); expect(CodecResult::DESTINATION);
    validBytes(); put16(bytes + 8, 65535); expect(CodecResult::SOURCE);
    validBytes(); put16(bytes + 8, 0); expect(CodecResult::SOURCE);
    validBytes(); put16(bytes + 8, 1); expect(CodecResult::DESTINATION);
    validBytes(); TEST_ASSERT_EQUAL(int(CodecResult::DESTINATION), int(decodeFrame(bytes, 28, 0x1234, 3, decoded)));
}
void test_nonzero_session_and_m2_fields() {
    validBytes(); put32(bytes + 12, 0); expect(CodecResult::SESSION);
    for (unsigned offset : {16u,22u,24u}) { validBytes(); bytes[offset] = 1; expect(CodecResult::M2_FIELDS); }
}
Frame ack() {
    const Frame h = hello();
    Frame a; a.type = MessageType::ACK; a.networkId = h.networkId;
    a.sourceId = 1; a.destinationId = 2; a.bootId = 9;
    setAckPayload(a, h); return a;
}
void test_ack_echo_layout_and_match() {
    const Frame a = ack();
    TEST_ASSERT_EQUAL(int(CodecResult::OK), int(encodeFrame(a, bytes, sizeof(bytes), length)));
    const uint8_t payload[] = {2,0,0x12,0x34,0x56,0x78,0xCD,0xAB,0};
    TEST_ASSERT_EQUAL_UINT(37, length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, bytes + 28, 9);
    TEST_ASSERT_EQUAL(int(CodecResult::OK), int(decodeFrame(bytes, length, 0x1234, 2, decoded)));
    TEST_ASSERT_TRUE(ackMatches(decoded, hello(), 1));
}
void test_ack_wrong_identity_and_sender() {
    Frame a = ack(); a.payload[0] = 3; TEST_ASSERT_FALSE(ackMatches(a, hello(), 1));
    a = ack(); a.payload[2] ^= 1; TEST_ASSERT_FALSE(ackMatches(a, hello(), 1));
    a = ack(); a.payload[6] ^= 1; TEST_ASSERT_FALSE(ackMatches(a, hello(), 1));
    a = ack(); a.sourceId = 3; TEST_ASSERT_FALSE(ackMatches(a, hello(), 1));
    a = ack(); a.networkId = 4; TEST_ASSERT_FALSE(ackMatches(a, hello(), 1));
    a = ack(); a.destinationId = 3; TEST_ASSERT_FALSE(ackMatches(a, hello(), 1));
}
void test_ack_invalid_length_status_and_fields() {
    for (uint8_t size : {uint8_t(0),uint8_t(8),uint8_t(10)}) {
        Frame a = ack(); a.payloadLength = size;
        TEST_ASSERT_EQUAL(int(CodecResult::PAYLOAD), int(encodeFrame(a, bytes, sizeof(bytes), length)));
    }
    Frame a = ack(); a.payload[8] = 1;
    TEST_ASSERT_EQUAL(int(CodecResult::PAYLOAD), int(encodeFrame(a, bytes, sizeof(bytes), length)));
    a = ack(); put32(a.payload + 2, 0);
    TEST_ASSERT_EQUAL(int(CodecResult::PAYLOAD), int(encodeFrame(a, bytes, sizeof(bytes), length)));
}
void test_failed_decode_does_not_mutate_output() {
    validBytes(); decoded.bootId = 42; bytes[2] = 7;
    expect(CodecResult::VERSION); TEST_ASSERT_EQUAL_UINT32(42, decoded.bootId);
}
}
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_golden_empty_hello_layout);
    RUN_TEST(test_payload_minimum_and_maximum);
    RUN_TEST(test_encoder_capacity_and_null);
    RUN_TEST(test_all_header_truncations);
    RUN_TEST(test_payload_truncation_and_excess);
    RUN_TEST(test_oversize_payload_rejected);
    RUN_TEST(test_magic);
    RUN_TEST(test_version);
    RUN_TEST(test_type_and_future_types);
    RUN_TEST(test_header_length);
    RUN_TEST(test_reserved_fields);
    RUN_TEST(test_network_filter);
    RUN_TEST(test_destination_and_source_ranges);
    RUN_TEST(test_nonzero_session_and_m2_fields);
    RUN_TEST(test_ack_echo_layout_and_match);
    RUN_TEST(test_ack_wrong_identity_and_sender);
    RUN_TEST(test_ack_invalid_length_status_and_fields);
    RUN_TEST(test_failed_decode_does_not_mutate_output);
    return UNITY_END();
}
