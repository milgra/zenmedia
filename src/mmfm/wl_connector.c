#ifndef wl_connector_h
#define wl_connector_h

#include "wm_event.c"
#include "zc_bm_argb.c"
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "zc_bm_argb.c"
#include "zc_draw.c"
#include "zc_log.c"
#include "zc_memory.c"
#include "zc_time.c"

#define MAX_MONITOR_NAME_LEN 255

enum wl_event_id_t
{
    WL_EVENT_OUTPUT_ADDED,
    WL_EVENT_OUTPUT_REMOVED,
    WL_EVENT_OUTPUT_CHANGED
};

typedef struct _wl_event_t wl_event_t;
struct _wl_event_t
{
    enum wl_event_id_t   id;
    struct monitor_info* monitors[16];
};

struct monitor_info
{
    int32_t physical_width;
    int32_t physical_height;
    int32_t logical_width;
    int32_t logical_height;
    double  scale;
    double  ratio;
    int     index;
    char    name[MAX_MONITOR_NAME_LEN + 1];

    enum wl_output_subpixel subpixel;
    struct zxdg_output_v1*  xdg_output;
    struct wl_output*       wl_output;
};

void wl_connector_init(
    int w,
    int h,
    int m,
    void (*init)(int w, int h, float scale),
    /* void (*event)(wl_event_t event), */
    void (*update)(ev_t),
    void (*render)(uint32_t time, uint32_t index, bm_argb_t* bm),
    void (*destroy)());

void wl_connector_draw();
void wl_hide();

void wl_connector_create_layer();
void wl_connector_create_window();
void wl_connector_create_eglwindow();

#endif

#if __INCLUDE_LEVEL__ == 0

struct keyboard_info
{
    struct wl_keyboard* kbd;
    struct xkb_context* xkb_context;
    struct xkb_keymap*  xkb_keymap;
    struct xkb_state*   xkb_state;
    bool                control;
    bool                shift;
};
struct wlc_t
{
    struct wl_display* display; // global display object

    struct wl_compositor* compositor; // active compositor
    struct wl_pointer*    pointer;    // active pointer for seat
    struct wl_seat*       seat;       // active seat

    struct wl_surface* surface;  // surface for window
    struct wl_buffer*  buffer;   // buffer for surface
    struct wl_shm*     shm;      // active shared memory buffer
    void*              shm_data; // active bufferdata
    int                shm_size;

    struct zxdg_output_manager_v1* xdg_output_manager; // active xdg output manager
    struct zwlr_layer_shell_v1*    layer_shell;        // active layer shell
    struct zwlr_layer_surface_v1*  layer_surface;      // active layer surface

    // outputs

    int                  monitor_count;
    struct monitor_info* monitors[16];
    struct monitor_info* monitor;
    struct keyboard_info keyboard;

    // window state

    bm_argb_t bitmap;

    int  win_width;
    int  win_height;
    int  win_margin;
    bool running;
    int  visible;

    void (*init)(int w, int h, float scale);
    void (*update)(ev_t);
    void (*render)(uint32_t time, uint32_t index, bm_argb_t* bm);
    void (*destroy)();

    struct xdg_surface*  xdg_surface;
    struct xdg_wm_base*  xdg_wm_base;
    struct xdg_toplevel* xdg_toplevel;

} wlc = {0};

void wl_connector_create_buffer();

/* ***WL OUTPUT EVENTS*** */

static void wl_connector_wl_output_handle_geometry(
    void*             data,
    struct wl_output* wl_output,
    int32_t           x,
    int32_t           y,
    int32_t           width_mm,
    int32_t           height_mm,
    int32_t           subpixel,
    const char*       make,
    const char*       model,
    int32_t           transform)
{
    struct monitor_info* monitor = data;

    zc_log_debug(
	"wl output handle geometry x %i y %i width_mm %i height_mm %i subpixel %i make %s model %s transform %i for monitor %i",
	x,
	y,
	width_mm,
	height_mm,
	subpixel,
	make,
	model,
	transform,
	monitor->index);

    monitor->subpixel = subpixel;
}

