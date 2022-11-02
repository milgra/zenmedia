#ifndef ku_window_h
#define ku_window_h

#include "ku_bitmap.c"
#include "ku_event.c"
#include "ku_view.c"

typedef struct _ku_window_t ku_window_t;
struct _ku_window_t
{
    ku_view_t* root;
    vec_t*     views;

    vec_t* implqueue; // views selected by roll over
    vec_t* explqueue; // views selected by click
};

ku_window_t* ku_window_create(int width, int height);
void         ku_window_event(ku_window_t* window, ku_event_t event);
void         ku_window_add(ku_window_t* window, ku_view_t* view);
void         ku_window_remove(ku_window_t* window, ku_view_t* view);
void         ku_window_render(ku_window_t* window, uint32_t time, ku_bitmap_t* bm, ku_rect_t dirty);
void         ku_window_activate(ku_window_t* window, ku_view_t* view);
ku_view_t*   ku_window_get_root(ku_window_t* window);
ku_rect_t    ku_window_update(ku_window_t* window, uint32_t time);
void         ku_window_resize_to_root(ku_window_t* window, ku_view_t* view);

#endif

#if __INCLUDE_LEVEL__ == 0

#include "ku_gl.c"
#include "zc_log.c"
#include "zc_map.c"
#include "zc_time.c"
#include "zc_util2.c"
#include "zc_vec2.c"
#include "zc_vector.c"

void ku_window_del(void* p)
{
    ku_window_t* win = p;

    REL(win->root); // REL 0
    REL(win->views);
    REL(win->implqueue);
    REL(win->explqueue);
}

ku_window_t* ku_window_create(int width, int height)
{
    ku_window_t* win = CAL(sizeof(ku_window_t), ku_window_del, NULL);

    win->root      = ku_view_new("root", (ku_rect_t){0, 0, width, height}); // REL 0
    win->views     = VNEW();
    win->implqueue = VNEW();
    win->explqueue = VNEW();

    /* ku_gl_init(); */

    return win;
}

