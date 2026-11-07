// menu.c -- upstream's menu.cpp on 32blit's engine/menu.hpp (MIT: LICENSE.32blit)

#include "sbk.h"

void menu_init(Menu *m, char *title, Font *font) {
	m->title = title;
	m->num_items = 0;
	m->font = font;
	m->dx = 0; m->dy = 0; m->dw = 0; m->dh = 0;
	m->current_item = 0;
	m->scroll_offset = 0.0f;
	m->repeat_start_time = 0;
	m->item_h = font->char_h + 10;
	m->item_padding_x = 9;
	m->item_adjust_y = 0;
	m->item_spacing = 1;
	m->header_h = title[0] ? m->item_h : 0;
	m->footer_h = 0;
	m->margin_y = 0;
	m->background = RGB(0x11, 0x11, 0x11);
	m->foreground = RGB(0xF7, 0xF7, 0xF7);
	m->selected_bg = RGB(0x22, 0x22, 0x22);
	m->header_bg = RGB(235, 245, 255);
	m->header_fg = RGB(3, 5, 7);
	m->on_item = 0;
}

void menu_add(Menu *m, int id, char *label) {
	m->ids[m->num_items] = id;
	m->labels[m->num_items] = label;
	m->num_items++;
}

void menu_set_rect(Menu *m, int x, int y, int w, int h) {
	m->dx = x; m->dy = y; m->dw = w; m->dh = h;
}

void menu_update(Menu *m, unsigned time) {
	int total_height, display_height, current_y, target_scroll, t;

	// key repeat for up/down
	if (btn_pressed & (BTN_UP | BTN_DOWN))
		m->repeat_start_time = time - 200;

	if (time - m->repeat_start_time >= 200) {
		if (btn_state & BTN_UP) {
			m->current_item--;
			if (m->current_item < 0) m->current_item += m->num_items;
		} else if (btn_state & BTN_DOWN) {
			m->current_item++;
			if (m->current_item == m->num_items) m->current_item = 0;
		}
		m->repeat_start_time = time;
	}

	if ((btn_released & BTN_A) && m->on_item)
		m->on_item(m->ids[m->current_item]);

	// scrolling
	total_height = m->num_items * (m->item_h + m->item_spacing);
	display_height = m->dh - (m->header_h + m->footer_h + m->margin_y * 2);
	current_y = m->current_item * (m->item_h + m->item_spacing);
	target_scroll = display_height / 2 - current_y;

	// std::min(0, std::max(-(total_height - display_height), target_scroll))
	t = -(total_height - display_height);
	target_scroll = t < target_scroll ? target_scroll : t;
	target_scroll = target_scroll < 0 ? target_scroll : 0;

	m->scroll_offset = m->scroll_offset + ((float) target_scroll - m->scroll_offset) * 0.2f;
}

void menu_render(Menu *m) {
	int x, y, w, display_height, oldx, oldy, oldw, oldh, i, cx, cy, cw, ch, size, px, py;
	Font *f;

	f = m->font;
	scr_pen = m->background;
	scr_rect(m->dx, m->dy, m->dw, m->dh);

	x = m->dx;
	y = m->dy;
	w = m->dw;

	// header
	scr_pen = m->header_bg;
	scr_rect(x, y, w, m->header_h);
	scr_pen = m->header_fg;
	scr_text(m->title, f, x + m->item_padding_x, y, w, m->header_h + f->spacing, ALIGN_CENTER_LEFT);

	y += m->header_h;
	display_height = m->dh - (m->header_h + m->footer_h);

	oldx = clip_x; oldy = clip_y; oldw = clip_w; oldh = clip_h;
	cx = clip_x > x ? clip_x : x;
	cy = clip_y > y ? clip_y : y;
	cw = (clip_x + clip_w < x + w ? clip_x + clip_w : x + w) - cx;
	ch = (clip_y + clip_h < y + display_height ? clip_y + clip_h : y + display_height) - cy;
	scr_clip(cx, cy, cw, ch);
	y += (int) m->scroll_offset + m->margin_y;

	for (i = 0; i < m->num_items; i++) {
		// blit::Menu::render_item
		if (i == m->current_item) {
			scr_pen = m->selected_bg;
			scr_rect(x, y, w, m->item_h);
		}
		scr_pen = m->foreground;
		scr_text(m->labels[i], f, x + m->item_padding_x, y + m->item_adjust_y, w, m->item_h + f->spacing, ALIGN_CENTER_LEFT);

		// and the pointer upstream adds
		if (i == m->current_item) {
			scr_pen = m->foreground;
			px = x + 2;
			size = m->item_h / 3;
			py = y + (m->item_h - size) / 2;
			scr_triangle(px, py, px + size / 2, py + size / 2, px, py + size);
		}
		y += m->item_h + m->item_spacing;
	}

	scr_clip(oldx, oldy, oldw, oldh);
}
