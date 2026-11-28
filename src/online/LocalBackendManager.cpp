#include "online/LocalBackendManager.h"
#include "core/AppSettings.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QUrl>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace virela {

LocalBackendManager::LocalBackendManager(AppSettings* settings, QObject* parent)
    : QObject(parent)
    , settings_(settings) {
    netease_.label = QStringLiteral("Netease");
    meting_.label = QStringLiteral("Meting");

    if (settings_) {
        connect(settings_, &AppSettings::onlineLocalServicesChanged,
                this, &LocalBackendManager::syncWithSettings);
    }
}

LocalBackendManager::~LocalBackendManager() {
    stopAllServices();
}

void LocalBackendManager::setLifecycleActive(bool active) {
    lifecycleActive_ = active;
}

QString LocalBackendManager::findNodeExecutable() {
    QString exe = QStandardPaths::findExecutable(QStringLiteral("node"));
    if (!exe.isEmpty() && QFileInfo::exists(exe)) {
        return exe;
    }

#ifdef Q_OS_WIN
    const QStringList candidates = {
        QDir::fromNativeSeparators(qEnvironmentVariable("ProgramFiles")) + QStringLiteral("/nodejs/node.exe"),
        QDir::fromNativeSeparators(qEnvironmentVariable("ProgramFiles(x86)")) + QStringLiteral("/nodejs/node.exe"),
        QStringLiteral("D:/nodejs/node.exe"),
        QStringLiteral("C:/Program Files/nodejs/node.exe")
    };
    for (const QString& cand : candidates) {
        if (!cand.isEmpty() && QFileInfo::exists(cand)) {
            return cand;
        }
    }
#endif
    return {};
}

QString LocalBackendManager::resolveEntryScript(const QString& dirPath) {
    const QString cleaned = dirPath.trimmed();
    if (cleaned.isEmpty()) return {};
    const QDir dir(cleaned);
    if (!dir.exists()) return {};

    static const QStringList kCandidates = {
        QStringLiteral("server.js"),
        QStringLiteral("server.mjs"),
        QStringLiteral("app.js"),
        QStringLiteral("index.js"),
        QStringLiteral("index.mjs")
    };
    for (const QString& name : kCandidates) {
        if (QFileInfo::exists(dir.filePath(name))) {
            return name;
        }
    }
    return {};
}

bool LocalBackendManager::isLoopbackHost(const QString& host) {
    const QString h = host.trimmed().toLower();
    return h.isEmpty()
        || h == QStringLiteral("127.0.0.1")
        || h == QStringLiteral("localhost")
        || h == QStringLiteral("::1");
}

bool LocalBackendManager::isPortAccepting(const QString& host, quint16 port, int timeoutMs) {
    if (port == 0) return false;
    QTcpSocket probe;
    probe.connectToHost(host.isEmpty() ? QStringLiteral("127.0.0.1") : host, port);
    const bool connected = probe.waitForConnected(timeoutMs);
    if (connected) {
        probe.disconnectFromHost();
    }
    return connected;
}

void LocalBackendManager::ensureJobObject() {
#ifdef Q_OS_WIN
    if (jobHandle_) return;
    HANDLE hJob = CreateJobObjectW(nullptr, nullptr);
    if (!hJob) return;

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limitInfo{};
    limitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &limitInfo, sizeof(limitInfo))) {
        CloseHandle(hJob);
        return;
    }
    jobHandle_ = hJob;
#endif
}

void LocalBackendManager::assignProcessToJob(qint64 pid) {
#ifdef Q_OS_WIN
    if (pid <= 0) return;
    ensureJobObject();
    if (!jobHandle_) return;

    HANDLE hProc = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!hProc) return;
    AssignProcessToJobObject(static_cast<HANDLE>(jobHandle_), hProc);
    CloseHandle(hProc);
#else
    Q_UNUSED(pid);
#endif
}

void LocalBackendManager::closeJobObject() {
#ifdef Q_OS_WIN
    if (jobHandle_) {
        CloseHandle(static_cast<HANDLE>(jobHandle_));
        jobHandle_ = nullptr;
    }
#endif
}

