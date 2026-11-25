#include "audio/MiniAudioOutput.h"

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_GENERATION
#include "miniaudio.h"

#include <QByteArray>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace sonora {

namespace {

QString formatNameFromMa(ma_format fmt) {
    switch (fmt) {
    case ma_format_f32: return QStringLiteral("32-bit Float");
    case ma_format_s32: return QStringLiteral("32-bit PCM");
    case ma_format_s24: return QStringLiteral("24-bit PCM");
    case ma_format_s16: return QStringLiteral("16-bit PCM");
    case ma_format_u8:  return QStringLiteral("8-bit PCM");
    default:            return QStringLiteral("PCM");
    }
}

QString encodeDeviceId(const ma_device_id& id) {
    return QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(&id), sizeof(ma_device_id)).toHex());
}

bool decodeDeviceId(const QString& hex, ma_device_id& outId) {
    const QByteArray raw = QByteArray::fromHex(hex.toLatin1());
    if (raw.size() != static_cast<qsizetype>(sizeof(ma_device_id))) {
        return false;
    }
    std::memcpy(&outId, raw.constData(), sizeof(ma_device_id));
    return true;
}

// Fast xorshift32 PRNG for lock-free TPDF dithering when narrowing float32 -> s16/s24
struct TpdfDither {
    uint32_t state = 0x243F6A88u;

    float nextTriangular() noexcept {
        auto nextU = [&]() noexcept -> float {
            uint32_t x = state;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            state = x;
            // Map to [-0.5, +0.5]
            return static_cast<float>(static_cast<int32_t>(x)) * (0.5f / 2147483648.0f);
        };
        return nextU() + nextU(); // Triangular distribution in [-1.0, +1.0]
    }
};

void convertFloatToHardware(const float* src, void* dst, ma_format fmt, size_t sampleCount, TpdfDither& dither) noexcept {
    switch (fmt) {
    case ma_format_f32:
        std::memcpy(dst, src, sampleCount * sizeof(float));
        break;
    case ma_format_s32: {
        auto* out = static_cast<int32_t*>(dst);
        for (size_t i = 0; i < sampleCount; ++i) {
            const double clamped = std::clamp<double>(src[i], -1.0, 1.0);
            const double scaled = clamped * 2147483647.0;
            out[i] = static_cast<int32_t>(std::llround(scaled));
        }
        break;
    }
    case ma_format_s24: {
        auto* out = static_cast<uint8_t*>(dst);
        for (size_t i = 0; i < sampleCount; ++i) {
            const float clamped = std::clamp(src[i], -1.0f, 1.0f);
            const float d = dither.nextTriangular() / 8388607.0f;
            const int32_t v = std::clamp<int32_t>(
                static_cast<int32_t>(std::lround((clamped + d) * 8388607.0f)),
                -8388608, 8388607);
            out[i * 3 + 0] = static_cast<uint8_t>(v & 0xFF);
            out[i * 3 + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
            out[i * 3 + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        }
        break;
    }
    case ma_format_s16: {
        auto* out = static_cast<int16_t*>(dst);
        for (size_t i = 0; i < sampleCount; ++i) {
            const float clamped = std::clamp(src[i], -1.0f, 1.0f);
            const float d = dither.nextTriangular() / 32767.0f;
            const int32_t v = std::clamp<int32_t>(
                static_cast<int32_t>(std::lround((clamped + d) * 32767.0f)),
                -32768, 32767);
            out[i] = static_cast<int16_t>(v);
        }
        break;
    }
    default:
        std::memset(dst, 0, sampleCount * ma_get_bytes_per_sample(fmt));
        break;
    }
}

} // namespace

struct MiniAudioOutput::Impl {
    ma_context context{};
    bool contextReady = false;

    ma_device device{};
    bool deviceOpen = false;
    std::atomic<bool> started{false};

    AudioRenderCallback renderCb;
    ActiveOutputStatus status;
    ma_format deviceFormat = ma_format_f32;
    uint32_t deviceChannels = 2;

    // Pre-allocated RT scratch buffer (zero heap allocations in audio callback)
    std::vector<float> rtScratch;
    TpdfDither dither;

    static void dataCallback(ma_device* pDevice, void* pOutput, const void* /*pInput*/, ma_uint32 frameCount) {
        auto* self = static_cast<Impl*>(pDevice->pUserData);
        if (!self || !self->renderCb || frameCount == 0) {
            std::memset(pOutput, 0, frameCount * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));
            return;
        }

        const uint32_t ch = pDevice->playback.channels;
        if (pDevice->playback.format == ma_format_f32) {
            self->renderCb(static_cast<float*>(pOutput), frameCount, ch);
            return;
        }

        // Process in fixed pre-allocated chunks to guarantee zero heap allocation on RT thread
        const size_t chunkFrames = std::max<size_t>(1, self->rtScratch.size() / ch);
        uint8_t* dstBytes = static_cast<uint8_t*>(pOutput);
        const size_t bytesPerFrame = ma_get_bytes_per_frame(pDevice->playback.format, ch);

        size_t remaining = frameCount;
        while (remaining > 0) {
            const uint32_t block = static_cast<uint32_t>(std::min(remaining, chunkFrames));
            self->renderCb(self->rtScratch.data(), block, ch);
            convertFloatToHardware(
                self->rtScratch.data(),
                dstBytes,
                pDevice->playback.format,
                static_cast<size_t>(block) * ch,
                self->dither);
            dstBytes += static_cast<size_t>(block) * bytesPerFrame;
            remaining -= block;
        }
    }

    bool tryInitDevice(const ma_device_id* devId,
                       ma_share_mode shareMode,
                       ma_format fmt,
                       uint32_t sampleRate,
                       uint32_t channels) {
        ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
        cfg.playback.pDeviceID = devId;
        cfg.playback.format = fmt;
        cfg.playback.channels = channels;
        cfg.playback.shareMode = shareMode;
        cfg.sampleRate = sampleRate;
        cfg.dataCallback = &Impl::dataCallback;
        cfg.pUserData = this;
        cfg.performanceProfile = ma_performance_profile_low_latency;

        // WASAPI audiophile flags: bypass OS resampler/offload in Exclusive mode
        if (shareMode == ma_share_mode_exclusive) {
            cfg.wasapi.noAutoConvertSRC = MA_TRUE;
            cfg.wasapi.noDefaultQualitySRC = MA_TRUE;
            cfg.wasapi.noAutoStreamRouting = MA_TRUE;
            cfg.wasapi.noHardwareOffloading = MA_TRUE;
        } else {
            cfg.wasapi.noAutoStreamRouting = MA_FALSE;
        }

        return ma_device_init(&context, &cfg, &device) == MA_SUCCESS;
    }
};

