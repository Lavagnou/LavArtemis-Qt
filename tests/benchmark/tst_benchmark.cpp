// Benchmark planning, scoring and reports (app/backend/benchmark). The reference values
// come from LavApollo's docs/benchmark.md; the Android client's BenchmarkScoringTest
// checks the same ones, so both clients score a run identically.

#include "../../app/backend/benchmark/benchmarkplanner.h"
#include "../../app/backend/benchmark/benchmarkreport.h"
#include "../../app/backend/benchmark/benchmarkscoring.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <set>

using namespace Benchmark;

namespace {
int failures = 0;

void check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

void near(double actual, double expected, const char* description)
{
    if (std::abs(actual - expected) > 0.0001) {
        std::fprintf(stderr, "FAIL: %s (got %.4f, expected %.4f)\n", description, actual, expected);
        ++failures;
    }
}

Metrics perfectAt60()
{
    Metrics m;
    m.targetFps = 60;
    m.framesRendered = 1200;
    m.framesExpected = 1200;
    m.presentP50Ms = 16.7;
    m.presentP99Ms = 16.7;
    return m;
}

const std::vector<std::string> kDesktopPacing = { "vsync-paced", "vsync-off", "vsync", "vrr-low-latency", "vrr-balanced" };

PlannerInput desktopInput(Mode mode)
{
    PlannerInput in;
    in.mode = mode;
    in.baseline = RunConfig { Codec::HEVC, 20000, "vsync-paced" };
    in.codecs = { Codec::H264, Codec::HEVC, Codec::AV1 };
    in.pacingModes = kDesktopPacing;
    in.defaultBitrateKbps = 20000;
    in.pyroWaveBitrateKbps = 300000;
    return in;
}

RunResult resultFor(const PlannedRun& run, double balanced)
{
    RunResult r;
    r.role = run.role;
    r.status = run.skip ? Status::Skipped : Status::Ok;
    r.requested = run.config;
    r.effective = run.config;
    r.scores.latency = balanced;
    r.scores.smoothness = balanced;
    r.scores.balanced = balanced;
    return r;
}

// Runs a planner to the end, scoring each run with `score`.
std::vector<PlannedRun> drive(Planner& planner, const std::function<double(const RunConfig&)>& score, std::vector<RunResult>* out = nullptr)
{
    std::vector<RunResult> results;
    std::vector<PlannedRun> runs;
    for (int guard = 0; guard < 100; guard++) {
        auto run = planner.next(results);
        if (!run) {
            break;
        }
        runs.push_back(*run);
        results.push_back(resultFor(*run, score(run->config)));
    }
    if (out) {
        *out = results;
    }
    return runs;
}

void testHistogram()
{
    PresentHistogram h;
    for (int i = 0; i < 98; i++) {
        h.record(16.7);
    }
    h.record(33.4);
    h.record(50.1);
    check(h.total() == 100, "histogram counts every interval");
    near(h.percentileMs(50), 16.625, "p50 at the bucket centre");
    near(h.percentileMs(99), 33.375, "p99 picks the 99th interval");
    check(h.countAbove(25.0) == 2, "two intervals over 1.5 periods");

    PresentHistogram warmup;
    warmup.record(16.7);
    warmup.record(50.1);
    PresentHistogram run = h - warmup;
    check(run.total() == 98, "subtracting the warm-up");
    check(run.countAbove(25.0) == 1, "the warm-up's hitch is gone");

    PresentHistogram empty;
    near(empty.percentileMs(50), 0, "no samples, no percentile");
    h.record(0);
    h.record(-3);
    check(h.total() == 100, "non-positive intervals are ignored");
    h.record(250);
    near(h.percentileMs(100), 99.875, "long intervals land in the last bucket");
}

void testScores()
{
    Metrics m = perfectAt60();
    m.totalMs = 10;
    near(computeScores(m).latency, 81.9, "latency at 10 ms");
    m.totalMs = 20;
    near(computeScores(m).latency, 67.0, "latency at 20 ms");
    m.totalMs = 40;
    near(computeScores(m).latency, 44.9, "latency at 40 ms");

    m = perfectAt60();
    near(computeScores(m).smoothness, 100.0, "a perfect run");
    m.stutterPct = 1;
    near(computeScores(m).smoothness, 81.9, "1% of hitches");
    m = perfectAt60();
    m.framesRendered = 1080;
    near(computeScores(m).smoothness, 81.0, "90% of frames delivered");
    m = perfectAt60();
    m.presentP99Ms = 25.0;
    near(computeScores(m).smoothness, 66.8, "a p99 half a period late");

    m = perfectAt60();
    m.totalMs = 20;
    Scores s = computeScores(m);
    near(s.balanced, 83.5, "balanced is the mean of the rounded scores");
    near(s.forGoal(Goal::Latency), 67.0, "goal picks latency");

    m = perfectAt60();
    m.framesRendered = 1300;
    near(computeScores(m).smoothness, 100.0, "delivery is capped at 100%");
    Metrics none;
    near(computeScores(none).smoothness, 0, "nothing rendered scores 0");

    near(standardDeviation({ 1, 3 }), 1.0, "standard deviation");
    near(standardDeviation({ 5 }), 0.0, "one value has no spread");
}

void testMetrics()
{
    RawStats s;
    s.elapsedUs = 20e6;
    s.framesTotal = 1210;
    s.framesNetworkLost = 10;
    s.framesReceived = 1200;
    s.framesDecoded = 1200;
    s.framesRendered = 1190;
    s.hostLatencySum = 1200 * 41;      // 4.1 ms per frame
    s.hostLatencyFrames = 1200;
    s.receiveSumUs = 1200 * 2300;
    s.decodeSumUs = 1200 * 1900;
    s.queueSumUs = 1190 * 1000;
    s.renderSumUs = 1190 * 6400;
    s.rttSumMs = 20 * 2.4;
    s.rttVarianceSumMs = 20 * 0.3;
    s.rttSamples = 20;
    for (int i = 0; i < 1189; i++) {
        s.presents.record(16.7);
    }

    Metrics m = metricsFromStats(s, 60);
    near(m.hostMs, 4.1, "host latency in ms");
    near(m.networkMs, 1.2, "half the RTT");
    near(m.receiveMs, 2.3, "receive time");
    near(m.decodeMs, 1.9, "decode time");
    near(m.queueMs, 1.0, "queue delay");
    near(m.renderMs, 6.4, "render time");
    near(m.totalMs, 16.9, "the sum of the stages");
    check(m.framesExpected == 1200, "20 s at 60 fps");
    near(m.networkLossPct, 100.0 * 10 / 1210, "network loss");
    near(m.stutterPct, 0, "no hitches");

    RawStats later = s;
    later.elapsedUs += 5e6;
    later.framesRendered += 300;
    later.presents.record(40);
    RawStats delta = later - s;
    near(delta.elapsedUs, 5e6, "snapshots subtract");
    check(delta.framesRendered == 300 && delta.presents.total() == 1, "counters subtract");
}

void testQuick()
{
    Planner planner(desktopInput(Mode::Quick));
    auto runs = drive(planner, [](const RunConfig&) { return 50.0; });
    check(runs.size() == 6, "quick: baseline, 2 codecs, 2 pacing modes, baseline again");
    check(runs.size() == static_cast<size_t>(planner.estimatedRuns()), "quick estimate is exact");
    check(runs.front().role == Role::Baseline && runs.back().role == Role::BaselineRepeat, "quick starts and ends with the baseline");
    check(runs[1].config == RunConfig { Codec::H264, 20000, "vsync-paced" }, "other codecs at the baseline bitrate");
    check(runs[2].config.codec == Codec::AV1, "every other codec");
    check(runs[3].config.pacing == "vsync-off" && runs[4].config.pacing == "vsync", "the first two other pacing modes");

    PlannerInput pyro = desktopInput(Mode::Quick);
    pyro.baseline = RunConfig { Codec::PyroWave, 300000, "vsync-paced" };
    pyro.codecs = { Codec::H264, Codec::PyroWave };
    Planner pyroPlanner(pyro);
    auto pyroRuns = drive(pyroPlanner, [](const RunConfig&) { return 50.0; });
    check(pyroRuns[1].config == RunConfig { Codec::H264, 20000, "vsync-paced" }, "PyroWave's bitrate is not given to H.264");
}

void testFull()
{
    PlannerInput in = desktopInput(Mode::Full);
    in.codecs = { Codec::H264, Codec::HEVC, Codec::AV1, Codec::PyroWave };
    Planner planner(in);
    auto runs = drive(planner, [](const RunConfig&) { return 50.0; });
    // baseline + h264 x3 + hevc x2 (1x is the baseline) + av1 x3 + pyrowave + 4 pacing modes + repeat
    check(runs.size() == 15, "full plan size");
    bool vrrSeen = false;
    bool orderKept = true;
    for (size_t i = 1; i + 1 < runs.size(); i++) {
        if (isVrrPacing(runs[i].config.pacing)) {
            vrrSeen = true;
        }
        else if (vrrSeen) {
            orderKept = false;
        }
    }
    check(vrrSeen && orderKept, "VRR runs come last, together");
    check(runs[1].config == RunConfig { Codec::H264, 10000, "vsync-paced" }, "half the default bitrate");
    check(runs[4].config == RunConfig { Codec::HEVC, 10000, "vsync-paced" }, "the baseline's own config is not repeated");
}

void testSkips()
{
    Planner planner(desktopInput(Mode::Full));
    std::vector<RunResult> results;
    int skipped = 0;
    for (int guard = 0; guard < 100; guard++) {
        auto run = planner.next(results);
        if (!run) {
            break;
        }
        RunResult r = resultFor(*run, 50);
        if (!run->skip && run->config.codec == Codec::AV1 && run->role == Role::Candidate) {
            r.status = Status::Fallback;
            r.effective.codec = Codec::HEVC;
        }
        if (!run->skip && run->config.pacing == "vrr-low-latency") {
            r.status = Status::Fallback;
            r.effective.pacing = "vsync-paced";
        }
        if (run->skip) {
            skipped++;
            check(run->config.codec == Codec::AV1 || isVrrPacing(run->config.pacing), "only the unavailable runs are skipped");
        }
        results.push_back(r);
    }
    check(skipped == 3, "two more AV1 runs and the other VRR run are skipped");
}

void testAdaptive()
{
    PlannerInput in = desktopInput(Mode::Adaptive);
    in.goal = Goal::Latency;
    Planner planner(in);
    // AV1 wins, latency peaks at 30 Mbps, and V-Sync off is best
    auto score = [](const RunConfig& c) {
        double s = c.codec == Codec::AV1 ? 60 : 50;
        s -= std::abs(c.bitrateKbps - 30000) / 2000.0;
        if (c.pacing == "vsync-off") {
            s += 5;
        }
        return s;
    };
    std::vector<RunResult> results;
    auto runs = drive(planner, score, &results);

    check(runs.front().role == Role::Baseline, "adaptive starts with the baseline");
    check(runs[1].config.codec == Codec::H264 && runs[2].config.codec == Codec::AV1, "then every other codec");
    check(runs[3].config == RunConfig { Codec::AV1, 10000, "vsync-paced" }, "then half the default on the best codec");
    check(runs[4].config.bitrateKbps == 30000 && runs[5].config.bitrateKbps == 40000 && runs[6].config.bitrateKbps == 60000,
          "climbs 1.5x, 2x, 3x");
    check(runs[7].config == RunConfig { Codec::AV1, 30000, "vsync-off" }, "pacing modes on the best bitrate");
    const auto& confirm = runs[runs.size() - 2];
    check(confirm.role == Role::Confirm && confirm.config == RunConfig { Codec::AV1, 30000, "vsync-off" }, "the winner runs again");
    check(runs.back().role == Role::BaselineRepeat, "and the baseline last");
    check(static_cast<int>(runs.size()) <= planner.estimatedRuns(), "adaptive stays within its estimate");

    // Climbing stops at the first lossy run
    Planner lossy(desktopInput(Mode::Adaptive));
    std::vector<RunResult> lossyResults;
    std::vector<PlannedRun> lossyRuns;
    for (int guard = 0; guard < 100; guard++) {
        auto run = lossy.next(lossyResults);
        if (!run) {
            break;
        }
        lossyRuns.push_back(*run);
        RunResult r = resultFor(*run, 50 + run->config.bitrateKbps / 1000.0);
        if (run->config.bitrateKbps >= 30000) {
            r.metrics.networkLossPct = 3;
        }
        lossyResults.push_back(r);
    }
    std::set<int> climbed;
    for (const auto& run : lossyRuns) {
        if (run.config.bitrateKbps > 20000) {
            climbed.insert(run.config.bitrateKbps);
        }
    }
    check(climbed.size() == 1, "a lossy bitrate ends the climb");
}

void testReport()
{
    Report report;
    report.id = "abc";
    report.createdAt = QDateTime(QDate(2026, 10, 16), QTime(22, 40), QTimeZone::UTC);
    report.mode = Mode::Adaptive;
    report.goal = Goal::Smoothness;
    report.hostUuid = "HOST";
    report.hostName = "ATLAS-PC";
    report.platform = "windows";
    report.width = 1920;
    report.height = 1080;
    report.fps = 60;
    RunResult base;
    base.role = Role::Baseline;
    base.requested = base.effective = RunConfig { Codec::HEVC, 20000, "vsync-paced" };
    base.scores.balanced = 80;
    base.metrics.totalMs = 16.9;
    RunResult repeat = base;
    repeat.role = Role::BaselineRepeat;
    repeat.scores.balanced = 73.5;
    RunResult fallback = base;
    fallback.role = Role::Candidate;
    fallback.status = Status::Fallback;
    fallback.requested.codec = Codec::AV1;
    fallback.note = "fell back";
    report.runs = { base, fallback, repeat };
    report.baselineDrift = computeBaselineDrift(report.runs);
    near(report.baselineDrift, 6.5, "drift between the two baseline runs");
    check(report.unstable(), "more than 5 points is unstable");

    Report back;
    check(fromJson(toJson(report), back), "a report reads back");
    check(back.id == "abc" && back.createdAt == report.createdAt && back.mode == Mode::Adaptive && back.goal == Goal::Smoothness,
          "header round trip");
    check(back.runs.size() == 3 && back.runs[1].status == Status::Fallback && back.runs[1].requested.codec == Codec::AV1
              && back.runs[1].effective.codec == Codec::HEVC && back.runs[1].note == "fell back",
          "runs round trip");
    near(back.runs[0].metrics.totalMs, 16.9, "metrics round trip");

    QJsonObject json = toJson(report);
    QJsonArray runs = json["runs"].toArray();
    QJsonObject future = runs[0].toObject();
    QJsonObject requested = future["requested"].toObject();
    requested["codec"] = "vvc";
    future["requested"] = requested;
    runs.append(future);
    json["runs"] = runs;
    json["somethingNew"] = 1;
    check(fromJson(json, back) && back.runs.size() == 3, "unknown codecs and fields are ignored");
    json["schema"] = "other";
    check(!fromJson(json, back), "another schema is refused");

    QTemporaryDir dir;
    History history(dir.path());
    for (int i = 0; i < History::kMaxReports + 3; i++) {
        Report r = report;
        r.id = QString("r%1").arg(i);
        r.createdAt = report.createdAt.addSecs(i);
        r.hostUuid = i % 2 ? "HOST" : "OTHER";
        check(history.save(r), "a report saves");
    }
    auto all = history.load();
    check(all.size() == History::kMaxReports, "history keeps 50 reports");
    check(all.front().id == QString("r%1").arg(History::kMaxReports + 2), "newest first");
    auto mine = history.load("host");
    check(!mine.isEmpty() && std::all_of(mine.begin(), mine.end(), [](const Report& r) { return r.hostUuid == "HOST"; }),
          "filtered by host, whatever the case");
    check(history.remove(all.front().id) && history.load().size() == History::kMaxReports - 1, "a report can be deleted");
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    testHistogram();
    testScores();
    testMetrics();
    testQuick();
    testFull();
    testSkips();
    testAdaptive();
    testReport();
    if (failures == 0) {
        std::printf("tst_benchmark: all checks passed\n");
    }
    return failures == 0 ? 0 : 1;
}
