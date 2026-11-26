#include "clipboardmanager.h"
#include "computermanager.h"
#include "nvcomputer.h"
#include "settings/artemissettings.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QMimeData>
#include <QPointer>
#include <QThreadPool>

// A clipboard request never holds a stream back long: « Déconnecter » waits for the fetch
static constexpr int CLIPBOARD_TIMEOUT_MS = 3000;

ClipboardManager* ClipboardManager::s_instance = nullptr;

ClipboardManager::ClipboardManager(QObject *parent)
    : QObject(parent)
#ifdef LAV_TEST_BUILD
    , m_clipboard(nullptr) // test build: an in-process text, never the system clipboard
#else
    , m_clipboard(QGuiApplication::clipboard())
#endif
    , m_connected(false)
    , m_generation(0)
{
#ifndef LAV_TEST_BUILD
    connect(m_clipboard, &QClipboard::dataChanged, this, &ClipboardManager::onClipboardChanged);
#endif

    // The properties follow the settings, whoever writes them (Réglages, a profile, the test channel)
    auto settings = ArtemisSettings::instance();
    connect(settings, &ArtemisSettings::clipboardSyncEnabledChanged, this, &ClipboardManager::enabledChanged);
    connect(settings, &ArtemisSettings::clipboardSyncMaxSizeChanged, this, &ClipboardManager::maxContentSizeMBChanged);
    connect(settings, &ArtemisSettings::clipboardSyncNotificationsChanged, this, &ClipboardManager::showNotificationsChanged);
}

ClipboardManager::~ClipboardManager()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

ClipboardManager* ClipboardManager::instance()
{
    if (!s_instance) {
        s_instance = new ClipboardManager();
    }
    return s_instance;
}

ClipboardManager* ClipboardManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    ClipboardManager* manager = instance();
    // Shared with the sessions: QML must never delete it
    QQmlEngine::setObjectOwnership(manager, QQmlEngine::CppOwnership);
    return manager;
}

bool ClipboardManager::isEnabled() const
{
    return ArtemisSettings::instance()->clipboardSyncEnabled();
}

int ClipboardManager::maxContentSizeMB() const
{
    return ArtemisSettings::instance()->clipboardSyncMaxSize() / (1024 * 1024);
}

bool ClipboardManager::showNotifications() const
{
    return ArtemisSettings::instance()->clipboardSyncNotifications();
}

void ClipboardManager::setEnabled(bool enabled)
{
    ArtemisSettings::instance()->setClipboardSyncEnabled(enabled);
}

void ClipboardManager::setMaxContentSizeMB(int sizeMB)
{
    ArtemisSettings::instance()->setClipboardSyncMaxSize(sizeMB * 1024 * 1024);
}

void ClipboardManager::setShowNotifications(bool show)
{
    ArtemisSettings::instance()->setClipboardSyncNotifications(show);
}

void ClipboardManager::setConnection(NvComputer *computer)
{
    m_target = NvHttpTarget::of(computer);
    m_generation++;
    if (!m_connected) {
        m_connected = true;
        emit connectionChanged();
    }
}

void ClipboardManager::disconnect()
{
    // Requests still under way finish, but their results belong to a stream that is over
    m_generation++;
    if (m_connected) {
        m_connected = false;
        emit connectionChanged();
    }
}

#ifdef LAV_TEST_BUILD
static QString s_TestClipboardText;

QString ClipboardManager::testClipboardText()
{
    return s_TestClipboardText;
}

void ClipboardManager::setTestClipboardText(const QString& text)
{
    s_TestClipboardText = text;
    instance()->onClipboardChanged();
}
#endif

QString ClipboardManager::localText() const
{
#ifdef LAV_TEST_BUILD
    return s_TestClipboardText.isEmpty() ? QString() : s_TestClipboardText;
#else
    const QMimeData *mimeData = m_clipboard->mimeData();
    if (mimeData == nullptr || !mimeData->hasText()) {
        return QString();
    }
    return mimeData->text();
#endif
}

void ClipboardManager::setLocalText(const QString& text)
{
#ifdef LAV_TEST_BUILD
    s_TestClipboardText = text;
#else
    m_clipboard->setText(text);
#endif
}