MiniAudioOutput::MiniAudioOutput()
    : impl_(std::make_unique<Impl>()) {
    // Pre-allocate 16384 frames * 8ch scratch buffer (~512 KB, reused forever)
    impl_->rtScratch.resize(16384 * 8, 0.0f);

    ma_context_config ctxCfg = ma_context_config_init();
    if (ma_context_init(nullptr, 0, &ctxCfg, &impl_->context) == MA_SUCCESS) {
        impl_->contextReady = true;
    }
}

MiniAudioOutput::~MiniAudioOutput() {
    close();
    if (impl_ && impl_->contextReady) {
        ma_context_uninit(&impl_->context);
        impl_->contextReady = false;
    }
}

QVector<AudioDeviceInfo> MiniAudioOutput::enumerateDevices() const {
    QVector<AudioDeviceInfo> result;
    if (!impl_->contextReady) {
        return result;
    }

    ma_device_info* pPlaybackInfos = nullptr;
    ma_uint32 playbackCount = 0;
    if (ma_context_get_devices(&impl_->context, &pPlaybackInfos, &playbackCount, nullptr, nullptr) != MA_SUCCESS) {
        return result;
    }

    result.reserve(static_cast<qsizetype>(playbackCount));
    for (ma_uint32 i = 0; i < playbackCount; ++i) {
        AudioDeviceInfo info;
        info.id = encodeDeviceId(pPlaybackInfos[i].id);
        info.name = QString::fromUtf8(pPlaybackInfos[i].name);
        info.isDefault = pPlaybackInfos[i].isDefault == MA_TRUE;
        result.push_back(std::move(info));
    }
    return result;
}

