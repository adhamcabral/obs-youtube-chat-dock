#pragma once

#include "obs-browser-panel.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QWidget>

#ifdef _WIN32
#include <thread>
#endif

namespace chatdock {

class ChatDock : public QWidget {
	Q_OBJECT

public:
	explicit ChatDock(QCef *cef, QWidget *parent = nullptr);
	~ChatDock() override;
	void shutdown();

private slots:
	void saveAndCheck();
	void showControls();
	void manualCheck();
	void checkForLive();

private:
	void buildUi(QCef *cef);
	void connectSignals();
	void loadSettings();
	void saveSettings();
	void handleLiveReply(QNetworkReply *reply);
	void handleLiveResponse(const QUrl &finalUrl, const QByteArray &body,
				const QString &errorMessage);
	void loadChat(const QString &videoId);
	void showWaitingMessage(const QString &message);
	void setStatus(const QString &message);
	void setControlsCollapsed(bool collapsed);

	QWidget *controlsWidget = nullptr;
	QLineEdit *channelEdit = nullptr;
	QPushButton *toggleControlsButton = nullptr;
	QPushButton *saveButton = nullptr;
	QPushButton *checkButton = nullptr;
	QLabel *statusLabel = nullptr;
	QLabel *fallbackLabel = nullptr;
	QCefWidget *browser = nullptr;
	QNetworkAccessManager network;
	QPointer<QNetworkReply> activeReply;
	QTimer pollTimer;
#ifdef _WIN32
	std::thread liveCheckThread;
#endif
	bool isShuttingDown = false;
	bool requestInFlight = false;
	QString currentVideoId;
	QString lastLoadedVideoId;
	QUrl currentChatUrl;
	int missesAfterLive = 0;
};

} // namespace chatdock
