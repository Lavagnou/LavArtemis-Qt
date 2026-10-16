#pragma once

// The benchmark's vocabulary, shared by the planner, the scoring and the report.
// The rules these types serve are specified in LavApollo's docs/benchmark.md, which
// the Android client implements too: change both clients together.

#include "presenthistogram.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Benchmark {

enum class Codec { H264, HEVC, AV1, PyroWave };
enum class Mode { Quick, Full, Adaptive };
enum class Goal { Latency, Smoothness, Balanced };
enum class Role { Baseline, Candidate, BaselineRepeat, Confirm };
enum class Status { Ok, Fallback, Failed, Skipped };

const char* toString(Codec codec);
const char* toString(Mode mode);
const char* toString(Goal goal);
const char* toString(Role role);
const char* toString(Status status);
bool fromString(const std::string& text, Codec& codec);
bool fromString(const std::string& text, Mode& mode);
bool fromString(const std::string& text, Goal& goal);
bool fromString(const std::string& text, Role& role);
bool fromString(const std::string& text, Status& status);

// Pacing modes are platform names ("vsync-paced", "ull", ...); the "vrr-" prefix is the
// part every platform shares, and the one that changes what the host does.
bool isVrrPacing(const std::string& pacing);

struct RunConfig {
    Codec codec = Codec::H264;
    int bitrateKbps = 0;
    std::string pacing;

    bool operator==(const RunConfig& other) const
    {
        return codec == other.codec && bitrateKbps == other.bitrateKbps && pacing == other.pacing;
    }
    bool operator!=(const RunConfig& other) const { return !(*this == other); }
};

// What a client measured over (part of) a run, as sums: two snapshots of the same
// session subtract into the stats of the time between them.
struct RawStats {
    double elapsedUs = 0;

    uint64_t framesTotal = 0;           // sent by the host, lost ones included
    uint64_t framesNetworkLost = 0;
    uint64_t framesReceived = 0;
    uint64_t framesDecoded = 0;
    uint64_t framesRendered = 0;
    uint64_t framesPacerDropped = 0;

    uint64_t hostLatencySum = 0;        // tenths of a millisecond, as the host sends them
    uint64_t hostLatencyFrames = 0;
    uint64_t receiveSumUs = 0;          // per received frame
    uint64_t decodeSumUs = 0;           // per decoded frame
    uint64_t queueSumUs = 0;            // per rendered frame
    uint64_t renderSumUs = 0;           // per rendered frame

    double rttSumMs = 0;
    double rttVarianceSumMs = 0;
    uint32_t rttSamples = 0;

    PresentHistogram presents;

    RawStats operator-(const RawStats& earlier) const;
    RawStats& operator+=(const RawStats& other);
};

struct Metrics {
    double hostMs = 0;
    double networkMs = 0;
    double receiveMs = 0;
    double decodeMs = 0;
    double queueMs = 0;
    double renderMs = 0;
    double totalMs = 0;
    double rttMs = 0;
    double rttVarianceMs = 0;
    double targetFps = 0;
    double measuredSeconds = 0;
    uint64_t framesRendered = 0;
    uint64_t framesExpected = 0;
    double networkLossPct = 0;
    double presentP50Ms = 0;
    double presentP95Ms = 0;
    double presentP99Ms = 0;
    double stutterPct = 0;
};

struct Scores {
    double latency = 0;
    double smoothness = 0;
    double balanced = 0;

    double forGoal(Goal goal) const;
};

struct RunResult {
    Role role = Role::Candidate;
    Status status = Status::Ok;
    std::string note;
    RunConfig requested;
    RunConfig effective;
    Metrics metrics;
    Scores scores;
    double spreadLatency = 0;
    double spreadSmoothness = 0;
};

} // namespace Benchmark