int32_t round_to_int(double val)
{
    return (int32_t) (val + 0.5);
}
static void wl_connector_wl_output_handle_mode(
    void*             data,
    struct wl_output* wl_output,
    uint32_t          flags,
    int32_t           width,
    int32_t           height,
    int32_t           refresh)
{
    struct monitor_info* monitor = data;

    zc_log_debug(
	"wl output handle mode flags %u width %i height %i for monitor %i",
	flags,
	width,
	height,
	monitor->index);

    monitor->physical_width  = width;
    monitor->physical_height = height;
}

static void wl_connector_wl_output_handle_done(void* data, struct wl_output* wl_output)
{
    struct monitor_info* monitor = data;

    zc_log_debug("wl output handle done for monitor %i", monitor->index);
}

static void wl_connector_wl_output_handle_scale(void* data, struct wl_output* wl_output, int32_t factor)
{
    struct monitor_info* monitor = data;

    zc_log_debug("wl output handle scale %i for monitor %i", factor, monitor->index);

    monitor->scale = factor;
}

struct wl_output_listener wl_output_listener = {
    .geometry = wl_connector_wl_output_handle_geometry,
    .mode     = wl_connector_wl_output_handle_mode,
    .done     = wl_connector_wl_output_handle_done,
    .scale    = wl_connector_wl_output_handle_scale,
};

/* ***XDG OUTPUT EVENTS*** */

static void wl_connector_xdg_output_handle_logical_position(void* data, struct zxdg_output_v1* xdg_output, int32_t x, int32_t y)
{
    struct monitor_info* monitor = data;
    zc_log_debug("xdg output handle logical position, %i %i for monitor %i", x, y, monitor->index);
}

static void wl_connector_xdg_output_handle_logical_size(void* data, struct zxdg_output_v1* xdg_output, int32_t width, int32_t height)
{
    struct monitor_info* monitor = data;
    zc_log_debug("xdg output handle logical size, %i %i for monitor %i", width, height, monitor->index);

    monitor->logical_width  = width;
    monitor->logical_height = height;
}

static void wl_connector_xdg_output_handle_done(void* data, struct zxdg_output_v1* xdg_output)
{
    struct monitor_info* monitor = data;
    zc_log_debug("xdg output handle done, for monitor %i", monitor->index);
}

static void wl_connector_xdg_output_handle_name(void* data, struct zxdg_output_v1* xdg_output, const char* name)
{
    struct monitor_info* monitor = data;
    strncpy(monitor->name, name, MAX_MONITOR_NAME_LEN);

    zc_log_debug("xdg output handle name, %s for monitor %i", name, monitor->index);
}

static void wl_connector_xdg_output_handle_description(void* data, struct zxdg_output_v1* xdg_output, const char* description)
{
    struct monitor_info* monitor = data;
    zc_log_debug("xdg output handle description for monitor %i", description, monitor->index);
}

struct zxdg_output_v1_listener xdg_output_listener = {
    .logical_position = wl_connector_xdg_output_handle_logical_position,
    .logical_size     = wl_connector_xdg_output_handle_logical_size,
    .done             = wl_connector_xdg_output_handle_done,
    .name             = wl_connector_xdg_output_handle_name,
    .description      = wl_connector_xdg_output_handle_description,
};

/* *** POINTER EVENTS *** */

// TODO differentiate these by wl_pointer address
int px   = 0;
int py   = 0;
int drag = 0;

void wl_connector_pointer_handle_enter(void* data, struct wl_pointer* wl_pointer, uint serial, struct wl_surface* surface, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    zc_log_debug("pointer handle enter");
}
void wl_connector_pointer_handle_leave(void* data, struct wl_pointer* wl_pointer, uint serial, struct wl_surface* surface)
{
    zc_log_debug("pointer handle leave");
}
void wl_connector_pointer_handle_motion(void* data, struct wl_pointer* wl_pointer, uint time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    /* zc_log_debug("pointer handle motion %f %f", wl_fixed_to_double(surface_x), wl_fixed_to_double(surface_y)); */

    ev_t event = {
	.type = EV_MMOVE,
	.drag = drag,
	.x    = (int) wl_fixed_to_double(surface_x) * wlc.monitor->scale,
	.y    = (int) wl_fixed_to_double(surface_y) * wlc.monitor->scale};

    px = event.x;
    py = event.y;

    if (drag) (*wlc.update)(event);
}

