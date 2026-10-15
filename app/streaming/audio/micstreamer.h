#pragma once

#include <QString>
#include <QStringList>

#include <atomic>
#include <vector>

#include "SDL_compat.h"

struct OpusEncoder;

/**
 * @brief Captures this PC's microphone and sends it to the host while streaming.
 *
 * LavApollo plays it into a virtual microphone (Steam Streaming Microphone by default), so voice
 * chat and games on the host hear the player. 20 ms Opus frames travel inside the encrypted
 * control stream (LiSendMicrophoneData), never on a port of their own.
 *
 * Capture, encoding and sending all happen on SDL's audio callback thread: encoding a 20 ms mono
 * frame takes a fraction of a millisecond, and LiSendMicrophoneData() must only ever be called
 * from one thread.
 */
class MicStreamer
{
public:
    MicStreamer() = default;
    ~MicStreamer();

    MicStreamer(const MicStreamer&) = delete;
    MicStreamer& operator=(const MicStreamer&) = delete;

    // Recording devices SDL can open. Usable while not streaming.
    static QStringList availableDevices();

    // Opens the device (empty name = system default) and starts sending. Must be called after
    // LiStartConnection() succeeded.
    bool start(const QString& deviceName);

    // Stops sending and closes the device. Must be called before LiStopConnection().
    void stop();

    // Muting keeps the device open, so unmuting is instant; nothing is sent meanwhile.
    void setMuted(bool muted);
    bool isMuted() const { return m_Muted; }
    bool isActive() const { return m_Device != 0; }

private:
    static void SDLCALL captureCallback(void* userdata, Uint8* stream, int len);
    void onSamples(const int16_t* samples, int count);

    SDL_AudioDeviceID m_Device = 0;
    bool m_AudioSubsystemInit = false;
    OpusEncoder* m_Encoder = nullptr;
    std::vector<int16_t> m_Pending;
    std::atomic<bool> m_Muted { false };
    std::atomic<bool> m_SendFailureLogged { false };
};
