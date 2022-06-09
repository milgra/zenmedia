/* table body view handler */

#ifndef vh_knob_h
#define vh_knob_h

#include "view.c"

typedef struct _vh_tbody_t
{
    void*  userdata;
    vec_t* items;

    float head_xpos; // horizontal position of head
    float head_ypos; // vertical position of head

    int full;       // list is full, no more elements needed
    int head_index; // index of upper element
    int tail_index; // index of lower element

    int top_index; // index of visible top element
    int bot_index; // index of visible bottom element

    view_t* (*item_create)(view_t* tview, int index, void* userdata);
    void (*item_recycle)(view_t* tview, view_t* item, void* userdata);

} vh_tbody_t;

void vh_tbody_attach(
    view_t* view,
    view_t* (*item_create)(view_t* tview, int index, void* userdata),
    void (*item_recycle)(view_t* tview, view_t* item, void* userdata),
    void* userdata);

void vh_tbody_move(
    view_t* view,
    float   dx,
    float   dy);

#endif

#if __INCLUDE_LEVEL__ == 0

#include "zc_log.c"

#define TBODY_PRELOAD_DISTANCE 100.0

void vh_tbody_del(void* p)
{
    vh_tbody_t* vh = p;
    REL(vh->items);
    if (vh->userdata) REL(vh->userdata);
}

void vh_tbody_desc(void* p, int level)
{
    printf("vh_tbody");
}

void vh_tbody_attach(
    view_t* view,
    view_t* (*item_create)(view_t* tview, int index, void* userdata),
    void (*item_recycle)(view_t* tview, view_t* item, void* userdata),
    void* userdata)
{
    assert(view->handler == NULL && view->handler_data == NULL);

    vh_tbody_t* vh = CAL(sizeof(vh_tbody_t), vh_tbody_del, vh_tbody_desc);
    if (userdata) vh->userdata = RET(userdata);
    vh->items        = VNEW(); // REL 0
    vh->item_create  = item_create;
    vh->item_recycle = item_recycle;

    view->handler_data = vh;
}

void vh_tbody_move(
    view_t* view,
    float   dx,
    float   dy)
{
    vh_tbody_t* vh = view->handler_data;

    vh->full = 0;

    // repos items

    vh->head_xpos += dx;
    vh->head_ypos += dy;

    for (int index = 0;
	 index < vh->items->length;
	 index++)
    {
	view_t* iview = vh->items->data[index];
	r2_t    frame = iview->frame.local;

	frame.x = vh->head_xpos;
	frame.y += dy;

	view_set_frame(iview, frame);

	if (frame.y < 0) vh->top_index = vh->head_index + index;
	if (frame.y < view->frame.local.h) vh->bot_index = vh->head_index + index;
    }

    // refill items

    while (vh->full == 0)
    {
	if (vh->items->length == 0)
	{
	    view_t* item = (*vh->item_create)(
		view,
		vh->head_index,
		vh->userdata);

	    if (item)
	    {
		VADD(vh->items, item);
		view_add_subview(view, item);
		view_set_frame(item, (r2_t){0, vh->head_ypos, item->frame.local.w, item->frame.local.h});
	    }
	    else
	    {
		vh->full = 1; // no more items
	    }
	}
	else
	{
	    // load head items if possible

	    view_t* head = vec_head(vh->items);

	    if (head->frame.local.y > 0.0 - TBODY_PRELOAD_DISTANCE)
	    {
		view_t* item = (*vh->item_create)(
		    view,
		    vh->head_index - 1,
		    vh->userdata);

		if (item)
		{
		    vh->full = 0; // there is probably more to come
		    vh->head_index -= 1;

		    vec_ins(vh->items, item, 0);
		    view_insert_subview(view, item, 0);

		    view_set_frame(item, (r2_t){0, head->frame.local.y - item->frame.local.h, item->frame.local.w, item->frame.local.h});
		}
		else
		{
		    vh->full = 1; // no more items
		}
	    }
	    else
	    {
		vh->full = 1; // no more items
	    }

	    // load tail items if possible

	    view_t* tail = vec_tail(vh->items);

	    if (tail->frame.local.y + tail->frame.local.h < view->frame.local.h + TBODY_PRELOAD_DISTANCE)
	    {
		view_t* item = (*vh->item_create)(
		    view,
		    vh->tail_index + 1,
		    vh->userdata);

		if (item)
		{
		    vh->full = 0; // there is probably more to come
		    vh->tail_index += 1;

		    VADD(vh->items, item);
		    view_add_subview(view, item);

		    view_set_frame(item, (r2_t){0, tail->frame.local.y + tail->frame.local.h, item->frame.local.w, item->frame.local.h});
		}
		else
		{
		    vh->full &= 1; // don't set to full if head item is added
		}
	    }
	    else
	    {
		vh->full &= 1; // don't set to full if head item is added
	    }

	    // remove items if needed

	    if (tail->frame.local.y - (head->frame.local.y + head->frame.local.h) > view->frame.local.h) // don't remove if list is not full
	    {
		// remove head if needed

		if (head->frame.local.y + head->frame.local.h < 0.0 - TBODY_PRELOAD_DISTANCE && vh->items->length > 1)
		{
		    VREM(vh->items, head);
		    vh->head_index += 1;
		    view_remove_from_parent(head);
		    if (vh->item_recycle) (*vh->item_recycle)(view, head, vh->userdata);
		}

		// remove tail if needed

		if (tail->frame.local.y > view->frame.local.h + TBODY_PRELOAD_DISTANCE && vh->items->length > 1)
		{
		    VREM(vh->items, tail);
		    vh->tail_index -= 1;
		    view_remove_from_parent(tail);
		    if (vh->item_recycle) (*vh->item_recycle)(view, tail, vh->userdata);
		}
	    }
	}
    }
}

#endif
