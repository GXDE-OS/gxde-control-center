#include "waylandblurhelper.h"
#include "waylandhelper.h"

#include <QWindow>
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#include <wayland-client.h>

#include "protocols/blur-client-protocol.h"

namespace Wayland {

struct BlurBindContext {
    org_kde_kwin_blur_manager *blurManager = nullptr;
};

static void registryGlobal(void *data, wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {
    auto *ctx = static_cast<BlurBindContext *>(data);
    if (strcmp(interface, org_kde_kwin_blur_manager_interface.name) == 0) {
        ctx->blurManager = static_cast<org_kde_kwin_blur_manager *>(
            wl_registry_bind(registry, name, &org_kde_kwin_blur_manager_interface, 1));
    }
}

static void registryGlobalRemove(void *, wl_registry *, uint32_t) {}

static const wl_registry_listener registryListener = {
    registryGlobal,
    registryGlobalRemove,
};

bool BlurHelper::isWayland() {
    return isWaylandSession();
}

void BlurHelper::applyBlur(QWindow *window) {
    if (!window || !isWayland()) {
        return;
    }

    auto *native = QGuiApplication::platformNativeInterface();
    if (!native) {
        return;
    }

    auto *display = static_cast<wl_display *>(
        native->nativeResourceForWindow("display", nullptr));
    auto *surface = static_cast<wl_surface *>(
        native->nativeResourceForWindow("surface", window));

    if (!display || !surface) {
        return;
    }

    wl_registry *registry = wl_display_get_registry(display);
    BlurBindContext ctx;
    wl_registry_add_listener(registry, &registryListener, &ctx);
    wl_display_roundtrip(display);
    wl_registry_destroy(registry);

    if (ctx.blurManager) {
        org_kde_kwin_blur *blur =
            org_kde_kwin_blur_manager_create(ctx.blurManager, surface);
        org_kde_kwin_blur_set_region(blur, nullptr);
        org_kde_kwin_blur_commit(blur);
        wl_display_flush(display);
        org_kde_kwin_blur_destroy(blur);

        org_kde_kwin_blur_manager_destroy(ctx.blurManager);
    }
}

}
