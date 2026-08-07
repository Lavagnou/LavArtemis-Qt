#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantMap>
#include <QVector>
#include <QQmlEngine>

/**
 * @brief Named snapshots of the streaming preferences.
 *
 * This is the desktop counterpart of the Artemis profiles feature on Android
 * (profiles/ProfilesManager.java). The idea is the same -- keep several named
 * sets of settings around and switch between them, e.g. a LAN 4K profile and a
 * remote 1080p one.
 *
 * The mechanism is not the same, deliberately. Android overlays a patch map on
 * top of SharedPreferences for reads but leaves edit() pointing at the base
 * store (ProfilesManager.java:251), so writes escape the profile and silently
 * land in the global preferences. Here reads and writes both go through this
 * class, so they cannot disagree.
 *
 * A profile holds a full snapshot rather than a sparse patch: StreamingPreferences
 * writes every key on save, so a freshly created profile inherits whatever is
 * currently in effect and is independent from then on.
 */
class ProfileManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString activeProfileId READ activeProfileId NOTIFY profilesChanged)
    Q_PROPERTY(QString activeProfileName READ activeProfileName NOTIFY profilesChanged)
    Q_PROPERTY(QVariantList profiles READ profileList NOTIFY profilesChanged)

public:
    static ProfileManager* instance();
    static ProfileManager* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    // Storage routing, used by StreamingPreferences. When a profile is active,
    // both of these operate on that profile's options and never touch QSettings.
    // Keys listed in isGlobalKey() always bypass the profile.
    QVariant value(const QString& key, const QVariant& defaultValue) const;
    void setValue(const QString& key, const QVariant& value);

    bool hasActiveProfile() const { return !m_ActiveProfileId.isEmpty(); }
    QString activeProfileId() const { return m_ActiveProfileId; }
    QString activeProfileName() const;
    QVariantList profileList() const;

    // Returns the new profile's id. The profile starts empty, which means it
    // inherits the current global settings until something is saved into it.
    Q_INVOKABLE QString createProfile(const QString& name);
    Q_INVOKABLE void renameProfile(const QString& id, const QString& name);
    Q_INVOKABLE void deleteProfile(const QString& id);

    // Pass an empty id to go back to the global (profile-less) settings.
    Q_INVOKABLE void setActiveProfile(const QString& id);

    // Writes profiles.json and flushes any QSettings writes made through
    // setValue(). StreamingPreferences::save() must call this or nothing that
    // went into the active profile survives the session.
    Q_INVOKABLE void save();
    Q_INVOKABLE void load();

signals:
    void profilesChanged();

    // Emitted after the active profile changes, once the new values are the
    // ones value() will return. StreamingPreferences listens for this to
    // reload itself and refresh anything bound in QML.
    void activeProfileSwitched();

private:
    explicit ProfileManager(QObject* parent = nullptr);

    struct Profile {
        QString id;
        QString name;
        qint64 createdUtc;
        qint64 modifiedUtc;
        QVariantMap options;
    };

    // Settings that describe the application rather than a stream, and so must
    // not follow the active profile.
    static bool isGlobalKey(const QString& key);

    QString filePath() const;
    Profile* findProfile(const QString& id);
    const Profile* findProfile(const QString& id) const;

    // Created on first use rather than in the constructor: QSettings resolves
    // its path from the application and organization names, which main() sets
    // up before anything reaches us, but only just.
    QSettings* settings() const;

    static ProfileManager* s_Instance;

    QVector<Profile> m_Profiles;
    QString m_ActiveProfileId;
    mutable QSettings* m_Settings;
};
