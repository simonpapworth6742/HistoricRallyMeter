#include "qr_display.h"
#include "rally_web_server.h"
#include "rally_types.h"
#include "rally_state.h"

#include <dlfcn.h>
#include <algorithm>
#include <cairo/cairo.h>

namespace {

struct QRcode {
    int version;
    int width;
    unsigned char* data;
};

using QRcode_encodeString_fn = QRcode* (*)(const char*, int, int, int, int);
using QRcode_free_fn = void (*)(QRcode*);

struct QrLib {
    void* handle = nullptr;
    QRcode_encodeString_fn encode = nullptr;
    QRcode_free_fn free_fn = nullptr;

    QrLib() {
        handle = dlopen("libqrencode.so.4", RTLD_LAZY);
        if (!handle) handle = dlopen("libqrencode.so", RTLD_LAZY);
        if (!handle) return;
        encode = reinterpret_cast<QRcode_encodeString_fn>(dlsym(handle, "QRcode_encodeString"));
        free_fn = reinterpret_cast<QRcode_free_fn>(dlsym(handle, "QRcode_free"));
        if (!encode || !free_fn) {
            dlclose(handle);
            handle = nullptr;
        }
    }

    ~QrLib() {
        if (handle) dlclose(handle);
    }
};

} // namespace

gboolean on_qr_draw(GtkWidget* widget, cairo_t* cr, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    if (!data || !data->webServer || !data->state->web_enabled) return FALSE;

    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    double w = alloc.width;
    double h = alloc.height;

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    std::string url = data->webServer->getWebUrl();
    static QrLib qrlib;
    if (!qrlib.encode) {
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12);
        cairo_move_to(cr, 4, h / 2);
        cairo_show_text(cr, "QR unavailable");
        return FALSE;
    }

    QRcode* code = qrlib.encode(url.c_str(), 0, 0, 2, 1);
    if (!code) return FALSE;

    int modules = code->width;
    double margin = 4;
    double scale = std::min((w - 2 * margin) / modules, (h - 2 * margin) / modules);
    double ox = (w - modules * scale) / 2;
    double oy = (h - modules * scale) / 2;

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    for (int y = 0; y < modules; y++) {
        for (int x = 0; x < modules; x++) {
            if (code->data[y * modules + x] & 1) {
                cairo_rectangle(cr, ox + x * scale, oy + y * scale, scale, scale);
            }
        }
    }
    cairo_fill(cr);
    qrlib.free_fn(code);
    return FALSE;
}
