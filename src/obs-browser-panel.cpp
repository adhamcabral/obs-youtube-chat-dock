#include "obs-browser-panel.hpp"

#include <obs-module.h>
#include <util/platform.h>

#ifdef __linux__
#include <obs-nix-platform.h>
#endif

namespace chatdock {

QCef *createObsBrowserCef()
{
#ifdef __linux__
	if (obs_get_nix_platform() == OBS_NIX_PLATFORM_WAYLAND) {
		blog(LOG_WARNING,
		     "[chat-dock] browser docks require X11/XWayland on Linux");
		return nullptr;
	}
#endif

	obs_module_t *browserModule = obs_get_module("obs-browser");
	if (!browserModule)
		return nullptr;

	void *browserLib = obs_get_module_lib(browserModule);
	if (!browserLib)
		return nullptr;

	auto *create = reinterpret_cast<QCef *(*)()>(
		os_dlsym(browserLib, "obs_browser_create_qcef"));
	if (!create)
		return nullptr;

	return create();
}

} // namespace chatdock
