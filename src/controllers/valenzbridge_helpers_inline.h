#pragma once

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QLocalSocket>
#include <QTimer>
#include <QProcess>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <pwd.h>
#include <unistd.h>
#include <QUrlQuery>
#include <QtGlobal>
#include <cmath>
#include <utility>

inline QStringList normalizePowerProfiles(const QVariant &value)
{
    Q_UNUSED(value)
    return QStringList { QStringLiteral("performance"), QStringLiteral("balanced"), QStringLiteral("power-saver") };
}

inline QString normalizeCurrentPowerProfile(const QString &value, const QStringList &profiles)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return profiles.value(0, QStringLiteral("balanced"));

    for (const QString &profile : profiles)
    {
        if (profile.compare(trimmed, Qt::CaseInsensitive) == 0)
            return profile;
    }

    return profiles.value(0, QStringLiteral("balanced"));
}

inline bool normalizeControlCenterBatteryCharging(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::Bool)
        return value.toBool();

    const QString normalized = value.toString().trimmed().toLower();
    if (normalized == QLatin1String("charging")
        || normalized == QLatin1String("true")
        || normalized == QLatin1String("1")
        || normalized == QLatin1String("on")
        || normalized == QLatin1String("yes")
        || normalized.contains(QLatin1String("charging")))
    {
        return true;
    }

    return false;
}

inline QString normalizeWeatherTemperatureUnit(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QLatin1String("fahrenheit"))
        return QStringLiteral("fahrenheit");

    return QStringLiteral("celsius");
}

inline double normalizeWeatherCoordinate(const QVariant &value, double minValue, double maxValue, double fallback)
{
    bool ok = false;
    const double parsed = value.toDouble(&ok);
    if (!ok || !std::isfinite(parsed))
        return fallback;

    return qBound(minValue, parsed, maxValue);
}

inline int normalizeWeatherRefreshMinutes(const QVariant &value)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok)
        return 20;

    return qBound(kWeatherRefreshMinMinutes, parsed, kWeatherRefreshMaxMinutes);
}

inline QString normalizePowerCommand(const QString &value)
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("wlogout") : trimmed;
}

inline QString normalizeControlCenterIconMode(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QLatin1String("system16") || normalized == QLatin1String("nerd"))
        return normalized;

    if (normalized == QLatin1String("auto") || normalized.isEmpty())
        return QStringLiteral("system16");

    return QStringLiteral("system16");
}

inline QString weatherIconFromCode(int weatherCode, bool isDay)
{
    Q_UNUSED(isDay)

    if (weatherCode == 0)
        return QStringLiteral("weather-clear");

    if (weatherCode == 1 || weatherCode == 2)
        return QStringLiteral("weather-few-clouds");

    if (weatherCode == 3)
        return QStringLiteral("weather-overcast");

    if (weatherCode == 45 || weatherCode == 48)
        return QStringLiteral("weather-fog");

    if ((weatherCode >= 51 && weatherCode <= 67) || (weatherCode >= 80 && weatherCode <= 82))
        return QStringLiteral("weather-showers");

    if ((weatherCode >= 71 && weatherCode <= 77) || (weatherCode >= 85 && weatherCode <= 86))
        return QStringLiteral("weather-snow");

    if (weatherCode >= 95 && weatherCode <= 99)
        return QStringLiteral("weather-storm");

    return QStringLiteral("weather-severe-alert");
}

inline QString weatherLabelFromCode(int weatherCode)
{
    switch (weatherCode)
    {
    case 0: return QStringLiteral("Clear");
    case 1: return QStringLiteral("Mainly clear");
    case 2: return QStringLiteral("Partly cloudy");
    case 3: return QStringLiteral("Overcast");
    case 45:
    case 48: return QStringLiteral("Fog");
    case 51:
    case 53:
    case 55: return QStringLiteral("Drizzle");
    case 56:
    case 57: return QStringLiteral("Freezing drizzle");
    case 61:
    case 63:
    case 65: return QStringLiteral("Rain");
    case 66:
    case 67: return QStringLiteral("Freezing rain");
    case 71:
    case 73:
    case 75:
    case 77: return QStringLiteral("Snow");
    case 80:
    case 81:
    case 82: return QStringLiteral("Rain showers");
    case 85:
    case 86: return QStringLiteral("Snow showers");
    case 95:
    case 96:
    case 99: return QStringLiteral("Thunderstorm");
    default: return QStringLiteral("Unknown");
    }
}