uint lasttouch = 0;
void wl_connector_pointer_handle_button(void* data, struct wl_pointer* wl_pointer, uint serial, uint time, uint button, uint state)
{
    zc_log_debug("pointer handle button %u state %u time %u", button, state, time);

    ev_t event = {.x = px, .y = py, .button = button == 272 ? 1 : 3};

    if (state)
    {
	uint delay = time - lasttouch;
	lasttouch  = time;

	event.dclick = delay < 300;
	event.type   = EV_MDOWN;
	drag         = 1;
    }
    else
    {
	event.type = EV_MUP;
	drag       = 0;
    }

    (*wlc.update)(event);
}
void wl_connector_pointer_handle_axis(void* data, struct wl_pointer* wl_pointer, uint time, uint axis, wl_fixed_t value)
{
    /* zc_log_debug("pointer handle axis %u %i", axis, value); */
    ev_t event =
	{.type = EV_SCROLL,
	 .x    = px,
	 .y    = py,
	 .dx   = axis == 1 ? (float) value / 100.0 : 0,
	 .dy   = axis == 0 ? (float) -value / 100.0 : 0};

    (*wlc.update)(event);
}
void wl_connector_pointer_handle_frame(void* data, struct wl_pointer* wl_pointer)
{
    zc_log_debug("pointer handle frame");
}
void wl_connector_pointer_handle_axis_source(void* data, struct wl_pointer* wl_pointer, uint axis_source)
{
    zc_log_debug("pointer handle axis source");
}
void wl_connector_pointer_handle_axis_stop(void* data, struct wl_pointer* wl_pointer, uint time, uint axis)
{
    zc_log_debug("pointer handle axis stop");
}
void wl_connector_pointer_handle_axis_discrete(void* data, struct wl_pointer* wl_pointer, uint axis, int discrete)
{
    zc_log_debug("pointer handle axis discrete");
}

struct wl_pointer_listener pointer_listener =
    {
	.enter         = wl_connector_pointer_handle_enter,
	.leave         = wl_connector_pointer_handle_leave,
	.motion        = wl_connector_pointer_handle_motion,
	.button        = wl_connector_pointer_handle_button,
	.axis          = wl_connector_pointer_handle_axis,
	.frame         = wl_connector_pointer_handle_frame,
	.axis_source   = wl_connector_pointer_handle_axis_source,
	.axis_stop     = wl_connector_pointer_handle_axis_stop,
	.axis_discrete = wl_connector_pointer_handle_axis_discrete,
};

/* frame events */

static const struct wl_callback_listener wl_surface_frame_listener;

static uint32_t lasttime  = 0;
static uint32_t lastcount = 0;

static void wl_surface_frame_done(void* data, struct wl_callback* cb, uint32_t time)
{
    /* zc_log_debug("*************************wl surface frame done %u", time); */
    /* Destroy this callback */
    wl_callback_destroy(cb);

    /* Request another frame */
    cb = wl_surface_frame(wlc.surface);
    wl_callback_add_listener(cb, &wl_surface_frame_listener, NULL);

    if (time - lasttime > 1000)
    {
	printf("FRAME PER SEC : %u\n", lastcount);
	lastcount = 0;
	lasttime  = time;
    }

    lastcount++;

    ev_t event = {
	.type = EV_TIME,
	.time = time};

    // TODO refresh rate can differ so animations should be time based
    (*wlc.update)(event);

    /* wl_connector_draw(); */
    /* Submit a frame for this event */

    /* wl_surface_attach(wlc.surface, buffer, 0, 0); */
    /* wl_surface_damage_buffer(wlc.surface, 0, 0, INT32_MAX, INT32_MAX); */
    /* wl_surface_commit(wlc.surface); */
}

static const struct wl_callback_listener wl_surface_frame_listener = {
    .done = wl_surface_frame_done,
};

