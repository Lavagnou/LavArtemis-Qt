#pragma once

#include "benchmarktypes.h"

#include <optional>
#include <string>
#include <vector>

namespace Benchmark {

struct PlannerInput {
    Mode mode = Mode::Quick;
    Goal goal = Goal::Balanced;
    RunConfig baseline;                     // the user's own settings
    std::vector<Codec> codecs;              // usable on this host and client, baseline's included
    std::vector<std::string> pacingModes;   // the platform list, in docs/benchmark.md's order
    int defaultBitrateKbps = 0;             // the client default for this resolution and rate
    int pyroWaveBitrateKbps = 0;            // PyroWave's own default
    int minBitrateKbps = 500;
    int maxBitrateKbps = 500000;
};

struct PlannedRun {
    Role role = Role::Candidate;
    RunConfig config;
    bool skip = false;          // record it as Skipped, without streaming
    std::string skipReason;
};

// Decides which configuration to stream next (docs/benchmark.md, "Modes").
//
// Quick and Full plan everything upfront. Adaptive plans stage by stage from the
// results so far. Either way the caller hands back one result per run already
// returned, skipped ones included: results[i] answers the i-th run.
class Planner
{
public:
    explicit Planner(PlannerInput input);

    std::optional<PlannedRun> next(const std::vector<RunResult>& results);

    // How many runs to expect; for Adaptive, the most it can take.
    int estimatedRuns() const;

    const PlannerInput& input() const { return m_Input; }

private:
    enum class Stage { Codecs, BitrateLow, BitrateClimb, Pacing, Confirm, Done };

    int bitrate(double factor) const;
    bool isPlanned(const RunConfig& config) const;
    bool append(Role role, const RunConfig& config);
    void advance(const std::vector<RunResult>& results);
    int bestIndex(const std::vector<RunResult>& results, bool (*filter)(const RunResult&, const RunConfig&), const RunConfig& reference) const;
    void markSkips(PlannedRun& run, const std::vector<RunResult>& results) const;
    void planFixed();

    PlannerInput m_Input;
    std::vector<PlannedRun> m_Plan;

    // Adaptive
    Stage m_Stage = Stage::Codecs;
    RunConfig m_Choice;             // the best configuration so far, stage by stage
    size_t m_ClimbStep = 0;
    int m_ClimbRunIndex = -1;
    int m_Declines = 0;
};

} // namespace Benchmark