inline QString systemUserRealName()
{
#if defined(Q_OS_UNIX)
    const struct passwd *userInfo = getpwuid(getuid());
    if (!userInfo)
        return QStringLiteral("User");

    const QByteArray userName = userInfo->pw_name ? QByteArray(userInfo->pw_name) : QByteArray();
    QString realName = userInfo->pw_gecos ? QString::fromLocal8Bit(userInfo->pw_gecos) : QString();
    realName = realName.section(QLatin1Char(','), 0, 0).trimmed();

    if (realName.contains(QLatin1Char('&')) && !userName.isEmpty())
    {
        QString replacement = QString::fromLocal8Bit(userName);
        if (!replacement.isEmpty())
        {
            replacement[0] = replacement[0].toUpper();
            realName.replace(QLatin1Char('&'), replacement);
            realName = realName.trimmed();
        }
    }

    if (!realName.isEmpty())
        return realName;

    if (!userName.isEmpty())
        return QString::fromLocal8Bit(userName);
#endif
    return QStringLiteral("User");
}

inline QVariant unwrapMprisVariant(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QDBusVariant>())
        return value.value<QDBusVariant>().variant();

    return value;
}

inline QVariantMap variantToVariantMap(const QVariant &value)
{
    const QVariant unwrapped = unwrapMprisVariant(value);

    if (unwrapped.metaType().id() == QMetaType::QVariantMap || unwrapped.canConvert<QVariantMap>())
        return unwrapped.toMap();

    if (unwrapped.metaType() == QMetaType::fromType<QDBusArgument>())
    {
        const QDBusArgument argument = unwrapped.value<QDBusArgument>();
        const QVariantMap decodedMap = qdbus_cast<QVariantMap>(argument);
        if (!decodedMap.isEmpty())
            return decodedMap;
    }

    return {};
}

inline QStringList variantToStringList(const QVariant &value)
{
    const QVariant unwrapped = unwrapMprisVariant(value);

    if (unwrapped.metaType().id() == QMetaType::QStringList)
        return unwrapped.toStringList();

    if (unwrapped.canConvert<QStringList>())
        return unwrapped.value<QStringList>();

    if (unwrapped.metaType() == QMetaType::fromType<QVariantList>())
    {
        QStringList strings;
        const QVariantList values = unwrapped.toList();

        for (const QVariant &entry : values)
        {
            const QString item = unwrapMprisVariant(entry).toString().trimmed();
            if (!item.isEmpty())
                strings << item;
        }

        return strings;
    }

    if (unwrapped.metaType() == QMetaType::fromType<QDBusArgument>())
    {
        const QDBusArgument argument = unwrapped.value<QDBusArgument>();
        const QStringList decodedList = qdbus_cast<QStringList>(argument);
        if (!decodedList.isEmpty())
            return decodedList;
    }

    const QString singleValue = unwrapped.toString().trimmed();
    return singleValue.isEmpty() ? QStringList{} : QStringList{singleValue};
}

inline bool runHyprctlJson(const QStringList &arguments, QJsonValue *result)
{
    QProcess process;
    process.start(QStringLiteral("hyprctl"), arguments);

    if (!process.waitForStarted(250) || !process.waitForFinished(1200))
        return false;

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return false;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return false;

    if (document.isObject())
        *result = document.object();
    else if (document.isArray())
        *result = document.array();
    else
        return false;

    return true;
}

inline bool runHyprctlDispatch(const QStringList &arguments)
{
    QProcess process;
    process.start(QStringLiteral("hyprctl"), arguments);

    if (!process.waitForStarted(250))
    {
        return false;
    }

    if (!process.waitForFinished(1200))
    {
        process.kill();
        process.waitForFinished(250);
        return false;
    }

    const QString stdOut = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    const QString stdErr = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    const bool processOk = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    const bool outputHasError = stdOut.contains(QStringLiteral("error:"), Qt::CaseInsensitive)
                             || stdErr.contains(QStringLiteral("error:"), Qt::CaseInsensitive);
    const bool outputLooksOk = stdOut.isEmpty() || stdOut.startsWith(QStringLiteral("ok"), Qt::CaseInsensitive);
    const bool ok = processOk && !outputHasError && outputLooksOk;

    return ok;
}

