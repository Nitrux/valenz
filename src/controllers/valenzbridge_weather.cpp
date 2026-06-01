#include "valenzbridge.h"
#include "valenzbridge_p.h"

void ValenzBridge::refreshWeather()
{
    if (!m_weatherNetwork)
    {
        qWarning() << "[weather] refresh skipped: network manager is null";
        return;
    }

    QUrl url(QString::fromLatin1(kOpenMeteoUrl));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("latitude"), QString::number(m_weatherLatitude, 'f', 4));
    query.addQueryItem(QStringLiteral("longitude"), QString::number(m_weatherLongitude, 'f', 4));
    query.addQueryItem(QStringLiteral("current"), QStringLiteral("temperature_2m,weather_code,is_day"));
    query.addQueryItem(QStringLiteral("temperature_unit"), m_weatherTemperatureUnit);
    query.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    url.setQuery(query);

    const QString requestUrl = url.toString(QUrl::FullyEncoded);
    qInfo().noquote() << QStringLiteral("[weather] request GET %1").arg(requestUrl);

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(7000);
    m_weatherNetwork->get(request);
}

void ValenzBridge::onWeatherReplyFinished(QNetworkReply *reply)
{
    if (!reply)
    {
        qWarning() << "[weather] reply callback received a null reply";
        return;
    }

    const QUrl replyUrl = reply->url();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool success = reply->error() == QNetworkReply::NoError;
    const QByteArray payload = reply->readAll();
    const QString payloadPreview = QString::fromUtf8(payload.left(320)).simplified();

    if (!success)
    {
        qWarning().noquote() << QStringLiteral("[weather] request failed url=%1 status=%2 error=%3 payload=%4")
                                 .arg(replyUrl.toString(QUrl::FullyEncoded))
                                 .arg(statusCode)
                                 .arg(reply->errorString())
                                 .arg(payloadPreview);
        reply->deleteLater();
        return;
    }

    qInfo().noquote() << QStringLiteral("[weather] response ok url=%1 status=%2 bytes=%3")
                         .arg(replyUrl.toString(QUrl::FullyEncoded))
                         .arg(statusCode)
                         .arg(payload.size());

    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        qWarning().noquote() << QStringLiteral("[weather] json parse error=%1 payload=%2")
                                 .arg(parseError.errorString())
                                 .arg(payloadPreview);
        return;
    }

    if (!document.isObject())
    {
        qWarning() << "[weather] response JSON root is not an object";
        return;
    }

    const QJsonObject root = document.object();
    const QString locationName = weatherLocationFromTimezone(root.value(QStringLiteral("timezone")).toString());
    setWeatherLocationName(locationName);

    const QJsonObject current = root.value(QStringLiteral("current")).toObject();
    if (current.isEmpty())
    {
        qWarning().noquote() << QStringLiteral("[weather] response missing current object. keys=%1")
                                 .arg(root.keys().join(QStringLiteral(",")));
        return;
    }

    if (!current.value(QStringLiteral("temperature_2m")).isDouble())
    {
        qWarning().noquote() << QStringLiteral("[weather] temperature_2m is missing or non-numeric in current=%1")
                                 .arg(QString::fromUtf8(QJsonDocument(current).toJson(QJsonDocument::Compact)));
        return;
    }

    const double temperature = current.value(QStringLiteral("temperature_2m")).toDouble();
    const int weatherCode = current.value(QStringLiteral("weather_code")).toInt(-1);
    const bool isDay = current.value(QStringLiteral("is_day")).toInt(1) > 0;

    const QString unitSuffix = m_weatherTemperatureUnit == QLatin1String("fahrenheit")
                               ? QStringLiteral("F")
                               : QStringLiteral("C");

    qInfo().noquote() << QStringLiteral("[weather] parsed temperature=%1 weather_code=%2 is_day=%3 unit=%4 location=%5")
                         .arg(QString::number(temperature, 'f', 1))
                         .arg(weatherCode)
                         .arg(isDay ? QStringLiteral("true") : QStringLiteral("false"))
                         .arg(m_weatherTemperatureUnit)
                         .arg(m_weatherLocationName);

    setWeatherTemperature(QStringLiteral("%1°%2").arg(QString::number(temperature, 'f', 0), unitSuffix));
    setWeatherIconName(weatherIconFromCode(weatherCode, isDay));
    setWeatherConditionLabel(weatherLabelFromCode(weatherCode));
}

