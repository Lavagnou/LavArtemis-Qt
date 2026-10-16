#include "benchmarkrunner.h"

#include "benchmarkscoring.h"

#include "backend/computermanager.h"
#include "backend/nvapp.h"
#include "backend/nvcomputer.h"
#include "backend/nvhttp.h"
#include "settings/profilemanager.h"
#include "settings/streamingpreferences.h"
#include "streaming/session.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QLocale>
#include <QReadLocker>
#include <QSysInfo>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include <algorithm>

using namespace Benchmark;

namespace {

// Pacing modes in docs/benchmark.md's order. VRR last: the host rebuilds its display for them.
const std::vector<std::string> kPacingModes = { "vsync-paced", "vsync-off", "vsync", "vrr-low-latency", "vrr-balanced" };

// Seconds a run costs beyond its measured time: connecting, warming up, tearing down
constexpr int kRunOverheadSeconds = 12;

std::string pacingOf(bool vsync, bool framePacing, bool vrr, int vrrLatencyMode)
{
    static const char* const vrrModes[] = { "vrr-smooth", "vrr-balanced", "vrr-low-latency" };
    if (vrr && vsync) {
        return vrrModes[qBound(0, vrrLatencyMode, 2)];
    }
    if (!vsync) {
        return "vsync-off";
    }
    return framePacing ? "vsync-paced" : "vsync";
}

QString codecLabel(Codec codec)
{
    switch (codec) {
    case Codec::H264: return QStringLiteral("H.264");
    case Codec::HEVC: return QStringLiteral("HEVC");
    case Codec::AV1: return QStringLiteral("AV1");
    case Codec::PyroWave: return QStringLiteral("PyroWave");
    }
    return QString();
}

QString pacingLabel(const std::string& pacing)
{
    if (pacing == "vsync-off") return BenchmarkRunner::tr("V-Sync off");
    if (pacing == "vsync") return BenchmarkRunner::tr("V-Sync");
    if (pacing == "vsync-paced") return BenchmarkRunner::tr("V-Sync + frame pacing");
    if (pacing == "vrr-low-latency") return BenchmarkRunner::tr("VRR, low latency");
    if (pacing == "vrr-balanced") return BenchmarkRunner::tr("VRR, balanced");
    if (pacing == "vrr-smooth") return BenchmarkRunner::tr("VRR, smooth");
    // Android's modes, for reports made there
    if (pacing == "default") return BenchmarkRunner::tr("Default pacing");
    if (pacing == "ull") return BenchmarkRunner::tr("Ultra low latency");
    if (pacing == "paced-ull") return BenchmarkRunner::tr("Paced ultra low latency");
    if (pacing == "predictive") return BenchmarkRunner::tr("Predictive pacing");
    return QString::fromStdString(pacing);
}

QString roleLabel(Role role)
{
    switch (role) {
    case Role::Baseline: return BenchmarkRunner::tr("Your settings");
    case Role::BaselineRepeat: return BenchmarkRunner::tr("Your settings, again");
    case Role::Confirm: return BenchmarkRunner::tr("Winner, again");
    case Role::Candidate: return QString();
    }
    return QString();
}

QVariantMap metricsToMap(const Metrics& m)
{
    return QVariantMap {
        { "hostMs", m.hostMs }, { "networkMs", m.networkMs }, { "receiveMs", m.receiveMs },
        { "decodeMs", m.decodeMs }, { "queueMs", m.queueMs }, { "renderMs", m.renderMs },
        { "totalMs", m.totalMs }, { "rttMs", m.rttMs }, { "targetFps", m.targetFps },
        { "framesRendered", static_cast<double>(m.framesRendered) },
        { "framesExpected", static_cast<double>(m.framesExpected) },
        { "deliveredPct", m.framesExpected > 0 ? 100.0 * static_cast<double>(m.framesRendered) / static_cast<double>(m.framesExpected) : 0.0 },
        { "networkLossPct", m.networkLossPct }, { "presentP50Ms", m.presentP50Ms },
        { "presentP95Ms", m.presentP95Ms }, { "presentP99Ms", m.presentP99Ms }, { "stutterPct", m.stutterPct },
    };
}

} // namespace

