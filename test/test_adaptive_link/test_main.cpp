#include <Arduino.h>
#include <unity.h>
#include <vector>
#include "app/multiclient_link.h"
#include "common/diagnostics/oled_status_display.h"
#include "common/radio/radio_driver.h"
#include "common/radio/sx1262_hardware.h"
#include "../support/fake_radio_transport.h"

using namespace adaptive_lora;
using namespace adaptive_lora::multiclient;
using namespace adaptive_lora::test;

namespace {
uint32_t identity = 0;
uint32_t boot(uint32_t) { return ++identity; }

DeviceConfig config(uint16_t id) {
    DeviceConfig device;
    device.role = id == 1 ? DeviceRole::GATEWAY : DeviceRole::CLIENT;
    device.nodeId = id;
    return device;
}

Roster roster(uint8_t count) {
    Roster result{kAdaptivePeriodUs, count, {}};
    for (uint8_t i = 0; i < count; ++i) result.nodes[i] = {uint16_t(i + 2), kMaxData};
    return result;
}

struct Peer {
    FakeRadioTransport radio;
    SerialLogger log;
    PayloadPort payload;
    MulticlientLink link;

    Peer(uint16_t id, Roster schedule = {}, PayloadPort::Receiver callback = nullptr, void* context = nullptr)
        : payload(callback, context), link(config(id), radio, log, payload, boot, schedule) { log.begin(config(id)); }
};

struct Tx {
    MessageType type;
    uint8_t sender;
    uint8_t sf;
    uint32_t frame;
    uint16_t round;
};

struct Network {
    unsigned delivered[kMaxClients]{};
    uint8_t minDataSf[kMaxClients]{};
    float snr[kMaxClients]{};
    uint8_t count;
    Peer gateway;
    Peer clients[kMaxClients]{{2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}};
    std::vector<Tx> tx;
    uint64_t now = 0;
    uint64_t txEnd = 0;
    int sender = -1;
    uint16_t listening = 0;
    uint8_t txSf = 0;
    Frame frame;
    bool dropGrant = false;
    bool dropFirstAck = false;
    bool dropAllAck = false;
    bool outage = false;

    explicit Network(uint8_t n, uint8_t rosterCount = 0)
        : count(n), gateway(1, roster(rosterCount ? rosterCount : n), accept, this) {}
    static bool accept(void* context, const PayloadDelivery& data) {
        auto& n = *static_cast<Network*>(context);
        ++n.delivered[data.source - 2];
        TEST_ASSERT_EQUAL_UINT16(uint16_t(data.frame), data.sequence);
        TEST_ASSERT_TRUE(data.size == 8 || data.size == 64);
        return true;
    }
    Peer& peer(uint8_t i) { return i ? clients[i - 1] : gateway; }

