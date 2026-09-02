// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Nitrux Latinoamericana S.C. <hello@nxos.org>

#include "valenzbridge.h"
#include "valenzbridge_p.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{

QString formatWeatherTemperature(const QJsonValue &value, const QString &unitSuffix, bool includeUnit)
{
    if (!value.isDouble())
        return QStringLiteral("--°") + (includeUnit ? unitSuffix : QString());

    const QString temperature = QString::number(value.toDouble(), 'f', 0);
    return temperature + QStringLiteral("°") + (includeUnit ? unitSuffix : QString());
}

QString formatWeatherValue(const QJsonValue &value)
{
    return value.isDouble() ? QString::number(value.toDouble(), 'f', 0) : QStringLiteral("--");
}

}

void ValenzBridge::refreshWeather()
{
    if (!m_weatherNetwork)
        return;

    QUrl url(QString::fromLatin1(kOpenMeteoUrl));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("latitude"), QString::number(m_weatherLatitude, 'f', 4));
    query.addQueryItem(QStringLiteral("longitude"), QString::number(m_weatherLongitude, 'f', 4));
    query.addQueryItem(QStringLiteral("current"), QStringLiteral("temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,is_day,uv_index,wind_speed_10m"));
    query.addQueryItem(QStringLiteral("daily"), QStringLiteral("weather_code,temperature_2m_max,temperature_2m_min"));
    query.addQueryItem(QStringLiteral("temperature_unit"), m_weatherTemperatureUnit);
    query.addQueryItem(QStringLiteral("wind_speed_unit"), QStringLiteral("kmh"));
    query.addQueryItem(QStringLiteral("forecast_days"), QStringLiteral("3"));
    query.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(7000);
    m_weatherNetwork->get(request);
}

void ValenzBridge::onWeatherReplyFinished(QNetworkReply *reply)
{
    if (!reply)
        return;

    const bool success = reply->error() == QNetworkReply::NoError;
    const QByteArray payload = success && reply->isReadable()
                                   ? reply->readAll()
                                   : QByteArray();
    const QString errorString = reply->errorString();
    reply->deleteLater();

    if (!success)
    {
        trace(QStringLiteral("weather"), QStringLiteral("refresh_failed"), errorString);
        QTimer::singleShot(15000, this, &ValenzBridge::refreshWeather);
        return;
    }

    if (payload.isEmpty())
    {
        trace(QStringLiteral("weather"), QStringLiteral("empty_reply"));
        QTimer::singleShot(15000, this, &ValenzBridge::refreshWeather);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        trace(QStringLiteral("weather"), QStringLiteral("parse_failed"), QString::fromUtf8(payload.left(120)));
        QTimer::singleShot(15000, this, &ValenzBridge::refreshWeather);
        return;
    }

    const QJsonObject root = document.object();
    const QString locationName = weatherLocationFromTimezone(root.value(QStringLiteral("timezone")).toString());
    setWeatherLocationName(locationName);

    const QJsonObject current = root.value(QStringLiteral("current")).toObject();
    if (current.isEmpty())
        return;

    if (!current.value(QStringLiteral("temperature_2m")).isDouble())
        return;

    const double temperature = current.value(QStringLiteral("temperature_2m")).toDouble();
    const int weatherCode = current.value(QStringLiteral("weather_code")).toInt(-1);
    const bool isDay = current.value(QStringLiteral("is_day")).toInt(1) > 0;

    const QString unitSuffix = m_weatherTemperatureUnit == QLatin1String("fahrenheit")
                               ? QStringLiteral("F")
                               : QStringLiteral("C");

    setWeatherTemperature(QStringLiteral("%1°%2").arg(QString::number(temperature, 'f', 0), unitSuffix));
    setWeatherIconName(weatherIconFromCode(weatherCode, isDay));
    setWeatherConditionLabel(weatherLabelFromCode(weatherCode));
    setWeatherFeelsLike(formatWeatherTemperature(current.value(QStringLiteral("apparent_temperature")), unitSuffix, true));
    setWeatherHumidity(formatWeatherValue(current.value(QStringLiteral("relative_humidity_2m"))) + QStringLiteral(" %"));
    setWeatherUvIndex(formatWeatherValue(current.value(QStringLiteral("uv_index"))));
    setWeatherWindSpeed(formatWeatherValue(current.value(QStringLiteral("wind_speed_10m"))) + QStringLiteral(" km/h"));

    const QJsonObject daily = root.value(QStringLiteral("daily")).toObject();
    const QJsonArray dates = daily.value(QStringLiteral("time")).toArray();
    const QJsonArray codes = daily.value(QStringLiteral("weather_code")).toArray();
    const QJsonArray highs = daily.value(QStringLiteral("temperature_2m_max")).toArray();
    const QJsonArray lows = daily.value(QStringLiteral("temperature_2m_min")).toArray();
    QVariantList forecast;
    const int forecastCount = qMin(3, static_cast<int>(qMin(dates.size(), qMin(codes.size(), qMin(highs.size(), lows.size())))));
    for (int i = 0; i < forecastCount; ++i)
    {
        const QString date = dates.at(i).toString();
        const int forecastCode = codes.at(i).toInt(-1);
        if (date.isEmpty() || !highs.at(i).isDouble() || !lows.at(i).isDouble())
            continue;

        QVariantMap day;
        day.insert(QStringLiteral("date"), date);
        day.insert(QStringLiteral("iconName"), weatherIconFromCode(forecastCode, true));
        day.insert(QStringLiteral("highTemperature"), formatWeatherTemperature(highs.at(i), unitSuffix, false));
        day.insert(QStringLiteral("lowTemperature"), formatWeatherTemperature(lows.at(i), unitSuffix, false));
        forecast.append(day);
    }

    setWeatherForecast(forecast);
}
