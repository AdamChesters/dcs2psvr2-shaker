#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <string_view>

namespace shaker {
inline constexpr wchar_t telemetryName[] = L"Local\\dcs2psvr2-shaker_DCS_v1";
inline constexpr uint32_t telemetryMagic = 0x31535350;
inline constexpr uint32_t telemetryVersion = 1;
inline constexpr size_t payloadCapacity = 32768;
struct alignas(8) TelemetryRegion {
    uint32_t magic = telemetryMagic, version = telemetryVersion;
    uint32_t regionBytes = sizeof(TelemetryRegion), payloadBytes = 0;
    uint32_t writerPid = 0, flags = 0;
    uint64_t session = 0, sequence = 0, publishedMs = 0;
    double simTime = 0;
    char payload[payloadCapacity]{};
};
static_assert(offsetof(TelemetryRegion, payload) == 56);
struct TelemetryPacket {
    uint64_t session = 0, sequence = 0, publishedMs = 0;
    uint32_t writerPid = 0;
    double simTime = 0;
    std::string payload;
};
class TelemetryWriter {
    HANDLE mapping_ = nullptr, mutex_ = nullptr;
    TelemetryRegion* region_ = nullptr;
    uint64_t session_ = 0;
public:
    ~TelemetryWriter();
    bool open(const std::wstring& name = telemetryName);
    bool publish(std::string_view payload, double simTime, uint32_t flags = 0);
    void newSession();
    void close();
};
class TelemetryReader {
    HANDLE mapping_ = nullptr, mutex_ = nullptr;
    const TelemetryRegion* region_ = nullptr;
    std::wstring name_;
public:
    std::string status = "Waiting for DCS";
    explicit TelemetryReader(std::wstring name = telemetryName) : name_(std::move(name)) {}
    ~TelemetryReader();
    bool read(TelemetryPacket& packet);
    void close();
};
}