bool MiniAudioOutput::open(const OutputConfig& config,
                           int requestedSampleRate,
                           int requestedChannels,
                           AudioRenderCallback callback,
                           ActiveOutputStatus* statusOut) {
    close();
    if (!impl_->contextReady) {
        return false;
    }

    impl_->renderCb = std::move(callback);
    const uint32_t channels = static_cast<uint32_t>(std::clamp(requestedChannels, 1, 8));
    const uint32_t reqRate = static_cast<uint32_t>(std::max(0, requestedSampleRate));

    ma_device_id chosenId{};
    const ma_device_id* pDevId = nullptr;
    if (!config.deviceId.isEmpty() && decodeDeviceId(config.deviceId, chosenId)) {
        pDevId = &chosenId;
    }

    bool opened = false;
    bool exclusiveAchieved = false;
    QString fallbackMsg;

    if (config.mode == OutputMode::WasapiExclusive) {
        // Negotiate highest precision hardware format supported by the DAC/driver in Exclusive mode
        const std::array<ma_format, 4> candidates = {
            ma_format_f32, ma_format_s32, ma_format_s24, ma_format_s16
        };
        for (ma_format fmt : candidates) {
            if (impl_->tryInitDevice(pDevId, ma_share_mode_exclusive, fmt, reqRate, channels)) {
                opened = true;
                exclusiveAchieved = true;
                break;
            }
        }
        // Retry with device default sample rate in exclusive mode before falling back to shared
        if (!opened && reqRate != 0) {
            for (ma_format fmt : candidates) {
                if (impl_->tryInitDevice(pDevId, ma_share_mode_exclusive, fmt, 0, channels)) {
                    opened = true;
                    exclusiveAchieved = true;
                    break;
                }
            }
        }
        if (!opened) {
            fallbackMsg = QStringLiteral("WASAPI Exclusive unavailable (device busy or format unsupported); fell back to Shared mode.");
        }
    }

    if (!opened) {
        // Shared mode: prefer float32 at requested or system mix rate
        opened = impl_->tryInitDevice(pDevId, ma_share_mode_shared, ma_format_f32, reqRate, channels);
        if (!opened && reqRate != 0) {
            opened = impl_->tryInitDevice(pDevId, ma_share_mode_shared, ma_format_f32, 0, channels);
        }
        if (!opened && pDevId != nullptr) {
            opened = impl_->tryInitDevice(nullptr, ma_share_mode_shared, ma_format_f32, 0, channels);
        }
    }

    if (!opened) {
        impl_->renderCb = nullptr;
        return false;
    }

    impl_->deviceOpen = true;
    impl_->deviceFormat = impl_->device.playback.format;
    impl_->deviceChannels = impl_->device.playback.channels;

    ActiveOutputStatus st;
    st.sampleRate = static_cast<int>(impl_->device.sampleRate);
    st.channels = static_cast<int>(impl_->device.playback.channels);
    st.hardwareFormat = formatNameFromMa(impl_->device.playback.format);
    st.exclusiveActive = exclusiveAchieved;
    st.bitPerfectActive = exclusiveAchieved && config.bitPerfectBypass;
    st.deviceName = QString::fromUtf8(impl_->device.playback.name);
    st.fallbackNotice = fallbackMsg;
    impl_->status = st;

    if (statusOut) {
        *statusOut = st;
    }
    return true;
}

bool MiniAudioOutput::start() {
    if (!impl_->deviceOpen) return false;
    if (impl_->started.load(std::memory_order_acquire)) return true;
    if (ma_device_start(&impl_->device) == MA_SUCCESS) {
        impl_->started.store(true, std::memory_order_release);
        return true;
    }
    return false;
}

void MiniAudioOutput::stop() {
    if (!impl_->deviceOpen) return;
    if (impl_->started.exchange(false, std::memory_order_acq_rel)) {
        ma_device_stop(&impl_->device);
    }
}

void MiniAudioOutput::close() {
    if (!impl_ || !impl_->deviceOpen) return;
    stop();
    ma_device_uninit(&impl_->device);
    impl_->deviceOpen = false;
    impl_->renderCb = nullptr;
}

bool MiniAudioOutput::isOpen() const {
    return impl_ && impl_->deviceOpen;
}

bool MiniAudioOutput::isStarted() const {
    return impl_ && impl_->started.load(std::memory_order_acquire);
}

ActiveOutputStatus MiniAudioOutput::activeStatus() const {
    return impl_ ? impl_->status : ActiveOutputStatus{};
}

} // namespace sonora
