// SPDX-License-Identifier: GPL-3.0-only
#include <Arduino.h>
#include <unity.h>
#include <algorithm>
#include <array>
#include <cstring>
#include "app/firmware_runtime.h"
#include "common/radio/sx1262_hardware.h"
#include "../support/fake_radio_hardware.h"
#include "../support/fake_radio_transport.h"

using namespace adaptive_lora;
using namespace adaptive_lora::multiclient;
using namespace adaptive_lora::secure;
using namespace adaptive_lora::test;

#if defined(MBEDTLS_PLATFORM_ZEROIZE_ALT)
extern "C" void mbedtls_platform_zeroize(void* buffer, size_t size) {
    volatile auto* byte = static_cast<volatile uint8_t*>(buffer);
    while (size--) *byte++ = 0;
}
#endif

namespace {
constexpr uint8_t publicPoint[publicKeyBytes]{
    0x04, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5,
    0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0, 0xf4,
    0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96, 0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a,
    0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33,
    0x57, 0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5};
uint8_t entropyFixture = 0;
int entropy(void*, unsigned char* out, size_t length) {
    // Deterministic, unique test boots. Production has the ESP32 startup source.
    const uint8_t base = ++entropyFixture;
    for (size_t i = 0; i < length; ++i) out[i] = uint8_t(i + base);
    return 0;
}
int unavailableEntropy(void*, unsigned char*, size_t) { return -1; }
uint32_t unusedBoot(uint32_t) { return 0; }

DeviceConfig config(uint16_t id) {
    DeviceConfig c;
    c.role = id == 1 ? DeviceRole::GATEWAY : DeviceRole::CLIENT;
    c.nodeId = id;
    return c;
}

struct Storage final : ConfigStorage {
    uint8_t normal[kConfigBlobSize]{};
    uint8_t record[28 + privateKeyBytes + keyBytes * kPopulationLimit]{};
    size_t length = 0;
    bool credential = false;
    bool shortRead = false;
    bool missing = false;
    bool legacyRoleOnly = false;
    explicit Storage(DeviceConfig c, uint16_t count = 1) {
        encodeConfig(c, normal, sizeof(normal));
        std::memcpy(record, "AL07", 4);
        record[4] = uint8_t(c.role);
        put16(record + 5, c.networkId);
        put16(record + 7, c.gatewayId);
        put16(record + 9, c.nodeId);
        record[11] = uint8_t(count);
        record[12] = 0x88;
        if (c.role == DeviceRole::GATEWAY) {
            length = 28 + privateKeyBytes + keyBytes * count;
            record[28 + privateKeyBytes - 1] = 1; // Public test scalar for the curve generator.
            for (uint16_t i = 0; i < count; ++i)
                std::memset(record + 28 + privateKeyBytes + i * keyBytes, i + 1, keyBytes);
        } else {
            length = 28 + publicKeyBytes + keyBytes;
            std::memcpy(record + 28, publicPoint, publicKeyBytes);
            std::memset(record + 28 + publicKeyBytes, c.nodeId - 1, keyBytes);
        }
    }
    bool begin(const char* name, bool) override {
        credential = !std::strcmp(name, "alora_sec");
        return credential ? !missing : (!legacyRoleOnly && !std::strcmp(name, "alora_v7cfg"));
    }
    size_t getBytesLength(const char*) override { return credential ? length : sizeof(normal); }
    size_t getBytes(const char*, void* output, size_t capacity) override {
        const size_t size = getBytesLength(nullptr);
        if (capacity < size) return 0;
        std::memcpy(output, credential ? record : normal, size);
        return shortRead && credential ? size - 1 : size;
    }
    size_t putBytes(const char*, const void*, size_t) override {
        TEST_FAIL_MESSAGE("secure startup must never repair or allocate credentials by writing NVS");
        return 0;
    }
    void end() override {}
};

void rfc3610_ccm_known_answer() {
    const uint8_t key[]{0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf};
    const uint8_t nonce[]{0,0,0,3,2,1,0,0xa0,0xa1,0xa2,0xa3,0xa4,0xa5};
    uint8_t input[31];
    for (uint8_t i = 0; i < sizeof(input); ++i) input[i] = i;
    const uint8_t expected[]{0x58,0x8c,0x97,0x9a,0x61,0xc6,0x63,0xd2,0xf0,0x66,0xd0,0xc2,0xc0,0xf9,0x89,0x80,
        0x6d,0x5f,0x6b,0x61,0xda,0xc3,0x84,0x17,0xe8,0xd1,0x2c,0xfd,0xf9,0x26,0xe0};
    constexpr size_t aad = 8, body = sizeof(input) - aad;
    uint8_t result[sizeof(expected)]{}, opened[body]{};
    TEST_ASSERT_TRUE(ccm(true, key, nonce, input, aad, input + aad, result, body, result + body, 8));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, result, sizeof(result));
    TEST_ASSERT_TRUE(ccm(false, key, nonce, input, aad, result, opened, body, result + body, 8));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(input + aad, opened, body);
    ++result[body];
    TEST_ASSERT_FALSE(ccm(false, key, nonce, input, aad, result, opened, body, result + body, 8));
}

