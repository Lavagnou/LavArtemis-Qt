#pragma once

#include "benchmarkplanner.h"
#include "benchmarkreport.h"

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <memory>

#include "streaming/session.h"

class ComputerManager;
class NvComputer;
class StreamingPreferences;

// Streams LavApollo's Benchmark scene once per configuration the planner picks,
// measures each run and keeps the report (docs/benchmark.md).
//
// Each run is an ordinary Session with a detached copy of the settings, so the
// user's own are never written. QML shows each session with a StreamSegue when
// sessionReady() fires; the runner measures from the session's own signals.
class BenchmarkRunner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(int runNumber READ runNumber NOTIFY changed)
    Q_PROPERTY(int runEstimate READ runEstimate NOTIFY changed)
    Q_PROPERTY(QVariantMap report READ report NOTIFY changed)

public:
    static constexpr int kWarmupSeconds = 5;
    static constexpr int kQuickMeasureSeconds = 15;
    static constexpr int kMeasureSeconds = 20;
    static constexpr int kWindowSeconds = 5;

    explicit BenchmarkRunner(QObject* parent = nullptr);
    ~BenchmarkRunner() override;

    bool running() const { return m_Running; }
    QString message() const { return m_Message; }
    int runNumber() const { return static_cast<int>(m_Report.runs.size()) + (m_Session ? 1 : 0); }
    int runEstimate() const { return m_Planner ? m_Planner->estimatedRuns() : m_Estimate; }
    QVariantMap report() const { return m_ReportView; }

    // What a benchmark of this host would do: {"available", "reason", "runs", "minutes",
    // "otherAppRunning"}. mode: quick|full|adaptive, goal: latency|smoothness|balanced.
    Q_INVOKABLE QVariantMap describe(ComputerManager* manager, const QString& hostUuid, const QString& mode, const QString& goal);
    Q_INVOKABLE void start(ComputerManager* manager, const QString& hostUuid, const QString& mode, const QString& goal);
    // Stops after the current run; what was measured is kept in the report.
    Q_INVOKABLE void cancel();
    // StreamSegue, when the run's session failed before it ever started streaming
    Q_INVOKABLE void abortRun(const QString& error);

    // Saved reports for a host, newest first: {"id", "createdAt", "mode", "goal", "runs", "best", "unstable", "cancelled"}
    Q_INVOKABLE QVariantList history(const QString& hostUuid);
    Q_INVOKABLE bool openReport(const QString& id);
    Q_INVOKABLE bool deleteReport(const QString& id);
    // A report written by the Android client opens here too (same JSON)
    Q_INVOKABLE bool importReport(const QUrl& file);

    // Puts a run's codec, bitrate and pacing into the user's settings (active profile),
    // or into a new profile made active. Only for reports made on a desktop client.
    Q_INVOKABLE bool applyRun(int runIndex);
    Q_INVOKABLE bool applyRunAsProfile(int runIndex, const QString& profileName);

    static QString describeConfig(const Benchmark::RunConfig& config);

signals:
    void changed();
    // QML pushes a StreamSegue for it. The runner owns the session.
    void sessionReady(Session* session, const QString& label);
    void finished();

private:
    NvComputer* findHost(ComputerManager* manager, const QString& hostUuid) const;
    Benchmark::PlannerInput plannerInput(NvComputer* host, Benchmark::Mode mode, Benchmark::Goal goal, const Benchmark::RunConfig& baseline) const;
    Benchmark::RunConfig requestedBaseline() const;
    void configure(StreamingPreferences* prefs, const Benchmark::RunConfig& config) const;

    void advance();
    void launch(Benchmark::Role role, const Benchmark::RunConfig& config, bool userSettings);
    void quitHostApp(std::function<void()> then);
    void onSessionFinished();
    void onReadyForDeletion();
    Benchmark::RunResult measure(Session* session) const;
    void finish();
    void setMessage(const QString& message);
    void refreshReportView();
    bool applyConfig(const Benchmark::RunConfig& config);

    bool m_Running = false;
    bool m_CancelRequested = false;
    QString m_Message;
    int m_Estimate = 0;

    NvComputer* m_Host = nullptr;
    Benchmark::Mode m_Mode = Benchmark::Mode::Quick;
    Benchmark::Goal m_Goal = Benchmark::Goal::Balanced;
    std::unique_ptr<Benchmark::Planner> m_Planner;
    Benchmark::Report m_Report;
    QVariantMap m_ReportView;

    // The run in progress
    QPointer<Session> m_Session;
    StreamingPreferences* m_RunPrefs = nullptr;
    Benchmark::Role m_RunRole = Benchmark::Role::Baseline;
    Benchmark::RunConfig m_RunRequested;
    Benchmark::RunConfig m_RunEffective;
    bool m_RunConnected = false;
    QString m_RunError;
    bool m_LastRunVrr = false;
    bool m_HostAppRunning = false;
};