inline bool dispatchWorkspaceFocus(const QString &selector)
{
    const QString luaDispatch = QStringLiteral("hl.dsp.focus({ workspace = \"%1\" })").arg(selector);

    if (runHyprctlDispatch(QStringList { QStringLiteral("dispatch"), luaDispatch }))
        return true;

    return runHyprctlDispatch(QStringList { QStringLiteral("dispatch"), QStringLiteral("workspace"), selector });
}

inline QString hyprlandEventSocketPath()
{
    const QString signature = QString::fromLocal8Bit(qgetenv("HYPRLAND_INSTANCE_SIGNATURE")).trimmed();
    if (signature.isEmpty())
        return {};

    QString runtimeDir = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR")).trimmed();
    if (runtimeDir.isEmpty())
        runtimeDir = QStringLiteral("/tmp");

    return QStringLiteral("%1/hypr/%2/.socket2.sock").arg(runtimeDir, signature);
}

inline bool isWorkspaceRelatedHyprlandEvent(const QString &eventName)
{
    return eventName == QLatin1String("workspace")
        || eventName == QLatin1String("workspacev2")
        || eventName == QLatin1String("focusedmon")
        || eventName == QLatin1String("focusedmonv2")
        || eventName == QLatin1String("createworkspace")
        || eventName == QLatin1String("createworkspacev2")
        || eventName == QLatin1String("destroyworkspace")
        || eventName == QLatin1String("destroyworkspacev2")
        || eventName == QLatin1String("moveworkspace")
        || eventName == QLatin1String("moveworkspacev2");
}

inline bool isFocusedWindowRelatedHyprlandEvent(const QString &eventName)
{
    return eventName == QLatin1String("activewindow")
        || eventName == QLatin1String("activewindowv2")
        || eventName == QLatin1String("windowtitle")
        || eventName == QLatin1String("windowtitlev2")
        || eventName == QLatin1String("openwindow")
        || eventName == QLatin1String("closewindow");
}

inline int hyprlandCurrentWorkspace(const QJsonValue &activeWorkspace)
{
    if (!activeWorkspace.isObject())
        return -1;

    return activeWorkspace.toObject().value(QStringLiteral("id")).toInt(-1);
}

inline int hyprlandWorkspaceCount(const QJsonValue &workspaces)
{
    if (!workspaces.isArray())
        return -1;

    int maxWorkspaceId = 0;
    const QJsonArray workspaceArray = workspaces.toArray();

    for (const QJsonValue &workspaceValue : workspaceArray)
    {
        const int id = workspaceValue.toObject().value(QStringLiteral("id")).toInt(0);
        if (id > maxWorkspaceId)
            maxWorkspaceId = id;
    }

    return maxWorkspaceId > 0 ? maxWorkspaceId : workspaceArray.size();
}

inline void addUniqueCaseInsensitive(QStringList *list, const QString &candidate)
{
    if (!list)
        return;

    const QString trimmed = candidate.trimmed();
    if (trimmed.isEmpty())
        return;

    for (const QString &entry : std::as_const(*list))
    {
        if (entry.compare(trimmed, Qt::CaseInsensitive) == 0)
            return;
    }

    list->append(trimmed);
}

inline QString withoutDesktopSuffix(const QString &value)
{
    QString normalized = value.trimmed();
    if (normalized.endsWith(QLatin1String(".desktop"), Qt::CaseInsensitive))
        normalized.chop(8);

    return normalized;
}

inline QString normalizedLookupKey(const QString &value)
{
    QString normalized = withoutDesktopSuffix(value).toLower().simplified();
    normalized.remove(QChar::fromLatin1(32));
    normalized.remove(QChar::fromLatin1(45));
    normalized.remove(QChar::fromLatin1(95));
    normalized.remove(QChar::fromLatin1(46));
    return normalized;
}

inline void addLookupVariants(QStringList *variants, const QString &value)
{
    const QString base = withoutDesktopSuffix(value);
    if (base.isEmpty())
        return;

    addUniqueCaseInsensitive(variants, base);

    const QString lower = base.toLower();
    addUniqueCaseInsensitive(variants, lower);

    const QString simplified = lower.simplified();
    addUniqueCaseInsensitive(variants, simplified);

    QString compact = simplified;
    compact.remove(QChar::fromLatin1(32));
    addUniqueCaseInsensitive(variants, compact);

    const QString tail = compact.section(QChar::fromLatin1(46), -1);
    addUniqueCaseInsensitive(variants, tail);

    const QString fileName = QFileInfo(base).fileName();
    addUniqueCaseInsensitive(variants, fileName);
}