void rfc5869_hkdf_known_answer() {
    uint8_t key[22], salt[13], info[10], out[42];
    std::memset(key, 0x0b, sizeof(key));
    for (uint8_t i = 0; i < sizeof(salt); ++i) salt[i] = i;
    for (uint8_t i = 0; i < sizeof(info); ++i) info[i] = uint8_t(0xf0 + i);
    const uint8_t expected[]{0x3c,0xb2,0x5f,0x25,0xfa,0xac,0xd5,0x7a,0x90,0x43,0x4f,0x64,0xd0,0x36,0x2f,0x2a,
        0x2d,0x2d,0x0a,0x90,0xcf,0x1a,0x5a,0x4c,0x5d,0xb0,0x2d,0x56,0xec,0xc4,0xc5,0xbf,
        0x34,0x00,0x72,0x08,0xd5,0xb8,0x87,0x18,0x58,0x65};
    TEST_ASSERT_TRUE(hkdf(key, sizeof(key), salt, sizeof(salt), info, sizeof(info), out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, sizeof(out));
}

struct Peer {
    const DeviceConfig device;
    Storage storage;
    SecureLink security;
    FakeRadioTransport radio;
    SerialLogger log;
    PayloadPort payload;
    MulticlientLink link;
    Peer(uint16_t id, PayloadPort::Receiver receiver = nullptr, void* context = nullptr, uint16_t count = 1)
        : device(config(id)), storage(device, count), payload(receiver, context),
          link(device, radio, log, payload, unusedBoot, count, 32, security) {
        TEST_ASSERT_NULL(security.load(storage, device, entropy));
        log.begin(device);
    }
};

struct Network {
    unsigned deliveries = 0;
    bool losePrimary = false;
    MessageType tamper = MessageType::HELLO;
    Peer gateway;
    Peer client;
    Network(uint16_t count = 1, uint16_t id = 2) : gateway(1, accept, this, count), client(id, nullptr, nullptr, count) {}
    CapturedPacket beacon, grant, request, data, ack;
    unsigned dataAttempts = 0;
    uint64_t now = 0;
    uint64_t txEnd = 0;
    uint64_t clientFreeAt = 0;
    uint32_t signingDelay = 0;
    uint32_t verificationDelay = 0;
    Peer* sender = nullptr;
    static bool accept(void* context, const PayloadDelivery& data) {
        TEST_ASSERT_EQUAL_UINT8(4, data.size);
        TEST_ASSERT_EQUAL_UINT8(0x42, data.bytes[0]);
        ++static_cast<Network*>(context)->deliveries;
        return true;
    }
    void submit() {
        const uint8_t sample[]{0x42, 0x61, 0x74, 0x61};
        TEST_ASSERT_EQUAL_INT(int(SubmitResult::QUEUED), int(client.payload.submit(sample, sizeof(sample))));
    }
    void tick() {
        host_clock::set(now);
        RadioEvent events[2];
        if (sender && now >= txEnd) {
            auto* receiver = sender == &gateway ? &client : &gateway;
            auto packet = sender->radio.lastTx;
            const auto type = MessageType(packet.bytes[3]);
            const auto source = sender == &gateway ? 0 : 1;
            events[source] = sender->radio.completeTx();
            const bool drop = type == MessageType::DATA && losePrimary && dataAttempts == 1;
            if (type == tamper) ++packet.bytes[packet.length - 1];
            if (!drop && receiver->radio.receiving() && receiver->radio.sf == sender->radio.sf)
                events[1 - source] = receiver->radio.deliver(packet);
            for (auto& event : events) { event.hasCompletionTime = true; event.completionUs = txEnd; }
            sender = nullptr;
        }
        // Gateway time advances while sealing. The Client has its own clock.
        if (events[0].type != RadioEventType::RX_DONE) host_clock::delayAfterNextRead(signingDelay);
        gateway.link.update(now, events[0]);
        if (now >= clientFreeAt) {
            host_clock::set(now);
            host_clock::delayAfterNextRead(events[1].type == RadioEventType::RX_DONE ? verificationDelay : 0);
            client.link.update(now, events[1]);
            clientFreeAt = host_clock::value();
        } else {
            TEST_ASSERT_EQUAL_INT(int(RadioEventType::NONE), int(events[1].type));
        }
        host_clock::delayAfterNextRead(0);
        for (auto* peer : {&gateway, &client}) {
            while (!peer->log.empty()) peer->log.drain();
            if (!peer->radio.transmitting() || peer == sender) continue;
            TEST_ASSERT_NULL(sender);
            sender = peer;
            const auto& packet = peer->radio.lastTx;
            txEnd = peer->radio.lastTxStartedAt + airAtSf(packet.length, peer->radio.sf);
            switch (MessageType(packet.bytes[3])) {
                case MessageType::BEACON: beacon = packet; break;
                case MessageType::SLOT_GRANT: grant = packet; break;
                case MessageType::PAYLOAD_REQUEST: request = packet; break;
                case MessageType::ACK_BITMAP: if (!ack.length) ack = packet; break;
                case MessageType::DATA:
                    if (dataAttempts) {
                        TEST_ASSERT_EQUAL_UINT(data.length, packet.length);
                        TEST_ASSERT_EQUAL_UINT8_ARRAY(data.bytes, packet.bytes, data.length);
                    }
                    data = packet;
                    ++dataAttempts;
                    break;
                default: TEST_FAIL_MESSAGE("unexpected secure packet");
            }
        }
        Serial.output.clear();
    }
    void frame() {
        for (; now < kAdaptivePeriodUs; now += 2000) tick();
    }
};

void secure_exchange_and_retry_reuse_ciphertext() {
    Network net;
    net.losePrimary = true;
    net.submit();
    net.frame();
    TEST_ASSERT_EQUAL_UINT(1, net.deliveries);
    TEST_ASSERT_EQUAL_UINT(2, net.dataAttempts);
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(net.client.payload.result()));
    TEST_ASSERT_EQUAL_UINT(kHeaderSize + kPayloadPrefix + 4 + tagBytes, net.data.length);
    TEST_ASSERT_NOT_EQUAL(0, std::memcmp(net.data.bytes + kHeaderSize + kPayloadPrefix, "\x42\x61\x74\x61", 4));
}