BenchmarkRunner::BenchmarkRunner(QObject* parent)
    : QObject(parent)
{
}

BenchmarkRunner::~BenchmarkRunner() = default;

QString BenchmarkRunner::describeConfig(const RunConfig& config)
{
    return tr("%1, %2 Mbps, %3").arg(codecLabel(config.codec),
                                     QString::number(config.bitrateKbps / 1000.0, 'f', config.bitrateKbps % 1000 ? 1 : 0),
                                     pacingLabel(config.pacing));
}

NvComputer* BenchmarkRunner::findHost(ComputerManager* manager, const QString& hostUuid) const
{
    if (manager == nullptr) {
        return nullptr;
    }
    const auto hosts = manager->getComputers();
    auto it = std::find_if(hosts.begin(), hosts.end(), [&](NvComputer* computer) {
        return computer->uuid == hostUuid;
    });
    return it != hosts.end() ? *it : nullptr;
}

RunConfig BenchmarkRunner::requestedBaseline() const
{
    StreamingPreferences* prefs = StreamingPreferences::get();
    RunConfig config;
    switch (prefs->videoCodecConfig) {
    case StreamingPreferences::VCC_FORCE_H264: config.codec = Codec::H264; break;
    case StreamingPreferences::VCC_FORCE_AV1: config.codec = Codec::AV1; break;
    case StreamingPreferences::VCC_FORCE_PYROWAVE: config.codec = Codec::PyroWave; break;
    default: config.codec = Codec::HEVC; break;   // Auto: what most hosts and GPUs end up with
    }
    config.bitrateKbps = prefs->bitrateKbps;
    config.pacing = pacingOf(prefs->enableVsync, prefs->framePacing, prefs->enableVrr, prefs->vrrLatencyMode);
    return config;
}

PlannerInput BenchmarkRunner::plannerInput(NvComputer* host, Mode mode, Goal goal, const RunConfig& baseline) const
{
    StreamingPreferences* prefs = StreamingPreferences::get();
    PlannerInput in;
    in.mode = mode;
    in.goal = goal;
    in.baseline = baseline;

    int codecSupport;
    {
        QReadLocker lock(&host->lock);
        codecSupport = host->serverCodecModeSupport;
    }
    // The host's word on what it encodes; what this PC decodes shows up as fallbacks
    in.codecs.push_back(Codec::H264);
    if (codecSupport & SCM_MASK_HEVC) {
        in.codecs.push_back(Codec::HEVC);
    }
    if (codecSupport & SCM_MASK_AV1) {
        in.codecs.push_back(Codec::AV1);
    }
#ifdef HAVE_PYROWAVE
    if (codecSupport & SCM_MASK_PYROWAVE) {
        in.codecs.push_back(Codec::PyroWave);
    }
#endif
    if (std::find(in.codecs.begin(), in.codecs.end(), baseline.codec) == in.codecs.end()) {
        in.codecs.push_back(baseline.codec);
    }

    in.pacingModes = kPacingModes;
    in.defaultBitrateKbps = StreamingPreferences::getDefaultBitrate(prefs->width, prefs->height, prefs->fps, prefs->enableYUV444);
    in.pyroWaveBitrateKbps = StreamingPreferences::getDefaultPyroWaveBitrate(prefs->width, prefs->height, prefs->fps, prefs->enableYUV444, prefs->enableHdr);
    in.minBitrateKbps = 500;
    in.maxBitrateKbps = prefs->unlockBitrate ? 500000 : 150000;
    return in;
}