inline void addWindowIconCandidates(QStringList *candidates, const QString &value)
{
    const QString base = withoutDesktopSuffix(value);
    if (base.isEmpty())
        return;

    addUniqueCaseInsensitive(candidates, base);

    const QString lower = base.toLower();
    addUniqueCaseInsensitive(candidates, lower);

    QString normalized = lower;
    normalized.replace(QChar::fromLatin1(32), QChar::fromLatin1(45));
    addUniqueCaseInsensitive(candidates, normalized);

    QString compact = lower;
    compact.remove(QChar::fromLatin1(32));
    addUniqueCaseInsensitive(candidates, compact);

    QString dotted = compact;
    dotted.replace(QChar::fromLatin1(46), QChar::fromLatin1(45));
    addUniqueCaseInsensitive(candidates, dotted);

    QString underscored = dotted;
    underscored.replace(QChar::fromLatin1(95), QChar::fromLatin1(45));
    addUniqueCaseInsensitive(candidates, underscored);

    const QString dottedTail = compact.section(QChar::fromLatin1(46), -1);
    addUniqueCaseInsensitive(candidates, dottedTail);

    const QFileInfo fileInfo(base);
    addUniqueCaseInsensitive(candidates, fileInfo.fileName());
    addUniqueCaseInsensitive(candidates, fileInfo.completeBaseName());
}

inline QString shortCommandFromExec(const QString &execField)
{
    const QString trimmed = execField.trimmed();
    if (trimmed.isEmpty())
        return {};

    const QStringList tokens = trimmed.split(QChar::fromLatin1(32), Qt::SkipEmptyParts);
    if (tokens.isEmpty())
        return {};

    int commandIndex = 0;
    if (tokens.at(0) == QLatin1String("env"))
    {
        commandIndex = 1;
        while (commandIndex < tokens.size())
        {
            const QString token = tokens.at(commandIndex);
            if (token.startsWith(QLatin1Char('-')) || token.contains(QLatin1Char('=')))
            {
                ++commandIndex;
                continue;
            }

            break;
        }
    }

    if (commandIndex >= tokens.size())
        return {};

    QString command = tokens.at(commandIndex).trimmed();
    if (command.startsWith(QLatin1Char('"')) && command.endsWith(QLatin1Char('"')) && command.size() > 1)
        command = command.mid(1, command.size() - 2);
    else if (command.startsWith(QLatin1Char('\'')) && command.endsWith(QLatin1Char('\'')) && command.size() > 1)
        command = command.mid(1, command.size() - 2);

    return QFileInfo(command).completeBaseName();
}

inline void registerDesktopLookupValue(QHash<QString, QString> *iconLookup, const QString &value, const QString &iconName)
{
    if (!iconLookup)
        return;

    const QString icon = iconName.trimmed();
    if (icon.isEmpty())
        return;

    QStringList variants;
    addLookupVariants(&variants, value);

    for (const QString &variant : std::as_const(variants))
    {
        const QString key = normalizedLookupKey(variant);
        if (key.isEmpty() || iconLookup->contains(key))
            continue;

        iconLookup->insert(key, icon);
    }
}

inline QStringList desktopEntryDirs()
{
    QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    if (dirs.isEmpty())
    {
        dirs << QDir::homePath() + QStringLiteral("/.local/share/applications")
             << QStringLiteral("/usr/local/share/applications")
             << QStringLiteral("/usr/share/applications");
    }

    QSet<QString> seen;
    QStringList uniqueDirs;
    uniqueDirs.reserve(dirs.size());

    for (const QString &dir : std::as_const(dirs))
    {
        const QString normalizedDir = QDir::cleanPath(dir.trimmed());
        if (normalizedDir.isEmpty() || seen.contains(normalizedDir))
            continue;

        seen.insert(normalizedDir);
        uniqueDirs << normalizedDir;
    }

    return uniqueDirs;
}

