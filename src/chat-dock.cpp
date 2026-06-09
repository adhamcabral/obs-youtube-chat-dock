#include "chat-dock.hpp"

#include "detector.hpp"

#include <obs-module.h>
#include <util/bmem.h>
#include <util/platform.h>

#include <QByteArray>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMetaObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrlQuery>
#include <QVBoxLayout>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <vector>
#endif

namespace chatdock {

namespace {

constexpr auto *DOCK_ID = "chat-dock";
constexpr int POLL_INTERVAL_MS = 30 * 1000;

#ifdef _WIN32
struct LiveFetchResult {
	QUrl finalUrl;
	QByteArray body;
	QString errorMessage;
};

struct InternetHandle {
	HINTERNET handle = nullptr;

	InternetHandle() = default;
	explicit InternetHandle(HINTERNET value) : handle(value) {}
	~InternetHandle()
	{
		if (handle)
			WinHttpCloseHandle(handle);
	}

	InternetHandle(const InternetHandle &) = delete;
	InternetHandle &operator=(const InternetHandle &) = delete;

	operator HINTERNET() const { return handle; }
};

QString windowsErrorMessage(DWORD error)
{
	wchar_t *message = nullptr;
	const DWORD size = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, error, 0, reinterpret_cast<LPWSTR>(&message), 0,
		nullptr);

	QString result = size && message ? QString::fromWCharArray(message).trimmed()
					 : QString("erro Windows %1").arg(error);
	if (message)
		LocalFree(message);
	return result;
}

QString queryFinalUrl(HINTERNET request, const QString &fallback)
{
	DWORD size = 0;
	if (WinHttpQueryOption(request, WINHTTP_OPTION_URL, nullptr, &size) ||
	    GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		return fallback;

	std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1);
	if (!WinHttpQueryOption(request, WINHTTP_OPTION_URL, buffer.data(), &size))
		return fallback;

	return QString::fromWCharArray(buffer.data());
}

LiveFetchResult fetchLivePageWinHttp(const QString &urlText)
{
	LiveFetchResult result;
	result.finalUrl = QUrl(urlText);

	const std::wstring url = urlText.toStdWString();
	URL_COMPONENTS parts = {};
	parts.dwStructSize = sizeof(parts);
	parts.dwSchemeLength = static_cast<DWORD>(-1);
	parts.dwHostNameLength = static_cast<DWORD>(-1);
	parts.dwUrlPathLength = static_cast<DWORD>(-1);
	parts.dwExtraInfoLength = static_cast<DWORD>(-1);

	if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) {
		result.errorMessage = QString("URL invalida: %1")
					      .arg(windowsErrorMessage(GetLastError()));
		return result;
	}

	const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
	std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
	path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
	if (path.empty())
		path = L"/";

	InternetHandle session(WinHttpOpen(
		L"Mozilla/5.0 ChatDock/0.1 OBS-Plugin",
		WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0));
	if (!session) {
		result.errorMessage = QString("Falha ao iniciar WinHTTP: %1")
					      .arg(windowsErrorMessage(GetLastError()));
		return result;
	}

	WinHttpSetTimeouts(session, 10000, 10000, 10000, 15000);

	InternetHandle connection(
		WinHttpConnect(session, host.c_str(), parts.nPort, 0));
	if (!connection) {
		result.errorMessage = QString("Falha ao conectar: %1")
					      .arg(windowsErrorMessage(GetLastError()));
		return result;
	}