void BenchmarkRunner::configure(StreamingPreferences* prefs, const RunConfig& config) const
{
    switch (config.codec) {
    case Codec::H264: prefs->videoCodecConfig = StreamingPreferences::VCC_FORCE_H264; break;
    case Codec::HEVC: prefs->videoCodecConfig = StreamingPreferences::VCC_FORCE_HEVC; break;
    case Codec::AV1: prefs->videoCodecConfig = StreamingPreferences::VCC_FORCE_AV1; break;
    case Codec::PyroWave: prefs->videoCodecConfig = StreamingPreferences::VCC_FORCE_PYROWAVE; break;
    }
    prefs->bitrateKbps = config.bitrateKbps;

    if (isVrrPacing(config.pacing)) {
        prefs->enableVsync = true;
        prefs->enableVrr = true;
        prefs->vrrLatencyMode = config.pacing == "vrr-low-latency" ? StreamingPreferences::VLM_LOW_LATENCY
                              : config.pacing == "vrr-smooth" ? StreamingPreferences::VLM_SMOOTH
                              : StreamingPreferences::VLM_BALANCED_TARGET;
    }
    else {
        prefs->enableVrr = false;
        prefs->enableVsync = config.pacing != "vsync-off";
        prefs->framePacing = config.pacing == "vsync-paced";
    }
}

QVariantMap BenchmarkRunner::describe(ComputerManager* manager, const QString& hostUuid, const QString& mode, const QString& goal)
{
    QVariantMap result;
    NvComputer* host = findHost(manager, hostUuid);
    int benchmarkAppId = 0;
    int currentGameId = 0;
    if (host != nullptr) {
        QReadLocker lock(&host->lock);
        benchmarkAppId = host->benchmarkAppId;
        currentGameId = host->currentGameId;
    }

    if (host == nullptr) {
        result["available"] = false;
        result["reason"] = tr("This host is not known.");
        return result;
    }
    if (benchmarkAppId == 0) {
        result["available"] = false;
        result["reason"] = tr("This host doesn't offer the benchmark. It needs LavApollo 2.3 or later, "
                              "with \"Benchmark app for LavArtemis\" turned on (Audio/Video settings).");
        return result;
    }

    Mode m = Mode::Quick;
    Goal g = Goal::Balanced;
    fromString(mode.toStdString(), m);
    fromString(goal.toStdString(), g);
    Planner planner(plannerInput(host, m, g, requestedBaseline()));
    const int runs = planner.estimatedRuns();
    const int measure = m == Mode::Quick ? kQuickMeasureSeconds : kMeasureSeconds;

    result["available"] = true;
    result["runs"] = runs;
    result["minutes"] = qMax(1, (runs * (kWarmupSeconds + measure + kRunOverheadSeconds) + 59) / 60);
    result["otherAppRunning"] = currentGameId != 0 && currentGameId != benchmarkAppId;
    result["baseline"] = describeConfig(requestedBaseline());
    return result;
}

void BenchmarkRunner::start(ComputerManager* manager, const QString& hostUuid, const QString& mode, const QString& goal)
{
    if (m_Running) {
        return;
    }
    const QVariantMap check = describe(manager, hostUuid, mode, goal);
    if (!check.value("available").toBool()) {
        setMessage(check.value("reason").toString());
        return;
    }

    m_Host = findHost(manager, hostUuid);
    fromString(mode.toStdString(), m_Mode);
    fromString(goal.toStdString(), m_Goal);
    m_Planner.reset();
    m_Estimate = check.value("runs").toInt();
    m_CancelRequested = false;
    m_LastRunVrr = false;

    StreamingPreferences* prefs = StreamingPreferences::get();
    m_Report = Report();
    m_Report.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_Report.createdAt = QDateTime::currentDateTimeUtc();
    m_Report.mode = m_Mode;
    m_Report.goal = m_Goal;
    {
        QReadLocker lock(&m_Host->lock);
        m_Report.hostUuid = m_Host->uuid;
        m_Report.hostName = m_Host->name;
        m_HostAppRunning = m_Host->currentGameId != 0;
    }
#if defined(Q_OS_WIN)
    m_Report.platform = "windows";
#elif defined(Q_OS_DARWIN)
    m_Report.platform = "macos";
#else
    m_Report.platform = "linux";
#endif
    m_Report.device = QSysInfo::machineHostName();
    m_Report.appVersion = QCoreApplication::applicationVersion();
    m_Report.width = prefs->width;
    m_Report.height = prefs->height;
    m_Report.fps = prefs->fps;
    m_Report.warmupSeconds = kWarmupSeconds;
    m_Report.measureSeconds = m_Mode == Mode::Quick ? kQuickMeasureSeconds : kMeasureSeconds;

    m_Running = true;
    refreshReportView();

    // Start from a clean host: another app would refuse the scene's launch (the setup page
    // said it would be quit), and a scene left by an earlier benchmark may have the wrong display
    if (m_HostAppRunning) {
        setMessage(tr("Quitting the app running on the host..."));
        quitHostApp([this]() { advance(); });
    }
    else {
        advance();
    }
}