inline QHash<QString, QString> buildDesktopIconLookup()
{
    QHash<QString, QString> iconLookup;

    const QStringList dirs = desktopEntryDirs();
    for (const QString &dirPath : dirs)
    {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;

        QDirIterator iterator(dirPath,
                              QStringList { QStringLiteral("*.desktop") },
                              QDir::Files,
                              QDirIterator::Subdirectories);

        while (iterator.hasNext())
        {
            const QString filePath = iterator.next();
            QSettings desktopFile(filePath, QSettings::IniFormat);

            const QString type = desktopFile.value(QStringLiteral("Desktop Entry/Type")).toString().trimmed();
            if (!type.isEmpty() && type.compare(QStringLiteral("Application"), Qt::CaseInsensitive) != 0)
                continue;

            const QString iconName = desktopFile.value(QStringLiteral("Desktop Entry/Icon")).toString().trimmed();
            if (iconName.isEmpty())
                continue;

            const QString appId = QFileInfo(filePath).completeBaseName();
            registerDesktopLookupValue(&iconLookup, appId, iconName);

            const QString startupWmClass = desktopFile.value(QStringLiteral("Desktop Entry/StartupWMClass")).toString().trimmed();
            registerDesktopLookupValue(&iconLookup, startupWmClass, iconName);

            const QString appName = desktopFile.value(QStringLiteral("Desktop Entry/Name")).toString().trimmed();
            registerDesktopLookupValue(&iconLookup, appName, iconName);

            const QString execField = desktopFile.value(QStringLiteral("Desktop Entry/Exec")).toString().trimmed();
            registerDesktopLookupValue(&iconLookup, shortCommandFromExec(execField), iconName);
        }
    }

    return iconLookup;
}

inline const QHash<QString, QString> &desktopIconLookup()
{
    static const QHash<QString, QString> lookup = buildDesktopIconLookup();
    return lookup;
}

inline QString lookupIconFromDesktopEntries(const QString &value)
{
    const auto &lookup = desktopIconLookup();
    if (lookup.isEmpty())
        return {};

    QStringList variants;
    addLookupVariants(&variants, value);

    for (const QString &variant : std::as_const(variants))
    {
        const QString key = normalizedLookupKey(variant);
        if (key.isEmpty())
            continue;

        const auto match = lookup.constFind(key);
        if (match != lookup.constEnd() && !match.value().trimmed().isEmpty())
            return match.value().trimmed();
    }

    return {};
}

inline bool isAgendaInstalled()
{
    const QStringList candidateDesktopIds {
        QStringLiteral("org.kde.agenda"),
        QStringLiteral("org.maui.agenda"),
        QStringLiteral("agenda"),
        QStringLiteral("maui-agenda")
    };

    const QStringList dirs = desktopEntryDirs();
    for (const QString &dirPath : dirs)
    {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;

        QDirIterator iterator(dirPath,
                              QStringList { QStringLiteral("*.desktop") },
                              QDir::Files,
                              QDirIterator::Subdirectories);

        while (iterator.hasNext())
        {
            const QString filePath = iterator.next();
            const QString desktopId = QFileInfo(filePath).completeBaseName();

            for (const QString &candidate : candidateDesktopIds)
            {
                if (desktopId.compare(candidate, Qt::CaseInsensitive) == 0)
                    return true;
            }
        }
    }

    for (const QString &candidate : candidateDesktopIds)
    {
        if (!lookupIconFromDesktopEntries(candidate).isEmpty())
            return true;
    }

    return false;
}

inline QString processNameFromPid(qint64 pid)
{
    if (pid <= 0)
        return {};

    QFile commFile(QStringLiteral("/proc/%1/comm").arg(pid));
    if (commFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        const QString comm = QString::fromLocal8Bit(commFile.readLine()).trimmed();
        if (!comm.isEmpty())
            return comm;
    }

    QFileInfo exeLink(QStringLiteral("/proc/%1/exe").arg(pid));
    if (!exeLink.exists())
        return {};

    const QString target = exeLink.symLinkTarget().trimmed();
    if (target.isEmpty())
        return {};

    return QFileInfo(target).completeBaseName();
}

inline bool isUsableIconSource(const QString &value)
{
    const QString candidate = value.trimmed();
    if (candidate.isEmpty())
        return false;

    return QIcon::hasThemeIcon(candidate) || QFileInfo::exists(candidate);
}


inline QString weatherLocationFromTimezone(const QString &timezoneName)
{
    const QString trimmed = timezoneName.trimmed();
    if (trimmed.isEmpty())
        return QString();

    QString citySegment = trimmed.section(QLatin1Char('/'), -1);
    if (citySegment.isEmpty())
        citySegment = trimmed;

    citySegment.replace(QLatin1Char('_'), QLatin1Char(' '));
    citySegment = citySegment.trimmed();

    if (citySegment.compare(QStringLiteral("GMT"), Qt::CaseInsensitive) == 0
        || citySegment.compare(QStringLiteral("UTC"), Qt::CaseInsensitive) == 0)
    {
        return QString();
    }

    return citySegment;
}

