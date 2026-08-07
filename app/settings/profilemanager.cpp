#include "profilemanager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <QtDebug>

#define PROFILES_FILE "profiles.json"

ProfileManager* ProfileManager::s_Instance = nullptr;

ProfileManager::ProfileManager(QObject* parent)
    : QObject(parent),
      m_Settings(nullptr)
{
    load();
}

QSettings* ProfileManager::settings() const
{
    if (m_Settings == nullptr) {
        m_Settings = new QSettings();
    }
    return m_Settings;
}

ProfileManager* ProfileManager::instance()
{
    if (s_Instance == nullptr) {
        s_Instance = new ProfileManager();
    }
    return s_Instance;
}

ProfileManager* ProfileManager::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    // Same reasoning as ArtemisSettings::create(): this is a process-wide
    // singleton that C++ callers reach through instance(), so the QML engine
    // must not own and destroy it.
    ProfileManager* manager = instance();
    QQmlEngine::setObjectOwnership(manager, QQmlEngine::CppOwnership);
    return manager;
}

bool ProfileManager::isGlobalKey(const QString& key)
{
    // The UI language and the defaults version describe the installation, not a
    // stream. Letting a profile carry them would change the app's language as a
    // side effect of switching profiles.
    return key == QLatin1String("language") ||
           key == QLatin1String("defaultver");
}

QString ProfileManager::filePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
           QLatin1String("/" PROFILES_FILE);
}

ProfileManager::Profile* ProfileManager::findProfile(const QString& id)
{
    for (Profile& profile : m_Profiles) {
        if (profile.id == id) {
            return &profile;
        }
    }
    return nullptr;
}

const ProfileManager::Profile* ProfileManager::findProfile(const QString& id) const
{
    for (const Profile& profile : m_Profiles) {
        if (profile.id == id) {
            return &profile;
        }
    }
    return nullptr;
}

QVariant ProfileManager::value(const QString& key, const QVariant& defaultValue) const
{
    if (!isGlobalKey(key)) {
        const Profile* active = findProfile(m_ActiveProfileId);
        if (active != nullptr) {
            auto it = active->options.constFind(key);
            if (it != active->options.constEnd()) {
                return it.value();
            }

            // Fall through: a profile that has never been saved into inherits
            // the global value rather than snapping back to the built-in default.
        }
    }

    return settings()->value(key, defaultValue);
}

void ProfileManager::setValue(const QString& key, const QVariant& value)
{
    if (!isGlobalKey(key)) {
        Profile* active = findProfile(m_ActiveProfileId);
        if (active != nullptr) {
            if (active->options.value(key) != value) {
                active->options.insert(key, value);
                active->modifiedUtc = QDateTime::currentSecsSinceEpoch();
            }
            return;
        }
    }

    settings()->setValue(key, value);
}

QString ProfileManager::activeProfileName() const
{
    const Profile* active = findProfile(m_ActiveProfileId);
    return active != nullptr ? active->name : QString();
}

QVariantList ProfileManager::profileList() const
{
    QVariantList list;

    for (const Profile& profile : m_Profiles) {
        QVariantMap entry;
        entry.insert(QLatin1String("id"), profile.id);
        entry.insert(QLatin1String("name"), profile.name);
        entry.insert(QLatin1String("createdUtc"), profile.createdUtc);
        entry.insert(QLatin1String("modifiedUtc"), profile.modifiedUtc);
        entry.insert(QLatin1String("active"), profile.id == m_ActiveProfileId);
        list.append(entry);
    }

    return list;
}

QString ProfileManager::createProfile(const QString& name)
{
    Profile profile;
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.name = name.trimmed().isEmpty() ? tr("New profile") : name.trimmed();
    profile.createdUtc = QDateTime::currentSecsSinceEpoch();
    profile.modifiedUtc = profile.createdUtc;

    m_Profiles.append(profile);
    save();
    emit profilesChanged();

    return profile.id;
}