static void xdg_wm_base_ping(void* data, struct xdg_wm_base* xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

/* xdg surface events */

static void xdg_surface_configure(void* data, struct xdg_surface* xdg_surface, uint32_t serial)
{
    zc_log_debug("xdg surface configure");

    xdg_surface_ack_configure(xdg_surface, serial);

    if (wlc.buffer == NULL)
    {
	/* struct wl_buffer* buffer = draw_frame(state); */

	double  ratio  = wlc.monitor->scale;
	int32_t width  = round_to_int(wlc.win_width * ratio);
	int32_t height = round_to_int(wlc.win_height * ratio);

	wlc.win_width      = width;
	wlc.win_height     = height;
	wlc.monitor->ratio = ratio;

	wl_connector_create_buffer(width, height);

	wl_surface_attach(wlc.surface, wlc.buffer, 0, 0);
	wl_surface_commit(wlc.surface);

	// init
	(*wlc.init)(width, height, ratio);

	ev_t event = {.type = EV_WINDOW_SHOW};
	(*wlc.update)(event);
    }
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

/* xdg toplevel events */

void xdg_toplevel_configure(void* data, struct xdg_toplevel* xdg_toplevel, int32_t width, int32_t height, struct wl_array* states)
{
    zc_log_debug("xdg toplevel configure w %i h %i", width, height);

    if (width > 0 && height > 0 && width != wlc.win_width && height != wlc.win_height)
    {
	zc_log_debug("RESIZE %i %i", width, height);
	double  ratio   = wlc.monitor->scale;
	int32_t nwidth  = round_to_int(width * ratio);
	int32_t nheight = round_to_int(height * ratio);

	wlc.win_width      = nwidth;
	wlc.win_height     = nheight;
	wlc.monitor->ratio = ratio;

	wl_connector_create_buffer(nwidth, nheight);

	wl_surface_attach(wlc.surface, wlc.buffer, 0, 0);
	wl_surface_commit(wlc.surface);

	ev_t event = {
	    .type = EV_RESIZE,
	    .w    = nwidth,
	    .h    = nheight};

	(*wlc.update)(event);
    }
}

void xdg_toplevel_close(void* data, struct xdg_toplevel* xdg_toplevel)
{
    zc_log_debug("xdg toplevel close");
}

void xdg_toplevel_configure_bounds(void* data, struct xdg_toplevel* xdg_toplevel, int32_t width, int32_t height)
{
    zc_log_debug("xdg toplevel configure bounds w %i h %i", width, height);
}

void xdg_toplevel_wm_capabilities(void* data, struct xdg_toplevel* xdg_toplevel, struct wl_array* capabilities)
{
    zc_log_debug("xdg toplevel wm capabilities");
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure        = xdg_toplevel_configure,
    .close            = xdg_toplevel_close,
    .configure_bounds = xdg_toplevel_configure_bounds,
    .wm_capabilities  = xdg_toplevel_wm_capabilities};

static void keyboard_keymap(void* data, struct wl_keyboard* wl_keyboard, uint32_t format, int32_t fd, uint32_t size)
{
    zc_log_debug("keyboard keymap");

    wlc.keyboard.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
    {
	close(fd);
	exit(1);
    }
    char* map_shm = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map_shm == MAP_FAILED)
    {
	close(fd);
	exit(1);
    }
    wlc.keyboard.xkb_keymap = xkb_keymap_new_from_string(wlc.keyboard.xkb_context, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
    munmap(map_shm, size);
    close(fd);

    wlc.keyboard.xkb_state = xkb_state_new(wlc.keyboard.xkb_keymap);
}

static void keyboard_enter(void* data, struct wl_keyboard* wl_keyboard, uint32_t serial, struct wl_surface* surface, struct wl_array* keys)
{
    zc_log_debug("keyboard enter");
}

static void keyboard_leave(void* data, struct wl_keyboard* wl_keyboard, uint32_t serial, struct wl_surface* surface)
{
    zc_log_debug("keyboard leave");
}

static void keyboard_key(void* data, struct wl_keyboard* wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t _key_state)
{
    enum wl_keyboard_key_state key_state = _key_state;
    xkb_keysym_t               sym       = xkb_state_key_get_one_sym(wlc.keyboard.xkb_state, key + 8);

    /* if (state != WL_KEYBOARD_KEY_STATE_PRESSED) return; */
    /* switch (xkb_keysym_to_lower(sym)) */
    /* XKB_KEY_a */
    /* witch (sym) { */
    /* 	case XKB_KEY_KP_Enter: /\* fallthrough *\/ */
    /* 	case XKB_KEY_Return: */
    char buf[8];
    if (xkb_keysym_to_utf8(sym, buf, 8))
    {
    }

    zc_log_debug("keyboard key %i %s", key, buf);

    /* wlc.on_keyevent(panel, key_state, sym, panel->keyboard.control, panel->keyboard.shift); */
}

static void keyboard_repeat_info(void* data, struct wl_keyboard* wl_keyboard, int32_t rate, int32_t delay)
{
    zc_log_debug("keyboard repeat info %i %i", rate, delay);
    /*    panel->repeat_delay = delay; */
    /*     if (rate > 0) */
    /* 	panel->repeat_period = 1000 / rate; */
    /*     else */
    /* 	panel->repeat_period = -1; */
}

static void keyboard_modifiers(void* data, struct wl_keyboard* keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    zc_log_debug("keyboard modifiers");
    xkb_state_update_mask(wlc.keyboard.xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
    wlc.keyboard.control = xkb_state_mod_name_is_active(wlc.keyboard.xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED);
    wlc.keyboard.shift   = xkb_state_mod_name_is_active(wlc.keyboard.xkb_state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED);
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap      = keyboard_keymap,
    .enter       = keyboard_enter,
    .leave       = keyboard_leave,
    .key         = keyboard_key,
    .modifiers   = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

/* ***SEAT EVENTS*** */

static void wl_connector_seat_handle_capabilities(void* data, struct wl_seat* wl_seat, enum wl_seat_capability caps)
{
    zc_log_debug("seat handle capabilities %i", caps);
    if (caps & WL_SEAT_CAPABILITY_KEYBOARD)
    {
	wlc.keyboard.kbd = wl_seat_get_keyboard(wl_seat);
	wl_keyboard_add_listener(wlc.keyboard.kbd, &keyboard_listener, NULL);
	zc_log_debug("added keyboard");
    }

    if (caps & WL_SEAT_CAPABILITY_POINTER)
    {
	wlc.pointer = wl_seat_get_pointer(wl_seat);
	wl_pointer_add_listener(wlc.pointer, &pointer_listener, NULL);
	zc_log_debug("added pointer");
    }
}

static void wl_connector_seat_handle_name(void* data, struct wl_seat* wl_seat, const char* name)
{
    zc_log_debug("seat handle name %s", name);
}

const struct wl_seat_listener seat_listener = {
    .capabilities = wl_connector_seat_handle_capabilities,
    .name         = wl_connector_seat_handle_name,
};

/* ***GLOBAL EVENTS*** */

static void wl_connector_handle_global(
    void*               data,
    struct wl_registry* registry,
    uint32_t            name,
    const char*         interface,
    uint32_t            version)
{
    zc_log_debug("handle global, interface : %s, version %u", interface, version);

    if (strcmp(interface, wl_compositor_interface.name) == 0)
    {
	wlc.compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	zc_log_debug("compositor stored");
    }
    else if (strcmp(interface, wl_seat_interface.name) == 0)
    {
	wlc.seat = wl_registry_bind(registry, name, &wl_seat_interface, 4);
	zc_log_debug("seat stored");
	wl_seat_add_listener(wlc.seat, &seat_listener, NULL);
	zc_log_debug("seat listener added");
    }
    else if (strcmp(interface, wl_shm_interface.name) == 0)
    {
	wlc.shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	zc_log_debug("shm stored");
    }
    else if (strcmp(interface, wl_output_interface.name) == 0)
    {
	if (wlc.monitor_count >= 16) return;

	struct monitor_info* monitor = malloc(sizeof(struct monitor_info));
	memset(monitor->name, 0, MAX_MONITOR_NAME_LEN);
	monitor->wl_output = wl_registry_bind(registry, name, &wl_output_interface, 2);
	monitor->index     = wlc.monitor_count;

	// get wl_output events
	wl_output_add_listener(monitor->wl_output, &wl_output_listener, monitor);
	zc_log_debug("wl_output listener added");

	// set up output if it comes after xdg_output_manager_init
	if (wlc.xdg_output_manager != NULL)
	{
	    monitor->xdg_output = zxdg_output_manager_v1_get_xdg_output(wlc.xdg_output_manager, monitor->wl_output);
	    zxdg_output_v1_add_listener(monitor->xdg_output, &xdg_output_listener, monitor);
	    zc_log_debug("xdg_output listener added");
	}

	wlc.monitors[wlc.monitor_count++] = monitor;
	zc_log_debug("output stored at index : %u", wlc.monitor_count - 1);
    }
    else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0)
    {
	wlc.layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
	zc_log_debug("layer shell stored");
    }
    else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0)
    {
	wlc.xdg_output_manager = wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, 2);
	zc_log_debug("xdg output manager stored");

	// set up outputs if event comes after interface setup
	for (int index = 0; index < wlc.monitor_count; index++)
	{
	    wlc.monitors[index]->xdg_output = zxdg_output_manager_v1_get_xdg_output(wlc.xdg_output_manager, wlc.monitors[index]->wl_output);
	    zxdg_output_v1_add_listener(wlc.monitors[index]->xdg_output, &xdg_output_listener, wlc.monitors[index]);
	    zc_log_debug("xdg_output listener added");
	}
    }
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
    {
	wlc.xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
	xdg_wm_base_add_listener(wlc.xdg_wm_base, &xdg_wm_base_listener, NULL);
	zc_log_debug("xdg_wm_base listener added");
    }
}

static void wl_connector_handle_global_remove(void* data, struct wl_registry* registry, uint32_t name)
{
    zc_log_debug("handle global remove");
}

static const struct wl_registry_listener registry_listener =
    {.global        = wl_connector_handle_global,
     .global_remove = wl_connector_handle_global_remove};

int wl_connector_shm_create()
{
    int  shmid = -1;
    char shm_name[NAME_MAX];
    for (int i = 0; i < UCHAR_MAX; ++i)
    {
	if (snprintf(shm_name, NAME_MAX, "/wcp-%d", i) >= NAME_MAX)
	{
	    break;
	}
	shmid = shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (shmid > 0 || errno != EEXIST)
	{
	    break;
	}
    }

    if (shmid < 0)
    {
	zc_log_debug("shm_open() failed: %s", strerror(errno));
	return -1;
    }

    if (shm_unlink(shm_name) != 0)
    {
	zc_log_debug("shm_unlink() failed: %s", strerror(errno));
	return -1;
    }

    return shmid;
}

void* wl_connector_shm_alloc(const int shmid, const size_t size)
{
    if (ftruncate(shmid, size) != 0)
    {
	zc_log_debug("ftruncate() failed: %s", strerror(errno));
	return NULL;
    }

    void* buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
    if (buffer == MAP_FAILED)
    {
	zc_log_debug("mmap() failed: %s", strerror(errno));
	return NULL;
    }

    return buffer;
}

static void wl_connector_buffer_release(void* data, struct wl_buffer* wl_buffer)
{
    /* zc_log_debug("buffer release"); */
}

static const struct wl_buffer_listener buffer_listener = {
    .release = wl_connector_buffer_release};

void wl_connector_create_buffer(int width, int height)
{
    zc_log_debug("create buffer %i %i", width, height);

    if (wlc.buffer)
    {
	// delete old buffer and bitmap
	wl_buffer_destroy(wlc.buffer);
	munmap(wlc.shm_data, wlc.shm_size);
	wlc.bitmap.data = NULL;
    }

    int stride = width * 4;
    int size   = stride * height;

    int fd = wl_connector_shm_create();
    if (fd < 0)
    {
	zc_log_error("creating a buffer file for %d B failed: %m", size);
	return;
    }

    wlc.shm_data = wl_connector_shm_alloc(fd, size);

    wlc.bitmap.w    = width;
    wlc.bitmap.h    = height;
    wlc.bitmap.size = size;
    wlc.bitmap.data = wlc.shm_data;

    if (wlc.shm_data == MAP_FAILED)
    {
	zc_log_error("mmap failed: %m");
	close(fd);
	return;
    }

    struct wl_shm_pool* pool   = wl_shm_create_pool(wlc.shm, fd, size);
    struct wl_buffer*   buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);

    wl_shm_pool_destroy(pool);

    wlc.buffer   = buffer;
    wlc.shm_size = size;

    wl_buffer_add_listener(buffer, &buffer_listener, NULL);
    zc_log_debug("buffer listener added");
}