void measured_signing_delay_allows_signed_control_and_retries() {
    for (const auto delay : {uint32_t(121595), signingBudgetUs}) {
        Network net;
        net.signingDelay = delay;
        net.verificationDelay = verificationBudgetUs;
        net.losePrimary = true;
        net.submit();
        net.frame();
        TEST_ASSERT_EQUAL_UINT(1, net.deliveries);
        TEST_ASSERT_EQUAL_UINT(2, net.dataAttempts);
        TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(net.client.payload.result()));
        TEST_ASSERT_EQUAL_UINT(0, net.gateway.link.telemetry().cryptoLate);
        TEST_ASSERT_EQUAL_UINT(net.signingDelay, net.gateway.link.telemetry().worstCryptoTxUs);
        TEST_ASSERT_EQUAL_UINT(net.verificationDelay, net.client.link.telemetry().worstCryptoRxUs);
    }
}

void altered_beacon_request_grant_ack_and_data_are_rejected() {
    for (auto type : {MessageType::BEACON, MessageType::PAYLOAD_REQUEST, MessageType::SLOT_GRANT,
                      MessageType::DATA, MessageType::ACK_BITMAP}) {
        Network net;
        net.tamper = type;
        net.submit();
        net.frame();
        if (type == MessageType::BEACON) TEST_ASSERT_EQUAL_UINT(0, net.client.link.telemetry().frame);
        if (type == MessageType::ACK_BITMAP) TEST_ASSERT_EQUAL_UINT(1, net.deliveries);
        else TEST_ASSERT_EQUAL_UINT(0, net.deliveries);
        TEST_ASSERT_NOT_EQUAL(int(PayloadResult::ACCEPTED), int(net.client.payload.result()));
        TEST_ASSERT_GREATER_THAN_UINT(0, net.client.link.telemetry().authDrops + net.gateway.link.telemetry().authDrops);
    }
}

