#include "bmscanprotocol.h"
#include <QCanBusFrame>
#include <QByteArray>
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "bq34z100_app.h"
#include "bq34z100_drv.h"
}

// Exercise the actual firmware sampling/payload code against the actual Qt parser.
static int failingRead = -1;
static int readIndex = 0;
static uint8_t mockSoc = 75;
#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #expr); std::exit(1); } } while (0)
#define MOCK_READ(name, type, value) \
extern "C" uint8_t name(type *out) { \
    if (readIndex++ == failingRead) return BQ34Z100_ERR_COMM; \
    *out = value; return BQ34Z100_OK; }
MOCK_READ(BQ34Z100_ReadSOC, uint8_t, mockSoc)
MOCK_READ(BQ34Z100_ReadMaxError, uint8_t, 1)
MOCK_READ(BQ34Z100_ReadRemainingCapacity_mAh, uint16_t, 3000)
MOCK_READ(BQ34Z100_ReadFullChargeCapacity_mAh, uint16_t, 4000)
MOCK_READ(BQ34Z100_ReadVoltage_mV, uint16_t, 34000)
MOCK_READ(BQ34Z100_ReadAverageCurrent_mA, int16_t, -120)
MOCK_READ(BQ34Z100_ReadCurrent_mA, int16_t, -150)
MOCK_READ(BQ34Z100_ReadTemperature_dC, int16_t, 250)
MOCK_READ(BQ34Z100_ReadFlags, uint16_t, 0x0100)
MOCK_READ(BQ34Z100_ReadFlagsB, uint16_t, 0)
MOCK_READ(BQ34Z100_ReadCycleCount, uint16_t, 10)
MOCK_READ(BQ34Z100_ReadSOH, uint8_t, 95)

static QCanBusFrame frameFor(const BQ34Z100_AppCtx_t &ctx)
{
    uint8_t bytes[8];
    CHECK(BQ34Z100_AppBuildCanPayload(&ctx, bytes) == 0);
    return QCanBusFrame(0x308, QByteArray(reinterpret_cast<const char *>(bytes), 8));
}

int main()
{
    BQ34Z100_AppCtx_t ctx;
    BmsGaugeData parsed;
    BQ34Z100_AppInit(&ctx);
    CHECK(!ctx.data_valid && ctx.last_error == BQ34Z100_APP_ERR_NOT_READY);
    CHECK(BmsCanProtocol::parseGaugeStatus(frameFor(ctx), parsed));
    CHECK(parsed.received && !parsed.valid);
    CHECK(BQ34Z100_AppRunCycle(nullptr) != 0);
    CHECK(BQ34Z100_AppBuildCanPayload(&ctx, nullptr) != 0);
    CHECK(BQ34Z100_AppRunCycle(&ctx) == 0);
    CHECK(ctx.current_mA == -150 && ctx.temperature_dC == 250);
    const auto good = ctx;
    auto frame = frameFor(ctx);
    CHECK(frame.payload().toHex() == "4b5fb80ba00f0100");
    CHECK(BmsCanProtocol::parseGaugeStatus(frame, parsed) && parsed.valid);
    CHECK(parsed.socPercent == 75 && parsed.sohPercent == 95);
    CHECK(parsed.remainingCapacityMah == 3000 && parsed.fullChargeCapacityMah == 4000);

    // Each failed register read must leave numeric fields from the last good sample.
    for (int i = 0; i < 12; ++i) {
        ctx = good;
        mockSoc = 42;
        readIndex = 0;
        failingRead = i;
        CHECK(BQ34Z100_AppRunCycle(&ctx) == 10 + i);
        CHECK(!ctx.data_valid && ctx.last_error == 10 + i);
        CHECK(ctx.soc_percent == 75 && ctx.remaining_capacity_mAh == 3000);
        CHECK(BmsCanProtocol::parseGaugeStatus(frameFor(ctx), parsed) && !parsed.valid);
        CHECK(parsed.socPercent == 0 && parsed.remainingCapacityMah == 0);
    }
    failingRead = -1;
    readIndex = 0;
    CHECK(BQ34Z100_AppRunCycle(&ctx) == 0 && ctx.data_valid); // Recovery
    mockSoc = 101;
    readIndex = 0;
    CHECK(BQ34Z100_AppRunCycle(&ctx) == BQ34Z100_APP_ERR_RANGE);
    CHECK(ctx.soc_percent == 42 && !ctx.data_valid);

    for (const uint8_t error : {0xF0, 0xF1, 0xF2, 0xF3}) {
        ctx.last_error = error;
        CHECK(BmsCanProtocol::parseGaugeStatus(frameFor(ctx), parsed));
        CHECK(!parsed.valid && parsed.lastError == error);
    }
    ctx = good;
    ctx.soc_percent = 0;
    ctx.soh_percent = 100;
    ctx.remaining_capacity_mAh = 0;
    ctx.full_charge_capacity_mAh = 65535;
    CHECK(BmsCanProtocol::parseGaugeStatus(frameFor(ctx), parsed) && parsed.valid);
    CHECK(parsed.socPercent == 0 && parsed.fullChargeCapacityMah == 65535);
    for (int size = 0; size < 8; ++size) {
        QCanBusFrame shortFrame(0x308, QByteArray(size, '\0'));
        CHECK(!BmsCanProtocol::parseGaugeStatus(shortFrame, parsed));
    }
    frame.setExtendedFrameFormat(true);
    CHECK(!BmsCanProtocol::parseGaugeStatus(frame, parsed));
    frame.setExtendedFrameFormat(false);
    frame.setFrameType(QCanBusFrame::RemoteRequestFrame);
    CHECK(!BmsCanProtocol::parseGaugeStatus(frame, parsed));
    frame = frameFor(good);
    frame.setFrameId(0x307);
    CHECK(!BmsCanProtocol::parseGaugeStatus(frame, parsed));
    frame = frameFor(good);
    auto bytes = frame.payload();
    bytes[0] = 101;
    frame.setPayload(bytes);
    CHECK(BmsCanProtocol::parseGaugeStatus(frame, parsed) && !parsed.valid);
    std::puts("Gauge sampling, failure/recovery, wire format and Qt parsing: PASS");
}