void wl_connector_draw()
{
    if (wlc.visible)
    {
	memset(wlc.bitmap.data, 0, wlc.bitmap.size);
	/* gfx_rect(wlc.bitmap, 0, 0, wlc.win_width, wlc.win_height, 0xFF0000FF, 0); */

	(*wlc.render)(0, 0, &wlc.bitmap);

	/* zc_time(NULL); */
	/* for (int i = 0; i < wlc.bitmap->size; i += 4) */
	/* { */
	/*     argb[i]     = wlc.bitmap->data[i + 2]; */
	/*     argb[i + 1] = wlc.bitmap->data[i + 1]; */
	/*     argb[i + 2] = wlc.bitmap->data[i]; */
	/*     argb[i + 3] = wlc.bitmap->data[i + 3]; */
	/* } */
	/* zc_time("ARGB"); */

	wl_surface_attach(wlc.surface, wlc.buffer, 0, 0);
	/* zwlr_layer_surface_v1_set_keyboard_interactivity(wlc.layer_surface, true); */
	wl_surface_damage(wlc.surface, 0, 0, wlc.win_width, wlc.win_height);
	wl_surface_commit(wlc.surface);
    }
}

/* Layer surface listener */

static void wl_connector_layer_surface_configure(void* data, struct zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t width, uint32_t height)
{
    zc_log_debug("layer surface configure serial %u width %i height %i", serial, width, height);

    zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static void wl_connector_layer_surface_closed(void* _data, struct zwlr_layer_surface_v1* surface)
{
    zc_log_debug("layer surface configure");
}

struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = wl_connector_layer_surface_configure,
    .closed    = wl_connector_layer_surface_closed,
};

