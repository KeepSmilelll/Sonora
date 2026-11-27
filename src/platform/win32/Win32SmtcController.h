#pragma once

#include <QtGlobal>
#if defined(_WIN32) || defined(Q_OS_WIN)

#include "audio/AudioTypes.h"

#include <QImage>
#include <QObject>
#include <memory>

class QWindow;

namespace virela {

// Windows 10/11 System Media Transport Controls (SMTC) via pure Windows SDK WRL/COM interop
// (no C++/WinRT projection generator required; links only runtimeobject.lib).
class Win32SmtcController final : public QObject {
    Q_OBJECT

public:
    explicit Win32SmtcController(QObject* parent = nullptr);
    ~Win32SmtcController() override;

    void init(QWindow* window);
    void updateTrack(const TrackInfo& track, const QImage& thumbnail);
    void updatePlaybackState(PlaybackState state, int64_t posMs, int64_t durMs);

signals:
    void playRequested();
    void pauseRequested();
    void stopRequested();
    void nextRequested();
    void previousRequested();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace virela

#endif // _WIN32
