#include "valenzbridge.h"
#include "valenzbridge_p.h"

void ValenzBridge::refreshWeather()
{
    if (!m_weatherNetwork)
        return;

    QUrl url(QString::fromLatin1(kOpenMeteoUrl));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("latitude"), QString::number(m_weatherLatitude, 'f', 4));
    query.addQueryItem(QStringLiteral("longitude"), QString::number(m_weatherLongitude, 'f', 4));
    query.addQueryItem(QStringLiteral("current"), QStringLiteral("temperature_2m,weather_code,is_day"));
    query.addQueryItem(QStringLiteral("temperature_unit"), m_weatherTemperatureUnit);
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
    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    if (!success)
        return;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return;

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
}
