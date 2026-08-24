#pragma once

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QQmlEngine>

#include "SDL_compat.h"

/**
 * @brief Builds SDL gamepad mappings by watching a device being operated.
 *
 * SDL only forwards a device as a game controller when it has an entry for
 * that GUID. MappingManager can guess one from the device's shape, but a
 * guess can put controls in the wrong place; this is how the user corrects
 * it, and how devices too unusual to guess get supported at all.
 *
 * The user picks the control to bind and then operates it, so there is only
 * ever one target. That is deliberate: the previous fixed 21-step walkthrough
 * moved on the instant a control was captured, and since a stick is still
 * deflected 20 ms later, one flick of it satisfied every remaining axis step
 * with the same axis. Nothing here advances on its own except an explicitly
 * requested queue, and every input has to return to rest before it counts
 * again.
 *
 * Works on the raw joystick API on purpose: the whole point is to reach
 * devices the game controller API refuses to expose.
 */
class SdlGamepadMapper : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool mapping READ isMapping NOTIFY mappingChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY mappingChanged)

    // The control waiting to be operated, "" when nothing is being captured.
    Q_PROPERTY(QString targetElement READ targetElement NOTIFY targetChanged)
    Q_PROPERTY(QString targetPrompt READ targetPrompt NOTIFY targetChanged)
    // Targets still queued behind this one. Drives the "n left" readout during
    // a stick pair or a full walkthrough.
    Q_PROPERTY(int queueRemaining READ queueRemaining NOTIFY targetChanged)

    // Element name -> SDL source token, e.g. "leftx" -> "a0~".
    Q_PROPERTY(QVariantMap bindings READ bindings NOTIFY bindingsChanged)
    Q_PROPERTY(int bindingCount READ bindingCount NOTIFY bindingsChanged)

    // Element name -> what that binding reads right now, evaluated the way SDL
    // would. This is what lets a wrong or inverted axis be seen before it is
    // saved rather than discovered in a game.
    Q_PROPERTY(QVariantMap elementValues READ elementValues NOTIFY liveStateChanged)
    // Whatever raw input is active, named. Proof the device is alive even when
    // nothing is bound yet.
    Q_PROPERTY(QString rawActivity READ rawActivity NOTIFY liveStateChanged)

public:
    static SdlGamepadMapper* instance();
    static SdlGamepadMapper* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    ~SdlGamepadMapper();

    // Lists every joystick, recognized or not, as maps with keys:
    // index, name, guid, recognized, guessed, numAxes, numButtons, numHats.
    Q_INVOKABLE QVariantList enumerateDevices();

    // Every bindable control as maps with keys: element, label, kind.
    // kind is "button", "trigger" or "axis".
    Q_INVOKABLE QVariantList elementCatalog() const;

    Q_INVOKABLE void startMapping(int deviceIndex);

    // Wait for the user to operate a control and bind it to this element.
    Q_INVOKABLE void selectElement(const QString& element);
    // Same, for several elements in a row -- a stick is two of them, and the
    // walkthrough is all of them.
    Q_INVOKABLE void selectElements(const QStringList& elements);
    Q_INVOKABLE void mapEverything();

    // Leave the current target unbound and move to the next queued one.
    Q_INVOKABLE void skipTarget();
    // Stop listening entirely, discarding the queue.
    Q_INVOKABLE void cancelSelection();

    Q_INVOKABLE void clearElement(const QString& element);
    Q_INVOKABLE void clearAll();

    // Persists through MappingManager and applies it to SDL immediately.
    Q_INVOKABLE bool saveMapping();

    Q_INVOKABLE void cancel();

    // The mapping built so far, for display while the user works.
    Q_INVOKABLE QString previewMapping() const;

    bool isMapping() const { return m_Joystick != nullptr; }
    QString deviceName() const { return m_DeviceName; }
    QString targetElement() const { return m_TargetElement; }
    QString targetPrompt() const;
    int queueRemaining() const { return m_TargetQueue.count(); }
    QVariantMap bindings() const;
    int bindingCount() const { return m_Bindings.count(); }
    QVariantMap elementValues() const { return m_ElementValues; }
    QString rawActivity() const { return m_RawActivity; }

signals:
    void mappingChanged();
    void targetChanged();
    void bindingsChanged();
    void liveStateChanged();
    void inputCaptured(QString element, QString binding);
    void queueFinished();

private slots:
    void onPollingTimerFired();

private:
    explicit SdlGamepadMapper(QObject* parent = nullptr);

    void closeDevice();
    void beginListening();
    void advanceQueue();
    bool captureFromState(QString& binding);
    QString axisToken(int axis, int deviation, bool signedAxis) const;
    void refreshLiveState();
    void loadExistingMapping();

    static SdlGamepadMapper* s_instance;

    QTimer* m_PollingTimer;
    SDL_Joystick* m_Joystick;
    QString m_DeviceGuid;
    QString m_DeviceName;

    QString m_TargetElement;
    QStringList m_TargetQueue;

    // Element name -> SDL source. Unbound elements are simply absent.
    QMap<QString, QString> m_Bindings;

    // Fields of the device's existing mapping this class doesn't model
    // (paddles, touchpad, half-axis outputs, crc). Kept verbatim and written
    // back on save, so correcting one stick can't quietly delete the rest of a
    // database entry.
    QStringList m_PassthroughFields;

    // Axis rest positions. Switches and triggers can idle at an extreme, so
    // deviation from rest is the only reliable signal that something moved.
    QVector<Sint16> m_AxisRestValues;

    // An input that is already active when listening starts is disarmed, and
    // only counts again once it has returned to rest. Without this a control
    // held from one capture immediately satisfies the next.
    QVector<bool> m_AxisArmed;
    QVector<bool> m_ButtonArmed;
    QVector<bool> m_HatArmed;

    QVariantMap m_ElementValues;
    QString m_RawActivity;
};
