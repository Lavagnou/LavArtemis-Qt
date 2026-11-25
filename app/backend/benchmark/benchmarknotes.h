#pragma once

#include "benchmarktypes.h"

#include <QString>

#include <string>

namespace Benchmark {

// A run's note, as the report stores it and as it is shown.
//
// Notes are written in English (docs/benchmark.md), the same sentences on both clients, and
// translated when shown: a report opens on either client, in any language. The Android client
// does the same (benchmark/BenchmarkNotes.java), with the same forms. A note of another form
// (a launch error, already in the language it was shown in) is shown as is.
namespace Notes {

std::string noFramesArrived();                       // "No frames arrived to measure."
std::string cutShort();                              // "The run was cut short."
std::string noFramesShown();                         // "No frames were shown."
std::string streamDidNotStart();                     // "The stream did not start."
std::string streamCouldNotStart();                   // "The stream could not start."
std::string ranAs(Codec codec, const std::string& pacing);       // "Ran as hevc, vsync instead."
std::string startFailed(const QString& stage, int errorCode);    // "Starting RTSP handshake failed: error -1."
std::string connectionEnded(int errorCode);                      // "The connection ended (error -1)."
std::string bitrateCapped(int bitrateKbps);                      // "The bitrate was capped at 50 Mbps."

// The note in the app's language (QCoreApplication::translate, context "BenchmarkNotes")
QString localized(const std::string& note);

}

// Display names, in the app's language (context "BenchmarkRunner", as before)
QString codecLabel(Codec codec);
QString pacingLabel(const std::string& pacing);
QString modeLabel(Mode mode);
QString goalLabel(Goal goal);

}
