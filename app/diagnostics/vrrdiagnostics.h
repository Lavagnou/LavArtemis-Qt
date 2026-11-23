#pragma once

#include <QObject>
#include <QString>

// The VRR trace actions of Réglages › Avancé (export the latest recording, open the folder) and
// the status line they share with the session that records. A QML singleton ("VrrDiagnostics"):
// these used to live on StreamingPreferences, a settings class. Whether a stream records is still
// the traceVrrFrames preference.
class VrrDiagnostics : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool exporting READ exporting NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)

public:
    static VrrDiagnostics* instance();

    bool exporting() const { return m_Exporting; }
    QString status() const { return m_Status; }

    Q_INVOKABLE void openFolder();
    Q_INVOKABLE void exportLatest();
    void setStatus(const QString& message);

signals:
    void changed();

private:
    explicit VrrDiagnostics(QObject* parent = nullptr) : QObject(parent) {}

    bool m_Exporting = false;
    QString m_Status;
};
