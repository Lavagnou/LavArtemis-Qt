#include "micstreamer.h"

#include <Limelight.h>
#include <opus.h>

namespace {
// The host decodes 48 kHz mono, in 20 ms frames.
constexpr int kSampleRate = 48000;
constexpr int kFrameSamples = kSampleRate / 50;

// Plenty for speech, and FEC still fits well under LI_MAX_MICROPHONE_FRAME_LENGTH.
constexpr int kBitrate = 32000;

// Tells the encoder how much redundancy to carry: the host rebuilds a lost frame from the
// FEC in the next one.
constexpr int kExpectedPacketLossPercent = 10;
}

MicStreamer::~MicStreamer()
{
    stop();
}

QStringList MicStreamer::availableDevices()
{
    QStringList devices;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Microphone: SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s",
                    SDL_GetError());
        return devices;
    }

    int count = SDL_GetNumAudioDevices(1);
    for (int i = 0; i < count; i++) {
        const char* name = SDL_GetAudioDeviceName(i, 1);
        if (name != nullptr) {
            devices.append(QString::fromUtf8(name));
        }
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return devices;
}

bool MicStreamer::start(const QString& deviceName)
{
    SDL_assert(m_Device == 0);

    int error = OPUS_OK;
    m_Encoder = opus_encoder_create(kSampleRate, 1, OPUS_APPLICATION_VOIP, &error);
    if (m_Encoder == nullptr || error != OPUS_OK) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Microphone: couldn't create the Opus encoder: %s",
                     opus_strerror(error));
        stop();
        return false;
    }

    opus_encoder_ctl(m_Encoder, OPUS_SET_BITRATE(kBitrate));
    opus_encoder_ctl(m_Encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(m_Encoder, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(m_Encoder, OPUS_SET_PACKET_LOSS_PERC(kExpectedPacketLossPercent));
    // Silence costs a couple of bytes per frame. Those frames are still sent, so the host's
    // sequence numbers stay contiguous and silence never reads as packet loss.
    opus_encoder_ctl(m_Encoder, OPUS_SET_DTX(1));

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Microphone: SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s",
                     SDL_GetError());
        stop();
        return false;
    }
    m_AudioSubsystemInit = true;

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = kSampleRate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = kFrameSamples;
    want.callback = captureCallback;
    want.userdata = this;

    QByteArray name = deviceName.toUtf8();
    m_Device = SDL_OpenAudioDevice(deviceName.isEmpty() ? nullptr : name.constData(), 1, &want, &have, 0);
    if (m_Device == 0 && !deviceName.isEmpty()) {
        // The chosen microphone was unplugged: the default one beats silence
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Microphone: couldn't open '%s' (%s), using the default microphone",
                    name.constData(), SDL_GetError());
        m_Device = SDL_OpenAudioDevice(nullptr, 1, &want, &have, 0);
    }
    if (m_Device == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Microphone: couldn't open a recording device: %s",
                     SDL_GetError());
        stop();
        return false;
    }

    m_Pending.clear();
    m_Pending.reserve(kFrameSamples * 2);
    m_SendFailureLogged = false;

    SDL_PauseAudioDevice(m_Device, 0);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Microphone: sending '%s' to the host (%d kbps Opus)",
                deviceName.isEmpty() ? "default microphone" : name.constData(),
                kBitrate / 1000);
    return true;
}

void MicStreamer::stop()
{
    if (m_Device != 0) {
        // Closing waits for the callback to return, so nothing is sent after this
        SDL_CloseAudioDevice(m_Device);
        m_Device = 0;
    }

    if (m_AudioSubsystemInit) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        m_AudioSubsystemInit = false;
    }

    if (m_Encoder != nullptr) {
        opus_encoder_destroy(m_Encoder);
        m_Encoder = nullptr;
    }
}

void MicStreamer::setMuted(bool muted)
{
    m_Muted = muted;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Microphone %s", muted ? "muted" : "unmuted");
}

void SDLCALL MicStreamer::captureCallback(void* userdata, Uint8* stream, int len)
{
    auto self = static_cast<MicStreamer*>(userdata);
    self->onSamples(reinterpret_cast<const int16_t*>(stream), len / (int)sizeof(int16_t));
}

void MicStreamer::onSamples(const int16_t* samples, int count)
{
    if (m_Muted) {
        // Resume on a fresh frame rather than with audio from before the mute
        m_Pending.clear();
        return;
    }

    // SDL may hand over blocks of any size: cut them into exact 20 ms frames
    m_Pending.insert(m_Pending.end(), samples, samples + count);

    size_t offset = 0;
    while (m_Pending.size() - offset >= (size_t)kFrameSamples) {
        unsigned char packet[LI_MAX_MICROPHONE_FRAME_LENGTH];
        int length = opus_encode(m_Encoder, &m_Pending[offset], kFrameSamples, packet, sizeof(packet));
        offset += kFrameSamples;

        if (length <= 0) {
            continue;
        }

        if (LiSendMicrophoneData(packet, length, kFrameSamples) != 0 && !m_SendFailureLogged) {
            // Only once: this runs 50 times a second
            m_SendFailureLogged = true;
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Microphone: the host isn't taking microphone audio on this connection");
        }
    }

    m_Pending.erase(m_Pending.begin(), m_Pending.begin() + offset);
}
