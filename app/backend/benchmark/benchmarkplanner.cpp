#include "benchmarkplanner.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace Benchmark {

namespace {

const double kClimbFactors[] = { 1.5, 2.0, 3.0 };
constexpr double kClimbLossLimitPct = 1.0;
constexpr int kClimbDeclineLimit = 2;

bool anyResult(const RunResult&, const RunConfig&)
{
    return true;
}

bool sameCodecAndPacing(const RunResult& result, const RunConfig& reference)
{
    return result.requested.codec == reference.codec && result.requested.pacing == reference.pacing;
}

} // namespace

Planner::Planner(PlannerInput input)
    : m_Input(std::move(input))
{
    if (m_Input.mode == Mode::Adaptive) {
        append(Role::Baseline, m_Input.baseline);
        for (Codec codec : m_Input.codecs) {
            const int kbps = codec == Codec::PyroWave ? m_Input.pyroWaveBitrateKbps : bitrate(1.0);
            append(Role::Candidate, RunConfig { codec, kbps, m_Input.baseline.pacing });
        }
    }
    else {
        planFixed();
    }
}

int Planner::bitrate(double factor) const
{
    const long kbps = std::lround(m_Input.defaultBitrateKbps * factor / 500.0) * 500;
    return static_cast<int>(std::clamp<long>(kbps, m_Input.minBitrateKbps, m_Input.maxBitrateKbps));
}

bool Planner::isPlanned(const RunConfig& config) const
{
    return std::any_of(m_Plan.begin(), m_Plan.end(), [&](const PlannedRun& run) {
        return run.config == config;
    });
}

bool Planner::append(Role role, const RunConfig& config)
{
    // Repeats are the only runs allowed to stream a configuration twice
    const bool repeat = role == Role::BaselineRepeat || role == Role::Confirm;
    if (!repeat && isPlanned(config)) {
        return false;
    }
    PlannedRun run;
    run.role = role;
    run.config = config;
    m_Plan.push_back(run);
    return true;
}

void Planner::planFixed()
{
    const RunConfig& base = m_Input.baseline;
    append(Role::Baseline, base);

    if (m_Input.mode == Mode::Quick) {
        for (Codec codec : m_Input.codecs) {
            if (codec != base.codec) {
                // PyroWave's bitrate means nothing to the other codecs: they get the default then
                const int kbps = codec == Codec::PyroWave ? m_Input.pyroWaveBitrateKbps
                               : base.codec == Codec::PyroWave ? bitrate(1.0) : base.bitrateKbps;
                append(Role::Candidate, RunConfig { codec, kbps, base.pacing });
            }
        }
        int variants = 0;
        for (const auto& pacing : m_Input.pacingModes) {
            if (variants < 2 && pacing != base.pacing && append(Role::Candidate, RunConfig { base.codec, base.bitrateKbps, pacing })) {
                variants++;
            }
        }
    }
    else {
        for (Codec codec : m_Input.codecs) {
            if (codec == Codec::PyroWave) {
                append(Role::Candidate, RunConfig { codec, m_Input.pyroWaveBitrateKbps, base.pacing });
                continue;
            }
            for (double factor : { 0.5, 1.0, 2.0 }) {
                append(Role::Candidate, RunConfig { codec, bitrate(factor), base.pacing });
            }
        }
        for (const auto& pacing : m_Input.pacingModes) {
            if (pacing != base.pacing) {
                append(Role::Candidate, RunConfig { base.codec, base.bitrateKbps, pacing });
            }
        }
    }

    // Runs that request VRR differently from the baseline go last, together: switching
    // makes the host quit and relaunch the scene to rebuild its virtual display.
    const bool baseVrr = isVrrPacing(base.pacing);
    std::stable_partition(m_Plan.begin() + 1, m_Plan.end(), [baseVrr](const PlannedRun& run) {
        return isVrrPacing(run.config.pacing) == baseVrr;
    });

    append(Role::BaselineRepeat, base);
}

int Planner::bestIndex(const std::vector<RunResult>& results, bool (*filter)(const RunResult&, const RunConfig&), const RunConfig& reference) const
{
    int best = -1;
    for (size_t i = 0; i < results.size(); i++) {
        const auto& r = results[i];
        if (r.status != Status::Ok || !filter(r, reference)) {
            continue;
        }
        if (best < 0 || r.scores.forGoal(m_Input.goal) > results[static_cast<size_t>(best)].scores.forGoal(m_Input.goal)) {
            best = static_cast<int>(i);
        }
    }
    return best;
}