QString ClipboardManager::textToSend(bool force) const
{
    QString text = localText();
    if (text.isEmpty()) {
        return QString();
    }
    if (text.toUtf8().size() > ArtemisSettings::instance()->clipboardSyncMaxSize()) {
        qWarning() << "ClipboardManager: clipboard larger than the maximum size, not sent";
        return QString();
    }
    // What we just received from the host is not sent back to it
    if (!force && text == m_lastReceivedContent) {
        return QString();
    }
    return text;
}

void ClipboardManager::onStreamStarted()
{
    if (!m_connected || !isEnabled()) {
        return;
    }
    QString text = textToSend(false);
    if (!text.isNull()) {
        upload(text, Origin::Automatic);
    }
}

void ClipboardManager::onClipboardChanged()
{
    // A copy made on this PC while the stream runs: sent at once. Never outside a stream.
    if (!m_connected || !isEnabled()) {
        return;
    }
    QString text = textToSend(false);
    if (!text.isNull()) {
        upload(text, Origin::Automatic);
    }
}

void ClipboardManager::sendClipboard()
{
    if (!m_connected) {
        return;
    }
    QString text = textToSend(true);
    if (text.isNull()) {
        // As Android: nothing to send, nothing said
        return;
    }
    upload(text, Origin::Menu);
}

void ClipboardManager::getClipboard()
{
    if (!m_connected) {
        return;
    }
    download(Origin::Menu);
}

void ClipboardManager::fetchBeforeDisconnect(std::function<void()> then)
{
    if (!m_connected || !isEnabled()) {
        then();
        return;
    }
    download(Origin::Automatic, std::move(then));
}

void ClipboardManager::report(Origin origin, const QString& message, bool failure)
{
    Q_UNUSED(failure);
    // A request made from the menu always answers; an automatic one only with « Notifier »
    if (origin == Origin::Menu || showNotifications()) {
        emit showToast(message);
    }
}

void ClipboardManager::upload(const QString& text, Origin origin)
{
    const NvHttpTarget target = m_target;
    const quint64 generation = m_generation;
    QPointer<ClipboardManager> self(this);
    QThreadPool::globalInstance()->start([self, target, generation, text, origin]() {
        bool ok = false;
        try {
            NvHTTP http(target);
            ok = http.sendClipboardContent(text);
        } catch (...) {
            ok = false;
        }
        QMetaObject::invokeMethod(qApp, [self, generation, ok, origin]() {
            if (!self || generation != self->m_generation) {
                return;
            }
            if (ok) {
                self->report(origin, tr("Clipboard sent successfully"), false);
            }
            else {
                self->report(origin, tr("Failed to send the clipboard: the PC did not accept it"), true);
            }
        }, Qt::QueuedConnection);
    });
}

void ClipboardManager::download(Origin origin, std::function<void()> then)
{
    const NvHttpTarget target = m_target;
    const quint64 generation = m_generation;
    QPointer<ClipboardManager> self(this);
    QThreadPool::globalInstance()->start([self, target, generation, origin, then]() {
        bool ok = false;
        QString content, error;
        try {
            NvHTTP http(target);
            content = http.openConnectionToString(http.m_BaseUrlHttps, "actions/clipboard", "type=text",
                                                  CLIPBOARD_TIMEOUT_MS, NvHTTP::NVLL_VERBOSE);
            ok = true;
        } catch (const GfeHttpResponseException& e) {
            // « erreur 404 », « connexion refusée »: never the exception's English text
            error = ComputerManager::hostErrorDetail(e);
        } catch (const QtNetworkReplyException& e) {
            error = ComputerManager::hostErrorDetail(e);
        }
        QMetaObject::invokeMethod(qApp, [self, generation, ok, content, error, origin, then]() {
            if (self && generation == self->m_generation) {
                if (ok) {
                    if (!content.isEmpty()) {
                        // Not sent back to the host when our clipboard changes because of it
                        self->m_lastReceivedContent = content;
                        self->setLocalText(content);
                    }
                    self->report(origin, tr("Clipboard fetched successfully"), false);
                }
                else {
                    self->report(origin, tr("Failed to fetch the clipboard: %1").arg(error), true);
                }
            }
            if (then) {
                then();
            }
        }, Qt::QueuedConnection);
    });
}
