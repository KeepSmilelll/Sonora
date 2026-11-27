#ifdef _WIN32

#include "platform/win32/Win32SmtcController.h"

#include <QWindow>

#if defined(_MSC_VER)
// Full Windows 10/11 SMTC via Windows SDK WRL/COM interop (MSVC 2022)
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QStandardPaths>
#include <algorithm>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <roapi.h>
#include <systemmediatransportcontrolsinterop.h>
#include <windows.foundation.h>
#include <windows.media.h>
#include <windows.storage.h>
#include <windows.storage.streams.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/wrappers/corewrappers.h>

namespace virela {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HString;
using Microsoft::WRL::Wrappers::HStringReference;

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Storage;
using namespace ABI::Windows::Storage::Streams;

struct Win32SmtcController::Impl {
    ComPtr<ISystemMediaTransportControls> smtc;
    ComPtr<ISystemMediaTransportControls2> smtc2;
    ComPtr<ISystemMediaTransportControlsDisplayUpdater> updater;
    EventRegistrationToken buttonToken{};
    bool hasButtonToken = false;
    QString tempThumbPath;
};

Win32SmtcController::Win32SmtcController(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>()) {}

Win32SmtcController::~Win32SmtcController() {
    if (impl_ && impl_->smtc && impl_->hasButtonToken) {
        impl_->smtc->remove_ButtonPressed(impl_->buttonToken);
    }
    if (impl_ && !impl_->tempThumbPath.isEmpty()) {
        QFile::remove(impl_->tempThumbPath);
    }
}

void Win32SmtcController::init(QWindow* window) {
    if (!window || impl_->smtc) return;

    HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(window->winId()));
    if (!hwnd) return;

    ComPtr<ISystemMediaTransportControlsInterop> interop;
    HRESULT hr = RoGetActivationFactory(
        HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControls).Get(),
        IID_PPV_ARGS(&interop));
    if (FAILED(hr) || !interop) {
        return;
    }

    hr = interop->GetForWindow(hwnd, IID_PPV_ARGS(&impl_->smtc));
    if (FAILED(hr) || !impl_->smtc) {
        return;
    }

    impl_->smtc.As(&impl_->smtc2);
    impl_->smtc->get_DisplayUpdater(&impl_->updater);

    impl_->smtc->put_IsEnabled(true);
    impl_->smtc->put_IsPlayEnabled(true);
    impl_->smtc->put_IsPauseEnabled(true);
    impl_->smtc->put_IsStopEnabled(true);
    impl_->smtc->put_IsNextEnabled(true);
    impl_->smtc->put_IsPreviousEnabled(true);

    auto handler = Callback<ITypedEventHandler<SystemMediaTransportControls*, SystemMediaTransportControlsButtonPressedEventArgs*>>(
        [this](ISystemMediaTransportControls*, ISystemMediaTransportControlsButtonPressedEventArgs* args) -> HRESULT {
            SystemMediaTransportControlsButton btn{};
            if (args && SUCCEEDED(args->get_Button(&btn))) {
                QMetaObject::invokeMethod(this, [this, btn]() {
                    switch (btn) {
                    case SystemMediaTransportControlsButton_Play:     emit playRequested(); break;
                    case SystemMediaTransportControlsButton_Pause:    emit pauseRequested(); break;
                    case SystemMediaTransportControlsButton_Stop:     emit stopRequested(); break;
                    case SystemMediaTransportControlsButton_Next:     emit nextRequested(); break;
                    case SystemMediaTransportControlsButton_Previous: emit previousRequested(); break;
                    default: break;
                    }
                }, Qt::QueuedConnection);
            }
            return S_OK;
        });

    if (SUCCEEDED(impl_->smtc->add_ButtonPressed(handler.Get(), &impl_->buttonToken))) {
        impl_->hasButtonToken = true;
    }
}