	const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS
				    ? WINHTTP_FLAG_SECURE
				    : 0;
	InternetHandle request(WinHttpOpenRequest(
		connection, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
	if (!request) {
		result.errorMessage = QString("Falha ao criar requisicao: %1")
					      .arg(windowsErrorMessage(GetLastError()));
		return result;
	}

	DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
	WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy,
			 sizeof(redirectPolicy));

	const wchar_t headers[] =
		L"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
		L"Accept-Language: pt-BR,pt;q=0.9,en;q=0.8\r\n";

	if (!WinHttpSendRequest(request, headers, static_cast<DWORD>(-1),
				nullptr, 0, 0, 0) ||
	    !WinHttpReceiveResponse(request, nullptr)) {
		result.errorMessage = QString("Erro de rede/TLS: %1")
					      .arg(windowsErrorMessage(GetLastError()));
		return result;
	}

	result.finalUrl = QUrl(queryFinalUrl(request, urlText));

	DWORD statusCode = 0;
	DWORD statusSize = sizeof(statusCode);
	WinHttpQueryHeaders(request,
			 WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			 WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
			 WINHTTP_NO_HEADER_INDEX);
	if (statusCode >= 400) {
		result.errorMessage = QString("HTTP %1 ao verificar live.").arg(statusCode);
		return result;
	}

	for (;;) {
		DWORD available = 0;
		if (!WinHttpQueryDataAvailable(request, &available)) {
			result.errorMessage = QString("Erro lendo resposta: %1")
					      .arg(windowsErrorMessage(GetLastError()));
			return result;
		}
		if (!available)
			break;

		const qsizetype start = result.body.size();
		result.body.resize(start + available);

		DWORD read = 0;
		if (!WinHttpReadData(request, result.body.data() + start, available,
				     &read)) {
			result.errorMessage = QString("Erro lendo dados: %1")
					      .arg(windowsErrorMessage(GetLastError()));
			return result;
		}
		result.body.resize(start + read);
	}

	return result;
}
#endif

QString obsConfigPath(const char *name)
{
	char *path = obs_module_config_path(name);
	if (!path)
		return {};

	QString result = QString::fromUtf8(path);
	bfree(path);
	return result;
}

void styleToggleButton(QPushButton *button)
{
	button->setFixedSize(18, 18);
	button->setToolTip("Mostrar controles");
	button->setFlat(true);
	button->setStyleSheet(
		"QPushButton {"
		"background: rgba(255, 255, 255, 0.08);"
		"border: 0;"
		"border-radius: 3px;"
		"color: rgba(255, 255, 255, 0.75);"
		"font-size: 10px;"
		"padding: 0;"
		"}"
		"QPushButton:hover {"
		"background: rgba(255, 255, 255, 0.18);"
		"color: white;"
		"}");
}

} // namespace

ChatDock::ChatDock(QCef *cef, QWidget *parent) : QWidget(parent)
{
	setObjectName(DOCK_ID);
	buildUi(cef);
	connectSignals();

	pollTimer.setInterval(POLL_INTERVAL_MS);

	loadSettings();
	showWaitingMessage("Cole o link de um canal do YouTube para iniciar.");

	if (!channelEdit->text().trimmed().isEmpty())
		QTimer::singleShot(0, this, &ChatDock::checkForLive);

	pollTimer.start();
}

ChatDock::~ChatDock()
{
	shutdown();
	saveSettings();
}

void ChatDock::shutdown()
{
	if (isShuttingDown)
		return;

	isShuttingDown = true;
	pollTimer.stop();
	requestInFlight = false;

#ifdef _WIN32
	if (liveCheckThread.joinable())
		liveCheckThread.join();
#endif

	if (activeReply) {
		disconnect(activeReply, nullptr, this, nullptr);
		activeReply->abort();
		activeReply->deleteLater();
		activeReply = nullptr;
	}

	browser = nullptr;
}

