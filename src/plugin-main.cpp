#include "obs-browser-panel.hpp"
#include "chat-dock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QCoreApplication>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Adham")
OBS_MODULE_USE_DEFAULT_LOCALE("chat-dock", "en-US")

namespace chatdock {

constexpr auto *DOCK_ID = "chat-dock";
constexpr auto *DOCK_TITLE = "Chat";

bool shuttingDown = false;
QCef *cef = nullptr;
ChatDock *dock = nullptr;

void destroyDock(bool removeDock)
{
	if (!dock)
		return;

	ChatDock *oldDock = dock;
	dock = nullptr;

	oldDock->shutdown();
	if (removeDock)
		obs_frontend_remove_dock(DOCK_ID);
	delete oldDock;
}

void handleFrontendEvent(enum obs_frontend_event event, void *)
{
	if (event != OBS_FRONTEND_EVENT_EXIT)
		return;

	shuttingDown = true;
	if (dock)
		dock->shutdown();
}

} // namespace chatdock

bool obs_module_load(void)
{
	blog(LOG_INFO, "[chat-dock] loading");

	chatdock::cef = chatdock::createObsBrowserCef();
	if (!chatdock::cef)
		blog(LOG_WARNING,
		     "[chat-dock] obs-browser panel API is unavailable");

	chatdock::dock = new chatdock::ChatDock(
		chatdock::cef,
		reinterpret_cast<QWidget *>(obs_frontend_get_main_window()));

	const bool added = obs_frontend_add_dock_by_id(
		chatdock::DOCK_ID, chatdock::DOCK_TITLE, chatdock::dock);
	if (!added) {
		blog(LOG_WARNING, "[chat-dock] failed to add dock");
		delete chatdock::dock;
		chatdock::dock = nullptr;
		return false;
	}

	QObject::connect(chatdock::dock, &QObject::destroyed,
			 []() { chatdock::dock = nullptr; });

	QObject::connect(qApp, &QCoreApplication::aboutToQuit,
			 []() { chatdock::shuttingDown = true; });
	obs_frontend_add_event_callback(chatdock::handleFrontendEvent, nullptr);

	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[chat-dock] unloading");

	obs_frontend_remove_event_callback(chatdock::handleFrontendEvent, nullptr);
	chatdock::destroyDock(!chatdock::shuttingDown);

	delete chatdock::cef;
	chatdock::cef = nullptr;
}
