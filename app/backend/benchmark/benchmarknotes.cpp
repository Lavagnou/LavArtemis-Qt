#include "benchmarknotes.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace Benchmark {

namespace {

// moonlight-common-c's stage names (LiGetStageName), translated in Session's context
QString stageLabel(const QString& stage)
{
    const QByteArray name = stage.toUtf8();
    return QCoreApplication::translate("Session", name.constData());
}

} // namespace

namespace Notes {

std::string noFramesArrived() { return "No frames arrived to measure."; }
std::string cutShort() { return "The run was cut short."; }
std::string noFramesShown() { return "No frames were shown."; }
std::string streamDidNotStart() { return "The stream did not start."; }
std::string streamCouldNotStart() { return "The stream could not start."; }

std::string ranAs(Codec codec, const std::string& pacing)
{
    return std::string("Ran as ") + toString(codec) + ", " + pacing + " instead.";
}

std::string startFailed(const QString& stage, int errorCode)
{
    return QStringLiteral("Starting %1 failed: error %2.").arg(stage).arg(errorCode).toStdString();
}

std::string connectionEnded(int errorCode)
{
    return QStringLiteral("The connection ended (error %1).").arg(errorCode).toStdString();
}

std::string bitrateCapped(int bitrateKbps)
{
    // C locale: the note is data, read back by both clients
    return QStringLiteral("The bitrate was capped at %1 Mbps.").arg(QString::number(bitrateKbps / 1000.0, 'g', 6)).toStdString();
}

QString localized(const std::string& text)
{
    const QString note = QString::fromStdString(text);
    if (note.isEmpty()) {
        return note;
    }
    if (text == noFramesArrived()) return QCoreApplication::translate("BenchmarkNotes", "No frames arrived to measure.");
    if (text == cutShort()) return QCoreApplication::translate("BenchmarkNotes", "The run was cut short.");
    if (text == noFramesShown()) return QCoreApplication::translate("BenchmarkNotes", "No frames were shown.");
    if (text == streamDidNotStart()) return QCoreApplication::translate("BenchmarkNotes", "The stream did not start.");
    if (text == streamCouldNotStart()) return QCoreApplication::translate("BenchmarkNotes", "The stream could not start.");
    // The planner's (shared with Android, line for line), and Android's older form
    if (note == QLatin1String("VRR is not available on this display") || note == QLatin1String("VRR pacing is not available")) {
        return QCoreApplication::translate("BenchmarkNotes", "VRR is not available on this display");
    }

    // "device" is the spec's form, written by both planners; "client" is what the PC wrote before 2026-11-26
    static const QRegularExpression fellBack(QStringLiteral("^(.+) is not available: the (?:device|client) fell back to (.+)$"));
    static const QRegularExpression ranAsRe(QStringLiteral("^Ran as (.+), (.+) instead\\.$"));
    static const QRegularExpression startFailedRe(QStringLiteral("^Starting (.+) failed: error (-?\\d+)\\.?$"));
    static const QRegularExpression connectionEndedRe(QStringLiteral("^The connection ended \\(error (-?\\d+)\\)\\.$"));
    static const QRegularExpression cappedRe(QStringLiteral("^The bitrate was capped at ([0-9.]+) Mbps\\.$"));

    // Codec ids ("hevc") as shown elsewhere ("HEVC"); an unknown one as is
    auto codecName = [](const QString& id) {
        Codec codec;
        return fromString(id.toStdString(), codec) ? codecLabel(codec) : id;
    };

    QRegularExpressionMatch m = fellBack.match(note);
    if (m.hasMatch()) {
        return QCoreApplication::translate("BenchmarkNotes", "%1 is not available: the device fell back to %2").arg(codecName(m.captured(1)), codecName(m.captured(2)));
    }
    m = ranAsRe.match(note);
    if (m.hasMatch()) {
        return QCoreApplication::translate("BenchmarkNotes", "Ran as %1, %2 instead.").arg(codecName(m.captured(1)), pacingLabel(m.captured(2).toStdString()));
    }
    m = startFailedRe.match(note);
    if (m.hasMatch()) {
        return QCoreApplication::translate("BenchmarkNotes", "Starting %1 failed: error %2.").arg(stageLabel(m.captured(1)), m.captured(2));
    }
    m = connectionEndedRe.match(note);
    if (m.hasMatch()) {
        return QCoreApplication::translate("BenchmarkNotes", "The connection ended (error %1).").arg(m.captured(1));
    }
    m = cappedRe.match(note);
    if (m.hasMatch()) {
        return QCoreApplication::translate("BenchmarkNotes", "The bitrate was capped at %1 Mbps.").arg(m.captured(1));
    }
    return note;
}

} // namespace Notes

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
    if (pacing == "vsync-off") return QCoreApplication::translate("BenchmarkRunner", "V-Sync off");
    if (pacing == "vsync") return QCoreApplication::translate("BenchmarkRunner", "V-Sync");
    if (pacing == "vsync-paced") return QCoreApplication::translate("BenchmarkRunner", "V-Sync + frame pacing");
    if (pacing == "vrr-low-latency") return QCoreApplication::translate("BenchmarkRunner", "VRR, low latency");
    if (pacing == "vrr-balanced") return QCoreApplication::translate("BenchmarkRunner", "VRR, balanced");
    if (pacing == "vrr-smooth") return QCoreApplication::translate("BenchmarkRunner", "VRR, smooth");
    // Android's modes, for reports made there, by Android's names (values/strings.xml, benchmark_pacing_*), lower case
    if (pacing == "default") return QCoreApplication::translate("BenchmarkRunner", "default pacing");
    if (pacing == "ull") return QCoreApplication::translate("BenchmarkRunner", "ultra low latency");
    if (pacing == "paced-ull") return QCoreApplication::translate("BenchmarkRunner", "paced ultra low latency");
    if (pacing == "predictive") return QCoreApplication::translate("BenchmarkRunner", "predictive pacing");
    return QString::fromStdString(pacing);
}

QString modeLabel(Mode mode)
{
    switch (mode) {
    case Mode::Quick: return QCoreApplication::translate("BenchmarkRunner", "Quick");
    case Mode::Full: return QCoreApplication::translate("BenchmarkRunner", "Full");
    case Mode::Adaptive: return QCoreApplication::translate("BenchmarkRunner", "Adaptive");
    }
    return QString();
}

QString goalLabel(Goal goal)
{
    switch (goal) {
    case Goal::Balanced: return QCoreApplication::translate("BenchmarkRunner", "Balanced");
    case Goal::Latency: return QCoreApplication::translate("BenchmarkRunner", "Lowest latency");
    case Goal::Smoothness: return QCoreApplication::translate("BenchmarkRunner", "Smoothest");
    }
    return QString();
}

} // namespace Benchmark