void ChatDock::buildUi(QCef *cef)
{
	channelEdit = new QLineEdit(this);
	channelEdit->setPlaceholderText("https://www.youtube.com/@canal");

	toggleControlsButton = new QPushButton("v", this);
	styleToggleButton(toggleControlsButton);
	toggleControlsButton->hide();

	saveButton = new QPushButton("Salvar", this);
	checkButton = new QPushButton("Checar", this);

	statusLabel = new QLabel(this);
	statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

	if (cef) {
		browser = cef->create_widget(this, "about:blank", nullptr);
		if (browser)
			browser->allowAllPopups(true);
	}

	if (!browser) {
		fallbackLabel = new QLabel(
			"obs-browser nao esta disponivel para criar o dock.", this);
		fallbackLabel->setWordWrap(true);
	}

	controlsWidget = new QWidget(this);
	auto *controlsLayout = new QHBoxLayout(controlsWidget);
	controlsLayout->setContentsMargins(0, 0, 0, 0);
	controlsLayout->addWidget(new QLabel("Canal:", controlsWidget));
	controlsLayout->addWidget(channelEdit, 1);
	controlsLayout->addWidget(saveButton);
	controlsLayout->addWidget(checkButton);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(6);
	layout->addWidget(toggleControlsButton, 0, Qt::AlignLeft);
	layout->addWidget(controlsWidget);
	layout->addWidget(statusLabel);

	if (browser)
		layout->addWidget(browser, 1);
	else
		layout->addWidget(fallbackLabel, 1);
}

void ChatDock::connectSignals()
{
	connect(&pollTimer, &QTimer::timeout, this, &ChatDock::checkForLive);
	connect(saveButton, &QPushButton::clicked, this,
		&ChatDock::saveAndCheck);
	connect(checkButton, &QPushButton::clicked, this,
		&ChatDock::manualCheck);
	connect(toggleControlsButton, &QPushButton::clicked, this,
		&ChatDock::showControls);
	connect(channelEdit, &QLineEdit::returnPressed, this,
		&ChatDock::saveAndCheck);
}

void ChatDock::saveAndCheck()
{
	saveSettings();
	currentVideoId.clear();
	lastLoadedVideoId.clear();
	missesAfterLive = 0;
	checkForLive();
}

void ChatDock::showControls()
{
	setControlsCollapsed(false);
}

void ChatDock::manualCheck()
{
	lastLoadedVideoId.clear();
	checkForLive();
}

void ChatDock::checkForLive()
{
	if (isShuttingDown)
		return;

	if (requestInFlight)
		return;

	const QString input = channelEdit->text().trimmed();
	if (input.isEmpty()) {
		setStatus("Aguardando link do canal.");
		return;
	}

	const QUrl normalized(normalizeInputUrl(input));
	const QString directVideoId = videoIdFromUrl(normalized);
	if (!directVideoId.isEmpty()) {
		loadChat(directVideoId);
		return;
	}

	const QString liveUrl = channelLiveUrlFromInput(input);
	if (liveUrl.isEmpty()) {
		setStatus("Link invalido. Use um canal do YouTube ou uma live.");
		return;
	}

	requestInFlight = true;
	setStatus(QString("Verificando live: %1").arg(liveUrl));

#ifdef _WIN32
	if (liveCheckThread.joinable())
		liveCheckThread.join();

	QPointer<ChatDock> self(this);
	liveCheckThread = std::thread([self, liveUrl]() {
		LiveFetchResult result = fetchLivePageWinHttp(liveUrl);
		if (!self)
			return;

		QMetaObject::invokeMethod(
			self,
			[self, result]() {
				if (!self || self->isShuttingDown)
					return;
				self->requestInFlight = false;
				self->handleLiveResponse(result.finalUrl, result.body,
							 result.errorMessage);
			},
			Qt::QueuedConnection);
	});
#else

	QNetworkRequest request{QUrl(liveUrl)};
	request.setHeader(QNetworkRequest::UserAgentHeader,
			  "Mozilla/5.0 ChatDock/0.1 OBS-Plugin");
	request.setRawHeader("Accept-Language", "pt-BR,pt;q=0.9,en;q=0.8");
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
			     QNetworkRequest::NoLessSafeRedirectPolicy);

	QNetworkReply *reply = network.get(request);
	activeReply = reply;
	connect(reply, &QNetworkReply::finished, this,
		[this, reply]() { handleLiveReply(reply); });
