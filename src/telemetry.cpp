#include "telemetry.hpp"
#include <cmath>
#include <cstring>

namespace shaker {
static bool alive(DWORD pid) {
    HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!p) return GetLastError() == ERROR_ACCESS_DENIED;
    const bool live = WaitForSingleObject(p, 0) == WAIT_TIMEOUT;
    CloseHandle(p); return live;
}
TelemetryWriter::~TelemetryWriter() { close(); }
void TelemetryWriter::newSession() {
    static uint64_t counter = 0;
    LARGE_INTEGER ticks{}; QueryPerformanceCounter(&ticks);
    session_ = static_cast<uint64_t>(ticks.QuadPart) ^ (uint64_t(GetCurrentProcessId()) << 32) ^ ++counter;
}
bool TelemetryWriter::open(const std::wstring& name) {
    if (region_) return true;
    mutex_ = CreateMutexW(nullptr, FALSE, (name + L"_Mutex").c_str());
    if (!mutex_) return false;
    mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(TelemetryRegion), name.c_str());
    if (!mapping_) { close(); return false; }
    region_ = static_cast<TelemetryRegion*>(MapViewOfFile(mapping_, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, sizeof(TelemetryRegion)));
    if (!region_) { close(); return false; }
    const DWORD lock = WaitForSingleObject(mutex_, 0);
    if (lock != WAIT_OBJECT_0 && lock != WAIT_ABANDONED) { close(); return false; }
    if (lock == WAIT_OBJECT_0 && region_->magic == telemetryMagic && region_->writerPid && alive(region_->writerPid)) {
        ReleaseMutex(mutex_); close(); return false;
    }
    newSession();
    *region_ = TelemetryRegion{};
    region_->writerPid = GetCurrentProcessId(); region_->session = session_;
    ReleaseMutex(mutex_); return true;
}
bool TelemetryWriter::publish(std::string_view data, double simTime, uint32_t flags) {
    if (!region_ || data.empty() || data.size() > payloadCapacity || !std::isfinite(simTime)) return false;
    const DWORD lock = WaitForSingleObject(mutex_, 0);
    if (lock != WAIT_OBJECT_0 && lock != WAIT_ABANDONED) return false;
    if (lock == WAIT_ABANDONED) { *region_ = TelemetryRegion{}; newSession(); }
    std::memcpy(region_->payload, data.data(), data.size());
    region_->payloadBytes = static_cast<uint32_t>(data.size());
    region_->writerPid = GetCurrentProcessId(); region_->session = session_;
    region_->simTime = simTime; region_->flags = flags;
    region_->publishedMs = GetTickCount64(); ++region_->sequence;
    ReleaseMutex(mutex_); return true;
}
void TelemetryWriter::close() {
    if (region_ && mutex_) {
        const auto lock=WaitForSingleObject(mutex_,0);
        if(lock==WAIT_OBJECT_0 || lock==WAIT_ABANDONED){
            if (region_->writerPid == GetCurrentProcessId() && region_->session == session_) region_->writerPid = 0;
            ReleaseMutex(mutex_);
        }
    }
    if (region_) UnmapViewOfFile(region_);
    if (mapping_) CloseHandle(mapping_);
    if (mutex_) CloseHandle(mutex_);
    region_ = nullptr; mapping_ = mutex_ = nullptr;
}
TelemetryReader::~TelemetryReader() { close(); }
void TelemetryReader::close() {
    if (region_) UnmapViewOfFile(region_);
    if (mapping_) CloseHandle(mapping_);
    if (mutex_) CloseHandle(mutex_);
    region_ = nullptr; mapping_ = mutex_ = nullptr;
}
bool TelemetryReader::read(TelemetryPacket& packet) {
    if (!region_) {
        mapping_ = OpenFileMappingW(FILE_MAP_READ, FALSE, name_.c_str());
        mutex_ = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, (name_ + L"_Mutex").c_str());
        if (!mapping_ || !mutex_) { close(); status = "Waiting for DCS"; return false; }
        region_ = static_cast<const TelemetryRegion*>(MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, sizeof(TelemetryRegion)));
        if (!region_) { close(); status = "Telemetry mapping unavailable"; return false; }
    }
    const DWORD lock = WaitForSingleObject(mutex_, 0);
    if (lock == WAIT_ABANDONED) { ReleaseMutex(mutex_); status = "Telemetry publisher interrupted"; close(); return false; }
    if (lock != WAIT_OBJECT_0) return false;
    const bool valid = region_->magic == telemetryMagic && region_->version == telemetryVersion &&
        region_->regionBytes == sizeof(TelemetryRegion) && region_->payloadBytes > 0 && region_->payloadBytes <= payloadCapacity &&
        std::isfinite(region_->simTime) && region_->writerPid != 0;
    if (valid) {
        try {
            packet = {region_->session, region_->sequence, region_->publishedMs, region_->writerPid, region_->simTime,
                std::string(region_->payload, region_->payloadBytes)};
        }catch(...){ReleaseMutex(mutex_);throw;}
    }
    ReleaseMutex(mutex_);
    if (!valid) { status = "Waiting for a valid DCS frame"; return false; }
    if (packet.publishedMs > GetTickCount64() || GetTickCount64() - packet.publishedMs > 2000 || !alive(packet.writerPid)) {
        status = "DCS stopped or telemetry stale"; close(); return false;
    }
    status = "DCS connected"; return true;
}
}