void LocalBackendManager::startConfiguredServices() {
    if (!settings_ || !settings_->onlineAutoStartLocalServices()) {
        emit statusChanged();
        return;
    }

    lastError_.clear();
    const QString netDir = settings_->onlineNeteaseServiceDir().trimmed();
    const QString metDir = settings_->onlineMetingServiceDir().trimmed();
    if (netDir.isEmpty() && metDir.isEmpty()) {
        emit statusChanged();
        return;
    }

    const QString nodeExe = findNodeExecutable();
    if (nodeExe.isEmpty()) {
        lastError_ = tr("Node.js runtime not found in PATH");
        emit statusChanged();
        return;
    }

    cohostedMeting_ = false;
    QVector<std::pair<QString, QString>> netExtraEnv;

    if (!netDir.isEmpty() && !metDir.isEmpty()) {
        const QUrl netUrl = QUrl::fromUserInput(settings_->onlineNeteaseApiUrl().trimmed());
        const QUrl metUrl = QUrl::fromUserInput(settings_->onlineMetingApiUrl().trimmed());
        const bool netLoop = netUrl.host().isEmpty() || isLoopbackHost(netUrl.host());
        const bool metLoop = metUrl.host().isEmpty() || isLoopbackHost(metUrl.host());
        const quint16 netPort = (netUrl.isValid() && netUrl.port() > 0) ? static_cast<quint16>(netUrl.port()) : 3000;
        const quint16 metPort = (metUrl.isValid() && metUrl.port() > 0) ? static_cast<quint16>(metUrl.port()) : 3001;
        const QString netEntry = resolveEntryScript(netDir);
        const QString metEntry = resolveEntryScript(metDir);

        if (netLoop && metLoop
            && !netEntry.isEmpty()
            && metEntry == QStringLiteral("server.mjs")
            && !isPortAccepting(QStringLiteral("127.0.0.1"), netPort, 80)
            && !isPortAccepting(QStringLiteral("127.0.0.1"), metPort, 80)) {
            QFile f(QDir(netDir).filePath(netEntry));
            if (f.open(QIODevice::ReadOnly)) {
                const QByteArray head = f.read(4096);
                if (head.contains("VIRELA_METING_DIR")) {
                    cohostedMeting_ = true;
                    meting_.port = metPort;
                    meting_.externalRunning = false;
                    meting_.lastError.clear();
                    netExtraEnv.append({QStringLiteral("VIRELA_METING_DIR"), metDir});
                    netExtraEnv.append({QStringLiteral("VIRELA_METING_PORT"), QString::number(metPort)});
                }
            }
        }
    }

    if (!netDir.isEmpty()) {
        startSlot(netease_, netDir, settings_->onlineNeteaseApiUrl(), 3000, nodeExe, netExtraEnv);
    }
    if (!metDir.isEmpty() && !cohostedMeting_) {
        startSlot(meting_, metDir, settings_->onlineMetingApiUrl(), 3001, nodeExe);
    }
    emit statusChanged();
}

void LocalBackendManager::startSlot(ServiceSlot& slot,
                                    const QString& serviceDir,
                                    const QString& configuredUrl,
                                    quint16 defaultPort,
                                    const QString& nodeExe,
                                    const QVector<std::pair<QString, QString>>& extraEnv) {
    if (slot.process && slot.process->state() != QProcess::NotRunning) {
        return;
    }

    slot.externalRunning = false;
    slot.lastError.clear();

    const QUrl url = QUrl::fromUserInput(configuredUrl.trimmed());
    if (!configuredUrl.trimmed().isEmpty() && url.isValid() && !url.host().isEmpty() && !isLoopbackHost(url.host())) {
        // Configured URL points to a remote host; do not spawn a local child process.
        return;
    }

    const quint16 port = (url.isValid() && url.port() > 0) ? static_cast<quint16>(url.port()) : defaultPort;
    slot.port = port;

    const QString entryScript = resolveEntryScript(serviceDir);
    if (entryScript.isEmpty()) {
        slot.lastError = tr("%1 entry script missing in %2").arg(slot.label, serviceDir);
        lastError_ = slot.lastError;
        return;
    }

    if (isPortAccepting(QStringLiteral("127.0.0.1"), port, 120)) {
        slot.externalRunning = true;
        return;
    }

    if (slot.process) {
        slot.process->deleteLater();
        slot.process = nullptr;
    }

    auto* proc = new QProcess(this);
    proc->setWorkingDirectory(serviceDir);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("HOST"), QStringLiteral("127.0.0.1"));
    env.insert(QStringLiteral("PORT"), QString::number(port));
    env.insert(QStringLiteral("NODE_ENV"), QStringLiteral("production"));
    env.insert(QStringLiteral("VIRELA_MANAGED"), QStringLiteral("1"));
    if (slot.label == QStringLiteral("Netease") && settings_) {
        const QString q = settings_->onlineAudioQuality().trimmed().toLower();
        const bool wantFlac = (q == QStringLiteral("lossless") || q == QStringLiteral("hires"));
        env.insert(QStringLiteral("ENABLE_FLAC"), wantFlac ? QStringLiteral("true") : QStringLiteral("false"));
    }
    for (const auto& kv : extraEnv) {
        env.insert(kv.first, kv.second);
    }
    proc->setProcessEnvironment(env);

