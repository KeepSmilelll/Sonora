#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace virela {

// Lock-free Single-Producer Single-Consumer (SPSC) audio ring buffer for interleaved float PCM.
// Designed for zero-allocation, zero-lock real-time audio callbacks.
class AudioRingBuffer {
public:
    // capacityFrames is rounded up to the next power of two.
    explicit AudioRingBuffer(size_t capacityFrames = 131072, uint32_t channels = 2)
        : channels_(channels) {
        resize(capacityFrames, channels);
    }

    void resize(size_t minFrames, uint32_t channels) {
        size_t cap = 1024;
        while (cap < minFrames) {
            cap <<= 1;
        }
        capacityFrames_ = cap;
        mask_ = cap - 1;
        channels_ = std::max<uint32_t>(1, channels);
        buffer_.assign(capacityFrames_ * channels_, 0.0f);
        writePos_.store(0, std::memory_order_relaxed);
        readPos_.store(0, std::memory_order_relaxed);
    }

    size_t capacityFrames() const noexcept { return capacityFrames_; }
    uint32_t channels() const noexcept { return channels_; }

    size_t availableRead() const noexcept {
        const size_t w = writePos_.load(std::memory_order_acquire);
        const size_t r = readPos_.load(std::memory_order_relaxed);
        return w - r;
    }

    size_t availableWrite() const noexcept {
        const size_t w = writePos_.load(std::memory_order_relaxed);
        const size_t r = readPos_.load(std::memory_order_acquire);
        return capacityFrames_ - (w - r);
    }

    // Producer-only: writes up to frameCount interleaved frames.
    // Returns number of frames actually written.
    size_t write(const float* src, size_t frameCount) noexcept {
        const size_t w = writePos_.load(std::memory_order_relaxed);
        const size_t r = readPos_.load(std::memory_order_acquire);
        const size_t freeFrames = capacityFrames_ - (w - r);
        const size_t toWrite = std::min(frameCount, freeFrames);
        if (toWrite == 0) {
            return 0;
        }

        const size_t startIdx = w & mask_;
        const size_t firstPart = std::min(toWrite, capacityFrames_ - startIdx);
        const size_t secondPart = toWrite - firstPart;

        std::memcpy(&buffer_[startIdx * channels_], src, firstPart * channels_ * sizeof(float));
        if (secondPart > 0) {
            std::memcpy(&buffer_[0], src + firstPart * channels_, secondPart * channels_ * sizeof(float));
        }

        writePos_.store(w + toWrite, std::memory_order_release);
        return toWrite;
    }

    // Consumer-only: reads up to frameCount interleaved frames.
    // Returns number of frames actually read.
    size_t read(float* dst, size_t frameCount) noexcept {
        const size_t r = readPos_.load(std::memory_order_relaxed);
        const size_t w = writePos_.load(std::memory_order_acquire);
        const size_t avail = w - r;
        const size_t toRead = std::min(frameCount, avail);
        if (toRead == 0) {
            return 0;
        }

        const size_t startIdx = r & mask_;
        const size_t firstPart = std::min(toRead, capacityFrames_ - startIdx);
        const size_t secondPart = toRead - firstPart;

        std::memcpy(dst, &buffer_[startIdx * channels_], firstPart * channels_ * sizeof(float));
        if (secondPart > 0) {
            std::memcpy(dst + firstPart * channels_, &buffer_[0], secondPart * channels_ * sizeof(float));
        }

        readPos_.store(r + toRead, std::memory_order_release);
        return toRead;
    }

    // Consumer-only or while stopped: discards all buffered frames.
    void clearFromConsumer() noexcept {
        const size_t w = writePos_.load(std::memory_order_acquire);
        readPos_.store(w, std::memory_order_release);
    }

    // Safe only when audio callback is paused/stopped or coordinated via reset flag.
    void reset() noexcept {
        writePos_.store(0, std::memory_order_relaxed);
        readPos_.store(0, std::memory_order_relaxed);
    }

    size_t totalWrittenFrames() const noexcept {
        return writePos_.load(std::memory_order_acquire);
    }

    size_t totalReadFrames() const noexcept {
        return readPos_.load(std::memory_order_acquire);
    }

private:
    std::vector<float> buffer_;
    size_t capacityFrames_ = 0;
    size_t mask_ = 0;
    uint32_t channels_ = 2;

    alignas(64) std::atomic<size_t> writePos_{0};
    alignas(64) std::atomic<size_t> readPos_{0};
};

// Lock-free SPSC queue for exact sample-accurate track transition markers.
struct StreamBoundaryMarker {
    uint64_t absoluteFrame = 0; // RingBuffer read frame index at which new track begins
    int queueIndex = -1;
    int64_t trackId = -1;
    int sampleRate = 48000;
    int channels = 2;
    double rgLinearGain = 1.0;
    double rgPeak = 1.0;
    int64_t trackStartOffsetMs = 0;
    bool isEndOfStream = false;
    bool needsDeviceReconfigure = false;
};

template <size_t Capacity = 16>
class LockFreeMarkerQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");

public:
    bool push(const StreamBoundaryMarker& marker) noexcept {
        const size_t w = write_.load(std::memory_order_relaxed);
        const size_t r = read_.load(std::memory_order_acquire);
        if (w - r >= Capacity) {
            return false;
        }
        items_[w & (Capacity - 1)] = marker;
        write_.store(w + 1, std::memory_order_release);
        return true;
    }

    bool peek(StreamBoundaryMarker& out) const noexcept {
        const size_t r = read_.load(std::memory_order_relaxed);
        const size_t w = write_.load(std::memory_order_acquire);
        if (r == w) {
            return false;
        }
        out = items_[r & (Capacity - 1)];
        return true;
    }

    bool pop(StreamBoundaryMarker& out) noexcept {
        const size_t r = read_.load(std::memory_order_relaxed);
        const size_t w = write_.load(std::memory_order_acquire);
        if (r == w) {
            return false;
        }
        out = items_[r & (Capacity - 1)];
        read_.store(r + 1, std::memory_order_release);
        return true;
    }

    void clearFromConsumer() noexcept {
        const size_t w = write_.load(std::memory_order_acquire);
        read_.store(w, std::memory_order_release);
    }

    void reset() noexcept {
        write_.store(0, std::memory_order_relaxed);
        read_.store(0, std::memory_order_relaxed);
    }

private:
    std::array<StreamBoundaryMarker, Capacity> items_{};
    alignas(64) std::atomic<size_t> write_{0};
    alignas(64) std::atomic<size_t> read_{0};
};

} // namespace virela
