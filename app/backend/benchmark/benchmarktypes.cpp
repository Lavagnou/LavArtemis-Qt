#include "benchmarktypes.h"

#include <cstring>

namespace Benchmark {

namespace {

template<typename E, size_t N>
bool lookup(const char* const (&names)[N], const std::string& text, E& value)
{
    for (size_t i = 0; i < N; i++) {
        if (text == names[i]) {
            value = static_cast<E>(i);
            return true;
        }
    }
    return false;
}

const char* const kCodecs[] = { "h264", "hevc", "av1", "pyrowave" };
const char* const kModes[] = { "quick", "full", "adaptive" };
const char* const kGoals[] = { "latency", "smoothness", "balanced" };
const char* const kRoles[] = { "baseline", "candidate", "baseline-repeat", "confirm" };
const char* const kStatuses[] = { "ok", "fallback", "failed", "skipped" };

} // namespace

const char* toString(Codec codec) { return kCodecs[static_cast<int>(codec)]; }
const char* toString(Mode mode) { return kModes[static_cast<int>(mode)]; }
const char* toString(Goal goal) { return kGoals[static_cast<int>(goal)]; }
const char* toString(Role role) { return kRoles[static_cast<int>(role)]; }
const char* toString(Status status) { return kStatuses[static_cast<int>(status)]; }
bool fromString(const std::string& text, Codec& codec) { return lookup(kCodecs, text, codec); }
bool fromString(const std::string& text, Mode& mode) { return lookup(kModes, text, mode); }
bool fromString(const std::string& text, Goal& goal) { return lookup(kGoals, text, goal); }
bool fromString(const std::string& text, Role& role) { return lookup(kRoles, text, role); }
bool fromString(const std::string& text, Status& status) { return lookup(kStatuses, text, status); }

bool isVrrPacing(const std::string& pacing)
{
    return pacing.rfind("vrr-", 0) == 0;
}

RawStats RawStats::operator-(const RawStats& earlier) const
{
    auto sub = [](uint64_t a, uint64_t b) { return a >= b ? a - b : 0; };
    RawStats d;
    d.elapsedUs = elapsedUs - earlier.elapsedUs;
    d.framesTotal = sub(framesTotal, earlier.framesTotal);
    d.framesNetworkLost = sub(framesNetworkLost, earlier.framesNetworkLost);
    d.framesReceived = sub(framesReceived, earlier.framesReceived);
    d.framesDecoded = sub(framesDecoded, earlier.framesDecoded);
    d.framesRendered = sub(framesRendered, earlier.framesRendered);
    d.framesPacerDropped = sub(framesPacerDropped, earlier.framesPacerDropped);
    d.hostLatencySum = sub(hostLatencySum, earlier.hostLatencySum);
    d.hostLatencyFrames = sub(hostLatencyFrames, earlier.hostLatencyFrames);
    d.receiveSumUs = sub(receiveSumUs, earlier.receiveSumUs);
    d.decodeSumUs = sub(decodeSumUs, earlier.decodeSumUs);
    d.queueSumUs = sub(queueSumUs, earlier.queueSumUs);
    d.renderSumUs = sub(renderSumUs, earlier.renderSumUs);
    d.rttSumMs = rttSumMs - earlier.rttSumMs;
    d.rttVarianceSumMs = rttVarianceSumMs - earlier.rttVarianceSumMs;
    d.rttSamples = rttSamples >= earlier.rttSamples ? rttSamples - earlier.rttSamples : 0;
    d.presents = presents - earlier.presents;
    return d;
}

RawStats& RawStats::operator+=(const RawStats& o)
{
    elapsedUs += o.elapsedUs;
    framesTotal += o.framesTotal;
    framesNetworkLost += o.framesNetworkLost;
    framesReceived += o.framesReceived;
    framesDecoded += o.framesDecoded;
    framesRendered += o.framesRendered;
    framesPacerDropped += o.framesPacerDropped;
    hostLatencySum += o.hostLatencySum;
    hostLatencyFrames += o.hostLatencyFrames;
    receiveSumUs += o.receiveSumUs;
    decodeSumUs += o.decodeSumUs;
    queueSumUs += o.queueSumUs;
    renderSumUs += o.renderSumUs;
    rttSumMs += o.rttSumMs;
    rttVarianceSumMs += o.rttVarianceSumMs;
    rttSamples += o.rttSamples;
    presents += o.presents;
    return *this;
}

double Scores::forGoal(Goal goal) const
{
    switch (goal) {
    case Goal::Latency:
        return latency;
    case Goal::Smoothness:
        return smoothness;
    case Goal::Balanced:
    default:
        return balanced;
    }
}

} // namespace Benchmark