    void tick() {
        RadioEvent events[kMaxClients + 1];
        if (sender >= 0 && now >= txEnd) {
            auto& radio = peer(sender).radio;
            events[sender] = radio.completeTx();
            events[sender].hasCompletionTime = true;
            events[sender].completionUs = txEnd;
            for (uint8_t i = 0; i <= count; ++i) {
                if (!(listening & (1u << i)) || !peer(i).radio.receiving() || peer(i).radio.sf != txSf) continue;
                if (frame.destinationId != kGroup && frame.destinationId != i + 1) continue;
                if (outage && (i == 1 || sender == 1)) continue;
                if (sender == 1 && txSf < minDataSf[0]) continue;
                if (sender > 1 && txSf < minDataSf[sender - 1]) continue;
                if (i == 1 && ((dropGrant && frame.type == MessageType::SLOT_GRANT) ||
                    (frame.type == MessageType::ACK_BITMAP &&
                     (dropAllAck || (dropFirstAck && frame.sequence == 0))))) continue;
                events[i] = peer(i).radio.deliver(radio.lastTx);
                events[i].hasCompletionTime = true;
                events[i].completionUs = txEnd;
                if (i == 0 && frame.type == MessageType::DATA) {
                    events[i].metrics = true;
                    events[i].rssi = -95.0f;
                    events[i].snr = snr[sender - 1];
                }
            }
            sender = -1;
        }
        host_clock::set(now);
        for (uint8_t i = 0; i <= count; ++i) peer(i).link.update(now, events[i]);
        for (uint8_t i = 0; i <= count; ++i) {
            auto& p = peer(i);
            TEST_ASSERT_EQUAL_UINT(0, p.radio.contractViolations);
            if (p.radio.transmitting() && int(i) != sender) {
                TEST_ASSERT_EQUAL_INT_MESSAGE(-1, sender, "Two transmissions overlap on the single-radio calendar");
                sender = i;
                txSf = p.radio.sf;
                const auto& bytes = p.radio.lastTx;
                const auto dest = get16(bytes.bytes + 10);
                TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(decode(bytes.bytes, bytes.length, 1, dest, frame)));
                txEnd = now + airAtSf(bytes.length, txSf);
                listening = 0;
                for (uint8_t j = 0; j <= count; ++j)
                    if (peer(j).radio.receiving() && peer(j).radio.sf == txSf) listening |= 1u << j;
                if (frame.type != MessageType::DATA) TEST_ASSERT_EQUAL_UINT8(kControlSf, txSf);
                tx.push_back({frame.type, i, txSf, frame.frameId, frame.sequence});
            }
            for (unsigned drain = 0; drain < 24 && !p.log.empty(); ++drain) p.log.drain();
        }
    }
    void rounds(unsigned n, uint8_t size = 8) {
        const uint8_t payload[kMaxData]{};
        for (unsigned frameIndex = 0; frameIndex < n; ++frameIndex) {
            for (uint8_t i = 0; i < count; ++i)
                if (!clients[i].payload.pending())
                    TEST_ASSERT_EQUAL_INT(int(SubmitResult::QUEUED), int(clients[i].payload.submit(payload, size)));
            const auto stop = now + kAdaptivePeriodUs;
            for (; now < stop; now += 1000) tick();
        }
    }
    bool sent(uint8_t client, uint8_t sf) const {
        for (const auto& packet : tx)
            if (packet.type == MessageType::DATA && packet.sender == client && packet.sf == sf) return true;
        return false;
    }
};

void mixed_profile_codec_and_budget() {
    const auto r = roster(8);
    Plan plan;
    plan.count = r.count;
    TEST_ASSERT_EQUAL_UINT32(164096, dataAir(64, 7));
    TEST_ASSERT_EQUAL_UINT32(297472, dataAir(64, 8));
    TEST_ASSERT_EQUAL_UINT32(533504, dataAir(64, 9));
    for (uint8_t i = 0; i < r.count; ++i) plan.slots[i] = {uint32_t(i + 1), 64, 7};
    TEST_ASSERT_TRUE(plan.matches(r));
    plan.slots[0] = {1, 8, 9};
    TEST_ASSERT_TRUE(plan.matches(r));
    TEST_ASSERT_EQUAL_UINT32(dataAir(8, 9) + 3 * dataAir(64, 7) + 4 * kSlotGuardUs, plan.retrySpan());
    Frame grant;
    grant.type = MessageType::SLOT_GRANT;
    grant.networkId = grant.sourceId = grant.bootId = grant.frameId = 1;
    grant.destinationId = kGroup;
    grant.scheduleVersion = kScheduleVersion;
    writePlan(grant, plan);
    uint8_t bytes[kMaxFrameSize]{};
    size_t length = 0;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(encode(grant, bytes, sizeof(bytes), length)));
    TEST_ASSERT_EQUAL_UINT8(4, bytes[2]);
    Frame decoded;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(decode(bytes, length, 1, 2, decoded)));
    Plan decodedPlan;
    TEST_ASSERT_TRUE(readPlan(decoded, decodedPlan));
    TEST_ASSERT_EQUAL_UINT8(9, decodedPlan.slots[0].sf);
    bytes[kHeaderSize + 6] = 10;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::PAYLOAD), int(decode(bytes, length, 1, 2, decoded)));
    bytes[2] = 3;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::VERSION), int(decode(bytes, length, 1, 2, decoded)));
    for (auto& slot : plan.slots) slot = {1, 64, 9};
    TEST_ASSERT_FALSE(plan.matches(r));
}

