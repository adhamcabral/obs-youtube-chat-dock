#pragma once

#include <QString>
#include <QUrl>

namespace chatdock {

QString normalizeInputUrl(QString text);
QString videoIdFromUrl(const QUrl &url);
QString channelLiveUrlFromInput(const QString &input);
QString firstLiveVideoIdNearSignal(const QString &html);

} // namespace chatdock