void ku_window_event(ku_window_t* win, ku_event_t ev)
{
    if (ev.type == KU_EVENT_TIME)
    {
	ku_view_evt(win->root, ev);
    }
    else if (ev.type == KU_EVENT_RESIZE)
    {
	ku_rect_t rf = win->root->frame.local;
	if (rf.w != (float) ev.w ||
	    rf.h != (float) ev.h)
	{
	    ku_view_set_frame(win->root, (ku_rect_t){0.0, 0.0, (float) ev.w, (float) ev.h});
	    ku_view_layout(win->root);
#ifdef DEBUG
	    ku_view_describe(win->root, 0);
#endif
	    ku_view_evt(win->root, ev);
	}
    }
    else if (ev.type == KU_EVENT_MMOVE)
    {
	ku_event_t outev = ev;
	outev.type       = KU_EVENT_MMOVE_OUT;
	for (int i = win->implqueue->length - 1; i > -1; i--)
	{
	    ku_view_t* v = win->implqueue->data[i];
	    if (v->needs_touch)
	    {
		if (ev.x > v->frame.global.x &&
		    ev.x < v->frame.global.x + v->frame.global.w &&
		    ev.y > v->frame.global.y &&
		    ev.y < v->frame.global.y + v->frame.global.h)
		{
		}
		else
		{
		    if (v->handler) (*v->handler)(v, outev);
		    if (v->blocks_touch) break;
		}
	    }
	}

	vec_reset(win->implqueue);
	ku_view_coll_touched(win->root, ev, win->implqueue);

	for (int i = win->implqueue->length - 1; i > -1; i--)
	{
	    ku_view_t* v = win->implqueue->data[i];
	    if (v->needs_touch && v->parent)
	    {
		if (v->handler) (*v->handler)(v, ev);
		if (v->blocks_touch) break;
	    }
	}
    }
    else if (ev.type == KU_EVENT_MDOWN || ev.type == KU_EVENT_MUP)
    {
	ku_event_t outev = ev;
	if (ev.type == KU_EVENT_MDOWN) outev.type = KU_EVENT_MDOWN_OUT;
	if (ev.type == KU_EVENT_MUP) outev.type = KU_EVENT_MUP_OUT;

	for (int i = win->explqueue->length - 1; i > -1; i--)
	{
	    ku_view_t* v = win->explqueue->data[i];
	    if (v->needs_touch)
	    {
		if (v->handler) (*v->handler)(v, outev);
		if (v->blocks_touch) break;
	    }
	}

	if (ev.type == KU_EVENT_MDOWN)
	{
	    vec_reset(win->explqueue);
	    ku_view_coll_touched(win->root, ev, win->explqueue);
	}

	for (int i = win->explqueue->length - 1; i > -1; i--)
	{
	    ku_view_t* v = win->explqueue->data[i];
	    if (v->needs_touch && v->parent)
	    {
		if (v->handler) (*v->handler)(v, ev);
		if (v->blocks_touch) break;
	    }
	}
    }
    else if (ev.type == KU_EVENT_SCROLL)
    {
	vec_reset(win->implqueue);
	ku_view_coll_touched(win->root, ev, win->implqueue);

	for (int i = win->implqueue->length - 1; i > -1; i--)
	{
	    ku_view_t* v = win->implqueue->data[i];
	    if (v->needs_touch && v->parent)
	    {
		if (v->handler) (*v->handler)(v, ev);
		if (v->blocks_scroll) break;
	    }
	}
    }
    else if (ev.type == KU_EVENT_KDOWN || ev.type == KU_EVENT_KUP)
    {
	for (int i = win->explqueue->length - 1; i > -1; i--)
	{
	    ku_view_t* v = win->explqueue->data[i];

	    if (v->needs_key)
	    {
		if (v->handler) (*v->handler)(v, ev);
		if (v->blocks_key) break;
	    }
	}
    }
    else if (ev.type == KU_EVENT_TEXT)
    {
	for (int i = win->explqueue->length - 1; i > -1; i--)
	{
	    ku_view_t* v = win->explqueue->data[i];
	    if (v->needs_text)
	    {
		if (v->handler) (*v->handler)(v, ev);
		break;
	    }
	}
    }
}

void ku_window_add(ku_window_t* win, ku_view_t* view)
{
    ku_view_add_subview(win->root, view);

    // layout, window could be resized since
    ku_view_layout(win->root);
}

void ku_window_remove(ku_window_t* win, ku_view_t* view)
{
    ku_view_remove_from_parent(view);
}

void ku_window_activate(ku_window_t* win, ku_view_t* view)
{
    vec_add_unique_data(win->explqueue, view);
}

void ku_window_rearrange(ku_window_t* win, ku_view_t* view, vec_t* views)
{
    if (!view->exclude) VADD(views, view);
    if (view->style.unmask == 1) view->style.unmask = 0; // reset unmasking
    vec_t* vec = view->views;
    for (int i = 0; i < vec->length; i++) ku_window_rearrange(win, vec->data[i], views);
    if (view->style.masked)
    {
	ku_view_t* last    = views->data[views->length - 1];
	last->style.unmask = 1;
    }
}