void failed_read_and_invalid_provisioning_keep_radio_off() {
    for (auto fault : {0, 1, 2, 3, 4}) {
        Storage storage(config(1));
        storage.missing = fault == 0;
        storage.shortRead = fault == 1;
        if (fault == 2) storage.record[6] ^= 1;
        storage.legacyRoleOnly = fault == 4;
        FakeRadioHardware radio;
        FirmwareRuntime runtime(storage, radio, false, nullptr, nullptr, nullptr,
                                fault == 3 ? unavailableEntropy : entropy);
        runtime.setup();
        runtime.loop();
        const uint8_t sample = 1;
        TEST_ASSERT_EQUAL_INT(int(SubmitResult::INACTIVE), int(runtime.submitPayload(&sample, 1)));
        TEST_ASSERT_EQUAL_UINT(0, radio.count(HardwareCall::BEGIN));
    }
    Storage provisioned(config(2));
    FakeRadioHardware radio;
    FirmwareRuntime runtime(provisioned, radio, false, nullptr, nullptr, nullptr, entropy);
    runtime.setup();
    const uint8_t sample = 1;
    TEST_ASSERT_EQUAL_UINT(1, radio.count(HardwareCall::BEGIN));
    TEST_ASSERT_EQUAL_INT(int(SubmitResult::QUEUED), int(runtime.submitPayload(&sample, 1)));
}

void old_grant_cannot_activate_a_restarted_client() {
    Network net;
    net.submit();
    net.frame();
    TEST_ASSERT_EQUAL_UINT(1, net.deliveries);
    Peer restarted(2);
    uint8_t bytes[4]{0x42};
    restarted.payload.submit(bytes, sizeof(bytes));
    host_clock::set(0);
    restarted.link.update(0, {});
    const auto beaconAt = beaconAir(1);
    host_clock::set(beaconAt);
    restarted.link.update(beaconAt, restarted.radio.deliver(net.beacon));
    const auto requestAt = beaconAt + requestStart(0);
    host_clock::set(requestAt);
    restarted.link.update(requestAt, {});
    TEST_ASSERT_TRUE(restarted.radio.transmitting());
    const auto reqDone = requestAt + requestAir();
    host_clock::set(reqDone);
    restarted.link.update(reqDone, restarted.radio.completeTx());
    const auto grantAt = beaconAt + grantStart(1) + grantAir(1);
    host_clock::set(grantAt);
    restarted.link.update(grantAt, restarted.radio.deliver(net.grant));
    TEST_ASSERT_EQUAL_UINT(1, restarted.link.telemetry().authDrops);
    TEST_ASSERT_FALSE(restarted.link.telemetry().peer.seen);
    TEST_ASSERT_EQUAL_UINT(1, restarted.radio.successfulTxStarts);
}

