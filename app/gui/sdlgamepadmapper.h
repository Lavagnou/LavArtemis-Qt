#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
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
 * Works on the raw joystick API on purpose: the whole point is to reach
 * devices the game controller API refuses to expose.
 */
class SdlGamepadMapper : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool mapping READ isMapping NOTIFY mappingChanged)
    Q_PROPERTY(QString currentElement READ currentElement NOTIFY currentStepChanged)
    Q_PROPERTY(QString currentPrompt READ currentPrompt NOTIFY currentStepChanged)
    Q_PROPERTY(int currentStep READ currentStep NOTIFY currentStepChanged)
    Q_PROPERTY(int stepCount READ stepCount CONSTANT)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY mappingChanged)
    // Bound elements so far. Shares currentStepChanged so QML bindings on it
    // actually re-evaluate -- previewMapping() is a function call and would
    // be captured once and never updated.
    Q_PROPERTY(int bindingCount READ bindingCount NOTIFY currentStepChanged)

public:
    static SdlGamepadMapper* instance();
    static SdlGamepadMapper* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    ~SdlGamepadMapper();

    // Lists every joystick, recognized or not, as maps with keys:
    // index, name, guid, recognized, guessed, numAxes, numButtons, numHats.
    Q_INVOKABLE QVariantList enumerateDevices();

    Q_INVOKABLE void startMapping(int deviceIndex);

    // Leaves the current element unbound and moves on. Essential: plenty of
    // devices have no triggers, no hat, or only a handful of buttons.
    Q_INVOKABLE void skipCurrentStep();

    Q_INVOKABLE void restart();

    Q_INVOKABLE void cancel();

    // Persists through MappingManager and applies it to SDL immediately.
    Q_INVOKABLE bool saveMapping();

    // The mapping built so far, for display while the wizard runs.
    Q_INVOKABLE QString previewMapping() const;

    bool isMapping() const { return m_Joystick != nullptr; }
    QString currentElement() const;
    QString currentPrompt() const;
    int currentStep() const { return m_CurrentStep; }
    int stepCount() const;
    QString deviceName() const { return m_DeviceName; }
    int bindingCount() const { return m_Bindings.count(); }

signals:
    void mappingChanged();
    void currentStepChanged();
    void inputCaptured(QString binding);
    void mappingComplete();

private slots:
    void onPollingTimerFired();

private:
    explicit SdlGamepadMapper(QObject* parent = nullptr);

    void captureBinding(const QString& binding);
    void advanceStep();
    void closeDevice();
    bool pollForInput(QString& binding);

    static SdlGamepadMapper* s_instance;

    QTimer* m_PollingTimer;
    SDL_Joystick* m_Joystick;
    QString m_DeviceGuid;
    QString m_DeviceName;
    int m_CurrentStep;

    // Element name -> SDL source (e.g. "leftx" -> "a0"). Skipped elements are
    // simply absent.
    QMap<QString, QString> m_Bindings;

    // Axis rest positions, sampled when mapping starts. Switches and triggers
    // can idle at an extreme, so deviation from rest is the only reliable
    // signal that the user moved something.
    QVector<Sint16> m_AxisRestValues;

    // Buttons held down at capture time are ignored until released, so one
    // press can't satisfy several consecutive steps.
    QVector<bool> m_ButtonWasDown;
};