bool wl_parse_input(const char* input_buffer, unsigned long* state)
{
    char *input_ptr, *newline_position;

    newline_position = strchr(input_buffer, '\n');
    if (newline_position == NULL) { return false; }

    if (newline_position == input_buffer) { return false; }

    *state = strtoul(input_buffer, &input_ptr, 10);
    if (input_ptr == newline_position) { return true; }
    else
	return false;
}

void wl_show()
{
    if (!wlc.visible)
    {
	wlc.visible     = 1;
	wlc.surface     = wl_compositor_create_surface(wlc.compositor);
	wlc.xdg_surface = xdg_wm_base_get_xdg_surface(wlc.xdg_wm_base, wlc.surface);
	xdg_surface_add_listener(wlc.xdg_surface, &xdg_surface_listener, NULL);
	wlc.xdg_toplevel = xdg_surface_get_toplevel(wlc.xdg_surface);
	xdg_toplevel_add_listener(wlc.xdg_toplevel, &xdg_toplevel_listener, NULL);
	xdg_toplevel_set_title(wlc.xdg_toplevel, "TITLE");
	wl_surface_commit(wlc.surface);
    }
}

void wl_hide()
{
    if (wlc.visible)
    {
	zwlr_layer_surface_v1_destroy(wlc.layer_surface);
	wl_surface_destroy(wlc.surface);

	wlc.surface       = NULL;
	wlc.layer_surface = NULL;

	wl_display_roundtrip(wlc.display);

	wlc.visible = 0;
    }
}