void wire_shape_and_gateway_restart_reject_old_requests() {
    Network net;
    net.submit();
    net.frame();
    const Roster group{kAdaptivePeriodUs, 1, {{2, 32}}};
    Frame decoded;
    uint8_t auxiliary[challengeBytes]{};
    TEST_ASSERT_TRUE(net.gateway.security.open(net.request.bytes, net.request.length, group, 0, decoded, auxiliary));
    TEST_ASSERT_FALSE(net.gateway.security.open(net.request.bytes, net.request.length - 1, group, 0, decoded, auxiliary));
    const uint8_t original = net.request.bytes[2];
    net.request.bytes[2] = 6;
    TEST_ASSERT_FALSE(net.gateway.security.open(net.request.bytes, net.request.length, group, 0, decoded, auxiliary));
    net.request.bytes[2] = original;
    TEST_ASSERT_EQUAL_UINT16(kScheduleVersion, get16(net.request.bytes + 24));
    put16(net.request.bytes + 24, 1);
    TEST_ASSERT_EQUAL_INT(int(CodecResult::RESERVED),
        int(decode(net.request.bytes, kHeaderSize + kRequestSize, 1, 1, decoded)));
    put16(net.request.bytes + 24, kScheduleVersion);
    net.request.bytes[12] ^= 1;
    TEST_ASSERT_FALSE(net.gateway.security.open(net.request.bytes, net.request.length, group, 0, decoded, auxiliary));
    net.request.bytes[12] ^= 1;
    Peer restarted(1);
    TEST_ASSERT_NOT_EQUAL(0, restarted.security.newBoot(0));
    TEST_ASSERT_FALSE(restarted.security.open(net.request.bytes, net.request.length, group, 0, decoded, auxiliary));
}

void data_rejects_each_altered_byte_and_all_short_lengths() {
    Network net;
    net.submit();
    net.frame();
    const Roster group{kAdaptivePeriodUs, 1, {{2, 32}}};
    Frame decoded;
    uint8_t auxiliary[challengeBytes]{};
    TEST_ASSERT_TRUE(net.gateway.security.open(net.data.bytes, net.data.length, group, 0, decoded, auxiliary));
    for (size_t size = 0; size < net.data.length; ++size)
        TEST_ASSERT_FALSE(net.gateway.security.open(net.data.bytes, size, group, 0, decoded, auxiliary));
    for (size_t i = 0; i < net.data.length; ++i) {
        net.data.bytes[i] ^= 1;
        TEST_ASSERT_FALSE(net.gateway.security.open(net.data.bytes, net.data.length, group, 0, decoded, auxiliary));
        net.data.bytes[i] ^= 1;
    }
    TEST_ASSERT_EQUAL_UINT(1, net.deliveries);
}

void restored_credentials_negotiate_fresh_keys_after_both_reboot() {
    Network before;
    before.submit();
    before.frame();
    Network after; // Same persisted provisioning, new entropy for each role.
    after.submit();
    after.frame();
    TEST_ASSERT_EQUAL_UINT(1, before.deliveries);
    TEST_ASSERT_EQUAL_UINT(1, after.deliveries);
    const Roster group{kAdaptivePeriodUs, 1, {{2, 32}}};
    Frame decoded;
    uint8_t auxiliary[challengeBytes]{};
    TEST_ASSERT_FALSE(after.gateway.security.open(before.data.bytes, before.data.length, group, 0, decoded, auxiliary));
    TEST_ASSERT_NOT_EQUAL(0, std::memcmp(before.data.bytes + kHeaderSize, after.data.bytes + kHeaderSize,
                                         before.data.length - kHeaderSize));
}

