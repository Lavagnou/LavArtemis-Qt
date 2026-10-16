#include "benchmarkreport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace Benchmark {

namespace {

const char* const kSchema = "lavartemis-benchmark";

QJsonObject configToJson(const RunConfig& c)
{
    return QJsonObject {
        { "codec", toString(c.codec) },
        { "bitrateKbps", c.bitrateKbps },
        { "pacing", QString::fromStdString(c.pacing) },
    };
}

bool configFromJson(const QJsonObject& json, RunConfig& c)
{
    c.bitrateKbps = json.value("bitrateKbps").toInt();
    c.pacing = json.value("pacing").toString().toStdString();
    return fromString(json.value("codec").toString().toStdString(), c.codec);
}

QJsonObject metricsToJson(const Metrics& m)
{
    return QJsonObject {
        { "hostMs", m.hostMs },
        { "networkMs", m.networkMs },
        { "receiveMs", m.receiveMs },
        { "decodeMs", m.decodeMs },
        { "queueMs", m.queueMs },
        { "renderMs", m.renderMs },
        { "totalMs", m.totalMs },
        { "rttMs", m.rttMs },
        { "rttVarianceMs", m.rttVarianceMs },
        { "targetFps", m.targetFps },
        { "measuredSeconds", m.measuredSeconds },
        { "framesRendered", static_cast<double>(m.framesRendered) },
        { "framesExpected", static_cast<double>(m.framesExpected) },
        { "networkLossPct", m.networkLossPct },
        { "presentP50Ms", m.presentP50Ms },
        { "presentP95Ms", m.presentP95Ms },
        { "presentP99Ms", m.presentP99Ms },
        { "stutterPct", m.stutterPct },
    };
}

Metrics metricsFromJson(const QJsonObject& j)
{
    Metrics m;
    m.hostMs = j.value("hostMs").toDouble();
    m.networkMs = j.value("networkMs").toDouble();
    m.receiveMs = j.value("receiveMs").toDouble();
    m.decodeMs = j.value("decodeMs").toDouble();
    m.queueMs = j.value("queueMs").toDouble();
    m.renderMs = j.value("renderMs").toDouble();
    m.totalMs = j.value("totalMs").toDouble();
    m.rttMs = j.value("rttMs").toDouble();
    m.rttVarianceMs = j.value("rttVarianceMs").toDouble();
    m.targetFps = j.value("targetFps").toDouble();
    m.measuredSeconds = j.value("measuredSeconds").toDouble();
    m.framesRendered = static_cast<uint64_t>(j.value("framesRendered").toDouble());
    m.framesExpected = static_cast<uint64_t>(j.value("framesExpected").toDouble());
    m.networkLossPct = j.value("networkLossPct").toDouble();
    m.presentP50Ms = j.value("presentP50Ms").toDouble();
    m.presentP95Ms = j.value("presentP95Ms").toDouble();
    m.presentP99Ms = j.value("presentP99Ms").toDouble();
    m.stutterPct = j.value("stutterPct").toDouble();
    return m;
}

} // namespace

double computeBaselineDrift(const std::vector<RunResult>& runs)
{
    const RunResult* first = nullptr;
    const RunResult* last = nullptr;
    for (const auto& r : runs) {
        if (r.status != Status::Ok) {
            continue;
        }
        if (r.role == Role::Baseline && !first) {
            first = &r;
        }
        else if (r.role == Role::BaselineRepeat) {
            last = &r;
        }
    }
    if (!first || !last) {
        return -1;
    }
    return std::round(std::abs(first->scores.balanced - last->scores.balanced) * 10.0) / 10.0;
}

QJsonObject toJson(const Report& report)
{
    QJsonArray runs;
    for (const auto& r : report.runs) {
        runs.append(QJsonObject {
            { "role", toString(r.role) },
            { "status", toString(r.status) },
            { "note", QString::fromStdString(r.note) },
            { "requested", configToJson(r.requested) },
            { "effective", configToJson(r.effective) },
            { "metrics", metricsToJson(r.metrics) },
            { "scores", QJsonObject { { "latency", r.scores.latency }, { "smoothness", r.scores.smoothness }, { "balanced", r.scores.balanced } } },
            { "spread", QJsonObject { { "latency", r.spreadLatency }, { "smoothness", r.spreadSmoothness } } },
        });
    }

    return QJsonObject {
        { "schema", kSchema },
        { "version", Report::kVersion },
        { "id", report.id },
        { "createdAt", report.createdAt.toUTC().toString(Qt::ISODate) },
        { "mode", toString(report.mode) },
        { "goal", toString(report.goal) },
        { "host", QJsonObject { { "uuid", report.hostUuid }, { "name", report.hostName } } },
        { "client", QJsonObject { { "platform", report.platform }, { "device", report.device }, { "appVersion", report.appVersion } } },
        { "stream", QJsonObject { { "width", report.width }, { "height", report.height }, { "fps", report.fps } } },
        { "warmupSeconds", report.warmupSeconds },
        { "measureSeconds", report.measureSeconds },
        { "cancelled", report.cancelled },
        { "baselineDrift", report.baselineDrift },
        { "runs", runs },
    };
}

