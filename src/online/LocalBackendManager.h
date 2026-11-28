#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

namespace virela {

class AppSettings;

class LocalBackendManager : public QObject {
    Q_OBJECT

public:
    explicit LocalBackendManager(AppSettings* settings, QObject* parent = nullptr);
    ~LocalBackendManager() override;

    void setLifecycleActive(bool active);
    bool lifecycleActive() const { return lifecycleActive_; }

    void startConfiguredServices();
    void stopAllServices();
    void restartServices();
    void syncWithSettings();

    bool neteaseRunning() const;
    bool metingRunning() const;
    QString statusText() const;
    QString lastError() const { return lastError_; }

    static QString findNodeExecutable();
    static QString resolveEntryScript(const QString& dirPath);
    static bool isLoopbackHost(const QString& host);
    static bool isPortAccepting(const QString& host, quint16 port, int timeoutMs = 120);

signals:
    void statusChanged();

private:
    struct ServiceSlot {
        QString label;
        QProcess* process = nullptr;
        bool externalRunning = false;
        quint16 port = 0;
        QString lastError;
    };

    void startSlot(ServiceSlot& slot,
                   const QString& serviceDir,
                   const QString& configuredUrl,
                   quint16 defaultPort,
                   const QString& nodeExe,
                   const QVector<std::pair<QString, QString>>& extraEnv = {});
    void stopSlot(ServiceSlot& slot);
    void ensureJobObject();
    void assignProcessToJob(qint64 pid);
    void closeJobObject();

    AppSettings* settings_ = nullptr;
    bool lifecycleActive_ = false;
    bool cohostedMeting_ = false;
    ServiceSlot netease_;
    ServiceSlot meting_;
    QString lastError_;
#ifdef _WIN32
    void* jobHandle_ = nullptr;
#endif
};

} // namespace virela
