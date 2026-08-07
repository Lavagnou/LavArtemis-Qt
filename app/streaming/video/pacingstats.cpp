#include "pacingstats.h"

#include <algorithm>

PacingStats::PacingStats()
{
    reset();
}

void PacingStats::reset()
{
    QMutexLocker locker(&m_Lock);

    m_WriteIndex = 0;
    m_Count = 0;
    m_LastPresentNs = 0;
}

void PacingStats::recordPresent(uint64_t presentTimeNs)
{
    QMutexLocker locker(&m_Lock);

    if (m_LastPresentNs != 0 && presentTimeNs > m_LastPresentNs) {
        uint64_t deltaNs = presentTimeNs - m_LastPresentNs;
        if (deltaNs < k_MaxPlausibleDeltaNs) {
            m_DeltasMs[m_WriteIndex] = deltaNs / 1000000.0f;
            m_WriteIndex = (m_WriteIndex + 1) % k_WindowSize;
            if (m_Count < k_WindowSize) {
                m_Count++;
            }
        }
    }

    m_LastPresentNs = presentTimeNs;
}

PacingStats::Snapshot PacingStats::snapshot()
{
    QMutexLocker locker(&m_Lock);

    Snapshot snap;
    snap.sampleCount = m_Count;
    if (m_Count == 0) {
        return snap;
    }

    // Copy in ring order rather than chronological order. That's fine because
    // we sort immediately, and it saves untangling the wrap.
    float sorted[k_WindowSize];
    std::copy(m_DeltasMs, m_DeltasMs + m_Count, sorted);
    std::sort(sorted, sorted + m_Count);

    snap.p50Ms = sorted[(int)(m_Count * 0.50f)];
    snap.p95Ms = sorted[std::min(m_Count - 1, (int)(m_Count * 0.95f))];
    snap.p99Ms = sorted[std::min(m_Count - 1, (int)(m_Count * 0.99f))];

    return snap;
}