void roaming_client_adapts_while_three_peers_deliver() {
    Network n(4, 8);
    n.rounds(3);
    TEST_ASSERT_EQUAL_UINT8(7, n.clients[0].link.telemetry().confirmedDataSf);
    n.minDataSf[0] = 9;
    n.rounds(7);
    TEST_ASSERT_TRUE(n.sent(1, 8));
    TEST_ASSERT_TRUE(n.sent(1, 9));
    TEST_ASSERT_EQUAL_UINT8(9, n.clients[0].link.telemetry().confirmedDataSf);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("sf=9,outcome=first,app=accepted"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("next_sf=9,locked=1,adr=locked"));
    for (uint8_t i = 1; i < 4; ++i) {
        TEST_ASSERT_EQUAL_UINT(10, n.delivered[i]);
        TEST_ASSERT_FALSE(n.sent(i + 1, 8));
        TEST_ASSERT_FALSE(n.sent(i + 1, 9));
    }
    n.rounds(4);  // Strong SNR estimate alone does not prove a lower SF works.
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("adr=rollback"));
    n.minDataSf[0] = 7;
    n.rounds(20);
    TEST_ASSERT_EQUAL_UINT8(7, n.clients[0].link.telemetry().confirmedDataSf);
    for (uint8_t i = 1; i < 4; ++i) TEST_ASSERT_EQUAL_UINT(34, n.delivered[i]);
}