void BenchmarkRunner::cancel()
{
    if (!m_Running) {
        return;
    }
    m_CancelRequested = true;
    setMessage(tr("Stopping after this run..."));
    if (m_Session) {
        m_Session->interrupt();
    }
    else {
        finish();
    }
}

void BenchmarkRunner::quitHostApp(std::function<void()> then)
{
    NvComputer* host = m_Host;
    QPointer<BenchmarkRunner> self(this);
    QThreadPool::globalInstance()->start([host, self, then]() {
        try {
            NvHTTP(host).quitApp();
        } catch (...) {
            // Nothing running is fine; anything else surfaces as the next launch's error
        }
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, then]() {
            if (self) {
                self->m_HostAppRunning = false;
                then();
            }
        }, Qt::QueuedConnection);
    });
}

void BenchmarkRunner::advance()
{
    if (!m_Running) {
        return;
    }
    if (m_CancelRequested) {
        finish();
        return;
    }

    if (!m_Planner) {
        if (m_Report.runs.empty()) {
            launch(Role::Baseline, requestedBaseline(), true);
            return;
        }
        // The plan starts from what the baseline really ran as: Auto settles on one codec
        const RunResult& baseline = m_Report.runs.front();
        if (baseline.status != Status::Ok) {
            setMessage(tr("Streaming with your settings failed, so there is nothing to compare with: %1")
                           .arg(QString::fromStdString(baseline.note)));
            finish();
            return;
        }
        m_Planner = std::make_unique<Planner>(plannerInput(m_Host, m_Mode, m_Goal, baseline.effective));
        emit changed();
    }

    auto run = m_Planner->next(m_Report.runs);
    if (!run) {
        finish();
        return;
    }

    if (run->skip) {
        RunResult skipped;
        skipped.role = run->role;
        skipped.status = Status::Skipped;
        skipped.note = run->skipReason;
        skipped.requested = run->config;
        skipped.effective = run->config;
        m_Report.runs.push_back(skipped);
        refreshReportView();
        QTimer::singleShot(0, this, &BenchmarkRunner::advance);
        return;
    }

    const bool userSettings = run->role == Role::Baseline || run->role == Role::BaselineRepeat;
    launch(run->role, run->config, userSettings);
}

void BenchmarkRunner::launch(Role role, const RunConfig& config, bool userSettings)
{
    const bool vrr = isVrrPacing(userSettings ? requestedBaseline().pacing : config.pacing);

    // The host only builds its 1000 Hz display for VRR at /launch: switch through a quit
    if (m_HostAppRunning && vrr != m_LastRunVrr) {
        setMessage(tr("Restarting the scene on the host..."));
        quitHostApp([this, role, config, userSettings]() { launch(role, config, userSettings); });
        return;
    }

    if (m_RunPrefs != nullptr) {
        m_RunPrefs->deleteLater();
    }
    m_RunPrefs = StreamingPreferences::createDetached(this);
    if (!userSettings) {
        configure(m_RunPrefs, config);
    }
    m_RunPrefs->quitAppAfter = false;
    m_RunPrefs->useMultiDisplay = false;

    NvApp app;
    {
        QReadLocker lock(&m_Host->lock);
        app.id = m_Host->benchmarkAppId;
        app.uuid = m_Host->benchmarkAppUuid;
    }
    app.name = QStringLiteral("Benchmark");

    m_RunRole = role;
    m_RunRequested = userSettings ? requestedBaseline() : config;
    m_RunConnected = false;
    m_RunError.clear();
    m_LastRunVrr = vrr;

    const int number = static_cast<int>(m_Report.runs.size()) + 1;
    const QString label = roleLabel(role).isEmpty() ? describeConfig(m_RunRequested)
                                                    : roleLabel(role) + " (" + describeConfig(m_RunRequested) + ")";
    const int measure = m_Mode == Mode::Quick ? kQuickMeasureSeconds : kMeasureSeconds;

    auto session = new Session(m_Host, app, m_RunPrefs);
    session->setBenchmarkMode(kWarmupSeconds, measure, kWindowSeconds,
                              tr("run %1 of %2, %3").arg(number).arg(runEstimate()).arg(label));
    connect(session, &Session::connectionStarted, this, [this]() {
        m_RunConnected = true;
        m_HostAppRunning = true;
    });
    connect(session, &Session::displayLaunchError, this, [this](QString text) {
        m_RunError = text;
    });
    connect(session, &Session::stageFailed, this, [this](QString stage, int errorCode, QString) {
        m_RunError = tr("Starting %1 failed: error %2").arg(stage).arg(errorCode);
    });
    connect(session, &Session::sessionFinished, this, &BenchmarkRunner::onSessionFinished);
    connect(session, &Session::readyForDeletion, this, &BenchmarkRunner::onReadyForDeletion);
    m_Session = session;

    setMessage(tr("Run %1 of %2: %3").arg(number).arg(runEstimate()).arg(label));
    emit sessionReady(session, label);
}