#ifdef Q_OS_WIN
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif

    // Drain stdout continuously so verbose logging never fills OS pipe buffers and stalls Node.
    connect(proc, &QProcess::readyReadStandardOutput, proc, [proc]() {
        (void)proc->readAllStandardOutput();
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc, label = slot.label]() {
        const QString errTxt = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        if (!errTxt.isEmpty()) {
            const QString snippet = errTxt.right(240);
            if (label == QStringLiteral("Netease")) netease_.lastError = snippet;
            else meting_.lastError = snippet;
            lastError_ = snippet;
        }
    });
    connect(proc, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        emit statusChanged();
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc, label = slot.label](QProcess::ProcessError) {
        const QString msg = proc->errorString();
        if (label == QStringLiteral("Netease")) netease_.lastError = msg;
        else meting_.lastError = msg;
        lastError_ = msg;
        emit statusChanged();
    });

    slot.process = proc;
    proc->start(nodeExe, {QStringLiteral("--max-old-space-size=96"), entryScript});
    if (proc->waitForStarted(1000)) {
        assignProcessToJob(proc->processId());
    } else {
        slot.lastError = proc->errorString();
        lastError_ = slot.lastError;
    }
}

void LocalBackendManager::stopSlot(ServiceSlot& slot) {
    slot.externalRunning = false;
    QProcess* proc = slot.process;
    if (!proc) return;
    slot.process = nullptr;

    proc->disconnect(this);
    if (proc->state() != QProcess::NotRunning) {
        proc->closeWriteChannel();
        proc->terminate();
    }

    if (proc->state() != QProcess::NotRunning && !proc->waitForFinished(1200)) {
        proc->kill();
        proc->waitForFinished(1500);
    }
    delete proc;
}

void LocalBackendManager::stopAllServices() {
    cohostedMeting_ = false;
    // Initiate graceful shutdown in parallel for both slots before waiting on any remaining child.
    auto requestGraceful = [this](ServiceSlot& slot) {
        slot.externalRunning = false;
        if (slot.process) {
            slot.process->disconnect(this);
            if (slot.process->state() != QProcess::NotRunning) {
                slot.process->closeWriteChannel();
                slot.process->terminate();
            }
        }
    };
    requestGraceful(netease_);
    requestGraceful(meting_);

    QElapsedTimer timer;
    timer.start();
    auto waitAndKill = [&timer](ServiceSlot& slot) {
        QProcess* proc = slot.process;
        if (!proc) return;
        slot.process = nullptr;
        const int remain = std::max(50, 1500 - static_cast<int>(timer.elapsed()));
        if (proc->state() != QProcess::NotRunning && !proc->waitForFinished(remain)) {
            proc->kill();
            proc->waitForFinished(1500);
        }
        delete proc;
    };
    waitAndKill(netease_);
    waitAndKill(meting_);

    closeJobObject();
    emit statusChanged();
}

void LocalBackendManager::restartServices() {
    stopAllServices();
    if (settings_ && settings_->onlineAutoStartLocalServices()) {
        startConfiguredServices();
    }
}

void LocalBackendManager::syncWithSettings() {
    if (!lifecycleActive_) return;
    if (!settings_ || !settings_->onlineAutoStartLocalServices()) {
        stopAllServices();
        return;
    }
    restartServices();
}

bool LocalBackendManager::neteaseRunning() const {
    return netease_.externalRunning
        || (netease_.process && netease_.process->state() != QProcess::NotRunning);
}

bool LocalBackendManager::metingRunning() const {
    if (cohostedMeting_) {
        return neteaseRunning();
    }
    return meting_.externalRunning
        || (meting_.process && meting_.process->state() != QProcess::NotRunning);
}

QString LocalBackendManager::statusText() const {
    if (!settings_ || !settings_->onlineAutoStartLocalServices()) {
        return tr("Auto-start disabled");
    }
    QStringList parts;
    if (!settings_->onlineNeteaseServiceDir().trimmed().isEmpty()) {
        if (neteaseRunning()) {
            parts.append(netease_.externalRunning
                ? tr("Netease: External (:%1)").arg(netease_.port)
                : tr("Netease: Running (:%1)").arg(netease_.port));
        } else if (!netease_.lastError.isEmpty()) {
            parts.append(tr("Netease: Stopped"));
        } else {
            parts.append(tr("Netease: Offline"));
        }
    }
    if (!settings_->onlineMetingServiceDir().trimmed().isEmpty()) {
        if (metingRunning()) {
            parts.append(meting_.externalRunning
                ? tr("Meting: External (:%1)").arg(meting_.port)
                : tr("Meting: Running (:%1)").arg(meting_.port));
        } else {
            parts.append(tr("Meting: Offline"));
        }
    }
    if (parts.isEmpty()) {
        return tr("No local service folder configured");
    }
    return parts.join(QStringLiteral(" · "));
}

} // namespace virela