void missing_grant_ack_and_gateway_outage_leave_peers_safe() {
    Network n(2);
    n.rounds(1);
    n.dropGrant = true;
    n.rounds(1);
    TEST_ASSERT_EQUAL_UINT(1, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT(2, n.delivered[1]);
    n.dropGrant = false;
    n.dropFirstAck = true;
    n.rounds(1);
    TEST_ASSERT_EQUAL_UINT(2, n.delivered[0]);
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(n.clients[0].payload.result()));
    for (const auto& packet : n.tx)
        if (packet.type == MessageType::DATA && packet.sender == 1)
            TEST_ASSERT_NOT_EQUAL(2u, packet.frame);
    n.outage = true;
    n.rounds(2);
    TEST_ASSERT_TRUE(n.clients[0].link.searchingForBeacon(n.now));
    TEST_ASSERT_EQUAL_UINT8(kControlSf, n.clients[0].radio.sf);
    n.outage = false;
    n.dropFirstAck = false;
    n.rounds(1);
    TEST_ASSERT_EQUAL_UINT(3, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT(6, n.delivered[1]);
}

void one_failed_retune_recovers_with_full_roster() {
    Network n(8);
    n.clients[0].radio.failNextRetune = true;
    n.rounds(2, 64);
    for (uint8_t i = 1; i < 8; ++i) TEST_ASSERT_EQUAL_UINT(2, n.delivered[i]);
    TEST_ASSERT_EQUAL_UINT(1, n.clients[0].link.telemetry().radioErrors);
    TEST_ASSERT_EQUAL_UINT(1, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT8(7, n.clients[0].link.telemetry().confirmedDataSf);
}

void higher_sf_reservations_protect_two_existing_clients() {
    Network n(8);
    n.minDataSf[0] = n.minDataSf[1] = 9;
    n.snr[0] = n.snr[1] = -12;
    n.rounds(7, 64);
    TEST_ASSERT_EQUAL_UINT8(9, n.clients[0].link.telemetry().confirmedDataSf);
    TEST_ASSERT_EQUAL_UINT8(9, n.clients[1].link.telemetry().confirmedDataSf);
    const auto firstDelivered = n.delivered[0];
    const auto secondDelivered = n.delivered[1];
    n.minDataSf[2] = 9;
    n.rounds(6, 64);
    TEST_ASSERT_FALSE(n.sent(3, 9));
    TEST_ASSERT_EQUAL_UINT(firstDelivered + 6, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT(secondDelivered + 6, n.delivered[1]);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("adr=budget_limited"));
    for (uint8_t i = 3; i < 8; ++i) TEST_ASSERT_EQUAL_UINT(13, n.delivered[i]);
}

void stable_lock_requires_client_ack_receipt() {
    Network n(1);
    n.dropAllAck = true;
    n.rounds(4);
    TEST_ASSERT_EQUAL_UINT(4, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT8(0, n.clients[0].link.telemetry().confirmedDataSf);
    TEST_ASSERT_EQUAL(std::string::npos, Serial.output.find("locked=1"));
    n.dropAllAck = false;
    n.rounds(2);
    TEST_ASSERT_EQUAL_UINT8(7, n.clients[0].link.telemetry().confirmedDataSf);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("next_sf=7,locked=1,adr=locked"));
}

void display_reports_the_last_acknowledged_data_sf() {
    DisplaySnapshot snapshot;
    snapshot.config = config(2);
    snapshot.protocol = kWireVersion;
    snapshot.multiclient.confirmedDataSf = 8;
    const auto formatted = formatOledStatus(snapshot, 0, 1);
    TEST_ASSERT_EQUAL_STRING("V4 DATA 2/3", formatted.rows[0]);
    TEST_ASSERT_EQUAL_STRING("ACKED DATA SF8", formatted.rows[6]);
}

void driver_controls_adapter_retune_and_failure() {
    auto& physical = host_adapter::state();
    physical = {};
    Sx1262Hardware hardware;
    RadioDriver driver(hardware);
    TEST_ASSERT_EQUAL_INT16(0, driver.begin());
    TEST_ASSERT_EQUAL_INT16(0, driver.receive());
    TEST_ASSERT_EQUAL_INT16(0, driver.setSpreadingFactor(9));
    TEST_ASSERT_EQUAL_INT(host_adapter::STANDBY, physical.trace[physical.traceLength - 2]);
    TEST_ASSERT_EQUAL_INT(host_adapter::RETUNE, physical.trace[physical.traceLength - 1]);
    TEST_ASSERT_EQUAL_UINT8(9, physical.sf);
    TEST_ASSERT_FALSE(driver.receiving());
    TEST_ASSERT_EQUAL_INT16(0, driver.receive());
    physical.codes[host_adapter::RETUNE] = -88;
    TEST_ASSERT_EQUAL_INT16(-88, driver.setSpreadingFactor(8));
    TEST_ASSERT_FALSE(driver.healthy());
    physical.codes[host_adapter::RETUNE] = 0;
    TEST_ASSERT_EQUAL_INT16(0, driver.setSpreadingFactor(9));
    const uint8_t packet[kHeaderSize]{};
    TEST_ASSERT_EQUAL_INT16(0, driver.transmit(packet, sizeof(packet), 0));
    const auto calls = physical.calls[host_adapter::RETUNE];
    TEST_ASSERT_NOT_EQUAL(0, driver.setSpreadingFactor(7));
    TEST_ASSERT_EQUAL_UINT(calls, physical.calls[host_adapter::RETUNE]);
}
}

void setUp() { Serial.output.clear(); }
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(mixed_profile_codec_and_budget);
    RUN_TEST(roaming_client_adapts_while_three_peers_deliver);
    RUN_TEST(missing_grant_ack_and_gateway_outage_leave_peers_safe);
    RUN_TEST(one_failed_retune_recovers_with_full_roster);
    RUN_TEST(higher_sf_reservations_protect_two_existing_clients);
    RUN_TEST(stable_lock_requires_client_ack_receipt);
    RUN_TEST(display_reports_the_last_acknowledged_data_sf);
    RUN_TEST(driver_controls_adapter_retune_and_failure);
    return UNITY_END();
}