void BenchmarkRunner::onSessionFinished()
{
    // A run quit before its planned end, without an error, is the user leaving the benchmark
    if (m_Session && !m_Session->benchmarkCompleted() && m_RunConnected && m_RunError.isEmpty()) {
        m_CancelRequested = true;
    }
}

void BenchmarkRunner::abortRun(const QString& error)
{
    if (!m_Session) {
        return;
    }
    m_RunError = error.isEmpty() ? tr("The stream could not start.") : error;
    onReadyForDeletion();
}

void BenchmarkRunner::onReadyForDeletion()
{
    Session* session = m_Session;
    if (session == nullptr) {
        return;
    }
    m_Session = nullptr;

    RunResult result = measure(session);
    session->deleteLater();

    // A run the user cut short measured nothing worth keeping
    if (!(m_CancelRequested && result.status == Status::Failed)) {
        m_Report.runs.push_back(result);
    }
    m_Report.baselineDrift = computeBaselineDrift(m_Report.runs);
    refreshReportView();

    // Give the window a moment to come back before the next stream takes the screen
    QTimer::singleShot(1000, this, &BenchmarkRunner::advance);
}

RunResult BenchmarkRunner::measure(Session* session) const
{
    RunResult r;
    r.role = m_RunRole;
    r.requested = m_RunRequested;
    r.effective = m_RunConnected ? session->effectiveBenchmarkConfig() : m_RunRequested;

    const auto& snapshots = session->benchmarkSnapshots();
    if (!m_RunConnected || !session->benchmarkCompleted() || snapshots.size() < 2) {
        r.status = Status::Failed;
        r.note = (!m_RunError.isEmpty() ? m_RunError
                  : !m_RunConnected ? tr("The stream did not start.")
                  : snapshots.size() < 2 ? tr("No frames arrived to measure.")
                  : tr("The run was cut short.")).toStdString();
        return r;
    }

    const double fps = session->streamFps();
    const RawStats run = snapshots.back() - snapshots.front();
    r.metrics = metricsFromStats(run, fps);
    r.scores = computeScores(r.metrics);
    if (r.metrics.framesRendered == 0) {
        r.status = Status::Failed;
        r.note = tr("No frames were shown.").toStdString();
        return r;
    }

    std::vector<double> latencies, smoothness;
    for (size_t i = 1; i < snapshots.size(); i++) {
        const Scores window = computeScores(metricsFromStats(snapshots[i] - snapshots[i - 1], fps));
        latencies.push_back(window.latency);
        smoothness.push_back(window.smoothness);
    }
    r.spreadLatency = roundScore(standardDeviation(latencies));
    r.spreadSmoothness = roundScore(standardDeviation(smoothness));

    // The reference streams whatever the user streams. Anything else must have run as asked.
    if (r.role == Role::Candidate || r.role == Role::Confirm) {
        if (r.effective.codec != r.requested.codec || r.effective.pacing != r.requested.pacing) {
            r.status = Status::Fallback;
            QString note = tr("Ran as %1 instead.").arg(describeConfig(r.effective));
            const QStringList warnings = session->property("launchWarnings").toStringList();
            if (!warnings.isEmpty()) {
                note += " " + warnings.join(" ");
            }
            r.note = note.toStdString();
        }
    }
    if (r.effective.bitrateKbps != r.requested.bitrateKbps && r.note.empty()) {
        r.note = tr("The bitrate was capped at %1 Mbps.").arg(r.effective.bitrateKbps / 1000.0).toStdString();
    }
    return r;
}

