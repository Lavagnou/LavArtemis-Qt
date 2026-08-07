#pragma once

#include <QMutex>

#include <stdint.h>

// Rolling window of present-to-present intervals, exposing p50/p95/p99.
//
// Ported from LavArtemis for Android (binding/video/PacingStats.java). The
// window size, the outlier filter and the (non-interpolating) percentile
// indexing are all kept identical to the Java implementation on purpose, so a
// CSV captured on Android and one captured here can be compared directly.
class PacingStats
{
public:
    struct Snapshot {
        float p50Ms = 0.0f;
        float p95Ms = 0.0f;
        float p99Ms = 0.0f;
        int sampleCount = 0;
    };

    PacingStats();

    // presentTimeNs must come from a monotonic clock. Prefer a real presentation
    // timestamp where the renderer can supply one (DXGI's SyncQPCTime); fall back
    // to the time the frame was handed to the renderer otherwise.
    void recordPresent(uint64_t presentTimeNs);

    Snapshot snapshot();

    void reset();

private:
    static const int k_WindowSize = 512;

    // Deltas at or above this are treated as stream pauses / codec recovery
    // rather than pacing, and are dropped so they can't skew the percentiles.
    static const uint64_t k_MaxPlausibleDeltaNs = 1000000000ULL;

    QMutex m_Lock;
    float m_DeltasMs[k_WindowSize];
    int m_WriteIndex;
    int m_Count;
    uint64_t m_LastPresentNs;
};