void tentative_epoch_does_not_clear_confirmed_delivery() {
    Network previous;
    previous.submit();
    previous.frame();
    Network current;
    current.submit();
    current.frame();
    TEST_ASSERT_EQUAL_UINT(1, current.deliveries);
    const auto confirmed = current.client.link.telemetry();
    TEST_ASSERT_TRUE(confirmed.peer.seen);
    const auto later = current.now + 2 * kAdaptivePeriodUs;
    host_clock::set(later);
    current.client.link.update(later, current.client.radio.deliver(previous.beacon));
    const auto tentative = current.client.link.telemetry();
    TEST_ASSERT_TRUE(tentative.peer.seen);
    TEST_ASSERT_EQUAL_UINT64(confirmed.peer.atUs, tentative.peer.atUs);
    TEST_ASSERT_EQUAL_UINT8(confirmed.confirmedDataSf, tentative.confirmedDataSf);

    const Roster group{kAdaptivePeriodUs, 1, {{2, 32}}};
    Frame live;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::OK),
        int(decode(current.beacon.bytes, kHeaderSize + current.beacon.bytes[26], 1, 2, live)));
    ++live.frameId;
    CapturedPacket packet;
    TEST_ASSERT_TRUE(current.gateway.security.seal(live, group, 0, packet.bytes, packet.length));
    host_clock::set(later + 2000);
    current.client.link.update(later + 2000, current.client.radio.deliver(packet));
    TEST_ASSERT_EQUAL_UINT(live.frameId, current.client.link.telemetry().frame);
    TEST_ASSERT_EQUAL_UINT64(confirmed.peer.atUs, current.client.link.telemetry().peer.atUs);
}

void wrong_and_revoked_client_credentials_are_rejected() {
    Network net;
    net.submit();
    net.frame();
    const Roster group{kAdaptivePeriodUs, 1, {{2, 32}}};
    Storage wrong(config(2));
    wrong.record[28 + publicKeyBytes] ^= 1;
    SecureLink other;
    TEST_ASSERT_NULL(other.load(wrong, config(2), entropy));
    Frame frame;
    uint8_t auxiliary[challengeBytes]{};
    TEST_ASSERT_TRUE(other.open(net.beacon.bytes, net.beacon.length, group, 0, frame, auxiliary));
    other.acceptBeacon(auxiliary, frame.frameId);
    TEST_ASSERT_TRUE(net.gateway.security.open(net.request.bytes, net.request.length, group, 0, frame, auxiliary));
    CapturedPacket wrongRequest;
    TEST_ASSERT_TRUE(other.seal(frame, group, 0, wrongRequest.bytes, wrongRequest.length));
    TEST_ASSERT_FALSE(net.gateway.security.open(wrongRequest.bytes, wrongRequest.length, group, 0, frame, auxiliary));

    Storage revoked(config(1), 2);
    std::memset(revoked.record + 28 + privateKeyBytes, 0, keyBytes);
    SecureLink gatewayWithRevocation;
    TEST_ASSERT_NULL(gatewayWithRevocation.load(revoked, config(1), entropy));
    TEST_ASSERT_NOT_EQUAL(0, gatewayWithRevocation.newBoot(0));
    TEST_ASSERT_FALSE(gatewayWithRevocation.open(net.request.bytes, net.request.length, group, 0, frame, auxiliary));
}

void last_registered_client_waits_and_delivers_on_its_group() {
    Network net(kPopulationLimit, 129);
    net.submit();
    net.frame();
    TEST_ASSERT_TRUE(net.client.payload.pending());
    TEST_ASSERT_EQUAL_UINT(0, net.client.radio.successfulTxStarts);
    uint8_t profiles[kPopulationLimit];
    std::fill_n(profiles, kPopulationLimit, kBaseSf);
    Rotation schedule{kPopulationLimit};
    uint64_t end = 0;
    do {
        TEST_ASSERT_GREATER_THAN_UINT(0, schedule.select(1, 32, profiles).count);
        end += kAdaptivePeriodUs;
    } while (schedule.next);
    for (; net.now < end; net.now += 2000) net.tick();
    TEST_ASSERT_EQUAL_UINT(1, net.deliveries);
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(net.client.payload.result()));
}

void missed_crypto_deadline_does_not_start_radio_tx() {
    Peer gateway(1);
    host_clock::set(0);
    gateway.link.update(0, {}); // Let the adapter retune to control SF first.
    host_clock::delayAfterNextRead(kControlLaunchUs);
    gateway.link.update(0, {});
    TEST_ASSERT_EQUAL_UINT(0, gateway.radio.successfulTxStarts);
    TEST_ASSERT_EQUAL_UINT(1, gateway.link.telemetry().cryptoLate);
}