void wl_connector_init(
    int w,
    int h,
    int margin,
    void (*init)(int w, int h, float scale),
    void (*update)(ev_t),
    void (*render)(uint32_t time, uint32_t index, bm_argb_t* bm),
    void (*destroy)())
{
    zc_log_debug("init %i %i", w, h);

    wlc.win_width  = w;
    wlc.win_height = h;
    wlc.win_margin = margin;
    wlc.init       = init;
    wlc.render     = render;
    wlc.update     = update;
    wlc.destroy    = destroy;

    wlc.display = wl_display_connect(NULL);
    if (wlc.display)
    {
	zc_log_debug("display connected");

	struct wl_registry* registry = wl_display_get_registry(wlc.display);
	wl_registry_add_listener(registry, &registry_listener, NULL);

	zc_log_debug("registry added");

	// first roundtrip triggers global events
	wl_display_roundtrip(wlc.display);

	// second roundtrip triggers events attached in global events
	wl_display_roundtrip(wlc.display);

	if (wlc.compositor)
	{
	    wlc.monitor = wlc.monitors[0];

	    wl_show();

	    struct wl_callback* cb = wl_surface_frame(wlc.surface);
	    wl_callback_add_listener(cb, &wl_surface_frame_listener, NULL);

	    while (wl_display_dispatch(wlc.display))
	    {
		/* This space deliberately left blank */
	    }

	    /* zc_log_debug("monitor selected"); */

	    /* wl_connector_create_buffer(); */

	    /* zc_log_debug("buffer created and drawn"); */

	    /* if (!wlc.layer_shell) zc_log_debug("Compositor does not implement wlr-layer-shell protocol."); */

	    /* // first draw */

	    /* struct pollfd fds[2] = { */
	    /* 	{.fd     = wl_display_get_fd(wlc.display), */
	    /* 	 .events = POLLIN}, */
	    /* 	{.fd     = STDIN_FILENO, */
	    /* 	 .events = POLLIN}}; */

	    /* const int nfds = sizeof(fds) / sizeof(*fds); */

	    /* wlc.running = true; */

	    /* zc_log_debug("starting loop"); */

	    /* char*         result    = NULL; */
	    /* char          buffer[3] = {0}; */
	    /* unsigned long state     = 0; */

	    /* while (wlc.running) */
	    /* { */
	    /* 	if (wl_display_flush(wlc.display) < 0) */
	    /* 	{ */
	    /* 	    if (errno == EAGAIN) */
	    /* 		continue; */
	    /* 	    break; */
	    /* 	} */

	    /* 	if (poll(fds, nfds, -1) < 0) */
	    /* 	{ */
	    /* 	    if (errno == EAGAIN) continue; */
	    /* 	    break; */
	    /* 	} */

	    /* 	if (fds[0].revents & POLLIN) */
	    /* 	{ */
	    /* 	    // wayland events */
	    /* 	    if (wl_display_dispatch(wlc.display) < 0) */
	    /* 	    { */
	    /* 		wlc.running = false; */
	    /* 	    } */
	    /* 	} */
	    /* else if (fds[1].revents & POLLIN) */
	    /* { */
	    /*     // stdin events */
	    /*     if (!(fds[1].revents & POLLIN)) */
	    /*     { */
	    /* 	zc_log_error("STDIN unexpectedly closed, revents = %hd", fds[1].revents); */
	    /* 	wlc.running = false; */
	    /* 	break; */
	    /*     } */

	    /*     result = fgets(buffer, 3, stdin); */

	    /*     if (feof(stdin)) */
	    /*     { */
	    /* 	zc_log_info("Received EOF"); */
	    /* 	wlc.running = false; */
	    /* 	break; */
	    /*     } */

	    /*     if (result == NULL) */
	    /*     { */
	    /* 	zc_log_error("fgets() failed: %s", strerror(errno)); */
	    /* 	wlc.running = false; */
	    /* 	break; */
	    /*     } */

	    /*     if (!wl_parse_input(buffer, &state)) */
	    /*     { */
	    /* 	zc_log_error("Received invalid input"); */
	    /* 	wlc.running = false; */
	    /* 	break; */
	    /*     } */

	    /*     if (state == 3) */
	    /*     { */
	    /* 	// exit */
	    /* 	wlc.running = false; */
	    /* 	break; */
	    /*     } */

	    /*     if (state == 2) */
	    /*     { */
	    /* 	// toggle */
	    /* 	if (wlc.visible) wl_hide(); */
	    /* 	else wl_show(); */
	    /*     } */

	    /*     if (state == 1) */
	    /*     { */
	    /* 	// show */
	    /* 	wl_show(); */
	    /*     } */

	    /*     if (state == 0) */
	    /*     { */
	    /* 	// hide */
	    /* 	wl_hide(); */
	    /*     } */
	    /* } */
	    /* } */

	    /* destroy buffer */

	    wl_buffer_destroy(wlc.buffer);
	    munmap(wlc.shm_data, wlc.shm_size);
	    wlc.bitmap.data = NULL;

	    wl_display_disconnect(wlc.display);
	}
	else zc_log_debug("compositor not received");
    }
    else zc_log_debug("cannot open display");

    (*wlc.destroy)();
}

#endif
