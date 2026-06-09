#include "detector.hpp"

#include <QRegularExpression>
#include <QStringList>
#include <QUrlQuery>

namespace chatdock {

namespace {

const QRegularExpression &videoIdPattern()
{
	static const QRegularExpression pattern("^[A-Za-z0-9_-]{11}$");
	return pattern;
}

bool isValidVideoId(const QString &value)
{
	return videoIdPattern().match(value).hasMatch();
}

} // namespace

QString normalizeInputUrl(QString text)
{
	text = text.trimmed();

	if (text.startsWith('@'))
		return QStringLiteral("https://www.youtube.com/%1").arg(text);

	if (!text.startsWith("http://", Qt::CaseInsensitive) &&
	    !text.startsWith("https://", Qt::CaseInsensitive))
		text.prepend("https://");

	return text;
}

QString videoIdFromUrl(const QUrl &url)
{
	const QUrlQuery query(url);
	const QString v = query.queryItemValue("v");
	if (isValidVideoId(v))
		return v;

	const QString host = url.host().toLower();
	const QString path = url.path();

	if (host == "youtu.be") {
		const QString id = path.section('/', 1, 1);
		if (isValidVideoId(id))
			return id;
	}

	static const QRegularExpression pathIdRe(
		R"(/(?:embed|shorts|live)/([A-Za-z0-9_-]{11}))");
	const auto pathMatch = pathIdRe.match(path);
	if (pathMatch.hasMatch())
		return pathMatch.captured(1);

	return {};
}

QString channelLiveUrlFromInput(const QString &input)
{
	const QString normalized = normalizeInputUrl(input);
	QUrl url(normalized);

	if (!url.isValid() || url.host().isEmpty())
		return {};

	const QString host = url.host().toLower();
	if (!host.endsWith("youtube.com") && !host.endsWith("youtube-nocookie.com"))
		return {};

	if (!videoIdFromUrl(url).isEmpty())
		return {};

	url.setQuery(QString());
	url.setFragment(QString());

	QString path = url.path();
	if (path.isEmpty() || path == "/")
		return {};

	while (path.endsWith('/'))
		path.chop(1);

	if (!path.endsWith("/live"))
		path.append("/live");

	url.setPath(path);
	return url.toString();
}

QString firstLiveVideoIdNearSignal(const QString &html)
{
	QString decoded = html;
	decoded.replace("\\u0026", "&");
	decoded.replace("\\/", "/");
	decoded.replace("&amp;", "&");

	static const QRegularExpression canonicalRe(
		R"REGEX(href="https://www\.youtube\.com/watch\?v=([A-Za-z0-9_-]{11})")REGEX");
	auto canonicalMatch = canonicalRe.match(decoded);
	if (canonicalMatch.hasMatch())
		return canonicalMatch.captured(1);

	const QStringList liveSignals = {
		QStringLiteral("\"isLiveNow\":true"),
		QStringLiteral("LIVE_NOW"),
		QStringLiteral("liveBroadcastDetails"),
		QStringLiteral("watching now"),
		QStringLiteral("assistindo agora"),
	};

	auto hasLiveSignalNear = [&](qsizetype pos) {
		const qsizetype start = qMax<qsizetype>(0, pos - 2000);
		const QString window = decoded.mid(start, 4000);

		for (const QString &signal : liveSignals) {
			if (window.contains(signal, Qt::CaseInsensitive))
				return true;
		}

		return false;
	};

	static const QRegularExpression watchRe(R"(/watch\?v=([A-Za-z0-9_-]{11}))");
	auto watchIt = watchRe.globalMatch(decoded);
	while (watchIt.hasNext()) {
		const auto match = watchIt.next();
		if (hasLiveSignalNear(match.capturedStart()))
			return match.captured(1);
	}

	static const QRegularExpression videoIdRe(
		R"REGEX("videoId"\s*:\s*"([A-Za-z0-9_-]{11})")REGEX");
	auto videoIt = videoIdRe.globalMatch(decoded);
	while (videoIt.hasNext()) {
		const auto match = videoIt.next();
		if (hasLiveSignalNear(match.capturedStart()))
			return match.captured(1);
	}

	return {};
}

} // namespace chatdock