#endif
}

void ChatDock::loadSettings()
{
	const QString settingsPath = obsConfigPath("settings.ini");
	if (settingsPath.isEmpty())
		return;

	QSettings settings(settingsPath, QSettings::IniFormat);
	const QString channelUrl = settings.value("chat/channelUrl").toString();
	if (!channelUrl.isEmpty()) {
		channelEdit->setText(channelUrl);
		return;
	}

	channelEdit->setText(settings.value("youtube/channelUrl").toString());
}

void ChatDock::saveSettings()
{
	const QString settingsPath = obsConfigPath("settings.ini");
	if (settingsPath.isEmpty())
		return;

	QFileInfo info(settingsPath);
	if (!info.absolutePath().isEmpty())
		os_mkdirs(info.absolutePath().toUtf8().constData());

	QSettings settings(settingsPath, QSettings::IniFormat);
	settings.setValue("chat/channelUrl", channelEdit->text().trimmed());
}

void ChatDock::handleLiveReply(QNetworkReply *reply)
{
	if (isShuttingDown)
		return;

	requestInFlight = false;
	if (activeReply == reply)
		activeReply = nullptr;
	reply->deleteLater();

	handleLiveResponse(reply->url(), reply->readAll(),
			 reply->error() == QNetworkReply::NoError ? QString()
							       : reply->errorString());
}

void ChatDock::handleLiveResponse(const QUrl &finalUrl, const QByteArray &body,
					 const QString &errorMessage)
{
	if (!errorMessage.isEmpty()) {
		blog(LOG_WARNING, "[chat-dock] live check failed: %s",
		     errorMessage.toUtf8().constData());
		setStatus(QString("Erro ao verificar: %1").arg(errorMessage));
		return;
	}

	QString videoId = videoIdFromUrl(finalUrl);

	if (videoId.isEmpty())
		videoId = firstLiveVideoIdNearSignal(QString::fromUtf8(body));

	if (!videoId.isEmpty()) {
		missesAfterLive = 0;
		loadChat(videoId);
		return;
	}

	if (!currentVideoId.isEmpty()) {
		++missesAfterLive;
		setStatus(QString("Live nao detectada nesta verificacao (%1/3).")
				  .arg(missesAfterLive));

		if (missesAfterLive >= 3) {
			currentVideoId.clear();
			lastLoadedVideoId.clear();
			currentChatUrl = QUrl();
			setControlsCollapsed(false);
			showWaitingMessage("Nenhuma live ativa detectada agora.");
		}

		return;
	}

	showWaitingMessage("Nenhuma live ativa detectada agora.");
}

void ChatDock::loadChat(const QString &videoId)
{
	currentVideoId = videoId;
	missesAfterLive = 0;

	if (lastLoadedVideoId == videoId) {
		setStatus(QString("Live ativa: %1").arg(videoId));
		return;
	}

	lastLoadedVideoId = videoId;

	QUrl chatUrl("https://www.youtube.com/live_chat");
	QUrlQuery query;
	query.addQueryItem("v", videoId);
	query.addQueryItem("is_popout", "1");
	chatUrl.setQuery(query);

	setStatus(QString("Live encontrada. Carregando chat: %1").arg(videoId));
	currentChatUrl = chatUrl;
	setControlsCollapsed(true);

	if (browser)
		browser->setURL(chatUrl.toString().toStdString());
}

void ChatDock::showWaitingMessage(const QString &message)
{
	setStatus(message);
	if (browser && currentChatUrl.isEmpty())
		browser->setURL("about:blank");
}

void ChatDock::setStatus(const QString &message)
{
	statusLabel->setText(message);
}

void ChatDock::setControlsCollapsed(bool collapsed)
{
	controlsWidget->setVisible(!collapsed);
	toggleControlsButton->setVisible(collapsed);
	statusLabel->setVisible(!collapsed);
}

} // namespace chatdock
