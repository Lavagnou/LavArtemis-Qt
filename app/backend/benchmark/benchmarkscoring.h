#pragma once

#include "benchmarktypes.h"

#include <vector>

namespace Benchmark {

// Averages and percentiles of a stretch of stream. targetFps is the stream's rate.
Metrics metricsFromStats(const RawStats& stats, double targetFps);

// The two scores out of 100 (docs/benchmark.md, "Scores"). Absolute formulas, not
// relative to the other runs, so that reports stay comparable over time.
Scores computeScores(const Metrics& metrics);

// Population standard deviation; 0 for fewer than two values.
double standardDeviation(const std::vector<double>& values);

// Rounds to one decimal, the precision reports carry.
double roundScore(double value);

} // namespace Benchmark
