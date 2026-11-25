#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <atomic>
#include <thread>

namespace sonora {

class LibraryDatabase;
class CoverArtProvider;

class LibraryScanner : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(int scannedCount READ scannedCount NOTIFY progressChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY progressChanged)

public:
    LibraryScanner(LibraryDatabase* db, CoverArtProvider* covers, QObject* parent = nullptr);
    ~LibraryScanner() override;

    bool isScanning() const noexcept { return scanning_.load(std::memory_order_acquire); }
    int scannedCount() const noexcept { return scannedCount_.load(std::memory_order_acquire); }
    int totalCount() const noexcept { return totalCount_.load(std::memory_order_acquire); }

    void setWatchedFolders(const QStringList& folders);
    void startScan(const QStringList& folders);
    void cancelScan();

    // Synchronous scan helper for deterministic automated tests
    int scanSynchronously(const QStringList& folders);

signals:
    void scanningChanged();
    void progressChanged();
    void scanFinished(int indexedTracks);

private:
    int runScanWork(const QStringList& folders);

    LibraryDatabase* db_ = nullptr;
    CoverArtProvider* covers_ = nullptr;
    QFileSystemWatcher watcher_;
    QTimer debounceTimer_;
    QStringList watchedFolders_;

    std::thread worker_;
    std::atomic<bool> scanning_{false};
    std::atomic<bool> cancel_{false};
    std::atomic<int> scannedCount_{0};
    std::atomic<int> totalCount_{0};
};

} // namespace sonora