void BenchmarkRunner::finish()
{
    if (!m_Running) {
        return;
    }
    m_Running = false;
    m_Report.cancelled = m_CancelRequested;
    m_Report.baselineDrift = computeBaselineDrift(m_Report.runs);

    const bool measuredSomething = std::any_of(m_Report.runs.begin(), m_Report.runs.end(), [](const RunResult& r) {
        return r.status == Status::Ok;
    });
    if (measuredSomething) {
        QString error;
        if (!History(History::defaultDirectory()).save(m_Report, &error)) {
            setMessage(tr("The report could not be saved: %1").arg(error));
        }
        else if (!m_CancelRequested) {
            setMessage(tr("Benchmark finished."));
        }
        else {
            setMessage(tr("Benchmark stopped. What was measured is kept."));
        }
    }
    refreshReportView();

    // Leave the host as it was: the scene off and its virtual display gone
    if (m_HostAppRunning) {
        quitHostApp([]() {});
    }
    emit finished();
}

void BenchmarkRunner::setMessage(const QString& message)
{
    m_Message = message;
    emit changed();
}

void BenchmarkRunner::refreshReportView()
{
    const Report& report = m_Report;
    QVariantMap view;
    view["id"] = report.id;
    view["createdAt"] = report.createdAt.toLocalTime().toString(QLocale().dateTimeFormat(QLocale::ShortFormat));
    view["mode"] = toString(report.mode);
    view["goal"] = toString(report.goal);
    view["hostName"] = report.hostName;
    view["platform"] = report.platform;
    view["device"] = report.device;
    view["stream"] = QString("%1x%2 %3 FPS").arg(report.width).arg(report.height).arg(report.fps);
    view["cancelled"] = report.cancelled;
    view["baselineDrift"] = report.baselineDrift;
    view["unstable"] = report.unstable();
    // Applying needs this platform's pacing names
    view["applicable"] = report.platform != "android";

    const RunResult* baseline = nullptr;
    for (const auto& r : report.runs) {
        if (r.role == Role::Baseline && r.status == Status::Ok) {
            baseline = &r;
            break;
        }
    }

    QVariantList runs;
    int bestIndex = -1;
    for (size_t i = 0; i < report.runs.size(); i++) {
        const RunResult& r = report.runs[i];
        QVariantMap run;
        run["index"] = static_cast<int>(i);
        run["role"] = toString(r.role);
        run["roleLabel"] = roleLabel(r.role);
        run["status"] = toString(r.status);
        run["note"] = QString::fromStdString(r.note);
        run["label"] = describeConfig(r.requested);
        run["effectiveLabel"] = describeConfig(r.effective);
        run["latency"] = r.scores.latency;
        run["smoothness"] = r.scores.smoothness;
        run["balanced"] = r.scores.balanced;
        run["goalScore"] = r.scores.forGoal(report.goal);
        run["spreadLatency"] = r.spreadLatency;
        run["spreadSmoothness"] = r.spreadSmoothness;
        run["metrics"] = metricsToMap(r.metrics);
        const bool comparable = baseline != nullptr && r.status == Status::Ok && &r != baseline;
        run["comparable"] = comparable;
        run["deltaLatency"] = comparable ? r.scores.latency - baseline->scores.latency : 0.0;
        run["deltaSmoothness"] = comparable ? r.scores.smoothness - baseline->scores.smoothness : 0.0;
        run["deltaTotalMs"] = comparable ? r.metrics.totalMs - baseline->metrics.totalMs : 0.0;
        runs.append(run);

        if (r.status == Status::Ok && r.role != Role::BaselineRepeat &&
                (bestIndex < 0 || r.scores.forGoal(report.goal) > report.runs[static_cast<size_t>(bestIndex)].scores.forGoal(report.goal))) {
            bestIndex = static_cast<int>(i);
        }
    }

    // Ranked by the report's goal, failed and skipped runs last
    QVariantList ranked = runs;
    std::stable_sort(ranked.begin(), ranked.end(), [](const QVariant& a, const QVariant& b) {
        const QVariantMap ra = a.toMap(), rb = b.toMap();
        const bool okA = ra["status"].toString() == "ok", okB = rb["status"].toString() == "ok";
        if (okA != okB) {
            return okA;
        }
        return ra["goalScore"].toDouble() > rb["goalScore"].toDouble();
    });
    view["runs"] = runs;
    view["ranked"] = ranked;
    view["bestIndex"] = bestIndex;
    m_ReportView = view;
    emit changed();
}

