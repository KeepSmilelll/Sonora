#include "audio/AudioRingBuffer.h"

#include <QtTest/QtTest>
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

using namespace virela;

class TestRingBuffer : public QObject {
    Q_OBJECT

private slots:
    void basicWriteReadWrapAround() {
        AudioRingBuffer rb(1024, 2);
        QCOMPARE(rb.capacityFrames(), size_t(1024));
        QCOMPARE(rb.availableRead(), size_t(0));
        QCOMPARE(rb.availableWrite(), size_t(1024));

        std::vector<float> src(700 * 2);
        for (size_t i = 0; i < src.size(); ++i) {
            src[i] = static_cast<float>(i) * 0.001f;
        }
        QCOMPARE(rb.write(src.data(), 700), size_t(700));
        QCOMPARE(rb.availableRead(), size_t(700));

        std::vector<float> dst(500 * 2, 0.0f);
        QCOMPARE(rb.read(dst.data(), 500), size_t(500));
        for (size_t i = 0; i < 500 * 2; ++i) {
            QCOMPARE(dst[i], src[i]);
        }

        // Trigger wrap-around across the 1024 boundary
        std::vector<float> src2(600 * 2);
        for (size_t i = 0; i < src2.size(); ++i) {
            src2[i] = static_cast<float>(10000 + i);
        }
        QCOMPARE(rb.write(src2.data(), 600), size_t(600));
        QCOMPARE(rb.availableRead(), size_t(800));

        std::vector<float> dst2(800 * 2, 0.0f);
        QCOMPARE(rb.read(dst2.data(), 800), size_t(800));
        for (size_t i = 0; i < 200 * 2; ++i) {
            QCOMPARE(dst2[i], src[500 * 2 + i]);
        }
        for (size_t i = 0; i < 600 * 2; ++i) {
            QCOMPARE(dst2[200 * 2 + i], src2[i]);
        }
        QCOMPARE(rb.availableRead(), size_t(0));
    }

    void concurrentSpscIntegrity() {
        AudioRingBuffer rb(4096, 2);
        constexpr size_t kTotalFrames = 200000;
        std::atomic<bool> failed{false};

        std::thread producer([&]() {
            std::vector<float> block(256 * 2);
            size_t produced = 0;
            while (produced < kTotalFrames) {
                const size_t n = std::min<size_t>(256, kTotalFrames - produced);
                for (size_t f = 0; f < n; ++f) {
                    block[f * 2 + 0] = static_cast<float>(produced + f);
                    block[f * 2 + 1] = -static_cast<float>(produced + f);
                }
                size_t off = 0;
                while (off < n) {
                    const size_t w = rb.write(block.data() + off * 2, n - off);
                    if (w == 0) std::this_thread::yield();
                    off += w;
                }
                produced += n;
            }
        });

        std::thread consumer([&]() {
            std::vector<float> block(193 * 2);
            size_t consumed = 0;
            while (consumed < kTotalFrames) {
                const size_t n = std::min<size_t>(193, kTotalFrames - consumed);
                const size_t r = rb.read(block.data(), n);
                if (r == 0) {
                    std::this_thread::yield();
                    continue;
                }
                for (size_t f = 0; f < r; ++f) {
                    const float expected = static_cast<float>(consumed + f);
                    if (block[f * 2 + 0] != expected || block[f * 2 + 1] != -expected) {
                        failed.store(true);
                        return;
                    }
                }
                consumed += r;
            }
        });

        producer.join();
        consumer.join();
        QVERIFY(!failed.load());
        QCOMPARE(rb.totalWrittenFrames(), kTotalFrames);
        QCOMPARE(rb.totalReadFrames(), kTotalFrames);
    }

    void markerQueueOrdering() {
        LockFreeMarkerQueue<8> q;
        for (int i = 0; i < 8; ++i) {
            StreamBoundaryMarker m{};
            m.absoluteFrame = static_cast<uint64_t>(i * 1000);
            m.queueIndex = i;
            QVERIFY(q.push(m));
        }
        StreamBoundaryMarker overflow{};
        QVERIFY(!q.push(overflow));

        for (int i = 0; i < 8; ++i) {
            StreamBoundaryMarker peeked{}, popped{};
            QVERIFY(q.peek(peeked));
            QCOMPARE(peeked.queueIndex, i);
            QVERIFY(q.pop(popped));
            QCOMPARE(popped.absoluteFrame, static_cast<uint64_t>(i * 1000));
        }
        StreamBoundaryMarker empty{};
        QVERIFY(!q.pop(empty));
    }
};

QTEST_MAIN(TestRingBuffer)
#include "test_ring_buffer.moc"
