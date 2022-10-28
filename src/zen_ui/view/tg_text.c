/*
  Text texture generator
  Shows text in view
 */

#ifndef texgen_text_h
#define texgen_text_h

#include "ku_text.c"
#include "view.c"

typedef struct _tg_text_t
{
    char*       text;
    textstyle_t style;
} tg_text_t;

void  tg_text_add(view_t* view);
void  tg_text_set(view_t* view, char* text, textstyle_t style);
char* tg_text_get(view_t* view);

#endif

#if __INCLUDE_LEVEL__ == 0

#include "ku_bitmap.c"
#include "ku_draw.c"
#include "tg_css.c"
#include "zc_cstring.c"

int tg_text_index = 0;

void tg_text_gen(view_t* view)
{
    tg_text_t* gen = view->tex_gen_data;
    if (view->frame.local.w > 0 && view->frame.local.h > 0)
    {
	bm_t*       fontmap = bm_new((int) view->frame.local.w, (int) view->frame.local.h); // REL 0
	textstyle_t style   = gen->style;

	if ((style.textcolor & 0xFF) < 0xFF || (style.backcolor & 0xFF) < 0xFF) view->texture.transparent = 1;
	else view->texture.transparent = 0;

	if (strlen(gen->text) > 0)
	{
	    text_render(gen->text, style, fontmap);
	}
	else
	{
	    gfx_rect(fontmap, 0, 0, fontmap->w, fontmap->h, style.backcolor, 0);
	}

	view_set_texture_bmp(view, fontmap);

	REL(fontmap); // REL 0
    }
}

void tg_text_del(void* p)
{
    tg_text_t* gen = p;
    if (gen->text) REL(gen->text);
}

void tg_text_desc(void* p, int level)
{
    printf("tg_text");
}

void tg_text_add(view_t* view)
{
    assert(view->tex_gen == NULL);

    tg_text_t* gen = CAL(sizeof(tg_text_t), tg_text_del, tg_text_desc);

    gen->text = NULL; // REL 1

    view->tex_gen_data = gen;
    view->tex_gen      = tg_text_gen;
    view->exclude      = 0;
}

void tg_text_set(view_t* view, char* text, textstyle_t style)
{
    tg_text_t* gen = view->tex_gen_data;

    if (gen->text) REL(gen->text);
    if (text) gen->text = cstr_new_cstring(text);

    gen->style          = style;
    view->texture.state = TS_BLANK;
}

char* tg_text_get(view_t* view)
{
    tg_text_t* gen = view->tex_gen_data;
    return gen->text;
}

#endif
