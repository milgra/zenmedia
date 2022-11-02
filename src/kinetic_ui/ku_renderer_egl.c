#ifndef ku_renderer_egl_h
#define ku_renderer_egl_h

#include "ku_bitmap.c"
#include "ku_rect.c"
#include "zc_vector.c"

void ku_renderer_egl_init();
void ku_renderer_egl_render(vec_t* views, ku_bitmap_t* bitmap, ku_rect_t dirty);

#endif

#if __INCLUDE_LEVEL__ == 0

#include "ku_gl.c"
#include "ku_view.c"
#include "zc_time.c"

void ku_renderer_egl_init()
{
    ku_gl_init();
}

void ku_renderer_egl_render(vec_t* views, ku_bitmap_t* bitmap, ku_rect_t dirty)
{
    /* cut out dirty rect */

    /* zc_time(NULL); */
    ku_gl_add_textures(views);
    /* zc_time("texture"); */
    ku_gl_add_vertexes(views);
    /* zc_time("vertex"); */

    /* we need to keep nested masks in mind */

    static ku_rect_t masks[32] = {0};
    static int       maski     = 0;

    masks[0] = dirty;

    /* draw views */

    for (int i = 0; i < views->length; i++)
    {
	ku_view_t* view = views->data[i];

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
		(bmr_t){0, 0, bitmap->w, bitmap->h});

	    bmr_t srcmsk = {(int) rect.x, (int) rect.y, (int) (rect.x + rect.w), (int) (rect.y + rect.h)};

	    /* if there is a region to draw, modify source mask */

	    if (view->frame.region.w > 0 && view->frame.region.h > 0)
	    {
		srcmsk.x += view->frame.region.x;
		srcmsk.y += view->frame.region.y;
		srcmsk.z = srcmsk.x + view->frame.region.w;
		srcmsk.w = srcmsk.y + view->frame.region.h;
	    }

	    dstmsk = ku_bitmap_is(dstmsk, srcmsk);

	    ku_gl_render_quad(bitmap, i, dstmsk);
	}

	if (view->style.unmask)
	{
	    maski--;
	    dirty = masks[maski];
	}
    }

    /* zc_time("render"); */
}

#endif