void Win32SmtcController::updateTrack(const TrackInfo& track, const QImage& thumbnail) {
    if (!impl_->updater) return;

    impl_->updater->put_Type(MediaPlaybackType_Music);

    ComPtr<IMusicDisplayProperties> musicProps;
    if (SUCCEEDED(impl_->updater->get_MusicProperties(&musicProps)) && musicProps) {
        const std::wstring titleW = track.displayTitle().toStdWString();
        const std::wstring artistW = track.artist.toStdWString();
        const std::wstring albumW = track.album.toStdWString();

        HString hTitle, hArtist, hAlbum;
        hTitle.Set(titleW.c_str(), static_cast<UINT32>(titleW.size()));
        hArtist.Set(artistW.c_str(), static_cast<UINT32>(artistW.size()));
        hAlbum.Set(albumW.c_str(), static_cast<UINT32>(albumW.size()));

        musicProps->put_Title(hTitle.Get());
        musicProps->put_Artist(hArtist.Get());
        musicProps->put_AlbumArtist(hArtist.Get());

        ComPtr<IMusicDisplayProperties2> musicProps2;
        if (SUCCEEDED(musicProps.As(&musicProps2)) && musicProps2) {
            musicProps2->put_AlbumTitle(hAlbum.Get());
        }
    }

    if (!thumbnail.isNull()) {
        if (impl_->tempThumbPath.isEmpty()) {
            const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            impl_->tempThumbPath = QDir(cacheDir).filePath(QStringLiteral("virela_smtc_thumb.jpg"));
        }
        const QImage scaled = thumbnail.scaled(240, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (scaled.save(impl_->tempThumbPath, "JPG", 85)) {
            ComPtr<IRandomAccessStreamReferenceStatics> rasStatics;
            if (SUCCEEDED(RoGetActivationFactory(
                    HStringReference(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference).Get(),
                    IID_PPV_ARGS(&rasStatics))) && rasStatics) {
                ComPtr<IUriRuntimeClassFactory> uriFactory;
                if (SUCCEEDED(RoGetActivationFactory(
                        HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
                        IID_PPV_ARGS(&uriFactory))) && uriFactory) {
                    const QString uriStr = QStringLiteral("file:///") + QDir::fromNativeSeparators(impl_->tempThumbPath);
                    const std::wstring uriW = uriStr.toStdWString();
                    HString hUri;
                    hUri.Set(uriW.c_str(), static_cast<UINT32>(uriW.size()));
                    ComPtr<IUriRuntimeClass> uri;
                    if (SUCCEEDED(uriFactory->CreateUri(hUri.Get(), &uri)) && uri) {
                        ComPtr<IRandomAccessStreamReference> streamRef;
                        if (SUCCEEDED(rasStatics->CreateFromUri(uri.Get(), &streamRef)) && streamRef) {
                            impl_->updater->put_Thumbnail(streamRef.Get());
                        }
                    }
                }
            }
        }
    }

    impl_->updater->Update();
}

void Win32SmtcController::updatePlaybackState(PlaybackState state, int64_t posMs, int64_t durMs) {
    if (!impl_->smtc) return;

    MediaPlaybackStatus status = MediaPlaybackStatus_Closed;
    switch (state) {
    case PlaybackState::Playing:   status = MediaPlaybackStatus_Playing; break;
    case PlaybackState::Paused:    status = MediaPlaybackStatus_Paused; break;
    case PlaybackState::Stopped:   status = MediaPlaybackStatus_Stopped; break;
    case PlaybackState::Buffering: status = MediaPlaybackStatus_Changing; break;
    }
    impl_->smtc->put_PlaybackStatus(status);

    if (impl_->smtc2 && durMs > 0) {
        ComPtr<ISystemMediaTransportControlsTimelineProperties> timeline;
        if (SUCCEEDED(RoActivateInstance(
                HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControlsTimelineProperties).Get(),
                reinterpret_cast<IInspectable**>(timeline.GetAddressOf()))) && timeline) {
            constexpr int64_t kMsTo100Ns = 10000;
            timeline->put_StartTime(TimeSpan{0});
            timeline->put_MinSeekTime(TimeSpan{0});
            timeline->put_EndTime(TimeSpan{durMs * kMsTo100Ns});
            timeline->put_MaxSeekTime(TimeSpan{durMs * kMsTo100Ns});
            timeline->put_Position(TimeSpan{std::clamp<int64_t>(posMs, 0, durMs) * kMsTo100Ns});
            impl_->smtc2->UpdateTimelineProperties(timeline.Get());
        }
    }
}

} // namespace virela

#else
// MinGW / LLVM-MinGW fallback: native Win32 WM_APPCOMMAND hardware media-key listener
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace virela {

struct Win32SmtcController::Impl final : public QAbstractNativeEventFilter {
    Win32SmtcController* owner = nullptr;
    uintptr_t hwnd = 0;
    bool installed = false;
    bool isPlaying = false;

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override {
        if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
            return false;
        }
        MSG* msg = static_cast<MSG*>(message);
        if (!msg || msg->message != WM_APPCOMMAND) {
            return false;
        }
        const short cmd = GET_APPCOMMAND_LPARAM(msg->lParam);
        switch (cmd) {
        case APPCOMMAND_MEDIA_PLAY_PAUSE:
            if (isPlaying) emit owner->pauseRequested();
            else emit owner->playRequested();
            if (result) *result = TRUE;
            return true;
        case APPCOMMAND_MEDIA_PLAY:
            emit owner->playRequested();
            if (result) *result = TRUE;
            return true;
        case APPCOMMAND_MEDIA_PAUSE:
            emit owner->pauseRequested();
            if (result) *result = TRUE;
            return true;
        case APPCOMMAND_MEDIA_STOP:
            emit owner->stopRequested();
            if (result) *result = TRUE;
            return true;
        case APPCOMMAND_MEDIA_NEXTTRACK:
            emit owner->nextRequested();
            if (result) *result = TRUE;
            return true;
        case APPCOMMAND_MEDIA_PREVIOUSTRACK:
            emit owner->previousRequested();
            if (result) *result = TRUE;
            return true;
        default:
            return false;
        }
    }
};

Win32SmtcController::Win32SmtcController(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>()) {
    impl_->owner = this;
}

Win32SmtcController::~Win32SmtcController() {
    if (impl_ && impl_->installed && QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(impl_.get());
        impl_->installed = false;
    }
}

void Win32SmtcController::init(QWindow* window) {
    if (!window || impl_->installed) return;
    impl_->hwnd = static_cast<uintptr_t>(window->winId());
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(impl_.get());
        impl_->installed = true;
    }
}

void Win32SmtcController::updateTrack(const TrackInfo&, const QImage&) {}

void Win32SmtcController::updatePlaybackState(PlaybackState state, int64_t, int64_t) {
    if (impl_) {
        impl_->isPlaying = (state == PlaybackState::Playing);
    }
}

} // namespace virela

#endif // _MSC_VER
#endif // _WIN32