bool fromJson(const QJsonObject& json, Report& report)
{
    if (json.value("schema").toString() != kSchema || json.value("version").toInt() < 1) {
        return false;
    }

    report = Report();
    report.id = json.value("id").toString();
    report.createdAt = QDateTime::fromString(json.value("createdAt").toString(), Qt::ISODate);
    if (report.id.isEmpty() || !report.createdAt.isValid()) {
        return false;
    }
    fromString(json.value("mode").toString().toStdString(), report.mode);
    fromString(json.value("goal").toString().toStdString(), report.goal);

    const auto host = json.value("host").toObject();
    report.hostUuid = host.value("uuid").toString();
    report.hostName = host.value("name").toString();
    const auto client = json.value("client").toObject();
    report.platform = client.value("platform").toString();
    report.device = client.value("device").toString();
    report.appVersion = client.value("appVersion").toString();
    const auto stream = json.value("stream").toObject();
    report.width = stream.value("width").toInt();
    report.height = stream.value("height").toInt();
    report.fps = stream.value("fps").toDouble();
    report.warmupSeconds = json.value("warmupSeconds").toInt();
    report.measureSeconds = json.value("measureSeconds").toInt();
    report.cancelled = json.value("cancelled").toBool();
    report.baselineDrift = json.value("baselineDrift").toDouble(-1);

    for (const auto& value : json.value("runs").toArray()) {
        const auto j = value.toObject();
        RunResult r;
        // A run this version cannot understand (a codec added later...) is dropped, not the report
        if (!fromString(j.value("role").toString().toStdString(), r.role)
            || !fromString(j.value("status").toString().toStdString(), r.status)
            || !configFromJson(j.value("requested").toObject(), r.requested)) {
            continue;
        }
        if (!configFromJson(j.value("effective").toObject(), r.effective)) {
            r.effective = r.requested;
        }
        r.note = j.value("note").toString().toStdString();
        r.metrics = metricsFromJson(j.value("metrics").toObject());
        const auto scores = j.value("scores").toObject();
        r.scores.latency = scores.value("latency").toDouble();
        r.scores.smoothness = scores.value("smoothness").toDouble();
        r.scores.balanced = scores.value("balanced").toDouble();
        const auto spread = j.value("spread").toObject();
        r.spreadLatency = spread.value("latency").toDouble();
        r.spreadSmoothness = spread.value("smoothness").toDouble();
        report.runs.push_back(r);
    }
    return true;
}

History::History(QString directory)
    : m_Directory(std::move(directory))
{
}

QString History::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/benchmarks";
}

QString History::fileFor(const Report& report) const
{
    // Sortable by name, and unique: two benchmarks never start in the same second with the same id
    return m_Directory + "/" + report.createdAt.toUTC().toString("yyyyMMdd-HHmmss") + "-" + report.id + ".json";
}

bool History::save(const Report& report, QString* error)
{
    if (!QDir().mkpath(m_Directory)) {
        if (error) {
            *error = QString("Cannot create %1").arg(m_Directory);
        }
        return false;
    }

    QSaveFile file(fileFor(report));
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(toJson(report)).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    prune();
    return true;
}

QVector<Report> History::load(const QString& hostUuid) const
{
    QVector<Report> reports;
    const auto files = QDir(m_Directory).entryInfoList({ "*.json" }, QDir::Files, QDir::Name | QDir::Reversed);
    for (const auto& info : files) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        Report report;
        if (!fromJson(QJsonDocument::fromJson(file.readAll()).object(), report)) {
            continue;
        }
        if (hostUuid.isEmpty() || report.hostUuid.compare(hostUuid, Qt::CaseInsensitive) == 0) {
            reports.append(report);
        }
    }
    std::stable_sort(reports.begin(), reports.end(), [](const Report& a, const Report& b) {
        return a.createdAt > b.createdAt;
    });
    return reports;
}

bool History::remove(const QString& id)
{
    const auto files = QDir(m_Directory).entryInfoList({ "*-" + id + ".json" }, QDir::Files);
    bool removed = false;
    for (const auto& info : files) {
        removed |= QFile::remove(info.absoluteFilePath());
    }
    return removed;
}

void History::prune()
{
    const auto files = QDir(m_Directory).entryInfoList({ "*.json" }, QDir::Files, QDir::Name | QDir::Reversed);
    for (int i = kMaxReports; i < files.size(); i++) {
        QFile::remove(files[i].absoluteFilePath());
    }
}

} // namespace Benchmark
