#include "benchmarkscoring.h"

#include <algorithm>
#include <cmath>

namespace Benchmark {

namespace {

double average(double sum, uint64_t count)
{
    return count > 0 ? sum / static_cast<double>(count) : 0;
}

} // namespace

Metrics metricsFromStats(const RawStats& s, double targetFps)
{
    Metrics m;
    m.targetFps = targetFps;
    m.measuredSeconds = s.elapsedUs / 1e6;

    m.hostMs = average(static_cast<double>(s.hostLatencySum), s.hostLatencyFrames) / 10.0;
    m.rttMs = s.rttSamples > 0 ? s.rttSumMs / s.rttSamples : 0;
    m.rttVarianceMs = s.rttSamples > 0 ? s.rttVarianceSumMs / s.rttSamples : 0;
    m.networkMs = m.rttMs / 2;
    m.receiveMs = average(static_cast<double>(s.receiveSumUs), s.framesReceived) / 1000.0;
    m.decodeMs = average(static_cast<double>(s.decodeSumUs), s.framesDecoded) / 1000.0;
    m.queueMs = average(static_cast<double>(s.queueSumUs), s.framesRendered) / 1000.0;
    m.renderMs = average(static_cast<double>(s.renderSumUs), s.framesRendered) / 1000.0;
    m.totalMs = m.hostMs + m.networkMs + m.receiveMs + m.decodeMs + m.queueMs + m.renderMs;

    m.framesRendered = s.framesRendered;
    m.framesExpected = static_cast<uint64_t>(std::llround(targetFps * m.measuredSeconds));
    m.networkLossPct = s.framesTotal > 0 ? 100.0 * static_cast<double>(s.framesNetworkLost) / static_cast<double>(s.framesTotal) : 0;

    m.presentP50Ms = s.presents.percentileMs(50);
    m.presentP95Ms = s.presents.percentileMs(95);
    m.presentP99Ms = s.presents.percentileMs(99);
    if (targetFps > 0 && s.presents.total() > 0) {
        const double periodMs = 1000.0 / targetFps;
        m.stutterPct = 100.0 * static_cast<double>(s.presents.countAbove(1.5 * periodMs)) / static_cast<double>(s.presents.total());
    }
    return m;
}

Scores computeScores(const Metrics& m)
{
    Scores scores;

    // A frame's whole trip, from capture to screen: every 50 ms divides the score by e.
    scores.latency = 100.0 * std::exp(-m.totalMs / 50.0);

    // Smoothness multiplies three ways a stream stops looking fluid: frames that never
    // show up (squared: a missing frame is seen twice, as a gap and a repeat), hitches
    // longer than one and a half frames, and the irregularity of the frames that do show.
    const double periodMs = m.targetFps > 0 ? 1000.0 / m.targetFps : 0;
    const double delivered = m.framesExpected > 0
        ? std::min(1.0, static_cast<double>(m.framesRendered) / static_cast<double>(m.framesExpected))
        : 0;
    const double jitter = periodMs > 0 ? std::max(0.0, m.presentP99Ms - m.presentP50Ms) / periodMs : 0;
    scores.smoothness = 100.0 * delivered * delivered * std::exp(-20.0 * m.stutterPct / 100.0) / (1.0 + jitter);

    scores.latency = roundScore(scores.latency);
    scores.smoothness = roundScore(scores.smoothness);
    scores.balanced = roundScore((scores.latency + scores.smoothness) / 2.0);
    return scores;
}

double standardDeviation(const std::vector<double>& values)
{
    if (values.size() < 2) {
        return 0;
    }
    double mean = 0;
    for (double v : values) {
        mean += v;
    }
    mean /= static_cast<double>(values.size());
    double squares = 0;
    for (double v : values) {
        squares += (v - mean) * (v - mean);
    }
    return std::sqrt(squares / static_cast<double>(values.size()));
}

double roundScore(double value)
{
    return std::round(value * 10.0) / 10.0;
}

} // namespace Benchmark