void Planner::advance(const std::vector<RunResult>& results)
{
    switch (m_Stage) {
    case Stage::Codecs: {
        const int best = bestIndex(results, anyResult, m_Input.baseline);
        if (best < 0) {
            m_Stage = Stage::Done;
            return;
        }
        m_Choice = results[static_cast<size_t>(best)].requested;
        if (m_Choice.codec == Codec::PyroWave) {
            m_Stage = Stage::Pacing;   // PyroWave's bitrate is what its link allows, not a dial
            return;
        }
        m_Stage = Stage::BitrateLow;
        append(Role::Candidate, RunConfig { m_Choice.codec, bitrate(0.5), m_Choice.pacing });
        return;
    }

    case Stage::BitrateLow:
        m_Stage = Stage::BitrateClimb;
        m_ClimbStep = 0;
        m_ClimbRunIndex = -1;
        m_Declines = 0;
        [[fallthrough]];

    case Stage::BitrateClimb: {
        bool climb = true;
        if (m_ClimbRunIndex >= 0) {
            const auto& last = results[static_cast<size_t>(m_ClimbRunIndex)];
            const int best = bestIndex(results, sameCodecAndPacing, m_Choice);
            if (last.status != Status::Ok || last.metrics.networkLossPct > kClimbLossLimitPct) {
                climb = false;
            }
            else if (best >= 0 && last.scores.forGoal(m_Input.goal) < results[static_cast<size_t>(best)].scores.forGoal(m_Input.goal)) {
                climb = ++m_Declines < kClimbDeclineLimit;
            }
            else {
                m_Declines = 0;
            }
        }
        while (climb && m_ClimbStep < std::size(kClimbFactors)) {
            const RunConfig next { m_Choice.codec, bitrate(kClimbFactors[m_ClimbStep++]), m_Choice.pacing };
            if (append(Role::Candidate, next)) {
                m_ClimbRunIndex = static_cast<int>(m_Plan.size()) - 1;
                return;
            }
        }
        const int best = bestIndex(results, sameCodecAndPacing, m_Choice);
        if (best >= 0) {
            m_Choice.bitrateKbps = results[static_cast<size_t>(best)].requested.bitrateKbps;
        }
        m_Stage = Stage::Pacing;
        return;
    }

    case Stage::Pacing: {
        m_Stage = Stage::Confirm;
        for (const auto& pacing : m_Input.pacingModes) {
            append(Role::Candidate, RunConfig { m_Choice.codec, m_Choice.bitrateKbps, pacing });
        }
        return;
    }

    case Stage::Confirm: {
        m_Stage = Stage::Done;
        const int winner = bestIndex(results, anyResult, m_Input.baseline);
        if (winner >= 0 && results[static_cast<size_t>(winner)].requested != m_Input.baseline) {
            append(Role::Confirm, results[static_cast<size_t>(winner)].requested);
        }
        append(Role::BaselineRepeat, m_Input.baseline);
        return;
    }

    case Stage::Done:
        return;
    }
}

void Planner::markSkips(PlannedRun& run, const std::vector<RunResult>& results) const
{
    if (run.role == Role::Baseline || run.role == Role::BaselineRepeat) {
        return;   // the reference streams whatever the user streams
    }
    for (const auto& r : results) {
        if (r.status != Status::Fallback) {
            continue;
        }
        if (r.requested.codec == run.config.codec && r.effective.codec != r.requested.codec) {
            run.skip = true;
            run.skipReason = std::string(toString(run.config.codec)) + " is not available: the client fell back to " + toString(r.effective.codec);
            return;
        }
        if (isVrrPacing(run.config.pacing) && isVrrPacing(r.requested.pacing) && !isVrrPacing(r.effective.pacing)) {
            run.skip = true;
            run.skipReason = "VRR is not available on this display";
            return;
        }
    }
}

std::optional<PlannedRun> Planner::next(const std::vector<RunResult>& results)
{
    if (m_Input.mode == Mode::Adaptive) {
        while (results.size() >= m_Plan.size() && m_Stage != Stage::Done) {
            advance(results);
        }
    }
    if (results.size() >= m_Plan.size()) {
        return std::nullopt;
    }
    PlannedRun run = m_Plan[results.size()];
    markSkips(run, results);
    return run;
}

int Planner::estimatedRuns() const
{
    if (m_Input.mode != Mode::Adaptive) {
        return static_cast<int>(m_Plan.size());
    }
    // Codec stage (already planned), the low bitrate, the full climb, the pacing modes and the two repeats
    return static_cast<int>(m_Plan.size()) + 1 + static_cast<int>(std::size(kClimbFactors))
        + static_cast<int>(m_Input.pacingModes.size()) - 1 + 2;
}

} // namespace Benchmark