void ProfileManager::renameProfile(const QString& id, const QString& name)
{
    Profile* profile = findProfile(id);
    if (profile == nullptr || name.trimmed().isEmpty()) {
        return;
    }

    profile->name = name.trimmed();
    profile->modifiedUtc = QDateTime::currentSecsSinceEpoch();
    save();
    emit profilesChanged();
}

void ProfileManager::deleteProfile(const QString& id)
{
    bool wasActive = (id == m_ActiveProfileId);

    for (int i = 0; i < m_Profiles.count(); i++) {
        if (m_Profiles.at(i).id == id) {
            m_Profiles.removeAt(i);
            break;
        }
    }

    if (wasActive) {
        // Deleting the active profile drops us back to the global settings.
        m_ActiveProfileId.clear();
    }

    save();
    emit profilesChanged();

    if (wasActive) {
        emit activeProfileSwitched();
    }
}

void ProfileManager::setActiveProfile(const QString& id)
{
    if (id == m_ActiveProfileId) {
        return;
    }

    // An unknown id would leave us pointing at nothing, which reads the same as
    // no profile but keeps a stale id in the settings file.
    if (!id.isEmpty() && findProfile(id) == nullptr) {
        qWarning() << "ProfileManager: ignoring switch to unknown profile" << id;
        return;
    }

    m_ActiveProfileId = id;
    save();

    emit profilesChanged();
    emit activeProfileSwitched();
}

void ProfileManager::load()
{
    m_Profiles.clear();
    m_ActiveProfileId.clear();

    QFile file(filePath());
    if (!file.exists()) {
        // Nothing saved yet is the normal first-run state, not an error.
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ProfileManager: unable to read" << file.fileName();
        return;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "ProfileManager: malformed" << file.fileName() << error.errorString();
        return;
    }

    QJsonObject root = doc.object();

    const QJsonArray profiles = root.value(QLatin1String("profiles")).toArray();
    for (const QJsonValue& entry : profiles) {
        QJsonObject obj = entry.toObject();

        Profile profile;
        profile.id = obj.value(QLatin1String("id")).toString();
        profile.name = obj.value(QLatin1String("name")).toString();
        profile.createdUtc = (qint64)obj.value(QLatin1String("createdUtc")).toDouble();
        profile.modifiedUtc = (qint64)obj.value(QLatin1String("modifiedUtc")).toDouble();
        profile.options = obj.value(QLatin1String("options")).toObject().toVariantMap();

        if (profile.id.isEmpty()) {
            continue;
        }

        m_Profiles.append(profile);
    }

    QString activeId = root.value(QLatin1String("activeProfileId")).toString();
    if (!activeId.isEmpty() && findProfile(activeId) != nullptr) {
        m_ActiveProfileId = activeId;
    }
}

void ProfileManager::save()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    if (!dir.mkpath(QLatin1String("."))) {
        qWarning() << "ProfileManager: unable to create" << dir.path();
        return;
    }

    QJsonArray profiles;
    for (const Profile& profile : m_Profiles) {
        QJsonObject obj;
        obj.insert(QLatin1String("id"), profile.id);
        obj.insert(QLatin1String("name"), profile.name);
        obj.insert(QLatin1String("createdUtc"), (double)profile.createdUtc);
        obj.insert(QLatin1String("modifiedUtc"), (double)profile.modifiedUtc);
        obj.insert(QLatin1String("options"), QJsonObject::fromVariantMap(profile.options));
        profiles.append(obj);
    }

    QJsonObject root;
    root.insert(QLatin1String("profiles"), profiles);
    root.insert(QLatin1String("activeProfileId"), m_ActiveProfileId);

    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "ProfileManager: unable to write" << file.fileName();
        return;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    // Anything routed to the global store still has to reach disk.
    if (m_Settings != nullptr) {
        m_Settings->sync();
    }
}