QVariantList BenchmarkRunner::history(const QString& hostUuid)
{
    QVariantList list;
    for (const Report& report : History(History::defaultDirectory()).load(hostUuid)) {
        const RunResult* best = nullptr;
        for (const auto& r : report.runs) {
            if (r.status == Status::Ok && r.role != Role::BaselineRepeat &&
                    (best == nullptr || r.scores.forGoal(report.goal) > best->scores.forGoal(report.goal))) {
                best = &r;
            }
        }
        list.append(QVariantMap {
            { "id", report.id },
            { "createdAt", report.createdAt.toLocalTime().toString(QLocale().dateTimeFormat(QLocale::ShortFormat)) },
            { "mode", toString(report.mode) },
            { "goal", toString(report.goal) },
            { "platform", report.platform },
            { "runs", static_cast<int>(report.runs.size()) },
            { "best", best ? describeConfig(best->requested) : QString() },
            { "unstable", report.unstable() },
            { "cancelled", report.cancelled },
        });
    }
    return list;
}

bool BenchmarkRunner::openReport(const QString& id)
{
    if (m_Running) {
        return false;
    }
    for (const Report& report : History(History::defaultDirectory()).load()) {
        if (report.id == id) {
            m_Report = report;
            refreshReportView();
            return true;
        }
    }
    return false;
}

bool BenchmarkRunner::deleteReport(const QString& id)
{
    return History(History::defaultDirectory()).remove(id);
}

bool BenchmarkRunner::importReport(const QUrl& url)
{
    QFile file(url.toLocalFile());
    Report report;
    if (m_Running || !file.open(QIODevice::ReadOnly) || !fromJson(QJsonDocument::fromJson(file.readAll()).object(), report)) {
        setMessage(tr("This file is not a LavArtemis benchmark report."));
        return false;
    }
    History(History::defaultDirectory()).save(report);
    m_Report = report;
    refreshReportView();
    return true;
}

bool BenchmarkRunner::applyConfig(const RunConfig& config)
{
    StreamingPreferences* prefs = StreamingPreferences::get();
    configure(prefs, config);
    prefs->save();
    return true;
}

bool BenchmarkRunner::applyRun(int runIndex)
{
    if (runIndex < 0 || runIndex >= static_cast<int>(m_Report.runs.size()) || m_Report.platform == "android") {
        return false;
    }
    const RunResult& r = m_Report.runs[static_cast<size_t>(runIndex)];
    applyConfig(r.status == Status::Ok ? r.effective : r.requested);
    setMessage(tr("Your settings now use %1.").arg(describeConfig(r.effective)));
    return true;
}

bool BenchmarkRunner::applyRunAsProfile(int runIndex, const QString& profileName)
{
    if (runIndex < 0 || runIndex >= static_cast<int>(m_Report.runs.size()) || m_Report.platform == "android") {
        return false;
    }
    // An empty profile inherits the current settings; switching to it makes it the one written
    ProfileManager* profiles = ProfileManager::instance();
    profiles->setActiveProfile(profiles->createProfile(profileName));
    return applyRun(runIndex);
}
