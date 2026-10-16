#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Present-to-present intervals over a whole session, in 0.25 ms buckets up to 100 ms.
//
// PacingStats keeps the last 512 intervals, enough for an overlay but not for a
// benchmark run, whose percentiles must cover every frame it measured. A histogram
// does, in constant memory, and two snapshots of the same session subtract into
// the histogram of the time between them -- which is how a run drops its warm-up.
// Not thread-safe: the pacer records and reads it under its own lock.
class PresentHistogram
{
public:
    static constexpr int kBucketsPerMs = 4;
    static constexpr int kBuckets = 100 * kBucketsPerMs;   // the last one also holds everything longer

    void record(double intervalMs)
    {
        if (!(intervalMs > 0)) {
            return;
        }
        int bucket = static_cast<int>(intervalMs * kBucketsPerMs);
        m_Counts[static_cast<size_t>(std::min(bucket, kBuckets - 1))]++;
        m_Total++;
    }

    uint64_t total() const { return m_Total; }
    uint32_t count(int bucket) const { return m_Counts[static_cast<size_t>(bucket)]; }

    // Nearest rank, reported at the bucket's centre; 0 without samples.
    double percentileMs(double p) const
    {
        if (m_Total == 0) {
            return 0;
        }
        auto rank = static_cast<uint64_t>(std::ceil(p / 100.0 * static_cast<double>(m_Total)));
        rank = std::max<uint64_t>(rank, 1);
        uint64_t seen = 0;
        for (int i = 0; i < kBuckets; i++) {
            seen += m_Counts[static_cast<size_t>(i)];
            if (seen >= rank) {
                return (i + 0.5) / kBucketsPerMs;
            }
        }
        return (kBuckets - 0.5) / kBucketsPerMs;
    }

    // Intervals whose bucket lies wholly above thresholdMs.
    uint64_t countAbove(double thresholdMs) const
    {
        uint64_t above = 0;
        for (int i = 0; i < kBuckets; i++) {
            if (static_cast<double>(i) / kBucketsPerMs >= thresholdMs) {
                above += m_Counts[static_cast<size_t>(i)];
            }
        }
        return above;
    }

    PresentHistogram operator-(const PresentHistogram& earlier) const
    {
        PresentHistogram result;
        for (size_t i = 0; i < m_Counts.size(); i++) {
            result.m_Counts[i] = m_Counts[i] >= earlier.m_Counts[i] ? m_Counts[i] - earlier.m_Counts[i] : 0;
            result.m_Total += result.m_Counts[i];
        }
        return result;
    }

    PresentHistogram& operator+=(const PresentHistogram& other)
    {
        for (size_t i = 0; i < m_Counts.size(); i++) {
            m_Counts[i] += other.m_Counts[i];
        }
        m_Total += other.m_Total;
        return *this;
    }

private:
    std::array<uint32_t, kBuckets> m_Counts {};
    uint64_t m_Total = 0;
};
