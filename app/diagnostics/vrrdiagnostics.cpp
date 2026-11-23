#include "vrrdiagnostics.h"
#include "diagnosticcapture.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QMetaObject>
#include <QThread>
#include <QUrl>

#include <memory>

VrrDiagnostics* VrrDiagnostics::instance()
{
    static VrrDiagnostics* s_Instance = [] {
        auto diagnostics = new VrrDiagnostics();
        // The session may be first to ask, from its own thread: the object belongs to the GUI
        diagnostics->moveToThread(QCoreApplication::instance()->thread());
        return diagnostics;
    }();
    return s_Instance;
}

void VrrDiagnostics::setStatus(const QString& message)
{
    // The session that records reports from its own thread
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, message] { setStatus(message); }, Qt::QueuedConnection);
        return;
    }
    m_Status = message;
    emit changed();
}

void VrrDiagnostics::openFolder()
{
    const auto root = DiagnosticCapture::rootDirectory();
    if (root.isEmpty() || !QDir().mkpath(root) || !QDesktopServices::openUrl(QUrl::fromLocalFile(root)))
        setStatus(tr("Unable to open the diagnostics folder: %1").arg(root));
    else
        setStatus(tr("Diagnostics folder: %1").arg(QDir::toNativeSeparators(root)));
}

void VrrDiagnostics::exportLatest()
{
    if (m_Exporting) return;
    m_Exporting = true;
    setStatus(tr("Exporting the latest diagnostic recording..."));
    const auto root = DiagnosticCapture::rootDirectory();
    // Trace ZIPs can be large. Keep export I/O out of both the GUI and pacing.
    struct ExportResult { QString destination, error; };
    const auto result = std::make_shared<ExportResult>();
    auto thread = QThread::create([root, result] {
        result->destination = DiagnosticCapture::exportLatest(root, result->error);
    });
    connect(thread, &QThread::finished, this, [this, root, result] {
        m_Exporting = false;
        setStatus(result->destination.isEmpty() ? result->error :
            tr("Exported: %1").arg(QDir::toNativeSeparators(result->destination)));
        if (!result->destination.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(root));
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    // Finish the atomic export on normal application exit. The worker never
    // dereferences this object; Qt discards the UI callback if it is gone.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, thread, [thread] { thread->wait(); });
    thread->start();
}