ku_rect_t ku_window_update(ku_window_t* win, uint32_t time)
{
    ku_rect_t result = {0};

    if (win->root->rearrange == 1)
    {
	vec_reset(win->views);
	ku_window_rearrange(win, win->root, win->views);

	result = ku_rect_add(result, win->root->frame.global);

	win->root->rearrange = 0;
    }

    for (int i = 0; i < win->views->length; i++)
    {
	ku_view_t* view = win->views->data[i];

	if (view->texture.type == TT_MANAGED && view->texture.state == TS_BLANK) ku_view_gen_texture(view);

	if (view->texture.changed)
	{
	    result                = ku_rect_add(result, view->frame.global);
	    view->texture.changed = 0;
	}
	else if (view->frame.dim_changed)
	{
	    result                  = ku_rect_add(result, view->frame.global);
	    view->frame.dim_changed = 0;
	}
	else if (view->frame.pos_changed)
	{
	    result                  = ku_rect_add(result, view->frame.global);
	    view->frame.pos_changed = 0;
	}
	else if (view->frame.reg_changed)
	{
	    result                  = ku_rect_add(result, view->frame.global);
	    view->frame.reg_changed = 0;
	}
	else if (view->texture.alpha_changed)
	{
	    result                      = ku_rect_add(result, view->frame.global);
	    view->texture.alpha_changed = 0;
	}
    }

    return result;
}

void ku_window_render(ku_window_t* win, uint32_t time, ku_bitmap_t* bm, ku_rect_t dirty)
{
    static ku_rect_t masks[32] = {0};
    static int       maski     = 0;

    masks[0] = dirty;

    /* zc_time(NULL); */
    /* ku_gl_add_textures(win->views); */
    /* zc_time("texture"); */
    /* ku_gl_add_vertexes(win->views); */
    /* zc_time("vertex"); */
    /* ku_gl_render(bm); */
    /* zc_time("render"); */
    /* return; */

    /* draw view into bitmap */

    for (int i = 0; i < win->views->length; i++)
    {
	ku_view_t* view = win->views->data[i];

	if (view->style.masked)
	{
	    dirty = ku_rect_is(masks[maski], view->frame.global);
	    maski++;
	    masks[maski] = dirty;
	    /* printf("%s masked, dirty %f %f %f %f\n", view->id, dirty.x, dirty.y, dirty.w, dirty.h); */
	}

	if (view->texture.bitmap)
	{
	    ku_rect_t rect = view->frame.global;

	    bmr_t dstmsk = ku_bitmap_is(
		(bmr_t){(int) dirty.x, (int) dirty.y, (int) (dirty.x + dirty.w), (int) (dirty.y + dirty.h)},
		(bmr_t){0, 0, bm->w, bm->h});
	    bmr_t srcmsk = {0, 0, view->texture.bitmap->w, view->texture.bitmap->h};

	    if (view->frame.region.w > 0 && view->frame.region.h > 0)
	    {
		srcmsk.x += view->frame.region.x;
		srcmsk.y += view->frame.region.y;
		srcmsk.z = srcmsk.x + view->frame.region.w;
		srcmsk.w = srcmsk.y + view->frame.region.h;
	    }

	    if (view->texture.transparent == 0 || i == 0)
	    {
		ku_bitmap_insert(
		    bm,
		    dstmsk,
		    view->texture.bitmap,
		    srcmsk,
		    rect.x - view->style.shadow_blur,
		    rect.y - view->style.shadow_blur);
	    }
	    else
	    {
		if (view->texture.alpha == 1.0)
		{
		    ku_bitmap_blend(
			bm,
			dstmsk,
			view->texture.bitmap,
			srcmsk,
			rect.x - view->style.shadow_blur,
			rect.y - view->style.shadow_blur);
		}
		else
		{
		    ku_bitmap_blend_with_alpha(
			bm,
			dstmsk,
			view->texture.bitmap,
			srcmsk,
			rect.x - view->style.shadow_blur,
			rect.y - view->style.shadow_blur,
			(255 - (int) (view->texture.alpha * 255.0)));
		}
	    }
	}

	if (view->style.unmask)
	{
	    maski--;
	    dirty = masks[maski];
	}
    }
}

ku_view_t* ku_window_get_root(ku_window_t* win)
{
    return win->root;
}

void ku_window_resize_to_root(ku_window_t* win, ku_view_t* view)
{
    ku_rect_t f = win->root->frame.local;

    ku_view_set_frame(view, (ku_rect_t){0.0, 0.0, (float) f.w, (float) f.h});
    ku_view_layout(view);
}

#endif
