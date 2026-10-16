#pragma once

#include "benchmarktypes.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace Benchmark {

// A benchmark's outcome, in the JSON both clients read and write (docs/benchmark.md,
// "Report"): a report made on Android opens here and the other way round.
struct Report {
    static constexpr int kVersion = 1;
    static constexpr double kUnstableDrift = 5.0;

    QString id;
    QDateTime createdAt;
    Mode mode = Mode::Quick;
    Goal goal = Goal::Balanced;
    QString hostUuid;
    QString hostName;
    QString platform;
    QString device;
    QString appVersion;
    int width = 0;
    int height = 0;
    double fps = 0;
    int warmupSeconds = 0;
    int measureSeconds = 0;
    bool cancelled = false;
    double baselineDrift = -1;      // -1 until the baseline has run twice
    std::vector<RunResult> runs;

    bool unstable() const { return baselineDrift > kUnstableDrift; }
};

// How far the baseline's balanced score moved between its first and its last run,
// or -1 if it has not run twice.
double computeBaselineDrift(const std::vector<RunResult>& runs);

QJsonObject toJson(const Report& report);
bool fromJson(const QJsonObject& json, Report& report);

// Saved reports, newest first. `directory` is <AppData>/benchmarks in the app.
class History
{
public:
    static constexpr int kMaxReports = 50;

    explicit History(QString directory);

    static QString defaultDirectory();

    bool save(const Report& report, QString* error = nullptr);
    QVector<Report> load(const QString& hostUuid = QString()) const;
    bool remove(const QString& id);

private:
    QString fileFor(const Report& report) const;
    void prune();

    QString m_Directory;
};

} // namespace Benchmark
