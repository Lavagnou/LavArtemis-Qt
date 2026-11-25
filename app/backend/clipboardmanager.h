#pragma once

#include <QObject>
#include <QClipboard>
#include <QQmlEngine>
#include <QStringList>

#include <functional>

#include "nvhttp.h"

class NvComputer;

/**
 * @brief Clipboard sync between this PC and the host during a stream (Apollo / LavApollo,
 * `/actions/clipboard`).
 *
 * Same moments as the Android client (Game.java): this PC's clipboard is sent when the stream
 * starts, the host's is fetched when leaving with « Déconnecter », and the in-game menu sends or
 * fetches it on demand. On top of that, a copy made on this PC while the stream runs (the user
 * switched to another window) is sent at once. Nothing happens outside a stream: the session
 * connects at its start and disconnects at its end (setConnection() / disconnect()).
 *
 * Requests run on a worker thread, never the GUI thread (which also runs the stream's SDL loop),
 * with a copy of how to reach the host (NvHttpTarget). Each result is reported by showToast():
 * always for a request made from the menu, only with « Notifier chaque synchronisation » for an
 * automatic one.
 */
class ClipboardManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isEnabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(int maxContentSizeMB READ maxContentSizeMB WRITE setMaxContentSizeMB NOTIFY maxContentSizeMBChanged)
    Q_PROPERTY(bool showNotifications READ showNotifications WRITE setShowNotifications NOTIFY showNotificationsChanged)
    QML_ELEMENT

public:
    explicit ClipboardManager(QObject *parent = nullptr);
    ~ClipboardManager();

    // One instance for the settings page and the sessions
    static ClipboardManager* instance();
    static ClipboardManager* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // The settings live in ArtemisSettings (Réglages › PC), read each time: never a stale copy
    bool isEnabled() const;
    bool isConnected() const { return m_connected; }
    int maxContentSizeMB() const;
    bool showNotifications() const;

    void setEnabled(bool enabled);
    void setMaxContentSizeMB(int sizeMB);
    void setShowNotifications(bool show);

    // The session's start and end (main thread). Between the two, the stream runs.
    void setConnection(NvComputer *computer);
    void disconnect();

    // Stream started: sends this PC's clipboard when the sync is on (as Android does)
    void onStreamStarted();

    // The in-game menu: send this PC's clipboard (even what came from the host) / fetch the host's
    Q_INVOKABLE void sendClipboard();
    Q_INVOKABLE void getClipboard();

    // « Déconnecter »: fetches the host's clipboard when the sync is on (as Android does), then
    // calls `then` (on the main thread): the host serves its clipboard only to a client that still
    // streams, so the stream ends afterwards. `then` is called at once when there is nothing to fetch.
    void fetchBeforeDisconnect(std::function<void()> then);

#ifdef LAV_TEST_BUILD
    // Test build (app/testctl `clipboard`): this PC's clipboard is an in-process text, never the
    // system clipboard. Setting it is a local copy (a change the sync reacts to). Main thread.
    static QString testClipboardText();
    static void setTestClipboardText(const QString& text);
#endif

signals:
    // A result to tell the user (Session shows it over the stream)
    void showToast(const QString &message);

    void enabledChanged();
    void connectionChanged();
    void maxContentSizeMBChanged();
    void showNotificationsChanged();

private slots:
    void onClipboardChanged();

private:
    enum class Origin { Automatic, Menu };

    // This PC's clipboard (the fake one in the test build)
    QString localText() const;
    void setLocalText(const QString& text);

    // Text of this PC's clipboard to send, or a null string (empty, too large, or what we just
    // received from the host when !force)
    QString textToSend(bool force) const;

    void upload(const QString& text, Origin origin);
    void download(Origin origin, std::function<void()> then = {});
    void report(Origin origin, const QString& message, bool failure);

    QClipboard *m_clipboard;

    // The stream in progress, if any
    bool m_connected;
    NvHttpTarget m_target;
    quint64 m_generation; // bumped at each connect / disconnect: late results of an old stream are dropped

    // Loop prevention: what we last received from the host (setting it changes our clipboard,
    // which must not be sent straight back)
    QString m_lastReceivedContent;

    static ClipboardManager* s_instance;
};