void capacity_accounts_for_each_secure_frame_and_guard() {
    for (uint16_t count : {1, 4, 8, 16, 32, 63, 64, 70, 84, 96, 128}) {
        for (uint8_t profile = kBaseSf; profile <= phy::maxDataSf + 1; ++profile) {
            std::array<uint8_t, kPopulationLimit> profiles;
            for (uint16_t i = 0; i < profiles.size(); ++i) {
                profiles[i] = profile <= phy::maxDataSf ? profile :
                    static_cast<uint8_t>(kBaseSf + i % (phy::maxDataSf - kBaseSf + 1));
            }
            Rotation rotation{count};
            unsigned groups = 0, visits = 0;
            do {
                const auto roster = rotation.select(1, 32, profiles.data());
                TEST_ASSERT_TRUE(roster.valid(1));
                Plan ceiling;
                ceiling.count = roster.count;
                for (uint8_t i = 0; i < roster.count; ++i) {
                    TEST_ASSERT_EQUAL_UINT16(visits + 2, roster.nodes[i].id);
                    ceiling.slots[i] = {1, 32, profiles[visits++]};
                }
                TEST_ASSERT_TRUE(ceiling.matches(roster));
                TEST_ASSERT_LESS_OR_EQUAL_UINT(maxPacket, kHeaderSize + grantBytes(roster.count) +
                                               roster.count * echoBytes + signatureBytes);
                ++groups;
            } while (rotation.next);
            TEST_ASSERT_EQUAL_UINT(count, visits);
            std::printf("SECURE_CAPACITY,n=%u,", unsigned(count));
            if (profile <= phy::maxDataSf) std::printf("sf=%u,", unsigned(profile));
            else std::printf("profile=mixed,");
            std::printf("sweep_s=%lu\n", static_cast<unsigned long>(groups) * kAdaptivePeriodUs / 1000000u);
        }
    }
}

void radio_stages_secure_packet_and_propagates_bounded_launch_failure() {
    auto& calls = host_adapter::state();
    calls = {};
    Sx1262Hardware chip;
    uint8_t bytes[maxPacket]{};
    calls.codes[host_adapter::TX] = -123;
    TEST_ASSERT_EQUAL_INT(-123, chip.startTransmit(bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_UINT(0, calls.calls[host_adapter::SET_TX]);
    calls.codes[host_adapter::TX] = 0;
    calls.codes[host_adapter::SET_TX] = -456;
    TEST_ASSERT_EQUAL_INT(-456, chip.startTransmit(bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_UINT(1, calls.calls[host_adapter::SET_TX]);
    TEST_ASSERT_EQUAL_UINT(maxPacket, calls.txLength);
    TEST_ASSERT_EQUAL_UINT(RADIOLIB_SX126X_CMD_SET_TX, calls.spiCommand);
    TEST_ASSERT_EQUAL_UINT(kTxTimeoutUs * uint64_t(64) / 1000, calls.hardwareTxTicks);
}
}

void setUp() { Serial.output.clear(); }
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(rfc3610_ccm_known_answer);
    RUN_TEST(rfc5869_hkdf_known_answer);
    RUN_TEST(secure_exchange_and_retry_reuse_ciphertext);
    RUN_TEST(measured_signing_delay_allows_signed_control_and_retries);
    RUN_TEST(altered_beacon_request_grant_ack_and_data_are_rejected);
    RUN_TEST(failed_read_and_invalid_provisioning_keep_radio_off);
    RUN_TEST(old_grant_cannot_activate_a_restarted_client);
    RUN_TEST(wire_shape_and_gateway_restart_reject_old_requests);
    RUN_TEST(data_rejects_each_altered_byte_and_all_short_lengths);
    RUN_TEST(restored_credentials_negotiate_fresh_keys_after_both_reboot);
    RUN_TEST(tentative_epoch_does_not_clear_confirmed_delivery);
    RUN_TEST(wrong_and_revoked_client_credentials_are_rejected);
    RUN_TEST(last_registered_client_waits_and_delivers_on_its_group);
    RUN_TEST(missed_crypto_deadline_does_not_start_radio_tx);
    RUN_TEST(capacity_accounts_for_each_secure_frame_and_guard);
    RUN_TEST(radio_stages_secure_packet_and_propagates_bounded_launch_failure);
    return UNITY_END();
}
