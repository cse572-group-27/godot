/**************************************************************************/
/*  rich_text_label.cpp                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "selectable_rich_text_label.h"

#include "core/input/input_map.h"
#include "core/math/math_defs.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/string/translation.h"
#include "scene/gui/label.h"
#include "scene/scene_string_names.h"
#include "servers/display_server.h"

#include "modules/modules_enabled.gen.h" // For regex.
#ifdef MODULE_REGEX_ENABLED
#include "modules/regex/regex.h"
#endif


SelectableRichTextLabel::Item *SelectableRichTextLabel::_get_next_item(Item *p_item, bool p_free) const {
	if (p_free) {
		if (p_item->subitems.size()) {
			return p_item->subitems.front()->get();
		} else if (!p_item->parent) {
			return nullptr;
		} else if (p_item->E->next()) {
			return p_item->E->next()->get();
		} else {
			// Go up until something with a next is found.
			while (p_item->parent && !p_item->E->next()) {
				p_item = p_item->parent;
			}

			if (p_item->parent) {
				return p_item->E->next()->get();
			} else {
				return nullptr;
			}
		}

	} else {
		if (p_item->subitems.size() && p_item->type != ITEM_TABLE) {
			return p_item->subitems.front()->get();
		} else if (p_item->type == ITEM_FRAME) {
			return nullptr;
		} else if (p_item->E->next()) {
			return p_item->E->next()->get();
		} else {
			// Go up until something with a next is found.
			while (p_item->type != ITEM_FRAME && !p_item->E->next()) {
				p_item = p_item->parent;
			}

			if (p_item->type != ITEM_FRAME) {
				return p_item->E->next()->get();
			} else {
				return nullptr;
			}
		}
	}
}

SelectableRichTextLabel::Item *SelectableRichTextLabel::_get_prev_item(Item *p_item, bool p_free) const {
	if (p_free) {
		if (p_item->subitems.size()) {
			return p_item->subitems.back()->get();
		} else if (!p_item->parent) {
			return nullptr;
		} else if (p_item->E->prev()) {
			return p_item->E->prev()->get();
		} else {
			// Go back until something with a prev is found.
			while (p_item->parent && !p_item->E->prev()) {
				p_item = p_item->parent;
			}

			if (p_item->parent) {
				return p_item->E->prev()->get();
			} else {
				return nullptr;
			}
		}

	} else {
		if (p_item->subitems.size() && p_item->type != ITEM_TABLE) {
			return p_item->subitems.back()->get();
		} else if (p_item->type == ITEM_FRAME) {
			return nullptr;
		} else if (p_item->E->prev()) {
			return p_item->E->prev()->get();
		} else {
			// Go back until something with a prev is found.
			while (p_item->type != ITEM_FRAME && !p_item->E->prev()) {
				p_item = p_item->parent;
			}

			if (p_item->type != ITEM_FRAME) {
				return p_item->E->prev()->get();
			} else {
				return nullptr;
			}
		}
	}
}

Rect2 SelectableRichTextLabel::_get_text_rect() {
	return Rect2(theme_cache.normal_style->get_offset(), get_size() - theme_cache.normal_style->get_minimum_size());
}

SelectableRichTextLabel::Item *SelectableRichTextLabel::_get_item_at_pos(SelectableRichTextLabel::Item *p_item_from, SelectableRichTextLabel::Item *p_item_to, int p_position) {
	int offset = 0;
	for (Item *it = p_item_from; it && it != p_item_to; it = _get_next_item(it)) {
		switch (it->type) {
			case ITEM_TEXT: {
				ItemText *t = static_cast<ItemText *>(it);
				offset += t->text.length();
				if (offset > p_position) {
					return it;
				}
			} break;
			case ITEM_NEWLINE: {
				offset += 1;
				if (offset == p_position) {
					return it;
				}
			} break;
			case ITEM_IMAGE: {
				offset += 1;
				if (offset > p_position) {
					return it;
				}
			} break;
			case ITEM_TABLE: {
				offset += 1;
			} break;
			default:
				break;
		}
	}
	return p_item_from;
}

String SelectableRichTextLabel::_roman(int p_num, bool p_capitalize) const {
	if (p_num > 3999) {
		return "ERR";
	};
	String s;
	if (p_capitalize) {
		const String roman_M[] = { "", "M", "MM", "MMM" };
		const String roman_C[] = { "", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM" };
		const String roman_X[] = { "", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC" };
		const String roman_I[] = { "", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX" };
		s = roman_M[p_num / 1000] + roman_C[(p_num % 1000) / 100] + roman_X[(p_num % 100) / 10] + roman_I[p_num % 10];
	} else {
		const String roman_M[] = { "", "m", "mm", "mmm" };
		const String roman_C[] = { "", "c", "cc", "ccc", "cd", "d", "dc", "dcc", "dccc", "cm" };
		const String roman_X[] = { "", "x", "xx", "xxx", "xl", "l", "lx", "lxx", "lxxx", "xc" };
		const String roman_I[] = { "", "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix" };
		s = roman_M[p_num / 1000] + roman_C[(p_num % 1000) / 100] + roman_X[(p_num % 100) / 10] + roman_I[p_num % 10];
	}
	return s;
}

String SelectableRichTextLabel::_letters(int p_num, bool p_capitalize) const {
	int64_t n = p_num;

	int chars = 0;
	do {
		n /= 24;
		chars++;
	} while (n);

	String s;
	s.resize(chars + 1);
	char32_t *c = s.ptrw();
	c[chars] = 0;
	n = p_num;
	do {
		int mod = ABS(n % 24);
		char a = (p_capitalize ? 'A' : 'a');
		c[--chars] = a + mod - 1;

		n /= 24;
	} while (n);

	return s;
}

void SelectableRichTextLabel::_update_line_font(ItemFrame *p_frame, int p_line, const Ref<Font> &p_base_font, int p_base_font_size) {
	ERR_FAIL_COND(p_frame == nullptr);
	ERR_FAIL_COND(p_line < 0 || p_line >= (int)p_frame->lines.size());

	Line &l = p_frame->lines[p_line];
	MutexLock lock(l.text_buf->get_mutex());

	RID t = l.text_buf->get_rid();
	int spans = TS->shaped_get_span_count(t);
	for (int i = 0; i < spans; i++) {
		ItemText *it = reinterpret_cast<ItemText *>((uint64_t)TS->shaped_get_span_meta(t, i));
		if (it) {
			Ref<Font> font = p_base_font;
			int font_size = p_base_font_size;

			ItemFont *font_it = _find_font(it);
			if (font_it) {
				if (font_it->font.is_valid()) {
					font = font_it->font;
				}
				if (font_it->font_size > 0) {
					font_size = font_it->font_size;
				}
			}
			ItemFontSize *font_size_it = _find_font_size(it);
			if (font_size_it && font_size_it->font_size > 0) {
				font_size = font_size_it->font_size;
			}
			TS->shaped_set_span_update_font(t, i, font->get_rids(), font_size, font->get_opentype_features());
			for (int j = 0; j < TextServer::SPACING_MAX; j++) {
				TS->shaped_text_set_spacing(t, TextServer::SpacingType(j), font->get_spacing(TextServer::SpacingType(j)));
			}
		}
	}

	Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;
	for (Item *it = l.from; it && it != it_to; it = _get_next_item(it)) {
		switch (it->type) {
			case ITEM_TABLE: {
				ItemTable *table = static_cast<ItemTable *>(it);
				for (Item *E : table->subitems) {
					ERR_CONTINUE(E->type != ITEM_FRAME); // Children should all be frames.
					ItemFrame *frame = static_cast<ItemFrame *>(E);
					for (int i = 0; i < (int)frame->lines.size(); i++) {
						_update_line_font(frame, i, p_base_font, p_base_font_size);
					}
				}
			} break;
			default:
				break;
		}
	}
}

float SelectableRichTextLabel::_resize_line(ItemFrame *p_frame, int p_line, const Ref<Font> &p_base_font, int p_base_font_size, int p_width, float p_h) {
	ERR_FAIL_COND_V(p_frame == nullptr, p_h);
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)p_frame->lines.size(), p_h);

	Line &l = p_frame->lines[p_line];
	MutexLock lock(l.text_buf->get_mutex());

	l.offset.x = _find_margin(l.from, p_base_font, p_base_font_size);
	l.text_buf->set_width(p_width - l.offset.x);

	PackedFloat32Array tab_stops = _find_tab_stops(l.from);
	if (!tab_stops.is_empty()) {
		l.text_buf->tab_align(tab_stops);
	} else if (tab_size > 0) { // Align inline tabs.
		Vector<float> tabs;
		tabs.push_back(tab_size * p_base_font->get_char_size(' ', p_base_font_size).width);
		l.text_buf->tab_align(tabs);
	}

	Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;
	for (Item *it = l.from; it && it != it_to; it = _get_next_item(it)) {
		switch (it->type) {
			case ITEM_TABLE: {
				ItemTable *table = static_cast<ItemTable *>(it);
				int col_count = table->columns.size();

				for (int i = 0; i < col_count; i++) {
					table->columns[i].width = 0;
				}

				int idx = 0;
				for (Item *E : table->subitems) {
					ERR_CONTINUE(E->type != ITEM_FRAME); // Children should all be frames.
					ItemFrame *frame = static_cast<ItemFrame *>(E);
					float prev_h = 0;
					for (int i = 0; i < (int)frame->lines.size(); i++) {
						MutexLock sub_lock(frame->lines[i].text_buf->get_mutex());
						int w = _find_margin(frame->lines[i].from, p_base_font, p_base_font_size) + 1;
						prev_h = _resize_line(frame, i, p_base_font, p_base_font_size, w, prev_h);
					}
					idx++;
				}

				// Compute minimum width for each cell.
				const int available_width = p_width - theme_cache.table_h_separation * (col_count - 1);

				// Compute available width and total ratio (for expanders).
				int total_ratio = 0;
				int remaining_width = available_width;
				table->total_width = theme_cache.table_h_separation;

				for (int i = 0; i < col_count; i++) {
					remaining_width -= table->columns[i].min_width;
					if (table->columns[i].max_width > table->columns[i].min_width) {
						table->columns[i].expand = true;
					}
					if (table->columns[i].expand) {
						total_ratio += table->columns[i].expand_ratio;
					}
				}

				// Assign actual widths.
				for (int i = 0; i < col_count; i++) {
					table->columns[i].width = table->columns[i].min_width;
					if (table->columns[i].expand && total_ratio > 0 && remaining_width > 0) {
						table->columns[i].width += table->columns[i].expand_ratio * remaining_width / total_ratio;
					}
					if (i != col_count - 1) {
						table->total_width += table->columns[i].width + theme_cache.table_h_separation;
					} else {
						table->total_width += table->columns[i].width;
					}
				}

				// Resize to max_width if needed and distribute the remaining space.
				bool table_need_fit = true;
				while (table_need_fit) {
					table_need_fit = false;
					// Fit slim.
					for (int i = 0; i < col_count; i++) {
						if (!table->columns[i].expand) {
							continue;
						}
						int dif = table->columns[i].width - table->columns[i].max_width;
						if (dif > 0) {
							table_need_fit = true;
							table->columns[i].width = table->columns[i].max_width;
							table->total_width -= dif;
							total_ratio -= table->columns[i].expand_ratio;
						}
					}
					// Grow.
					remaining_width = available_width - table->total_width;
					if (remaining_width > 0 && total_ratio > 0) {
						for (int i = 0; i < col_count; i++) {
							if (table->columns[i].expand) {
								int dif = table->columns[i].max_width - table->columns[i].width;
								if (dif > 0) {
									int slice = table->columns[i].expand_ratio * remaining_width / total_ratio;
									int incr = MIN(dif, slice);
									table->columns[i].width += incr;
									table->total_width += incr;
								}
							}
						}
					}
				}

				// Update line width and get total height.
				idx = 0;
				table->total_height = 0;
				table->rows.clear();
				table->rows_baseline.clear();

				Vector2 offset;
				float row_height = 0.0;

				for (Item *E : table->subitems) {
					ERR_CONTINUE(E->type != ITEM_FRAME); // Children should all be frames.
					ItemFrame *frame = static_cast<ItemFrame *>(E);

					int column = idx % col_count;

					offset.x += frame->padding.position.x;
					float yofs = frame->padding.position.y;
					float prev_h = 0;
					float row_baseline = 0.0;
					for (int i = 0; i < (int)frame->lines.size(); i++) {
						MutexLock sub_lock(frame->lines[i].text_buf->get_mutex());
						frame->lines[i].text_buf->set_width(table->columns[column].width);
						table->columns[column].width = MAX(table->columns[column].width, ceil(frame->lines[i].text_buf->get_size().x));

						frame->lines[i].offset.y = prev_h;

						float h = frame->lines[i].text_buf->get_size().y + (frame->lines[i].text_buf->get_line_count() - 1) * theme_cache.line_separation;
						if (i > 0) {
							h += theme_cache.line_separation;
						}
						if (frame->min_size_over.y > 0) {
							h = MAX(h, frame->min_size_over.y);
						}
						if (frame->max_size_over.y > 0) {
							h = MIN(h, frame->max_size_over.y);
						}
						yofs += h;
						prev_h = frame->lines[i].offset.y + frame->lines[i].text_buf->get_size().y + frame->lines[i].text_buf->get_line_count() * theme_cache.line_separation;

						frame->lines[i].offset += offset;
						row_baseline = MAX(row_baseline, frame->lines[i].text_buf->get_line_ascent(frame->lines[i].text_buf->get_line_count() - 1));
					}
					yofs += frame->padding.size.y;
					offset.x += table->columns[column].width + theme_cache.table_h_separation + frame->padding.size.x;

					row_height = MAX(yofs, row_height);
					if (column == col_count - 1) {
						offset.x = 0;
						row_height += theme_cache.table_v_separation;
						table->total_height += row_height;
						offset.y += row_height;
						table->rows.push_back(row_height);
						table->rows_baseline.push_back(table->total_height - row_height + row_baseline);
						row_height = 0;
					}
					idx++;
				}
				int row_idx = (table->align_to_row < 0) ? table->rows_baseline.size() - 1 : table->align_to_row;
				if (table->rows_baseline.size() != 0 && row_idx < (int)table->rows_baseline.size() - 1) {
					l.text_buf->resize_object((uint64_t)it, Size2(table->total_width, table->total_height), table->inline_align, Math::round(table->rows_baseline[row_idx]));
				} else {
					l.text_buf->resize_object((uint64_t)it, Size2(table->total_width, table->total_height), table->inline_align);
				}
			} break;
			default:
				break;
		}
	}

	l.offset.y = p_h;
	return _calculate_line_vertical_offset(l);
}

float SelectableRichTextLabel::_shape_line(ItemFrame *p_frame, int p_line, const Ref<Font> &p_base_font, int p_base_font_size, int p_width, float p_h, int *r_char_offset) {
	ERR_FAIL_COND_V(p_frame == nullptr, p_h);
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)p_frame->lines.size(), p_h);

	Line &l = p_frame->lines[p_line];
	MutexLock lock(l.text_buf->get_mutex());

	BitField<TextServer::LineBreakFlag> autowrap_flags = TextServer::BREAK_MANDATORY;
	switch (autowrap_mode) {
		case TextServer::AUTOWRAP_WORD_SMART:
			autowrap_flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_ADAPTIVE | TextServer::BREAK_MANDATORY;
			break;
		case TextServer::AUTOWRAP_WORD:
			autowrap_flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_MANDATORY;
			break;
		case TextServer::AUTOWRAP_ARBITRARY:
			autowrap_flags = TextServer::BREAK_GRAPHEME_BOUND | TextServer::BREAK_MANDATORY;
			break;
		case TextServer::AUTOWRAP_OFF:
			break;
	}
	autowrap_flags = autowrap_flags | TextServer::BREAK_TRIM_EDGE_SPACES;

	// Clear cache.
	l.text_buf->clear();
	l.text_buf->set_break_flags(autowrap_flags);
	l.text_buf->set_justification_flags(_find_jst_flags(l.from));
	l.char_offset = *r_char_offset;
	l.char_count = 0;

	// Add indent.
	l.offset.x = _find_margin(l.from, p_base_font, p_base_font_size);
	l.text_buf->set_width(p_width - l.offset.x);
	l.text_buf->set_alignment(_find_alignment(l.from));
	l.text_buf->set_direction(_find_direction(l.from));

	PackedFloat32Array tab_stops = _find_tab_stops(l.from);
	if (!tab_stops.is_empty()) {
		l.text_buf->tab_align(tab_stops);
	} else if (tab_size > 0) { // Align inline tabs.
		Vector<float> tabs;
		tabs.push_back(tab_size * p_base_font->get_char_size(' ', p_base_font_size).width);
		l.text_buf->tab_align(tabs);
	}

	// Shape current paragraph.
	String txt;
	Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;
	int remaining_characters = visible_characters - l.char_offset;
	for (Item *it = l.from; it && it != it_to; it = _get_next_item(it)) {
		if (visible_chars_behavior == TextServer::VC_CHARS_BEFORE_SHAPING && visible_characters >= 0 && remaining_characters <= 0) {
			break;
		}
		switch (it->type) {
			case ITEM_DROPCAP: {
				// Add dropcap.
				const ItemDropcap *dc = static_cast<ItemDropcap *>(it);
				l.text_buf->set_dropcap(dc->text, dc->font, dc->font_size, dc->dropcap_margins);
				l.dc_color = dc->color;
				l.dc_ol_size = dc->ol_size;
				l.dc_ol_color = dc->ol_color;
			} break;
			case ITEM_NEWLINE: {
				Ref<Font> font = p_base_font;
				int font_size = p_base_font_size;

				ItemFont *font_it = _find_font(it);
				if (font_it) {
					if (font_it->font.is_valid()) {
						font = font_it->font;
					}
					if (font_it->font_size > 0) {
						font_size = font_it->font_size;
					}
				}
				ItemFontSize *font_size_it = _find_font_size(it);
				if (font_size_it && font_size_it->font_size > 0) {
					font_size = font_size_it->font_size;
				}
				l.text_buf->add_string("\n", font, font_size);
				txt += "\n";
				l.char_count++;
				remaining_characters--;
			} break;
			case ITEM_TEXT: {
				ItemText *t = static_cast<ItemText *>(it);
				Ref<Font> font = p_base_font;
				int font_size = p_base_font_size;

				ItemFont *font_it = _find_font(it);
				if (font_it) {
					if (font_it->font.is_valid()) {
						font = font_it->font;
					}
					if (font_it->font_size > 0) {
						font_size = font_it->font_size;
					}
				}
				ItemFontSize *font_size_it = _find_font_size(it);
				if (font_size_it && font_size_it->font_size > 0) {
					font_size = font_size_it->font_size;
				}
				String lang = _find_language(it);
				String tx = t->text;
				if (visible_chars_behavior == TextServer::VC_CHARS_BEFORE_SHAPING && visible_characters >= 0 && remaining_characters >= 0) {
					tx = tx.substr(0, remaining_characters);
				}
				remaining_characters -= tx.length();

				l.text_buf->add_string(tx, font, font_size, lang, (uint64_t)it);
				txt += tx;
				l.char_count += tx.length();
			} break;
			case ITEM_IMAGE: {
				ItemImage *img = static_cast<ItemImage *>(it);
				l.text_buf->add_object((uint64_t)it, img->size, img->inline_align, 1);
				txt += String::chr(0xfffc);
				l.char_count++;
				remaining_characters--;
			} break;
			case ITEM_TABLE: {
				ItemTable *table = static_cast<ItemTable *>(it);
				int col_count = table->columns.size();
				int t_char_count = 0;
				// Set minimums to zero.
				for (int i = 0; i < col_count; i++) {
					table->columns[i].min_width = 0;
					table->columns[i].max_width = 0;
					table->columns[i].width = 0;
				}
				// Compute minimum width for each cell.
				const int available_width = p_width - theme_cache.table_h_separation * (col_count - 1);

				int idx = 0;
				for (Item *E : table->subitems) {
					ERR_CONTINUE(E->type != ITEM_FRAME); // Children should all be frames.
					ItemFrame *frame = static_cast<ItemFrame *>(E);

					int column = idx % col_count;
					float prev_h = 0;
					for (int i = 0; i < (int)frame->lines.size(); i++) {
						MutexLock sub_lock(frame->lines[i].text_buf->get_mutex());

						int char_offset = l.char_offset + l.char_count;
						int w = _find_margin(frame->lines[i].from, p_base_font, p_base_font_size) + 1;
						prev_h = _shape_line(frame, i, p_base_font, p_base_font_size, w, prev_h, &char_offset);
						int cell_ch = (char_offset - (l.char_offset + l.char_count));
						l.char_count += cell_ch;
						t_char_count += cell_ch;
						remaining_characters -= cell_ch;

						table->columns[column].min_width = MAX(table->columns[column].min_width, ceil(frame->lines[i].text_buf->get_size().x));
						table->columns[column].max_width = MAX(table->columns[column].max_width, ceil(frame->lines[i].text_buf->get_non_wrapped_size().x));
					}
					idx++;
				}

				// Compute available width and total ratio (for expanders).
				int total_ratio = 0;
				int remaining_width = available_width;
				table->total_width = theme_cache.table_h_separation;

				for (int i = 0; i < col_count; i++) {
					remaining_width -= table->columns[i].min_width;
					if (table->columns[i].max_width > table->columns[i].min_width) {
						table->columns[i].expand = true;
					}
					if (table->columns[i].expand) {
						total_ratio += table->columns[i].expand_ratio;
					}
				}

				// Assign actual widths.
				for (int i = 0; i < col_count; i++) {
					table->columns[i].width = table->columns[i].min_width;
					if (table->columns[i].expand && total_ratio > 0 && remaining_width > 0) {
						table->columns[i].width += table->columns[i].expand_ratio * remaining_width / total_ratio;
					}
					if (i != col_count - 1) {
						table->total_width += table->columns[i].width + theme_cache.table_h_separation;
					} else {
						table->total_width += table->columns[i].width;
					}
				}

				// Resize to max_width if needed and distribute the remaining space.
				bool table_need_fit = true;
				while (table_need_fit) {
					table_need_fit = false;
					// Fit slim.
					for (int i = 0; i < col_count; i++) {
						if (!table->columns[i].expand) {
							continue;
						}
						int dif = table->columns[i].width - table->columns[i].max_width;
						if (dif > 0) {
							table_need_fit = true;
							table->columns[i].width = table->columns[i].max_width;
							table->total_width -= dif;
							total_ratio -= table->columns[i].expand_ratio;
						}
					}
					// Grow.
					remaining_width = available_width - table->total_width;
					if (remaining_width > 0 && total_ratio > 0) {
						for (int i = 0; i < col_count; i++) {
							if (table->columns[i].expand) {
								int dif = table->columns[i].max_width - table->columns[i].width;
								if (dif > 0) {
									int slice = table->columns[i].expand_ratio * remaining_width / total_ratio;
									int incr = MIN(dif, slice);
									table->columns[i].width += incr;
									table->total_width += incr;
								}
							}
						}
					}
				}

				// Update line width and get total height.
				idx = 0;
				table->total_height = 0;
				table->rows.clear();
				table->rows_baseline.clear();

				Vector2 offset;
				float row_height = 0.0;

				for (const List<Item *>::Element *E = table->subitems.front(); E; E = E->next()) {
					ERR_CONTINUE(E->get()->type != ITEM_FRAME); // Children should all be frames.
					ItemFrame *frame = static_cast<ItemFrame *>(E->get());

					int column = idx % col_count;

					offset.x += frame->padding.position.x;
					float yofs = frame->padding.position.y;
					float prev_h = 0;
					float row_baseline = 0.0;
					for (int i = 0; i < (int)frame->lines.size(); i++) {
						MutexLock sub_lock(frame->lines[i].text_buf->get_mutex());

						frame->lines[i].text_buf->set_width(table->columns[column].width);
						table->columns[column].width = MAX(table->columns[column].width, ceil(frame->lines[i].text_buf->get_size().x));

						frame->lines[i].offset.y = prev_h;

						float h = frame->lines[i].text_buf->get_size().y + (frame->lines[i].text_buf->get_line_count() - 1) * theme_cache.line_separation;
						if (i > 0) {
							h += theme_cache.line_separation;
						}
						if (frame->min_size_over.y > 0) {
							h = MAX(h, frame->min_size_over.y);
						}
						if (frame->max_size_over.y > 0) {
							h = MIN(h, frame->max_size_over.y);
						}
						yofs += h;
						prev_h = frame->lines[i].offset.y + frame->lines[i].text_buf->get_size().y + frame->lines[i].text_buf->get_line_count() * theme_cache.line_separation;

						frame->lines[i].offset += offset;
						row_baseline = MAX(row_baseline, frame->lines[i].text_buf->get_line_ascent(frame->lines[i].text_buf->get_line_count() - 1));
					}
					yofs += frame->padding.size.y;
					offset.x += table->columns[column].width + theme_cache.table_h_separation + frame->padding.size.x;

					row_height = MAX(yofs, row_height);
					// Add row height after last column of the row or last cell of the table.
					if (column == col_count - 1 || E->next() == nullptr) {
						offset.x = 0;
						row_height += theme_cache.table_v_separation;
						table->total_height += row_height;
						offset.y += row_height;
						table->rows.push_back(row_height);
						table->rows_baseline.push_back(table->total_height - row_height + row_baseline);
						row_height = 0;
					}
					idx++;
				}
				int row_idx = (table->align_to_row < 0) ? table->rows_baseline.size() - 1 : table->align_to_row;
				if (table->rows_baseline.size() != 0 && row_idx < (int)table->rows_baseline.size() - 1) {
					l.text_buf->add_object((uint64_t)it, Size2(table->total_width, table->total_height), table->inline_align, t_char_count, Math::round(table->rows_baseline[row_idx]));
				} else {
					l.text_buf->add_object((uint64_t)it, Size2(table->total_width, table->total_height), table->inline_align, t_char_count);
				}
				txt += String::chr(0xfffc).repeat(t_char_count);
			} break;
			default:
				break;
		}
	}

	// Apply BiDi override.
	l.text_buf->set_bidi_override(structured_text_parser(_find_stt(l.from), st_args, txt));

	*r_char_offset = l.char_offset + l.char_count;

	l.offset.y = p_h;
	return _calculate_line_vertical_offset(l);
}

int SelectableRichTextLabel::_draw_line(ItemFrame *p_frame, int p_line, const Vector2 &p_ofs, int p_width, const Color &p_base_color, int p_outline_size, const Color &p_outline_color, const Color &p_font_shadow_color, int p_shadow_outline_size, const Point2 &p_shadow_ofs, int &r_processed_glyphs) {
	ERR_FAIL_COND_V(p_frame == nullptr, 0);
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)p_frame->lines.size(), 0);

	Vector2 off;

	Line &l = p_frame->lines[p_line];
	MutexLock lock(l.text_buf->get_mutex());

	Item *it_from = l.from;
	Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;

	if (it_from == nullptr) {
		return 0;
	}

	RID ci = get_canvas_item();
	bool rtl = (l.text_buf->get_direction() == TextServer::DIRECTION_RTL);
	bool lrtl = is_layout_rtl();

	bool trim_chars = (visible_characters >= 0) && (visible_chars_behavior == TextServer::VC_CHARS_AFTER_SHAPING);
	bool trim_glyphs_ltr = (visible_characters >= 0) && ((visible_chars_behavior == TextServer::VC_GLYPHS_LTR) || ((visible_chars_behavior == TextServer::VC_GLYPHS_AUTO) && !lrtl));
	bool trim_glyphs_rtl = (visible_characters >= 0) && ((visible_chars_behavior == TextServer::VC_GLYPHS_RTL) || ((visible_chars_behavior == TextServer::VC_GLYPHS_AUTO) && lrtl));
	int total_glyphs = (trim_glyphs_ltr || trim_glyphs_rtl) ? get_total_glyph_count() : 0;
	int visible_glyphs = total_glyphs * visible_ratio;

	Vector<int> list_index;
	Vector<ItemList *> list_items;
	_find_list(l.from, list_index, list_items);

	String prefix;
	for (int i = 0; i < list_index.size(); i++) {
		if (rtl) {
			prefix = prefix + ".";
		} else {
			prefix = "." + prefix;
		}
		String segment;
		if (list_items[i]->list_type == LIST_DOTS) {
			prefix = list_items[i]->bullet;
			break;
		} else if (list_items[i]->list_type == LIST_NUMBERS) {
			segment = itos(list_index[i]);
			if (is_localizing_numeral_system()) {
				segment = TS->format_number(segment, _find_language(l.from));
			}
		} else if (list_items[i]->list_type == LIST_LETTERS) {
			segment = _letters(list_index[i], list_items[i]->capitalize);
		} else if (list_items[i]->list_type == LIST_ROMAN) {
			segment = _roman(list_index[i], list_items[i]->capitalize);
		}
		if (rtl) {
			prefix = prefix + segment;
		} else {
			prefix = segment + prefix;
		}
	}
	if (!prefix.is_empty()) {
		Ref<Font> font = theme_cache.normal_font;
		int font_size = theme_cache.normal_font_size;

		ItemFont *font_it = _find_font(l.from);
		if (font_it) {
			if (font_it->font.is_valid()) {
				font = font_it->font;
			}
			if (font_it->font_size > 0) {
				font_size = font_it->font_size;
			}
		}
		ItemFontSize *font_size_it = _find_font_size(l.from);
		if (font_size_it && font_size_it->font_size > 0) {
			font_size = font_size_it->font_size;
		}
		if (rtl) {
			float offx = 0.0f;
			if (!lrtl && p_frame == main) { // Skip Scrollbar.
				offx -= scroll_w;
			}
			font->draw_string(ci, p_ofs + Vector2(p_width - l.offset.x + offx, l.text_buf->get_line_ascent(0)), " " + prefix, HORIZONTAL_ALIGNMENT_LEFT, l.offset.x, font_size, _find_color(l.from, p_base_color));
		} else {
			float offx = 0.0f;
			if (lrtl && p_frame == main) { // Skip Scrollbar.
				offx += scroll_w;
			}
			font->draw_string(ci, p_ofs + Vector2(offx, l.text_buf->get_line_ascent(0)), prefix + " ", HORIZONTAL_ALIGNMENT_RIGHT, l.offset.x, font_size, _find_color(l.from, p_base_color));
		}
	}

	// Draw dropcap.
	int dc_lines = l.text_buf->get_dropcap_lines();
	float h_off = l.text_buf->get_dropcap_size().x;
	if (l.dc_ol_size > 0) {
		l.text_buf->draw_dropcap_outline(ci, p_ofs + ((rtl) ? Vector2() : Vector2(l.offset.x, 0)), l.dc_ol_size, l.dc_ol_color);
	}
	l.text_buf->draw_dropcap(ci, p_ofs + ((rtl) ? Vector2() : Vector2(l.offset.x, 0)), l.dc_color);

	int line_count = 0;
	Size2 ctrl_size = get_size();
	// Draw text.
	for (int line = 0; line < l.text_buf->get_line_count(); line++) {
		if (line > 0) {
			off.y += theme_cache.line_separation;
		}

		if (p_ofs.y + off.y >= ctrl_size.height) {
			break;
		}

		const Size2 line_size = l.text_buf->get_line_size(line);
		if (p_ofs.y + off.y + line_size.y <= 0) {
			off.y += line_size.y;
			continue;
		}

		float width = l.text_buf->get_width();
		float length = line_size.x;

		// Draw line.
		line_count++;

		if (rtl) {
			off.x = p_width - l.offset.x - width;
			if (!lrtl && p_frame == main) { // Skip Scrollbar.
				off.x -= scroll_w;
			}
		} else {
			off.x = l.offset.x;
			if (lrtl && p_frame == main) { // Skip Scrollbar.
				off.x += scroll_w;
			}
		}

		// Draw text.
		switch (l.text_buf->get_alignment()) {
			case HORIZONTAL_ALIGNMENT_FILL:
			case HORIZONTAL_ALIGNMENT_LEFT: {
				if (rtl) {
					off.x += width - length;
				}
			} break;
			case HORIZONTAL_ALIGNMENT_CENTER: {
				off.x += Math::floor((width - length) / 2.0);
			} break;
			case HORIZONTAL_ALIGNMENT_RIGHT: {
				if (!rtl) {
					off.x += width - length;
				}
			} break;
		}

		if (line <= dc_lines) {
			if (rtl) {
				off.x -= h_off;
			} else {
				off.x += h_off;
			}
		}

		RID rid = l.text_buf->get_line_rid(line);
		//draw_rect(Rect2(p_ofs + off, TS->shaped_text_get_size(rid)), Color(1,0,0), false, 2); //DEBUG_RECTS

		off.y += TS->shaped_text_get_ascent(rid);
		// Draw inlined objects.
		Array objects = TS->shaped_text_get_objects(rid);
		for (int i = 0; i < objects.size(); i++) {
			Item *it = reinterpret_cast<Item *>((uint64_t)objects[i]);
			if (it != nullptr) {
				Rect2 rect = TS->shaped_text_get_object_rect(rid, objects[i]);
				//draw_rect(rect, Color(1,0,0), false, 2); //DEBUG_RECTS
				switch (it->type) {
					case ITEM_IMAGE: {
						ItemImage *img = static_cast<ItemImage *>(it);
						img->image->draw_rect(ci, Rect2(p_ofs + rect.position + off, rect.size), false, img->color);
					} break;
					case ITEM_TABLE: {
						ItemTable *table = static_cast<ItemTable *>(it);
						Color odd_row_bg = theme_cache.table_odd_row_bg;
						Color even_row_bg = theme_cache.table_even_row_bg;
						Color border = theme_cache.table_border;
						int h_separation = theme_cache.table_h_separation;

						int col_count = table->columns.size();
						int row_count = table->rows.size();

						int idx = 0;
						for (Item *E : table->subitems) {
							ItemFrame *frame = static_cast<ItemFrame *>(E);

							int col = idx % col_count;
							int row = idx / col_count;

							if (frame->lines.size() != 0 && row < row_count) {
								Vector2 coff = frame->lines[0].offset;
								if (rtl) {
									coff.x = rect.size.width - table->columns[col].width - coff.x;
								}
								if (row % 2 == 0) {
									draw_rect(Rect2(p_ofs + rect.position + off + coff - frame->padding.position, Size2(table->columns[col].width + h_separation + frame->padding.position.x + frame->padding.size.x, table->rows[row])), (frame->odd_row_bg != Color(0, 0, 0, 0) ? frame->odd_row_bg : odd_row_bg), true);
								} else {
									draw_rect(Rect2(p_ofs + rect.position + off + coff - frame->padding.position, Size2(table->columns[col].width + h_separation + frame->padding.position.x + frame->padding.size.x, table->rows[row])), (frame->even_row_bg != Color(0, 0, 0, 0) ? frame->even_row_bg : even_row_bg), true);
								}
								draw_rect(Rect2(p_ofs + rect.position + off + coff - frame->padding.position, Size2(table->columns[col].width + h_separation + frame->padding.position.x + frame->padding.size.x, table->rows[row])), (frame->border != Color(0, 0, 0, 0) ? frame->border : border), false);
							}

							for (int j = 0; j < (int)frame->lines.size(); j++) {
								_draw_line(frame, j, p_ofs + rect.position + off + Vector2(0, frame->lines[j].offset.y), rect.size.x, p_base_color, p_outline_size, p_outline_color, p_font_shadow_color, p_shadow_outline_size, p_shadow_ofs, r_processed_glyphs);
							}
							idx++;
						}
					} break;
					default:
						break;
				}
			}
		}

		const Glyph *glyphs = TS->shaped_text_get_glyphs(rid);
		int gl_size = TS->shaped_text_get_glyph_count(rid);

		Vector2 gloff = off;
		// Draw outlines and shadow.
		int processed_glyphs_ol = r_processed_glyphs;
		for (int i = 0; i < gl_size; i++) {
			Item *it = _get_item_at_pos(it_from, it_to, glyphs[i].start);
			int size = _find_outline_size(it, p_outline_size);
			Color font_color = _find_color(it, p_base_color);
			Color font_outline_color = _find_outline_color(it, p_outline_color);
			Color font_shadow_color = p_font_shadow_color;
			if ((size <= 0 || font_outline_color.a == 0) && (font_shadow_color.a == 0)) {
				gloff.x += glyphs[i].advance;
				continue;
			}

			// Get FX.
			ItemFade *fade = nullptr;
			Item *fade_item = it;
			while (fade_item) {
				if (fade_item->type == ITEM_FADE) {
					fade = static_cast<ItemFade *>(fade_item);
					break;
				}
				fade_item = fade_item->parent;
			}

			Vector<ItemFX *> fx_stack;
			_fetch_item_fx_stack(it, fx_stack);
			bool custom_fx_ok = true;

			Point2 fx_offset = Vector2(glyphs[i].x_off, glyphs[i].y_off);
			RID frid = glyphs[i].font_rid;
			uint32_t gl = glyphs[i].index;
			uint16_t gl_fl = glyphs[i].flags;
			uint8_t gl_cn = glyphs[i].count;
			bool cprev_cluster = false;
			bool cprev_conn = false;
			if (gl_cn == 0) { // Parts of the same cluster, always connected.
				cprev_cluster = true;
			}
			if (gl_fl & TextServer::GRAPHEME_IS_RTL) { // Check if previous grapheme cluster is connected.
				if (i > 0 && (glyphs[i - 1].flags & TextServer::GRAPHEME_IS_CONNECTED)) {
					cprev_conn = true;
				}
			} else {
				if (glyphs[i].flags & TextServer::GRAPHEME_IS_CONNECTED) {
					cprev_conn = true;
				}
			}

			//Apply fx.
			if (fade) {
				float faded_visibility = 1.0f;
				if (glyphs[i].start >= fade->starting_index) {
					faded_visibility -= (float)(glyphs[i].start - fade->starting_index) / (float)fade->length;
					faded_visibility = faded_visibility < 0.0f ? 0.0f : faded_visibility;
				}
				font_outline_color.a = faded_visibility;
				font_shadow_color.a = faded_visibility;
			}

			bool txt_visible = (font_outline_color.a != 0) || (font_shadow_color.a != 0);

			for (int j = 0; j < fx_stack.size(); j++) {
				ItemFX *item_fx = fx_stack[j];
				bool cn = cprev_cluster || (cprev_conn && item_fx->connected);

				if (item_fx->type == ITEM_CUSTOMFX && custom_fx_ok) {
					ItemCustomFX *item_custom = static_cast<ItemCustomFX *>(item_fx);

					Ref<CharFXTransform> charfx = item_custom->char_fx_transform;
					Ref<RichTextEffect> custom_effect = item_custom->custom_effect;

					if (!custom_effect.is_null()) {
						charfx->elapsed_time = item_custom->elapsed_time;
						charfx->range = Vector2i(l.char_offset + glyphs[i].start, l.char_offset + glyphs[i].end);
						charfx->relative_index = l.char_offset + glyphs[i].start - item_fx->char_ofs;
						charfx->visibility = txt_visible;
						charfx->outline = true;
						charfx->font = frid;
						charfx->glyph_index = gl;
						charfx->glyph_flags = gl_fl;
						charfx->glyph_count = gl_cn;
						charfx->offset = fx_offset;
						charfx->color = font_color;

						bool effect_status = custom_effect->_process_effect_impl(charfx);
						custom_fx_ok = effect_status;

						fx_offset += charfx->offset;
						font_color = charfx->color;
						frid = charfx->font;
						gl = charfx->glyph_index;
						txt_visible &= charfx->visibility;
					}
				} else if (item_fx->type == ITEM_SHAKE) {
					ItemShake *item_shake = static_cast<ItemShake *>(item_fx);

					if (!cn) {
						uint64_t char_current_rand = item_shake->offset_random(glyphs[i].start);
						uint64_t char_previous_rand = item_shake->offset_previous_random(glyphs[i].start);
						uint64_t max_rand = 2147483647;
						double current_offset = Math::remap(char_current_rand % max_rand, 0, max_rand, 0.0f, 2.f * (float)Math_PI);
						double previous_offset = Math::remap(char_previous_rand % max_rand, 0, max_rand, 0.0f, 2.f * (float)Math_PI);
						double n_time = (double)(item_shake->elapsed_time / (0.5f / item_shake->rate));
						n_time = (n_time > 1.0) ? 1.0 : n_time;
						item_shake->prev_off = Point2(Math::lerp(Math::sin(previous_offset), Math::sin(current_offset), n_time), Math::lerp(Math::cos(previous_offset), Math::cos(current_offset), n_time)) * (float)item_shake->strength / 10.0f;
					}
					fx_offset += item_shake->prev_off;
				} else if (item_fx->type == ITEM_WAVE) {
					ItemWave *item_wave = static_cast<ItemWave *>(item_fx);

					if (!cn) {
						double value = Math::sin(item_wave->frequency * item_wave->elapsed_time + ((p_ofs.x + gloff.x) / 50)) * (item_wave->amplitude / 10.0f);
						item_wave->prev_off = Point2(0, 1) * value;
					}
					fx_offset += item_wave->prev_off;
				} else if (item_fx->type == ITEM_TORNADO) {
					ItemTornado *item_tornado = static_cast<ItemTornado *>(item_fx);

					if (!cn) {
						double torn_x = Math::sin(item_tornado->frequency * item_tornado->elapsed_time + ((p_ofs.x + gloff.x) / 50)) * (item_tornado->radius);
						double torn_y = Math::cos(item_tornado->frequency * item_tornado->elapsed_time + ((p_ofs.x + gloff.x) / 50)) * (item_tornado->radius);
						item_tornado->prev_off = Point2(torn_x, torn_y);
					}
					fx_offset += item_tornado->prev_off;
				} else if (item_fx->type == ITEM_RAINBOW) {
					ItemRainbow *item_rainbow = static_cast<ItemRainbow *>(item_fx);

					font_color = font_color.from_hsv(item_rainbow->frequency * (item_rainbow->elapsed_time + ((p_ofs.x + gloff.x) / 50)), item_rainbow->saturation, item_rainbow->value, font_color.a);
				}
			}

			// Draw glyph outlines.
			const Color modulated_outline_color = font_outline_color * Color(1, 1, 1, font_color.a);
			const Color modulated_shadow_color = font_shadow_color * Color(1, 1, 1, font_color.a);
			for (int j = 0; j < glyphs[i].repeat; j++) {
				if (txt_visible) {
					bool skip = (trim_chars && l.char_offset + glyphs[i].end > visible_characters) || (trim_glyphs_ltr && (processed_glyphs_ol >= visible_glyphs)) || (trim_glyphs_rtl && (processed_glyphs_ol < total_glyphs - visible_glyphs));
					if (!skip && frid != RID()) {
						if (modulated_shadow_color.a > 0) {
							TS->font_draw_glyph(frid, ci, glyphs[i].font_size, p_ofs + fx_offset + gloff + p_shadow_ofs, gl, modulated_shadow_color);
						}
						if (modulated_shadow_color.a > 0 && p_shadow_outline_size > 0) {
							TS->font_draw_glyph_outline(frid, ci, glyphs[i].font_size, p_shadow_outline_size, p_ofs + fx_offset + gloff + p_shadow_ofs, gl, modulated_shadow_color);
						}
						if (modulated_outline_color.a != 0.0 && size > 0) {
							TS->font_draw_glyph_outline(frid, ci, glyphs[i].font_size, size, p_ofs + fx_offset + gloff, gl, modulated_outline_color);
						}
					}
					processed_glyphs_ol++;
				}
				gloff.x += glyphs[i].advance;
			}
		}

		Vector2 fbg_line_off = off + p_ofs;
		// Draw background color box
		Vector2i chr_range = TS->shaped_text_get_range(rid);
		_draw_fbg_boxes(ci, rid, fbg_line_off, it_from, it_to, chr_range.x, chr_range.y, 0);

		// Draw main text.
		Color selection_bg = theme_cache.selection_color;

		int sel_start = -1;
		int sel_end = -1;

		if (selection.active && (selection.from_frame->lines[selection.from_line].char_offset + selection.from_char) <= (l.char_offset + TS->shaped_text_get_range(rid).y) && (selection.to_frame->lines[selection.to_line].char_offset + selection.to_char) >= (l.char_offset + TS->shaped_text_get_range(rid).x)) {
			sel_start = MAX(TS->shaped_text_get_range(rid).x, (selection.from_frame->lines[selection.from_line].char_offset + selection.from_char) - l.char_offset);
			sel_end = MIN(TS->shaped_text_get_range(rid).y, (selection.to_frame->lines[selection.to_line].char_offset + selection.to_char) - l.char_offset);

			Vector<Vector2> sel = TS->shaped_text_get_selection(rid, sel_start, sel_end);
			for (int i = 0; i < sel.size(); i++) {
				Rect2 rect = Rect2(sel[i].x + p_ofs.x + off.x, p_ofs.y + off.y - TS->shaped_text_get_ascent(rid), sel[i].y - sel[i].x, TS->shaped_text_get_size(rid).y);
				RenderingServer::get_singleton()->canvas_item_add_rect(ci, rect, selection_bg);
			}
		}

		Vector2 ul_start;
		bool ul_started = false;
		Color ul_color;

		Vector2 dot_ul_start;
		bool dot_ul_started = false;
		Color dot_ul_color;

		Vector2 st_start;
		bool st_started = false;
		Color st_color;

		for (int i = 0; i < gl_size; i++) {
			bool selected = selection.active && (sel_start != -1) && (glyphs[i].start >= sel_start) && (glyphs[i].end <= sel_end);
			Item *it = _get_item_at_pos(it_from, it_to, glyphs[i].start);
			Color font_color = _find_color(it, p_base_color);
			if (_find_underline(it) || (_find_meta(it, nullptr) && underline_meta)) {
				if (!ul_started) {
					ul_started = true;
					ul_start = p_ofs + Vector2(off.x, off.y);
					ul_color = font_color;
					ul_color.a *= 0.5;
				}
			} else if (ul_started) {
				ul_started = false;
				float y_off = TS->shaped_text_get_underline_position(rid);
				float underline_width = MAX(1.0, TS->shaped_text_get_underline_thickness(rid) * theme_cache.base_scale);
				draw_line(ul_start + Vector2(0, y_off), p_ofs + Vector2(off.x, off.y + y_off), ul_color, underline_width);
			}
			if (_find_hint(it, nullptr) && underline_hint) {
				if (!dot_ul_started) {
					dot_ul_started = true;
					dot_ul_start = p_ofs + Vector2(off.x, off.y);
					dot_ul_color = font_color;
					dot_ul_color.a *= 0.5;
				}
			} else if (dot_ul_started) {
				dot_ul_started = false;
				float y_off = TS->shaped_text_get_underline_position(rid);
				float underline_width = MAX(1.0, TS->shaped_text_get_underline_thickness(rid) * theme_cache.base_scale);
				draw_dashed_line(dot_ul_start + Vector2(0, y_off), p_ofs + Vector2(off.x, off.y + y_off), dot_ul_color, underline_width, MAX(2.0, underline_width * 2));
			}
			if (_find_strikethrough(it)) {
				if (!st_started) {
					st_started = true;
					st_start = p_ofs + Vector2(off.x, off.y);
					st_color = font_color;
					st_color.a *= 0.5;
				}
			} else if (st_started) {
				st_started = false;
				float y_off = -TS->shaped_text_get_ascent(rid) + TS->shaped_text_get_size(rid).y / 2;
				float underline_width = MAX(1.0, TS->shaped_text_get_underline_thickness(rid) * theme_cache.base_scale);
				draw_line(st_start + Vector2(0, y_off), p_ofs + Vector2(off.x, off.y + y_off), st_color, underline_width);
			}

			// Get FX.
			ItemFade *fade = nullptr;
			Item *fade_item = it;
			while (fade_item) {
				if (fade_item->type == ITEM_FADE) {
					fade = static_cast<ItemFade *>(fade_item);
					break;
				}
				fade_item = fade_item->parent;
			}

			Vector<ItemFX *> fx_stack;
			_fetch_item_fx_stack(it, fx_stack);
			bool custom_fx_ok = true;

			Point2 fx_offset = Vector2(glyphs[i].x_off, glyphs[i].y_off);
			RID frid = glyphs[i].font_rid;
			uint32_t gl = glyphs[i].index;
			uint16_t gl_fl = glyphs[i].flags;
			uint8_t gl_cn = glyphs[i].count;
			bool cprev_cluster = false;
			bool cprev_conn = false;
			if (gl_cn == 0) { // Parts of the same grapheme cluster, always connected.
				cprev_cluster = true;
			}
			if (gl_fl & TextServer::GRAPHEME_IS_RTL) { // Check if previous grapheme cluster is connected.
				if (i > 0 && (glyphs[i - 1].flags & TextServer::GRAPHEME_IS_CONNECTED)) {
					cprev_conn = true;
				}
			} else {
				if (glyphs[i].flags & TextServer::GRAPHEME_IS_CONNECTED) {
					cprev_conn = true;
				}
			}

			//Apply fx.
			if (fade) {
				float faded_visibility = 1.0f;
				if (glyphs[i].start >= fade->starting_index) {
					faded_visibility -= (float)(glyphs[i].start - fade->starting_index) / (float)fade->length;
					faded_visibility = faded_visibility < 0.0f ? 0.0f : faded_visibility;
				}
				font_color.a = faded_visibility;
			}

			bool txt_visible = (font_color.a != 0);

			for (int j = 0; j < fx_stack.size(); j++) {
				ItemFX *item_fx = fx_stack[j];
				bool cn = cprev_cluster || (cprev_conn && item_fx->connected);

				if (item_fx->type == ITEM_CUSTOMFX && custom_fx_ok) {
					ItemCustomFX *item_custom = static_cast<ItemCustomFX *>(item_fx);

					Ref<CharFXTransform> charfx = item_custom->char_fx_transform;
					Ref<RichTextEffect> custom_effect = item_custom->custom_effect;

					if (!custom_effect.is_null()) {
						charfx->elapsed_time = item_custom->elapsed_time;
						charfx->range = Vector2i(l.char_offset + glyphs[i].start, l.char_offset + glyphs[i].end);
						charfx->relative_index = l.char_offset + glyphs[i].start - item_fx->char_ofs;
						charfx->visibility = txt_visible;
						charfx->outline = false;
						charfx->font = frid;
						charfx->glyph_index = gl;
						charfx->glyph_flags = gl_fl;
						charfx->glyph_count = gl_cn;
						charfx->offset = fx_offset;
						charfx->color = font_color;

						bool effect_status = custom_effect->_process_effect_impl(charfx);
						custom_fx_ok = effect_status;

						fx_offset += charfx->offset;
						font_color = charfx->color;
						frid = charfx->font;
						gl = charfx->glyph_index;
						txt_visible &= charfx->visibility;
					}
				} else if (item_fx->type == ITEM_SHAKE) {
					ItemShake *item_shake = static_cast<ItemShake *>(item_fx);

					if (!cn) {
						uint64_t char_current_rand = item_shake->offset_random(glyphs[i].start);
						uint64_t char_previous_rand = item_shake->offset_previous_random(glyphs[i].start);
						uint64_t max_rand = 2147483647;
						double current_offset = Math::remap(char_current_rand % max_rand, 0, max_rand, 0.0f, 2.f * (float)Math_PI);
						double previous_offset = Math::remap(char_previous_rand % max_rand, 0, max_rand, 0.0f, 2.f * (float)Math_PI);
						double n_time = (double)(item_shake->elapsed_time / (0.5f / item_shake->rate));
						n_time = (n_time > 1.0) ? 1.0 : n_time;
						item_shake->prev_off = Point2(Math::lerp(Math::sin(previous_offset), Math::sin(current_offset), n_time), Math::lerp(Math::cos(previous_offset), Math::cos(current_offset), n_time)) * (float)item_shake->strength / 10.0f;
					}
					fx_offset += item_shake->prev_off;
				} else if (item_fx->type == ITEM_WAVE) {
					ItemWave *item_wave = static_cast<ItemWave *>(item_fx);

					if (!cn) {
						double value = Math::sin(item_wave->frequency * item_wave->elapsed_time + ((p_ofs.x + off.x) / 50)) * (item_wave->amplitude / 10.0f);
						item_wave->prev_off = Point2(0, 1) * value;
					}
					fx_offset += item_wave->prev_off;
				} else if (item_fx->type == ITEM_TORNADO) {
					ItemTornado *item_tornado = static_cast<ItemTornado *>(item_fx);

					if (!cn) {
						double torn_x = Math::sin(item_tornado->frequency * item_tornado->elapsed_time + ((p_ofs.x + off.x) / 50)) * (item_tornado->radius);
						double torn_y = Math::cos(item_tornado->frequency * item_tornado->elapsed_time + ((p_ofs.x + off.x) / 50)) * (item_tornado->radius);
						item_tornado->prev_off = Point2(torn_x, torn_y);
					}
					fx_offset += item_tornado->prev_off;
				} else if (item_fx->type == ITEM_RAINBOW) {
					ItemRainbow *item_rainbow = static_cast<ItemRainbow *>(item_fx);

					font_color = font_color.from_hsv(item_rainbow->frequency * (item_rainbow->elapsed_time + ((p_ofs.x + off.x) / 50)), item_rainbow->saturation, item_rainbow->value, font_color.a);
				}
			}

			if (selected && use_selected_font_color) {
				font_color = theme_cache.font_selected_color;
			}

			// Draw glyphs.
			for (int j = 0; j < glyphs[i].repeat; j++) {
				bool skip = (trim_chars && l.char_offset + glyphs[i].end > visible_characters) || (trim_glyphs_ltr && (r_processed_glyphs >= visible_glyphs)) || (trim_glyphs_rtl && (r_processed_glyphs < total_glyphs - visible_glyphs));
				if (txt_visible) {
					if (!skip) {
						if (frid != RID()) {
							TS->font_draw_glyph(frid, ci, glyphs[i].font_size, p_ofs + fx_offset + off, gl, font_color);
						} else if (((glyphs[i].flags & TextServer::GRAPHEME_IS_VIRTUAL) != TextServer::GRAPHEME_IS_VIRTUAL) && ((glyphs[i].flags & TextServer::GRAPHEME_IS_EMBEDDED_OBJECT) != TextServer::GRAPHEME_IS_EMBEDDED_OBJECT)) {
							TS->draw_hex_code_box(ci, glyphs[i].font_size, p_ofs + fx_offset + off, gl, font_color);
						}
					}
					r_processed_glyphs++;
				}
				if (skip) {
					// End underline/overline/strikethrough is previous glyph is skipped.
					if (ul_started) {
						ul_started = false;
						float y_off = TS->shaped_text_get_underline_position(rid);
						float underline_width = MAX(1.0, TS->shaped_text_get_underline_thickness(rid) * theme_cache.base_scale);
						draw_line(ul_start + Vector2(0, y_off), p_ofs + Vector2(off.x, off.y + y_off), ul_color, underline_width);
					}
					if (dot_ul_started) {
						dot_ul_started = false;
						float y_off = TS->shaped_text_get_underline_position(rid);
						float underline_width = MAX(1.0, TS->shaped_text_get_underline_thickness(rid) * theme_cache.base_scale);
						draw_dashed_line(dot_ul_start + Vector2(0, y_off), p_ofs + Vector2(off.x, off.y + y_off), dot_ul_color, underline_width, MAX(2.0, underline_width * 2));
					}
					if (st_started) {
						st_started = false;
						float y_off = -TS->shaped_text_get_ascent(rid) + TS->shaped_text_get_size(rid).y / 2;
						float underline_width = MAX(1.0, TS->shaped_text_get_underline_thickness(rid) * theme_cache.base_scale);
						draw_line(st_start + Vector2(0, y_off), p_ofs + Vector2(off.x, off.y + y_off), st_color, underline_width);
					}
				}
				off.x += glyphs[i].advance;
			}
		}
		if (ul_started) {
			ul_started = false;
			float y_off = TS->shaped_text_get_underline_position(rid);
			float underline_width = MAX(1.0, TS->shaped_text_get_underline_thickness(rid) * theme_cache.base_scale);
			draw_line(ul_start + Vector2(0, y_off), p_ofs + Vector2(off.x, off.y + y_off), ul_color, underline_width);
		}
		if (dot_ul_started) {
			dot_ul_started = false;
			float y_off = TS->shaped_text_get_underline_position(rid);
			float underline_width = MAX(1.0, TS->shaped_text_get_underline_thickness(rid) * theme_cache.base_scale);
			draw_dashed_line(dot_ul_start + Vector2(0, y_off), p_ofs + Vector2(off.x, off.y + y_off), dot_ul_color, underline_width, MAX(2.0, underline_width * 2));
		}
		if (st_started) {
			st_started = false;
			float y_off = -TS->shaped_text_get_ascent(rid) + TS->shaped_text_get_size(rid).y / 2;
			float underline_width = MAX(1.0, TS->shaped_text_get_underline_thickness(rid) * theme_cache.base_scale);
			draw_line(st_start + Vector2(0, y_off), p_ofs + Vector2(off.x, off.y + y_off), st_color, underline_width);
		}
		// Draw foreground color box
		_draw_fbg_boxes(ci, rid, fbg_line_off, it_from, it_to, chr_range.x, chr_range.y, 1);

		off.y += TS->shaped_text_get_descent(rid);
	}

	return line_count;
}

void SelectableRichTextLabel::_find_click(ItemFrame *p_frame, const Point2i &p_click, ItemFrame **r_click_frame, int *r_click_line, Item **r_click_item, int *r_click_char, bool *r_outside, bool p_meta) {
	if (r_click_item) {
		*r_click_item = nullptr;
	}
	if (r_click_char != nullptr) {
		*r_click_char = 0;
	}
	if (r_outside != nullptr) {
		*r_outside = true;
	}

	Size2 size = get_size();
	Rect2 text_rect = _get_text_rect();

	int vofs = vscroll->get_value();

	// Search for the first line.
	int to_line = main->first_invalid_line.load();
	int from_line = _find_first_line(0, to_line, vofs);

	Point2 ofs = text_rect.get_position() + Vector2(0, main->lines[from_line].offset.y - vofs);
	while (ofs.y < size.height && from_line < to_line) {
		MutexLock lock(main->lines[from_line].text_buf->get_mutex());
		_find_click_in_line(p_frame, from_line, ofs, text_rect.size.x, p_click, r_click_frame, r_click_line, r_click_item, r_click_char, false, p_meta);
		ofs.y += main->lines[from_line].text_buf->get_size().y + main->lines[from_line].text_buf->get_line_count() * theme_cache.line_separation;
		if (((r_click_item != nullptr) && ((*r_click_item) != nullptr)) || ((r_click_frame != nullptr) && ((*r_click_frame) != nullptr))) {
			if (r_outside != nullptr) {
				*r_outside = false;
			}
			return;
		}
		from_line++;
	}
}

float SelectableRichTextLabel::_find_click_in_line(ItemFrame *p_frame, int p_line, const Vector2 &p_ofs, int p_width, const Point2i &p_click, ItemFrame **r_click_frame, int *r_click_line, Item **r_click_item, int *r_click_char, bool p_table, bool p_meta) {
	Vector2 off;

	bool line_clicked = false;
	float text_rect_begin = 0.0;
	int char_pos = -1;
	Line &l = p_frame->lines[p_line];
	MutexLock lock(l.text_buf->get_mutex());

	bool rtl = (l.text_buf->get_direction() == TextServer::DIRECTION_RTL);
	bool lrtl = is_layout_rtl();

	// Table hit test results.
	bool table_hit = false;
	Vector2i table_range;
	float table_offy = 0.f;
	ItemFrame *table_click_frame = nullptr;
	int table_click_line = -1;
	Item *table_click_item = nullptr;
	int table_click_char = -1;

	for (int line = 0; line < l.text_buf->get_line_count(); line++) {
		RID rid = l.text_buf->get_line_rid(line);

		float width = l.text_buf->get_width();
		float length = TS->shaped_text_get_width(rid);

		if (rtl) {
			off.x = p_width - l.offset.x - width;
			if (!lrtl && p_frame == main) { // Skip Scrollbar.
				off.x -= scroll_w;
			}
		} else {
			off.x = l.offset.x;
			if (lrtl && p_frame == main) { // Skip Scrollbar.
				off.x += scroll_w;
			}
		}

		switch (l.text_buf->get_alignment()) {
			case HORIZONTAL_ALIGNMENT_FILL:
			case HORIZONTAL_ALIGNMENT_LEFT: {
				if (rtl) {
					off.x += width - length;
				}
			} break;
			case HORIZONTAL_ALIGNMENT_CENTER: {
				off.x += Math::floor((width - length) / 2.0);
			} break;
			case HORIZONTAL_ALIGNMENT_RIGHT: {
				if (!rtl) {
					off.x += width - length;
				}
			} break;
		}
		// Adjust for dropcap.
		int dc_lines = l.text_buf->get_dropcap_lines();
		float h_off = l.text_buf->get_dropcap_size().x;
		if (line <= dc_lines) {
			if (rtl) {
				off.x -= h_off;
			} else {
				off.x += h_off;
			}
		}
		off.y += TS->shaped_text_get_ascent(rid);

		Array objects = TS->shaped_text_get_objects(rid);
		for (int i = 0; i < objects.size(); i++) {
			Item *it = reinterpret_cast<Item *>((uint64_t)objects[i]);
			if (it != nullptr) {
				Rect2 rect = TS->shaped_text_get_object_rect(rid, objects[i]);
				rect.position += p_ofs + off;
				if (p_click.y >= rect.position.y && p_click.y <= rect.position.y + rect.size.y) {
					switch (it->type) {
						case ITEM_TABLE: {
							ItemTable *table = static_cast<ItemTable *>(it);

							int idx = 0;
							int col_count = table->columns.size();
							int row_count = table->rows.size();

							for (Item *E : table->subitems) {
								ItemFrame *frame = static_cast<ItemFrame *>(E);

								int col = idx % col_count;
								int row = idx / col_count;

								if (frame->lines.size() != 0 && row < row_count) {
									Vector2 coff = frame->lines[0].offset;
									if (rtl) {
										coff.x = rect.size.width - table->columns[col].width - coff.x;
									}
									Rect2 crect = Rect2(rect.position + coff - frame->padding.position, Size2(table->columns[col].width + theme_cache.table_h_separation, table->rows[row] + theme_cache.table_v_separation) + frame->padding.position + frame->padding.size);
									if (col == col_count - 1) {
										if (rtl) {
											crect.size.x = crect.position.x + crect.size.x;
											crect.position.x = 0;
										} else {
											crect.size.x = get_size().x;
										}
									}
									if (crect.has_point(p_click)) {
										for (int j = 0; j < (int)frame->lines.size(); j++) {
											_find_click_in_line(frame, j, rect.position + Vector2(frame->padding.position.x, frame->lines[j].offset.y), rect.size.x, p_click, &table_click_frame, &table_click_line, &table_click_item, &table_click_char, true, p_meta);
											if (table_click_frame && table_click_item) {
												// Save cell detected cell hit data.
												table_range = Vector2i(INT32_MAX, 0);
												for (Item *F : table->subitems) {
													ItemFrame *sub_frame = static_cast<ItemFrame *>(F);
													for (int k = 0; k < (int)sub_frame->lines.size(); k++) {
														table_range.x = MIN(table_range.x, sub_frame->lines[k].char_offset);
														table_range.y = MAX(table_range.y, sub_frame->lines[k].char_offset + sub_frame->lines[k].char_count);
													}
												}
												table_offy = off.y;
												table_hit = true;
											}
										}
									}
								}
								idx++;
							}
						} break;
						default:
							break;
					}
				}
			}
		}
		Rect2 rect = Rect2(p_ofs + off - Vector2(0, TS->shaped_text_get_ascent(rid)) - p_frame->padding.position, TS->shaped_text_get_size(rid) + p_frame->padding.position + p_frame->padding.size);
		if (p_table) {
			rect.size.y += theme_cache.table_v_separation;
		}

		if (p_click.y >= rect.position.y && p_click.y <= rect.position.y + rect.size.y) {
			if ((!rtl && p_click.x >= rect.position.x) || (rtl && p_click.x <= rect.position.x + rect.size.x)) {
				if (p_meta) {
					int64_t glyph_idx = TS->shaped_text_hit_test_grapheme(rid, p_click.x - rect.position.x);
					if (glyph_idx >= 0) {
						const Glyph *glyphs = TS->shaped_text_get_glyphs(rid);
						char_pos = glyphs[glyph_idx].start;
					}
				} else {
					char_pos = TS->shaped_text_hit_test_position(rid, p_click.x - rect.position.x);
				}
			}
			line_clicked = true;
			text_rect_begin = rtl ? rect.position.x + rect.size.x : rect.position.x;
		}

		// If table hit was detected, and line hit is in the table bounds use table hit.
		if (table_hit && (((char_pos + p_frame->lines[p_line].char_offset) >= table_range.x && (char_pos + p_frame->lines[p_line].char_offset) <= table_range.y) || char_pos == -1)) {
			if (r_click_frame != nullptr) {
				*r_click_frame = table_click_frame;
			}

			if (r_click_line != nullptr) {
				*r_click_line = table_click_line;
			}

			if (r_click_item != nullptr) {
				*r_click_item = table_click_item;
			}

			if (r_click_char != nullptr) {
				*r_click_char = table_click_char;
			}
			return table_offy;
		}

		off.y += TS->shaped_text_get_descent(rid) + theme_cache.line_separation;
	}

	// Text line hit.
	if (line_clicked) {
		// Find item.
		if (r_click_item != nullptr) {
			Item *it = p_frame->lines[p_line].from;
			Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;
			if (char_pos >= 0) {
				*r_click_item = _get_item_at_pos(it, it_to, char_pos);
			} else {
				int stop = text_rect_begin;
				*r_click_item = _find_indentable(it);
				while (*r_click_item) {
					Ref<Font> font = theme_cache.normal_font;
					int font_size = theme_cache.normal_font_size;
					ItemFont *font_it = _find_font(*r_click_item);
					if (font_it) {
						if (font_it->font.is_valid()) {
							font = font_it->font;
						}
						if (font_it->font_size > 0) {
							font_size = font_it->font_size;
						}
					}
					ItemFontSize *font_size_it = _find_font_size(*r_click_item);
					if (font_size_it && font_size_it->font_size > 0) {
						font_size = font_size_it->font_size;
					}
					if (rtl) {
						stop += tab_size * font->get_char_size(' ', font_size).width;
						if (stop > p_click.x) {
							break;
						}
					} else {
						stop -= tab_size * font->get_char_size(' ', font_size).width;
						if (stop < p_click.x) {
							break;
						}
					}
					*r_click_item = _find_indentable((*r_click_item)->parent);
				}
			}
		}

		if (r_click_frame != nullptr) {
			*r_click_frame = p_frame;
		}

		if (r_click_line != nullptr) {
			*r_click_line = p_line;
		}

		if (r_click_char != nullptr) {
			*r_click_char = char_pos;
		}
	}

	return off.y;
}

void SelectableRichTextLabel::_scroll_changed(double) {
	if (updating_scroll) {
		return;
	}

	if (scroll_follow && vscroll->get_value() >= (vscroll->get_max() - vscroll->get_page())) {
		scroll_following = true;
	} else {
		scroll_following = false;
	}

	scroll_updated = true;

	queue_redraw();
}

void SelectableRichTextLabel::_update_fx(SelectableRichTextLabel::ItemFrame *p_frame, double p_delta_time) {
	Item *it = p_frame;
	while (it) {
		ItemFX *ifx = nullptr;

		if (it->type == ITEM_CUSTOMFX || it->type == ITEM_SHAKE || it->type == ITEM_WAVE || it->type == ITEM_TORNADO || it->type == ITEM_RAINBOW) {
			ifx = static_cast<ItemFX *>(it);
		}

		if (!ifx) {
			it = _get_next_item(it, true);
			continue;
		}

		ifx->elapsed_time += p_delta_time;

		ItemShake *shake = nullptr;

		if (it->type == ITEM_SHAKE) {
			shake = static_cast<ItemShake *>(it);
		}

		if (shake) {
			bool cycle = (shake->elapsed_time > (1.0f / shake->rate));
			if (cycle) {
				shake->elapsed_time -= (1.0f / shake->rate);
				shake->reroll_random();
			}
		}

		it = _get_next_item(it, true);
	}
}

int SelectableRichTextLabel::_find_first_line(int p_from, int p_to, int p_vofs) const {
	int l = p_from;
	int r = p_to;
	while (l < r) {
		int m = Math::floor(double(l + r) / 2.0);
		MutexLock lock(main->lines[m].text_buf->get_mutex());
		int ofs = _calculate_line_vertical_offset(main->lines[m]);
		if (ofs < p_vofs) {
			l = m + 1;
		} else {
			r = m;
		}
	}
	return MIN(l, (int)main->lines.size() - 1);
}

_FORCE_INLINE_ float SelectableRichTextLabel::_calculate_line_vertical_offset(const SelectableRichTextLabel::Line &line) const {
	return line.get_height(theme_cache.line_separation);
}

void SelectableRichTextLabel::_update_theme_item_cache() {
	Control::_update_theme_item_cache();

	theme_cache.normal_style = get_theme_stylebox(SNAME("normal"));
	theme_cache.focus_style = get_theme_stylebox(SNAME("focus"));
	theme_cache.progress_bg_style = get_theme_stylebox(SNAME("background"), SNAME("ProgressBar"));
	theme_cache.progress_fg_style = get_theme_stylebox(SNAME("fill"), SNAME("ProgressBar"));

	theme_cache.line_separation = get_theme_constant(SNAME("line_separation"));

	theme_cache.normal_font = get_theme_font(SNAME("normal_font"));
	theme_cache.normal_font_size = get_theme_font_size(SNAME("normal_font_size"));

	theme_cache.default_color = get_theme_color(SNAME("default_color"));
	theme_cache.font_selected_color = get_theme_color(SNAME("font_selected_color"));
	use_selected_font_color = theme_cache.font_selected_color != Color(0, 0, 0, 0);
	theme_cache.selection_color = get_theme_color(SNAME("selection_color"));
	theme_cache.font_outline_color = get_theme_color(SNAME("font_outline_color"));
	theme_cache.font_shadow_color = get_theme_color(SNAME("font_shadow_color"));
	theme_cache.shadow_outline_size = get_theme_constant(SNAME("shadow_outline_size"));
	theme_cache.shadow_offset_x = get_theme_constant(SNAME("shadow_offset_x"));
	theme_cache.shadow_offset_y = get_theme_constant(SNAME("shadow_offset_y"));
	theme_cache.outline_size = get_theme_constant(SNAME("outline_size"));

	theme_cache.bold_font = get_theme_font(SNAME("bold_font"));
	theme_cache.bold_font_size = get_theme_font_size(SNAME("bold_font_size"));
	theme_cache.bold_italics_font = get_theme_font(SNAME("bold_italics_font"));
	theme_cache.bold_italics_font_size = get_theme_font_size(SNAME("bold_italics_font_size"));
	theme_cache.italics_font = get_theme_font(SNAME("italics_font"));
	theme_cache.italics_font_size = get_theme_font_size(SNAME("italics_font_size"));
	theme_cache.mono_font = get_theme_font(SNAME("mono_font"));
	theme_cache.mono_font_size = get_theme_font_size(SNAME("mono_font_size"));

	theme_cache.table_h_separation = get_theme_constant(SNAME("table_h_separation"));
	theme_cache.table_v_separation = get_theme_constant(SNAME("table_v_separation"));
	theme_cache.table_odd_row_bg = get_theme_color(SNAME("table_odd_row_bg"));
	theme_cache.table_even_row_bg = get_theme_color(SNAME("table_even_row_bg"));
	theme_cache.table_border = get_theme_color(SNAME("table_border"));

	theme_cache.base_scale = get_theme_default_base_scale();
}

void SelectableRichTextLabel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_MOUSE_EXIT: {
			if (meta_hovering) {
				meta_hovering = nullptr;
				emit_signal(SNAME("meta_hover_ended"), current_meta);
				current_meta = false;
				queue_redraw();
			}
		} break;

		case NOTIFICATION_RESIZED: {
			_stop_thread();
			main->first_resized_line.store(0); //invalidate ALL
			queue_redraw();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_stop_thread();
			main->first_invalid_font_line.store(0); //invalidate ALL
			queue_redraw();
		} break;

		case NOTIFICATION_ENTER_TREE: {
			_stop_thread();
			if (!text.is_empty()) {
				set_text(text);
			}

			main->first_invalid_line.store(0); //invalidate ALL
			queue_redraw();
		} break;

		case NOTIFICATION_PREDELETE:
		case NOTIFICATION_EXIT_TREE: {
			_stop_thread();
		} break;

		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED: {
			_apply_translation();
			queue_redraw();
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (is_visible_in_tree()) {
				queue_redraw();
			}
		} break;

		case NOTIFICATION_DRAW: {
			RID ci = get_canvas_item();
			Size2 size = get_size();

			draw_style_box(theme_cache.normal_style, Rect2(Point2(), size));

			if (has_focus()) {
				RenderingServer::get_singleton()->canvas_item_add_clip_ignore(ci, true);
				draw_style_box(theme_cache.focus_style, Rect2(Point2(), size));
				RenderingServer::get_singleton()->canvas_item_add_clip_ignore(ci, false);
			}

			// Start text shaping.
			if (_validate_line_caches()) {
				set_physics_process_internal(false); // Disable auto refresh, if text is fully processed.
			} else {
				// Draw loading progress bar.
				if ((progress_delay > 0) && (OS::get_singleton()->get_ticks_msec() - loading_started >= (uint64_t)progress_delay)) {
					Vector2 p_size = Vector2(size.width - (theme_cache.normal_style->get_offset().x + vscroll->get_combined_minimum_size().width) * 2, vscroll->get_combined_minimum_size().width);
					Vector2 p_pos = Vector2(theme_cache.normal_style->get_offset().x, size.height - theme_cache.normal_style->get_offset().y - vscroll->get_combined_minimum_size().width);

					draw_style_box(theme_cache.progress_bg_style, Rect2(p_pos, p_size));

					bool right_to_left = is_layout_rtl();
					double r = loaded.load();
					int mp = theme_cache.progress_fg_style->get_minimum_size().width;
					int p = round(r * (p_size.width - mp));
					if (right_to_left) {
						int p_remaining = round((1.0 - r) * (p_size.width - mp));
						draw_style_box(theme_cache.progress_fg_style, Rect2(p_pos + Point2(p_remaining, 0), Size2(p + theme_cache.progress_fg_style->get_minimum_size().width, p_size.height)));
					} else {
						draw_style_box(theme_cache.progress_fg_style, Rect2(p_pos, Size2(p + theme_cache.progress_fg_style->get_minimum_size().width, p_size.height)));
					}
				}
			}

			// Draw main text.
			Rect2 text_rect = _get_text_rect();
			float vofs = vscroll->get_value();

			// Search for the first line.
			int to_line = main->first_invalid_line.load();
			int from_line = _find_first_line(0, to_line, vofs);

			Point2 shadow_ofs(theme_cache.shadow_offset_x, theme_cache.shadow_offset_y);

			visible_paragraph_count = 0;
			visible_line_count = 0;

			// New cache draw.
			Point2 ofs = text_rect.get_position() + Vector2(0, main->lines[from_line].offset.y - vofs);
			int processed_glyphs = 0;
			while (ofs.y < size.height && from_line < to_line) {
				MutexLock lock(main->lines[from_line].text_buf->get_mutex());

				visible_paragraph_count++;
				visible_line_count += _draw_line(main, from_line, ofs, text_rect.size.x, theme_cache.default_color, theme_cache.outline_size, theme_cache.font_outline_color, theme_cache.font_shadow_color, theme_cache.shadow_outline_size, shadow_ofs, processed_glyphs);
				ofs.y += main->lines[from_line].text_buf->get_size().y + main->lines[from_line].text_buf->get_line_count() * theme_cache.line_separation;
				from_line++;
			}
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			if (is_visible_in_tree()) {
				if (!is_ready()) {
					return;
				}
				double dt = get_process_delta_time();
				_update_fx(main, dt);
				queue_redraw();
			}
		} break;

		case NOTIFICATION_FOCUS_EXIT: {
			if (deselect_on_focus_loss_enabled) {
				deselect();
			}
		} break;

		case NOTIFICATION_DRAG_END: {
			selection.drag_attempt = false;
		} break;
	}
}

Control::CursorShape SelectableRichTextLabel::get_cursor_shape(const Point2 &p_pos) const {
	if (selection.click_item) {
		return CURSOR_IBEAM;
	}

	Item *item = nullptr;
	bool outside = true;
	const_cast<SelectableRichTextLabel *>(this)->_find_click(main, p_pos, nullptr, nullptr, &item, nullptr, &outside, true);

	if (item && !outside && const_cast<SelectableRichTextLabel *>(this)->_find_meta(item, nullptr)) {
		return CURSOR_POINTING_HAND;
	}
	return get_default_cursor_shape();
}

void SelectableRichTextLabel::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseButton> b = p_event;

	if (b.is_valid()) {
		if (b->get_button_index() == MouseButton::LEFT) {
			if (b->is_pressed() && !b->is_double_click()) {
				scroll_updated = false;
				ItemFrame *c_frame = nullptr;
				int c_line = 0;
				Item *c_item = nullptr;
				int c_index = 0;
				bool outside;

				selection.drag_attempt = false;

				_find_click(main, b->get_position(), &c_frame, &c_line, &c_item, &c_index, &outside, false);
				if (c_item != nullptr) {
					if (selection.enabled) {
						selection.click_frame = c_frame;
						selection.click_item = c_item;
						selection.click_line = c_line;
						selection.click_char = c_index;

						// Erase previous selection.
						if (selection.active) {
							selection.from_frame = nullptr;
							selection.from_line = 0;
							selection.from_item = nullptr;
							selection.from_char = 0;
							selection.to_frame = nullptr;
							selection.to_line = 0;
							selection.to_item = nullptr;
							selection.to_char = 0;
							deselect();
						}
					}
				}
			} else if (b->is_pressed() && b->is_double_click() && selection.enabled) {
				//double_click: select word

				ItemFrame *c_frame = nullptr;
				int c_line = 0;
				Item *c_item = nullptr;
				int c_index = 0;
				bool outside;

				selection.drag_attempt = false;

				_find_click(main, b->get_position(), &c_frame, &c_line, &c_item, &c_index, &outside, false);

				if (c_frame) {
					const Line &l = c_frame->lines[c_line];
					MutexLock lock(l.text_buf->get_mutex());
					PackedInt32Array words = TS->shaped_text_get_word_breaks(l.text_buf->get_rid());
					for (int i = 0; i < words.size(); i = i + 2) {
						if (c_index >= words[i] && c_index < words[i + 1]) {
							selection.from_frame = c_frame;
							selection.from_line = c_line;
							selection.from_item = c_item;
							selection.from_char = words[i];

							selection.to_frame = c_frame;
							selection.to_line = c_line;
							selection.to_item = c_item;
							selection.to_char = words[i + 1];

							selection.active = true;
							emit_signal("selection_active", true);
							if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CLIPBOARD_PRIMARY)) {
								DisplayServer::get_singleton()->clipboard_set_primary(get_selected_text());
							}
							queue_redraw();
							break;
						}
					}
				}
			} else if (!b->is_pressed()) {
				if (selection.enabled && DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CLIPBOARD_PRIMARY)) {
					DisplayServer::get_singleton()->clipboard_set_primary(get_selected_text());
				}
				selection.click_item = nullptr;
				if (selection.drag_attempt) {
					selection.drag_attempt = false;
					if (_is_click_inside_selection()) {
						selection.from_frame = nullptr;
						selection.from_line = 0;
						selection.from_item = nullptr;
						selection.from_char = 0;
						selection.to_frame = nullptr;
						selection.to_line = 0;
						selection.to_item = nullptr;
						selection.to_char = 0;
						deselect();
					}
				}
				if (!b->is_double_click() && !scroll_updated && !selection.active) {
					Item *c_item = nullptr;

					bool outside = true;
					_find_click(main, b->get_position(), nullptr, nullptr, &c_item, nullptr, &outside, true);

					if (c_item) {
						Variant meta;
						if (!outside && _find_meta(c_item, &meta)) {
							//meta clicked
							emit_signal(SNAME("meta_clicked"), meta);
						}
					}
				}
			}
		}

		if (b->get_button_index() == MouseButton::WHEEL_UP) {
			if (scroll_active) {
				vscroll->set_value(vscroll->get_value() - vscroll->get_page() * b->get_factor() * 0.5 / 8);
			}
		}
		if (b->get_button_index() == MouseButton::WHEEL_DOWN) {
			if (scroll_active) {
				vscroll->set_value(vscroll->get_value() + vscroll->get_page() * b->get_factor() * 0.5 / 8);
			}
		}
		if (b->get_button_index() == MouseButton::RIGHT && context_menu_enabled) {
			_update_context_menu();
			menu->set_position(get_screen_position() + b->get_position());
			menu->reset_size();
			menu->popup();
			grab_focus();
		}
	}

	Ref<InputEventPanGesture> pan_gesture = p_event;
	if (pan_gesture.is_valid()) {
		if (scroll_active) {
			vscroll->set_value(vscroll->get_value() + vscroll->get_page() * pan_gesture->get_delta().y * 0.5 / 8);
		}

		return;
	}

	Ref<InputEventKey> k = p_event;

	if (k.is_valid()) {
		if (k->is_pressed()) {
			bool handled = false;

			if (k->is_action("ui_page_up", true) && vscroll->is_visible_in_tree()) {
				vscroll->set_value(vscroll->get_value() - vscroll->get_page());
				handled = true;
			}
			if (k->is_action("ui_page_down", true) && vscroll->is_visible_in_tree()) {
				vscroll->set_value(vscroll->get_value() + vscroll->get_page());
				handled = true;
			}
			if (k->is_action("ui_up", true) && vscroll->is_visible_in_tree()) {
				vscroll->set_value(vscroll->get_value() - theme_cache.normal_font->get_height(theme_cache.normal_font_size));
				handled = true;
			}
			if (k->is_action("ui_down", true) && vscroll->is_visible_in_tree()) {
				vscroll->set_value(vscroll->get_value() + theme_cache.normal_font->get_height(theme_cache.normal_font_size));
				handled = true;
			}
			if (k->is_action("ui_home", true) && vscroll->is_visible_in_tree()) {
				vscroll->set_value(0);
				handled = true;
			}
			if (k->is_action("ui_end", true) && vscroll->is_visible_in_tree()) {
				vscroll->set_value(vscroll->get_max());
				handled = true;
			}
			if (is_shortcut_keys_enabled()) {
				if (k->is_action("ui_text_select_all", true)) {
					select_all();
					handled = true;
				}
				if (k->is_action("ui_copy", true)) {
					selection_copy();
					handled = true;
				}
			}
			if (k->is_action("ui_menu", true)) {
				if (context_menu_enabled) {
					_update_context_menu();
					menu->set_position(get_screen_position());
					menu->reset_size();
					menu->popup();
					menu->grab_focus();
				}
				handled = true;
			}

			if (handled) {
				accept_event();
			}
		}
	}

	Ref<InputEventMouseMotion> m = p_event;
	if (m.is_valid()) {
		ItemFrame *c_frame = nullptr;
		int c_line = 0;
		Item *c_item = nullptr;
		int c_index = 0;
		bool outside;

		_find_click(main, m->get_position(), &c_frame, &c_line, &c_item, &c_index, &outside, false);
		if (selection.click_item && c_item) {
			selection.from_frame = selection.click_frame;
			selection.from_line = selection.click_line;
			selection.from_item = selection.click_item;
			selection.from_char = selection.click_char;

			selection.to_frame = c_frame;
			selection.to_line = c_line;
			selection.to_item = c_item;
			selection.to_char = c_index;

			bool swap = false;
			if (selection.click_frame && c_frame) {
				const Line &l1 = c_frame->lines[c_line];
				const Line &l2 = selection.click_frame->lines[selection.click_line];
				if (l1.char_offset + c_index < l2.char_offset + selection.click_char) {
					swap = true;
				} else if (l1.char_offset + c_index == l2.char_offset + selection.click_char) {
					deselect();
					return;
				}
			}

			if (swap) {
				SWAP(selection.from_frame, selection.to_frame);
				SWAP(selection.from_line, selection.to_line);
				SWAP(selection.from_item, selection.to_item);
				SWAP(selection.from_char, selection.to_char);
			}

			selection.active = true;
			emit_signal("selection_active", true);
			queue_redraw();
		}

		Variant meta;
		ItemMeta *item_meta;
		if (c_item && !outside && _find_meta(c_item, &meta, &item_meta)) {
			if (meta_hovering != item_meta) {
				if (meta_hovering) {
					emit_signal(SNAME("meta_hover_ended"), current_meta);
				}
				meta_hovering = item_meta;
				current_meta = meta;
				emit_signal(SNAME("meta_hover_started"), meta);
			}
		} else if (meta_hovering) {
			meta_hovering = nullptr;
			emit_signal(SNAME("meta_hover_ended"), current_meta);
			current_meta = false;
		}
	}
}

String SelectableRichTextLabel::get_tooltip(const Point2 &p_pos) const {
	Item *c_item = nullptr;
	bool outside;

	const_cast<SelectableRichTextLabel *>(this)->_find_click(main, p_pos, nullptr, nullptr, &c_item, nullptr, &outside, true);

	String description;
	if (c_item && !outside && const_cast<SelectableRichTextLabel *>(this)->_find_hint(c_item, &description)) {
		return description;
	} else {
		return Control::get_tooltip(p_pos);
	}
}

void SelectableRichTextLabel::_find_frame(Item *p_item, ItemFrame **r_frame, int *r_line) {
	if (r_frame != nullptr) {
		*r_frame = nullptr;
	}
	if (r_line != nullptr) {
		*r_line = 0;
	}

	Item *item = p_item;

	while (item) {
		if (item->parent != nullptr && item->parent->type == ITEM_FRAME) {
			if (r_frame != nullptr) {
				*r_frame = static_cast<ItemFrame *>(item->parent);
			}
			if (r_line != nullptr) {
				*r_line = item->line;
			}
			return;
		}

		item = item->parent;
	}
}

SelectableRichTextLabel::Item *SelectableRichTextLabel::_find_indentable(Item *p_item) {
	Item *indentable = p_item;

	while (indentable) {
		if (indentable->type == ITEM_INDENT || indentable->type == ITEM_LIST) {
			return indentable;
		}
		indentable = indentable->parent;
	}

	return indentable;
}

SelectableRichTextLabel::ItemFont *SelectableRichTextLabel::_find_font(Item *p_item) {
	Item *fontitem = p_item;

	while (fontitem) {
		if (fontitem->type == ITEM_FONT) {
			ItemFont *fi = static_cast<ItemFont *>(fontitem);
			switch (fi->def_font) {
				case NORMAL_FONT: {
					if (fi->variation) {
						Ref<FontVariation> fc = fi->font;
						if (fc.is_valid()) {
							fc->set_base_font(theme_cache.normal_font);
						}
					} else {
						fi->font = theme_cache.normal_font;
					}
					if (fi->def_size) {
						fi->font_size = theme_cache.normal_font_size;
					}
				} break;
				case BOLD_FONT: {
					if (fi->variation) {
						Ref<FontVariation> fc = fi->font;
						if (fc.is_valid()) {
							fc->set_base_font(theme_cache.bold_font);
						}
					} else {
						fi->font = theme_cache.bold_font;
					}
					if (fi->def_size) {
						fi->font_size = theme_cache.bold_font_size;
					}
				} break;
				case ITALICS_FONT: {
					if (fi->variation) {
						Ref<FontVariation> fc = fi->font;
						if (fc.is_valid()) {
							fc->set_base_font(theme_cache.italics_font);
						}
					} else {
						fi->font = theme_cache.italics_font;
					}
					if (fi->def_size) {
						fi->font_size = theme_cache.italics_font_size;
					}
				} break;
				case BOLD_ITALICS_FONT: {
					if (fi->variation) {
						Ref<FontVariation> fc = fi->font;
						if (fc.is_valid()) {
							fc->set_base_font(theme_cache.bold_italics_font);
						}
					} else {
						fi->font = theme_cache.bold_italics_font;
					}
					if (fi->def_size) {
						fi->font_size = theme_cache.bold_italics_font_size;
					}
				} break;
				case MONO_FONT: {
					if (fi->variation) {
						Ref<FontVariation> fc = fi->font;
						if (fc.is_valid()) {
							fc->set_base_font(theme_cache.mono_font);
						}
					} else {
						fi->font = theme_cache.mono_font;
					}
					if (fi->def_size) {
						fi->font_size = theme_cache.mono_font_size;
					}
				} break;
				default: {
				} break;
			}
			return fi;
		}

		fontitem = fontitem->parent;
	}

	return nullptr;
}

SelectableRichTextLabel::ItemFontSize *SelectableRichTextLabel::_find_font_size(Item *p_item) {
	Item *sizeitem = p_item;

	while (sizeitem) {
		if (sizeitem->type == ITEM_FONT_SIZE) {
			ItemFontSize *fi = static_cast<ItemFontSize *>(sizeitem);
			return fi;
		}

		sizeitem = sizeitem->parent;
	}

	return nullptr;
}

int SelectableRichTextLabel::_find_outline_size(Item *p_item, int p_default) {
	Item *sizeitem = p_item;

	while (sizeitem) {
		if (sizeitem->type == ITEM_OUTLINE_SIZE) {
			ItemOutlineSize *fi = static_cast<ItemOutlineSize *>(sizeitem);
			return fi->outline_size;
		}

		sizeitem = sizeitem->parent;
	}

	return p_default;
}

SelectableRichTextLabel::ItemDropcap *SelectableRichTextLabel::_find_dc_item(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_DROPCAP) {
			return static_cast<ItemDropcap *>(item);
		}
		item = item->parent;
	}

	return nullptr;
}

SelectableRichTextLabel::ItemList *SelectableRichTextLabel::_find_list_item(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_LIST) {
			return static_cast<ItemList *>(item);
		}
		item = item->parent;
	}

	return nullptr;
}

int SelectableRichTextLabel::_find_list(Item *p_item, Vector<int> &r_index, Vector<ItemList *> &r_list) {
	Item *item = p_item;
	Item *prev_item = p_item;

	int level = 0;

	while (item) {
		if (item->type == ITEM_LIST) {
			ItemList *list = static_cast<ItemList *>(item);

			ItemFrame *frame = nullptr;
			int line = -1;
			_find_frame(list, &frame, &line);

			int index = 1;
			if (frame != nullptr) {
				for (int i = list->line + 1; i <= prev_item->line && i < (int)frame->lines.size(); i++) {
					if (_find_list_item(frame->lines[i].from) == list) {
						index++;
					}
				}
			}

			r_index.push_back(index);
			r_list.push_back(list);

			prev_item = item;
		}
		level++;
		item = item->parent;
	}

	return level;
}

int SelectableRichTextLabel::_find_margin(Item *p_item, const Ref<Font> &p_base_font, int p_base_font_size) {
	Item *item = p_item;

	float margin = 0.0;

	while (item) {
		if (item->type == ITEM_INDENT) {
			Ref<Font> font = p_base_font;
			int font_size = p_base_font_size;

			ItemFont *font_it = _find_font(item);
			if (font_it) {
				if (font_it->font.is_valid()) {
					font = font_it->font;
				}
				if (font_it->font_size > 0) {
					font_size = font_it->font_size;
				}
			}
			ItemFontSize *font_size_it = _find_font_size(item);
			if (font_size_it && font_size_it->font_size > 0) {
				font_size = font_size_it->font_size;
			}
			margin += tab_size * font->get_char_size(' ', font_size).width;

		} else if (item->type == ITEM_LIST) {
			Ref<Font> font = p_base_font;
			int font_size = p_base_font_size;

			ItemFont *font_it = _find_font(item);
			if (font_it) {
				if (font_it->font.is_valid()) {
					font = font_it->font;
				}
				if (font_it->font_size > 0) {
					font_size = font_it->font_size;
				}
			}
			ItemFontSize *font_size_it = _find_font_size(item);
			if (font_size_it && font_size_it->font_size > 0) {
				font_size = font_size_it->font_size;
			}
			margin += tab_size * font->get_char_size(' ', font_size).width;
		}

		item = item->parent;
	}

	return margin;
}

BitField<TextServer::JustificationFlag> SelectableRichTextLabel::_find_jst_flags(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_PARAGRAPH) {
			ItemParagraph *p = static_cast<ItemParagraph *>(item);
			return p->jst_flags;
		}

		item = item->parent;
	}

	return default_jst_flags;
}

PackedFloat32Array SelectableRichTextLabel::_find_tab_stops(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_PARAGRAPH) {
			ItemParagraph *p = static_cast<ItemParagraph *>(item);
			return p->tab_stops;
		}

		item = item->parent;
	}

	return PackedFloat32Array();
}

HorizontalAlignment SelectableRichTextLabel::_find_alignment(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_PARAGRAPH) {
			ItemParagraph *p = static_cast<ItemParagraph *>(item);
			return p->alignment;
		}

		item = item->parent;
	}

	return default_alignment;
}

TextServer::Direction SelectableRichTextLabel::_find_direction(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_PARAGRAPH) {
			ItemParagraph *p = static_cast<ItemParagraph *>(item);
			if (p->direction != Control::TEXT_DIRECTION_INHERITED) {
				return (TextServer::Direction)p->direction;
			}
		}

		item = item->parent;
	}

	if (text_direction == Control::TEXT_DIRECTION_INHERITED) {
		return is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR;
	} else {
		return (TextServer::Direction)text_direction;
	}
}

TextServer::StructuredTextParser SelectableRichTextLabel::_find_stt(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_PARAGRAPH) {
			ItemParagraph *p = static_cast<ItemParagraph *>(item);
			return p->st_parser;
		}

		item = item->parent;
	}

	return st_parser;
}

String SelectableRichTextLabel::_find_language(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_PARAGRAPH) {
			ItemParagraph *p = static_cast<ItemParagraph *>(item);
			return p->language;
		}

		item = item->parent;
	}

	return language;
}

Color SelectableRichTextLabel::_find_color(Item *p_item, const Color &p_default_color) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_COLOR) {
			ItemColor *color = static_cast<ItemColor *>(item);
			return color->color;
		}

		item = item->parent;
	}

	return p_default_color;
}

Color SelectableRichTextLabel::_find_outline_color(Item *p_item, const Color &p_default_color) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_OUTLINE_COLOR) {
			ItemOutlineColor *color = static_cast<ItemOutlineColor *>(item);
			return color->color;
		}

		item = item->parent;
	}

	return p_default_color;
}

bool SelectableRichTextLabel::_find_underline(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_UNDERLINE) {
			return true;
		}

		item = item->parent;
	}

	return false;
}

bool SelectableRichTextLabel::_find_strikethrough(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_STRIKETHROUGH) {
			return true;
		}

		item = item->parent;
	}

	return false;
}

void SelectableRichTextLabel::_fetch_item_fx_stack(Item *p_item, Vector<ItemFX *> &r_stack) {
	Item *item = p_item;
	while (item) {
		if (item->type == ITEM_CUSTOMFX || item->type == ITEM_SHAKE || item->type == ITEM_WAVE || item->type == ITEM_TORNADO || item->type == ITEM_RAINBOW) {
			r_stack.push_back(static_cast<ItemFX *>(item));
		}

		item = item->parent;
	}
}

void SelectableRichTextLabel::_normalize_subtags(Vector<String> &subtags) {
	for (String &subtag : subtags) {
		subtag = subtag.unquote();
	}
}

bool SelectableRichTextLabel::_find_meta(Item *p_item, Variant *r_meta, ItemMeta **r_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_META) {
			ItemMeta *meta = static_cast<ItemMeta *>(item);
			if (r_meta) {
				*r_meta = meta->meta;
			}
			if (r_item) {
				*r_item = meta;
			}
			return true;
		}

		item = item->parent;
	}

	return false;
}

bool SelectableRichTextLabel::_find_hint(Item *p_item, String *r_description) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_HINT) {
			ItemHint *hint = static_cast<ItemHint *>(item);
			if (r_description) {
				*r_description = hint->description;
			}
			return true;
		}

		item = item->parent;
	}

	return false;
}

Color SelectableRichTextLabel::_find_bgcolor(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_BGCOLOR) {
			ItemBGColor *color = static_cast<ItemBGColor *>(item);
			return color->color;
		}

		item = item->parent;
	}

	return Color(0, 0, 0, 0);
}

Color SelectableRichTextLabel::_find_fgcolor(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_FGCOLOR) {
			ItemFGColor *color = static_cast<ItemFGColor *>(item);
			return color->color;
		}

		item = item->parent;
	}

	return Color(0, 0, 0, 0);
}

bool SelectableRichTextLabel::_find_layout_subitem(Item *from, Item *to) {
	if (from && from != to) {
		if (from->type != ITEM_FONT && from->type != ITEM_COLOR && from->type != ITEM_UNDERLINE && from->type != ITEM_STRIKETHROUGH) {
			return true;
		}

		for (Item *E : from->subitems) {
			bool layout = _find_layout_subitem(E, to);

			if (layout) {
				return true;
			}
		}
	}

	return false;
}

void SelectableRichTextLabel::_thread_function(void *p_userdata) {
	set_current_thread_safe_for_nodes(true);
	_process_line_caches();
	updating.store(false);
	call_deferred(SNAME("thread_end"));
}

void SelectableRichTextLabel::_thread_end() {
	set_physics_process_internal(false);
	if (is_visible_in_tree()) {
		queue_redraw();
	}
}

void SelectableRichTextLabel::_stop_thread() {
	if (threaded) {
		stop_thread.store(true);
		if (task != WorkerThreadPool::INVALID_TASK_ID) {
			WorkerThreadPool::get_singleton()->wait_for_task_completion(task);
			task = WorkerThreadPool::INVALID_TASK_ID;
		}
	}
}

int SelectableRichTextLabel::get_pending_paragraphs() const {
	int to_line = main->first_invalid_line.load();
	int lines = main->lines.size();

	return lines - to_line;
}

bool SelectableRichTextLabel::is_ready() const {
	const_cast<SelectableRichTextLabel *>(this)->_validate_line_caches();

	if (updating.load()) {
		return false;
	}
	return (main->first_invalid_line.load() == (int)main->lines.size() && main->first_resized_line.load() == (int)main->lines.size() && main->first_invalid_font_line.load() == (int)main->lines.size());
}

bool SelectableRichTextLabel::is_updating() const {
	return updating.load() || validating.load();
}

void SelectableRichTextLabel::set_threaded(bool p_threaded) {
	if (threaded != p_threaded) {
		_stop_thread();
		threaded = p_threaded;
		queue_redraw();
	}
}

bool SelectableRichTextLabel::is_threaded() const {
	return threaded;
}

void SelectableRichTextLabel::set_progress_bar_delay(int p_delay_ms) {
	progress_delay = p_delay_ms;
}

int SelectableRichTextLabel::get_progress_bar_delay() const {
	return progress_delay;
}

_FORCE_INLINE_ float SelectableRichTextLabel::_update_scroll_exceeds(float p_total_height, float p_ctrl_height, float p_width, int p_idx, float p_old_scroll, float p_text_rect_height) {
	updating_scroll = true;

	float total_height = p_total_height;
	bool exceeds = p_total_height > p_ctrl_height && scroll_active;
	if (exceeds != scroll_visible) {
		if (exceeds) {
			scroll_visible = true;
			scroll_w = vscroll->get_combined_minimum_size().width;
			vscroll->show();
			vscroll->set_anchor_and_offset(SIDE_LEFT, ANCHOR_END, -scroll_w);
		} else {
			scroll_visible = false;
			scroll_w = 0;
			vscroll->hide();
		}

		main->first_resized_line.store(0);

		total_height = 0;
		for (int j = 0; j <= p_idx; j++) {
			total_height = _resize_line(main, j, theme_cache.normal_font, theme_cache.normal_font_size, p_width - scroll_w, total_height);

			main->first_resized_line.store(j);
		}
	}
	vscroll->set_max(total_height);
	vscroll->set_page(p_text_rect_height);
	if (scroll_follow && scroll_following) {
		vscroll->set_value(total_height);
	} else {
		vscroll->set_value(p_old_scroll);
	}
	updating_scroll = false;

	return total_height;
}

bool SelectableRichTextLabel::_validate_line_caches() {
	if (updating.load()) {
		return false;
	}
	validating.store(true);
	if (main->first_invalid_line.load() == (int)main->lines.size()) {
		MutexLock data_lock(data_mutex);
		Rect2 text_rect = _get_text_rect();

		float ctrl_height = get_size().height;

		// Update fonts.
		float old_scroll = vscroll->get_value();
		if (main->first_invalid_font_line.load() != (int)main->lines.size()) {
			for (int i = main->first_invalid_font_line.load(); i < (int)main->lines.size(); i++) {
				_update_line_font(main, i, theme_cache.normal_font, theme_cache.normal_font_size);
			}
			main->first_resized_line.store(main->first_invalid_font_line.load());
			main->first_invalid_font_line.store(main->lines.size());
		}

		if (main->first_resized_line.load() == (int)main->lines.size()) {
			vscroll->set_value(old_scroll);
			validating.store(false);
			return true;
		}

		// Resize lines without reshaping.
		int fi = main->first_resized_line.load();

		float total_height = (fi == 0) ? 0 : _calculate_line_vertical_offset(main->lines[fi - 1]);
		for (int i = fi; i < (int)main->lines.size(); i++) {
			total_height = _resize_line(main, i, theme_cache.normal_font, theme_cache.normal_font_size, text_rect.get_size().width - scroll_w, total_height);
			total_height = _update_scroll_exceeds(total_height, ctrl_height, text_rect.get_size().width, i, old_scroll, text_rect.size.height);
			main->first_resized_line.store(i);
		}

		main->first_resized_line.store(main->lines.size());

		if (fit_content) {
			update_minimum_size();
		}
		validating.store(false);
		return true;
	}
	validating.store(false);
	stop_thread.store(false);
	if (threaded) {
		updating.store(true);
		loaded.store(true);
		task = WorkerThreadPool::get_singleton()->add_template_task(this, &SelectableRichTextLabel::_thread_function, nullptr, true, vformat("SelectableRichTextLabelShape:%x", (int64_t)get_instance_id()));
		set_physics_process_internal(true);
		loading_started = OS::get_singleton()->get_ticks_msec();
		return false;
	} else {
		updating.store(true);
		_process_line_caches();
		updating.store(false);
		queue_redraw();
		return true;
	}
}

void SelectableRichTextLabel::_process_line_caches() {
	// Shape invalid lines.
	if (!is_inside_tree()) {
		return;
	}

	MutexLock data_lock(data_mutex);
	Rect2 text_rect = _get_text_rect();

	float ctrl_height = get_size().height;
	int fi = main->first_invalid_line.load();
	int total_chars = main->lines[fi].char_offset;
	float old_scroll = vscroll->get_value();

	float total_height = 0;
	if (fi != 0) {
		int sr = MIN(main->first_invalid_font_line.load(), main->first_resized_line.load());

		// Update fonts.
		for (int i = main->first_invalid_font_line.load(); i < fi; i++) {
			_update_line_font(main, i, theme_cache.normal_font, theme_cache.normal_font_size);

			main->first_invalid_font_line.store(i);

			if (stop_thread.load()) {
				return;
			}
		}

		// Resize lines without reshaping.
		if (sr != 0) {
			total_height = _calculate_line_vertical_offset(main->lines[sr - 1]);
		}

		for (int i = sr; i < fi; i++) {
			total_height = _resize_line(main, i, theme_cache.normal_font, theme_cache.normal_font_size, text_rect.get_size().width - scroll_w, total_height);
			total_height = _update_scroll_exceeds(total_height, ctrl_height, text_rect.get_size().width, i, old_scroll, text_rect.size.height);

			main->first_resized_line.store(i);

			if (stop_thread.load()) {
				return;
			}
		}
	}

	total_height = (fi == 0) ? 0 : _calculate_line_vertical_offset(main->lines[fi - 1]);
	for (int i = fi; i < (int)main->lines.size(); i++) {
		total_height = _shape_line(main, i, theme_cache.normal_font, theme_cache.normal_font_size, text_rect.get_size().width - scroll_w, total_height, &total_chars);
		total_height = _update_scroll_exceeds(total_height, ctrl_height, text_rect.get_size().width, i, old_scroll, text_rect.size.height);

		main->first_invalid_line.store(i);
		main->first_resized_line.store(i);
		main->first_invalid_font_line.store(i);

		if (stop_thread.load()) {
			return;
		}
		loaded.store(double(i) / double(main->lines.size()));
	}

	main->first_invalid_line.store(main->lines.size());
	main->first_resized_line.store(main->lines.size());
	main->first_invalid_font_line.store(main->lines.size());

	if (fit_content) {
		update_minimum_size();
	}
	emit_signal(SNAME("finished"));
}

void SelectableRichTextLabel::_invalidate_current_line(ItemFrame *p_frame) {
	if ((int)p_frame->lines.size() - 1 <= p_frame->first_invalid_line) {
		p_frame->first_invalid_line = (int)p_frame->lines.size() - 1;
	}
}

void SelectableRichTextLabel::add_text(const String &p_text) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	if (current->type == ITEM_TABLE) {
		return; //can't add anything here
	}

	int pos = 0;

	while (pos < p_text.length()) {
		int end = p_text.find("\n", pos);
		String line;
		bool eol = false;
		if (end == -1) {
			end = p_text.length();
		} else {
			eol = true;
		}

		if (pos == 0 && end == p_text.length()) {
			line = p_text;
		} else {
			line = p_text.substr(pos, end - pos);
		}

		if (line.length() > 0) {
			if (current->subitems.size() && current->subitems.back()->get()->type == ITEM_TEXT) {
				//append text condition!
				ItemText *ti = static_cast<ItemText *>(current->subitems.back()->get());
				ti->text += line;
				_invalidate_current_line(main);

			} else {
				//append item condition
				ItemText *item = memnew(ItemText);
				item->text = line;
				_add_item(item, false);
			}
		}

		if (eol) {
			ItemNewline *item = memnew(ItemNewline);
			item->line = current_frame->lines.size();
			_add_item(item, false);
			current_frame->lines.resize(current_frame->lines.size() + 1);
			if (item->type != ITEM_NEWLINE) {
				current_frame->lines[current_frame->lines.size() - 1].from = item;
			}
			_invalidate_current_line(current_frame);
		}

		pos = end + 1;
	}
	queue_redraw();
}

void SelectableRichTextLabel::_add_item(Item *p_item, bool p_enter, bool p_ensure_newline) {
	p_item->parent = current;
	p_item->E = current->subitems.push_back(p_item);
	p_item->index = current_idx++;
	p_item->char_ofs = current_char_ofs;
	if (p_item->type == ITEM_TEXT) {
		ItemText *t = static_cast<ItemText *>(p_item);
		current_char_ofs += t->text.length();
	} else if (p_item->type == ITEM_IMAGE) {
		current_char_ofs++;
	}

	if (p_enter) {
		current = p_item;
	}

	if (p_ensure_newline) {
		Item *from = current_frame->lines[current_frame->lines.size() - 1].from;
		// only create a new line for Item types that generate content/layout, ignore those that represent formatting/styling
		if (_find_layout_subitem(from, p_item)) {
			_invalidate_current_line(current_frame);
			current_frame->lines.resize(current_frame->lines.size() + 1);
		}
	}

	if (current_frame->lines[current_frame->lines.size() - 1].from == nullptr) {
		current_frame->lines[current_frame->lines.size() - 1].from = p_item;
	}
	p_item->line = current_frame->lines.size() - 1;

	_invalidate_current_line(current_frame);

	if (fit_content) {
		update_minimum_size();
	}
	queue_redraw();
}

void SelectableRichTextLabel::_remove_item(Item *p_item, const int p_line, const int p_subitem_line) {
	int size = p_item->subitems.size();
	if (size == 0) {
		p_item->parent->subitems.erase(p_item);
		// If a newline was erased, all lines AFTER the newline need to be decremented.
		if (p_item->type == ITEM_NEWLINE) {
			current_frame->lines.remove_at(p_line);
			if (p_line < (int)current_frame->lines.size() && current_frame->lines[p_line].from) {
				for (List<Item *>::Element *E = current_frame->lines[p_line].from->E; E; E = E->next()) {
					if (E->get()->line > p_subitem_line) {
						E->get()->line--;
					}
				}
			}
		}
	} else {
		// First, remove all child items for the provided item.
		while (p_item->subitems.size()) {
			_remove_item(p_item->subitems.front()->get(), p_line, p_subitem_line);
		}
		// Then remove the provided item itself.
		p_item->parent->subitems.erase(p_item);
	}
	memdelete(p_item);
}

void SelectableRichTextLabel::add_image(const Ref<Texture2D> &p_image, const int p_width, const int p_height, const Color &p_color, InlineAlignment p_alignment, const Rect2 &p_region) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	if (current->type == ITEM_TABLE) {
		return;
	}

	ERR_FAIL_COND(p_image.is_null());
	ERR_FAIL_COND(p_image->get_width() == 0);
	ERR_FAIL_COND(p_image->get_height() == 0);
	ItemImage *item = memnew(ItemImage);

	if (p_region.has_area()) {
		Ref<AtlasTexture> atlas_tex = memnew(AtlasTexture);
		atlas_tex->set_atlas(p_image);
		atlas_tex->set_region(p_region);
		item->image = atlas_tex;
	} else {
		item->image = p_image;
	}

	item->color = p_color;
	item->inline_align = p_alignment;

	if (p_width > 0) {
		// custom width
		item->size.width = p_width;
		if (p_height > 0) {
			// custom height
			item->size.height = p_height;
		} else {
			// calculate height to keep aspect ratio
			if (p_region.has_area()) {
				item->size.height = p_region.get_size().height * p_width / p_region.get_size().width;
			} else {
				item->size.height = p_image->get_height() * p_width / p_image->get_width();
			}
		}
	} else {
		if (p_height > 0) {
			// custom height
			item->size.height = p_height;
			// calculate width to keep aspect ratio
			if (p_region.has_area()) {
				item->size.width = p_region.get_size().width * p_height / p_region.get_size().height;
			} else {
				item->size.width = p_image->get_width() * p_height / p_image->get_height();
			}
		} else {
			if (p_region.has_area()) {
				// if the image has a region, keep the region size
				item->size = p_region.get_size();
			} else {
				// keep original width and height
				item->size = p_image->get_size();
			}
		}
	}

	_add_item(item, false);
}

void SelectableRichTextLabel::add_newline() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	if (current->type == ITEM_TABLE) {
		return;
	}
	ItemNewline *item = memnew(ItemNewline);
	item->line = current_frame->lines.size();
	_add_item(item, false);
	current_frame->lines.resize(current_frame->lines.size() + 1);
	_invalidate_current_line(current_frame);
	queue_redraw();
}

bool SelectableRichTextLabel::remove_paragraph(const int p_paragraph) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	if (p_paragraph >= (int)current_frame->lines.size() || p_paragraph < 0) {
		return false;
	}

	// Remove all subitems with the same line as that provided.
	Vector<List<Item *>::Element *> subitem_to_remove;
	if (current_frame->lines[p_paragraph].from) {
		for (List<Item *>::Element *E = current_frame->lines[p_paragraph].from->E; E; E = E->next()) {
			if (E->get()->line == p_paragraph) {
				subitem_to_remove.push_back(E);
			} else {
				break;
			}
		}
	}

	bool had_newline = false;
	// Reverse for loop to remove items from the end first.
	for (int i = subitem_to_remove.size() - 1; i >= 0; i--) {
		List<Item *>::Element *subitem = subitem_to_remove[i];
		had_newline = had_newline || subitem->get()->type == ITEM_NEWLINE;
		_remove_item(subitem->get(), subitem->get()->line, p_paragraph);
	}

	if (!had_newline) {
		current_frame->lines.remove_at(p_paragraph);
		if (current_frame->lines.size() == 0) {
			current_frame->lines.resize(1);
		}
	}

	if (p_paragraph == 0 && current->subitems.size() > 0) {
		main->lines[0].from = main;
	}

	int to_line = main->first_invalid_line.load();
	main->first_invalid_line.store(MIN(to_line, p_paragraph));
	queue_redraw();

	return true;
}

void SelectableRichTextLabel::push_dropcap(const String &p_string, const Ref<Font> &p_font, int p_size, const Rect2 &p_dropcap_margins, const Color &p_color, int p_ol_size, const Color &p_ol_color) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ERR_FAIL_COND(p_string.is_empty());
	ERR_FAIL_COND(p_font.is_null());
	ERR_FAIL_COND(p_size <= 0);

	ItemDropcap *item = memnew(ItemDropcap);

	item->text = p_string;
	item->font = p_font;
	item->font_size = p_size;
	item->color = p_color;
	item->ol_size = p_ol_size;
	item->ol_color = p_ol_color;
	item->dropcap_margins = p_dropcap_margins;
	_add_item(item, false);
}

void SelectableRichTextLabel::_push_def_font_var(DefaultFont p_def_font, const Ref<Font> &p_font, int p_size) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemFont *item = memnew(ItemFont);

	item->def_font = p_def_font;
	item->variation = true;
	item->font = p_font;
	item->font_size = p_size;
	item->def_size = (p_size <= 0);
	_add_item(item, true);
}

void SelectableRichTextLabel::_push_def_font(DefaultFont p_def_font) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemFont *item = memnew(ItemFont);

	item->def_font = p_def_font;
	item->def_size = true;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_font(const Ref<Font> &p_font, int p_size) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ERR_FAIL_COND(p_font.is_null());
	ItemFont *item = memnew(ItemFont);

	item->font = p_font;
	item->font_size = p_size;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_normal() {
	ERR_FAIL_COND(theme_cache.normal_font.is_null());

	_push_def_font(NORMAL_FONT);
}

void SelectableRichTextLabel::push_bold() {
	ERR_FAIL_COND(theme_cache.bold_font.is_null());

	ItemFont *item_font = _find_font(current);
	_push_def_font((item_font && item_font->def_font == ITALICS_FONT) ? BOLD_ITALICS_FONT : BOLD_FONT);
}

void SelectableRichTextLabel::push_bold_italics() {
	ERR_FAIL_COND(theme_cache.bold_italics_font.is_null());

	_push_def_font(BOLD_ITALICS_FONT);
}

void SelectableRichTextLabel::push_italics() {
	ERR_FAIL_COND(theme_cache.italics_font.is_null());

	ItemFont *item_font = _find_font(current);
	_push_def_font((item_font && item_font->def_font == BOLD_FONT) ? BOLD_ITALICS_FONT : ITALICS_FONT);
}

void SelectableRichTextLabel::push_mono() {
	ERR_FAIL_COND(theme_cache.mono_font.is_null());

	_push_def_font(MONO_FONT);
}

void SelectableRichTextLabel::push_font_size(int p_font_size) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemFontSize *item = memnew(ItemFontSize);

	item->font_size = p_font_size;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_outline_size(int p_ol_size) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemOutlineSize *item = memnew(ItemOutlineSize);

	item->outline_size = p_ol_size;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_color(const Color &p_color) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemColor *item = memnew(ItemColor);

	item->color = p_color;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_outline_color(const Color &p_color) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemOutlineColor *item = memnew(ItemOutlineColor);

	item->color = p_color;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_underline() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemUnderline *item = memnew(ItemUnderline);

	_add_item(item, true);
}

void SelectableRichTextLabel::push_strikethrough() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemStrikethrough *item = memnew(ItemStrikethrough);

	_add_item(item, true);
}

void SelectableRichTextLabel::push_paragraph(HorizontalAlignment p_alignment, Control::TextDirection p_direction, const String &p_language, TextServer::StructuredTextParser p_st_parser, BitField<TextServer::JustificationFlag> p_jst_flags, const PackedFloat32Array &p_tab_stops) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);

	ItemParagraph *item = memnew(ItemParagraph);
	item->alignment = p_alignment;
	item->direction = p_direction;
	item->language = p_language;
	item->st_parser = p_st_parser;
	item->jst_flags = p_jst_flags;
	item->tab_stops = p_tab_stops;
	_add_item(item, true, true);
}

void SelectableRichTextLabel::push_indent(int p_level) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ERR_FAIL_COND(p_level < 0);

	ItemIndent *item = memnew(ItemIndent);
	item->level = p_level;
	_add_item(item, true, true);
}

void SelectableRichTextLabel::push_list(int p_level, ListType p_list, bool p_capitalize, const String &p_bullet) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ERR_FAIL_COND(p_level < 0);

	ItemList *item = memnew(ItemList);

	item->list_type = p_list;
	item->level = p_level;
	item->capitalize = p_capitalize;
	item->bullet = p_bullet;
	_add_item(item, true, true);
}

void SelectableRichTextLabel::push_meta(const Variant &p_meta) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemMeta *item = memnew(ItemMeta);

	item->meta = p_meta;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_hint(const String &p_string) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemHint *item = memnew(ItemHint);

	item->description = p_string;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_table(int p_columns, InlineAlignment p_alignment, int p_align_to_row) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ERR_FAIL_COND(p_columns < 1);
	ItemTable *item = memnew(ItemTable);

	item->columns.resize(p_columns);
	item->total_width = 0;
	item->inline_align = p_alignment;
	item->align_to_row = p_align_to_row;
	for (int i = 0; i < (int)item->columns.size(); i++) {
		item->columns[i].expand = false;
		item->columns[i].expand_ratio = 1;
	}
	_add_item(item, true, false);
}

void SelectableRichTextLabel::push_fade(int p_start_index, int p_length) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ItemFade *item = memnew(ItemFade);
	item->starting_index = p_start_index;
	item->length = p_length;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_shake(int p_strength = 10, float p_rate = 24.0f, bool p_connected = true) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ItemShake *item = memnew(ItemShake);
	item->strength = p_strength;
	item->rate = p_rate;
	item->connected = p_connected;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_wave(float p_frequency = 1.0f, float p_amplitude = 10.0f, bool p_connected = true) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ItemWave *item = memnew(ItemWave);
	item->frequency = p_frequency;
	item->amplitude = p_amplitude;
	item->connected = p_connected;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_tornado(float p_frequency = 1.0f, float p_radius = 10.0f, bool p_connected = true) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ItemTornado *item = memnew(ItemTornado);
	item->frequency = p_frequency;
	item->radius = p_radius;
	item->connected = p_connected;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_rainbow(float p_saturation, float p_value, float p_frequency) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ItemRainbow *item = memnew(ItemRainbow);
	item->frequency = p_frequency;
	item->saturation = p_saturation;
	item->value = p_value;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_bgcolor(const Color &p_color) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemBGColor *item = memnew(ItemBGColor);

	item->color = p_color;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_fgcolor(const Color &p_color) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemFGColor *item = memnew(ItemFGColor);

	item->color = p_color;
	_add_item(item, true);
}

void SelectableRichTextLabel::push_customfx(Ref<RichTextEffect> p_custom_effect, Dictionary p_environment) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ItemCustomFX *item = memnew(ItemCustomFX);
	item->custom_effect = p_custom_effect;
	item->char_fx_transform->environment = p_environment;
	_add_item(item, true);

	set_process_internal(true);
}

void SelectableRichTextLabel::set_table_column_expand(int p_column, bool p_expand, int p_ratio) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type != ITEM_TABLE);

	ItemTable *table = static_cast<ItemTable *>(current);
	ERR_FAIL_INDEX(p_column, (int)table->columns.size());
	table->columns[p_column].expand = p_expand;
	table->columns[p_column].expand_ratio = p_ratio;
}

void SelectableRichTextLabel::set_cell_row_background_color(const Color &p_odd_row_bg, const Color &p_even_row_bg) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type != ITEM_FRAME);

	ItemFrame *cell = static_cast<ItemFrame *>(current);
	ERR_FAIL_COND(!cell->cell);
	cell->odd_row_bg = p_odd_row_bg;
	cell->even_row_bg = p_even_row_bg;
}

void SelectableRichTextLabel::set_cell_border_color(const Color &p_color) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type != ITEM_FRAME);

	ItemFrame *cell = static_cast<ItemFrame *>(current);
	ERR_FAIL_COND(!cell->cell);
	cell->border = p_color;
}

void SelectableRichTextLabel::set_cell_size_override(const Size2 &p_min_size, const Size2 &p_max_size) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type != ITEM_FRAME);

	ItemFrame *cell = static_cast<ItemFrame *>(current);
	ERR_FAIL_COND(!cell->cell);
	cell->min_size_over = p_min_size;
	cell->max_size_over = p_max_size;
}

void SelectableRichTextLabel::set_cell_padding(const Rect2 &p_padding) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type != ITEM_FRAME);

	ItemFrame *cell = static_cast<ItemFrame *>(current);
	ERR_FAIL_COND(!cell->cell);
	cell->padding = p_padding;
}

void SelectableRichTextLabel::push_cell() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type != ITEM_TABLE);

	ItemFrame *item = memnew(ItemFrame);
	item->parent_frame = current_frame;
	_add_item(item, true);
	current_frame = item;
	item->cell = true;
	item->lines.resize(1);
	item->lines[0].from = nullptr;
	item->first_invalid_line.store(0); // parent frame last line ???
}

int SelectableRichTextLabel::get_current_table_column() const {
	ERR_FAIL_COND_V(current->type != ITEM_TABLE, -1);

	ItemTable *table = static_cast<ItemTable *>(current);
	return table->subitems.size() % table->columns.size();
}

void SelectableRichTextLabel::pop() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_NULL(current->parent);

	if (current->type == ITEM_FRAME) {
		current_frame = static_cast<ItemFrame *>(current)->parent_frame;
	}
	current = current->parent;
}

void SelectableRichTextLabel::clear() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	main->_clear_children();
	current = main;
	current_frame = main;
	main->lines.clear();
	main->lines.resize(1);
	main->first_invalid_line.store(0);

	selection.click_frame = nullptr;
	selection.click_item = nullptr;
	deselect();

	current_idx = 1;
	current_char_ofs = 0;
	if (scroll_follow) {
		scroll_following = true;
	}

	if (fit_content) {
		update_minimum_size();
	}
}

void SelectableRichTextLabel::set_tab_size(int p_spaces) {
	if (tab_size == p_spaces) {
		return;
	}

	_stop_thread();

	tab_size = p_spaces;
	main->first_resized_line.store(0);
	queue_redraw();
}

int SelectableRichTextLabel::get_tab_size() const {
	return tab_size;
}

void SelectableRichTextLabel::set_fit_content(bool p_enabled) {
	if (p_enabled == fit_content) {
		return;
	}

	fit_content = p_enabled;
	update_minimum_size();
}

bool SelectableRichTextLabel::is_fit_content_enabled() const {
	return fit_content;
}

void SelectableRichTextLabel::set_meta_underline(bool p_underline) {
	if (underline_meta == p_underline) {
		return;
	}

	underline_meta = p_underline;
	queue_redraw();
}

bool SelectableRichTextLabel::is_meta_underlined() const {
	return underline_meta;
}

void SelectableRichTextLabel::set_hint_underline(bool p_underline) {
	underline_hint = p_underline;
	queue_redraw();
}

bool SelectableRichTextLabel::is_hint_underlined() const {
	return underline_hint;
}

void SelectableRichTextLabel::set_offset(int p_pixel) {
	vscroll->set_value(p_pixel);
}

void SelectableRichTextLabel::set_scroll_active(bool p_active) {
	if (scroll_active == p_active) {
		return;
	}

	scroll_active = p_active;
	vscroll->set_drag_node_enabled(p_active);
	queue_redraw();
}

bool SelectableRichTextLabel::is_scroll_active() const {
	return scroll_active;
}

void SelectableRichTextLabel::set_scroll_follow(bool p_follow) {
	scroll_follow = p_follow;
	if (!vscroll->is_visible_in_tree() || vscroll->get_value() >= (vscroll->get_max() - vscroll->get_page())) {
		scroll_following = true;
	}
}

bool SelectableRichTextLabel::is_scroll_following() const {
	return scroll_follow;
}

void SelectableRichTextLabel::parse_bbcode(const String &p_bbcode) {
	clear();
	append_text(p_bbcode);
}

void SelectableRichTextLabel::append_text(const String &p_bbcode) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	int pos = 0;

	List<String> tag_stack;

	int indent_level = 0;

	bool in_bold = false;
	bool in_italics = false;
	bool after_list_open_tag = false;
	bool after_list_close_tag = false;

	set_process_internal(false);

	while (pos <= p_bbcode.length()) {
		int brk_pos = p_bbcode.find("[", pos);

		if (brk_pos < 0) {
			brk_pos = p_bbcode.length();
		}

		String txt = brk_pos > pos ? p_bbcode.substr(pos, brk_pos - pos) : "";

		// Trim the first newline character, it may be added later as needed.
		if (after_list_close_tag || after_list_open_tag) {
			txt = txt.trim_prefix("\n");
		}

		if (brk_pos == p_bbcode.length()) {
			// For tags that are not properly closed.
			if (txt.is_empty() && after_list_open_tag) {
				txt = "\n";
			}

			if (!txt.is_empty()) {
				add_text(txt);
			}
			break; //nothing else to add
		}

		int brk_end = p_bbcode.find("]", brk_pos + 1);

		if (brk_end == -1) {
			//no close, add the rest
			txt += p_bbcode.substr(brk_pos, p_bbcode.length() - brk_pos);
			add_text(txt);
			break;
		}

		String tag = p_bbcode.substr(brk_pos + 1, brk_end - brk_pos - 1);
		Vector<String> split_tag_block = tag.split(" ", false);

		// Find optional parameters.
		String bbcode_name;
		typedef HashMap<String, String> OptionMap;
		OptionMap bbcode_options;
		if (!split_tag_block.is_empty()) {
			bbcode_name = split_tag_block[0];
			for (int i = 1; i < split_tag_block.size(); i++) {
				const String &expr = split_tag_block[i];
				int value_pos = expr.find("=");
				if (value_pos > -1) {
					bbcode_options[expr.substr(0, value_pos)] = expr.substr(value_pos + 1).unquote();
				}
			}
		} else {
			bbcode_name = tag;
		}

		// Find main parameter.
		String bbcode_value;
		int main_value_pos = bbcode_name.find("=");
		if (main_value_pos > -1) {
			bbcode_value = bbcode_name.substr(main_value_pos + 1);
			bbcode_name = bbcode_name.substr(0, main_value_pos);
		}

		if (tag.begins_with("/") && tag_stack.size()) {
			bool tag_ok = tag_stack.size() && tag_stack.front()->get() == tag.substr(1, tag.length());

			if (tag_stack.front()->get() == "b") {
				in_bold = false;
			}
			if (tag_stack.front()->get() == "i") {
				in_italics = false;
			}
			if ((tag_stack.front()->get() == "indent") || (tag_stack.front()->get() == "ol") || (tag_stack.front()->get() == "ul")) {
				indent_level--;
			}

			if (!tag_ok) {
				txt += "[" + tag;
				add_text(txt);
				after_list_open_tag = false;
				after_list_close_tag = false;
				pos = brk_end;
				continue;
			}

			if (txt.is_empty() && after_list_open_tag) {
				txt = "\n"; // Make empty list have at least one item.
			}
			after_list_open_tag = false;

			if (tag == "/ol" || tag == "/ul") {
				if (!txt.is_empty()) {
					// Make sure text ends with a newline character, that is, the last item
					// will wrap at the end of block.
					if (!txt.ends_with("\n")) {
						txt += "\n";
					}
				} else if (!after_list_close_tag) {
					txt = "\n"; // Make the innermost list item wrap at the end of lists.
				}
				after_list_close_tag = true;
			} else {
				after_list_close_tag = false;
			}

			if (!txt.is_empty()) {
				add_text(txt);
			}

			tag_stack.pop_front();
			pos = brk_end + 1;
			if (tag != "/img" && tag != "/dropcap") {
				pop();
			}
			continue;
		}

		if (tag == "ol" || tag.begins_with("ol ") || tag == "ul" || tag.begins_with("ul ")) {
			if (txt.is_empty() && after_list_open_tag) {
				txt = "\n"; // Make each list have at least one item at the beginning.
			}
			after_list_open_tag = true;
		} else {
			after_list_open_tag = false;
		}
		if (!txt.is_empty()) {
			add_text(txt);
		}
		after_list_close_tag = false;

		if (tag == "b") {
			//use bold font
			in_bold = true;
			if (in_italics) {
				_push_def_font(BOLD_ITALICS_FONT);
			} else {
				_push_def_font(BOLD_FONT);
			}
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "i") {
			//use italics font
			in_italics = true;
			if (in_bold) {
				_push_def_font(BOLD_ITALICS_FONT);
			} else {
				_push_def_font(ITALICS_FONT);
			}
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "code") {
			//use monospace font
			_push_def_font(MONO_FONT);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag.begins_with("table=")) {
			Vector<String> subtag = tag.substr(6, tag.length()).split(",");
			_normalize_subtags(subtag);

			int columns = subtag[0].to_int();
			if (columns < 1) {
				columns = 1;
			}

			int alignment = INLINE_ALIGNMENT_TOP;
			if (subtag.size() > 2) {
				if (subtag[1] == "top" || subtag[1] == "t") {
					alignment = INLINE_ALIGNMENT_TOP_TO;
				} else if (subtag[1] == "center" || subtag[1] == "c") {
					alignment = INLINE_ALIGNMENT_CENTER_TO;
				} else if (subtag[1] == "baseline" || subtag[1] == "l") {
					alignment = INLINE_ALIGNMENT_BASELINE_TO;
				} else if (subtag[1] == "bottom" || subtag[1] == "b") {
					alignment = INLINE_ALIGNMENT_BOTTOM_TO;
				}
				if (subtag[2] == "top" || subtag[2] == "t") {
					alignment |= INLINE_ALIGNMENT_TO_TOP;
				} else if (subtag[2] == "center" || subtag[2] == "c") {
					alignment |= INLINE_ALIGNMENT_TO_CENTER;
				} else if (subtag[2] == "baseline" || subtag[2] == "l") {
					alignment |= INLINE_ALIGNMENT_TO_BASELINE;
				} else if (subtag[2] == "bottom" || subtag[2] == "b") {
					alignment |= INLINE_ALIGNMENT_TO_BOTTOM;
				}
			} else if (subtag.size() > 1) {
				if (subtag[1] == "top" || subtag[1] == "t") {
					alignment = INLINE_ALIGNMENT_TOP;
				} else if (subtag[1] == "center" || subtag[1] == "c") {
					alignment = INLINE_ALIGNMENT_CENTER;
				} else if (subtag[1] == "bottom" || subtag[1] == "b") {
					alignment = INLINE_ALIGNMENT_BOTTOM;
				}
			}
			int row = -1;
			if (subtag.size() > 3) {
				row = subtag[3].to_int();
			}

			push_table(columns, (InlineAlignment)alignment, row);
			pos = brk_end + 1;
			tag_stack.push_front("table");
		} else if (tag == "cell") {
			push_cell();
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag.begins_with("cell=")) {
			int ratio = tag.substr(5, tag.length()).to_int();
			if (ratio < 1) {
				ratio = 1;
			}

			set_table_column_expand(get_current_table_column(), true, ratio);
			push_cell();

			pos = brk_end + 1;
			tag_stack.push_front("cell");
		} else if (tag.begins_with("cell ")) {
			Vector<String> subtag = tag.substr(5, tag.length()).split(" ");
			_normalize_subtags(subtag);

			for (int i = 0; i < subtag.size(); i++) {
				Vector<String> subtag_a = subtag[i].split("=");
				_normalize_subtags(subtag_a);

				if (subtag_a.size() == 2) {
					if (subtag_a[0] == "expand") {
						int ratio = subtag_a[1].to_int();
						if (ratio < 1) {
							ratio = 1;
						}
						set_table_column_expand(get_current_table_column(), true, ratio);
					}
				}
			}
			push_cell();
			const Color fallback_color = Color(0, 0, 0, 0);
			for (int i = 0; i < subtag.size(); i++) {
				Vector<String> subtag_a = subtag[i].split("=");
				_normalize_subtags(subtag_a);

				if (subtag_a.size() == 2) {
					if (subtag_a[0] == "border") {
						Color color = Color::from_string(subtag_a[1], fallback_color);
						set_cell_border_color(color);
					} else if (subtag_a[0] == "bg") {
						Vector<String> subtag_b = subtag_a[1].split(",");
						_normalize_subtags(subtag_b);

						if (subtag_b.size() == 2) {
							Color color1 = Color::from_string(subtag_b[0], fallback_color);
							Color color2 = Color::from_string(subtag_b[1], fallback_color);
							set_cell_row_background_color(color1, color2);
						}
						if (subtag_b.size() == 1) {
							Color color1 = Color::from_string(subtag_a[1], fallback_color);
							set_cell_row_background_color(color1, color1);
						}
					} else if (subtag_a[0] == "padding") {
						Vector<String> subtag_b = subtag_a[1].split(",");
						_normalize_subtags(subtag_b);

						if (subtag_b.size() == 4) {
							set_cell_padding(Rect2(subtag_b[0].to_float(), subtag_b[1].to_float(), subtag_b[2].to_float(), subtag_b[3].to_float()));
						}
					}
				}
			}

			pos = brk_end + 1;
			tag_stack.push_front("cell");
		} else if (tag == "u") {
			//use underline
			push_underline();
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "s") {
			//use strikethrough
			push_strikethrough();
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "lb") {
			add_text("[");
			pos = brk_end + 1;
		} else if (tag == "rb") {
			add_text("]");
			pos = brk_end + 1;
		} else if (tag == "lrm") {
			add_text(String::chr(0x200E));
			pos = brk_end + 1;
		} else if (tag == "rlm") {
			add_text(String::chr(0x200F));
			pos = brk_end + 1;
		} else if (tag == "lre") {
			add_text(String::chr(0x202A));
			pos = brk_end + 1;
		} else if (tag == "rle") {
			add_text(String::chr(0x202B));
			pos = brk_end + 1;
		} else if (tag == "lro") {
			add_text(String::chr(0x202D));
			pos = brk_end + 1;
		} else if (tag == "rlo") {
			add_text(String::chr(0x202E));
			pos = brk_end + 1;
		} else if (tag == "pdf") {
			add_text(String::chr(0x202C));
			pos = brk_end + 1;
		} else if (tag == "alm") {
			add_text(String::chr(0x061c));
			pos = brk_end + 1;
		} else if (tag == "lri") {
			add_text(String::chr(0x2066));
			pos = brk_end + 1;
		} else if (tag == "rli") {
			add_text(String::chr(0x2027));
			pos = brk_end + 1;
		} else if (tag == "fsi") {
			add_text(String::chr(0x2068));
			pos = brk_end + 1;
		} else if (tag == "pdi") {
			add_text(String::chr(0x2069));
			pos = brk_end + 1;
		} else if (tag == "zwj") {
			add_text(String::chr(0x200D));
			pos = brk_end + 1;
		} else if (tag == "zwnj") {
			add_text(String::chr(0x200C));
			pos = brk_end + 1;
		} else if (tag == "wj") {
			add_text(String::chr(0x2060));
			pos = brk_end + 1;
		} else if (tag == "shy") {
			add_text(String::chr(0x00AD));
			pos = brk_end + 1;
		} else if (tag == "center") {
			push_paragraph(HORIZONTAL_ALIGNMENT_CENTER);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "fill") {
			push_paragraph(HORIZONTAL_ALIGNMENT_FILL);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "left") {
			push_paragraph(HORIZONTAL_ALIGNMENT_LEFT);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "right") {
			push_paragraph(HORIZONTAL_ALIGNMENT_RIGHT);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "ul") {
			indent_level++;
			push_list(indent_level, LIST_DOTS, false);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag.begins_with("ul bullet=")) {
			String bullet = tag.substr(10, 1);
			indent_level++;
			push_list(indent_level, LIST_DOTS, false, bullet);
			pos = brk_end + 1;
			tag_stack.push_front("ul");
		} else if ((tag == "ol") || (tag == "ol type=1")) {
			indent_level++;
			push_list(indent_level, LIST_NUMBERS, false);
			pos = brk_end + 1;
			tag_stack.push_front("ol");
		} else if (tag == "ol type=a") {
			indent_level++;
			push_list(indent_level, LIST_LETTERS, false);
			pos = brk_end + 1;
			tag_stack.push_front("ol");
		} else if (tag == "ol type=A") {
			indent_level++;
			push_list(indent_level, LIST_LETTERS, true);
			pos = brk_end + 1;
			tag_stack.push_front("ol");
		} else if (tag == "ol type=i") {
			indent_level++;
			push_list(indent_level, LIST_ROMAN, false);
			pos = brk_end + 1;
			tag_stack.push_front("ol");
		} else if (tag == "ol type=I") {
			indent_level++;
			push_list(indent_level, LIST_ROMAN, true);
			pos = brk_end + 1;
			tag_stack.push_front("ol");
		} else if (tag == "indent") {
			indent_level++;
			push_indent(indent_level);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "p") {
			push_paragraph(HORIZONTAL_ALIGNMENT_LEFT);
			pos = brk_end + 1;
			tag_stack.push_front("p");
		} else if (tag.begins_with("p ")) {
			Vector<String> subtag = tag.substr(2, tag.length()).split(" ");
			_normalize_subtags(subtag);

			HorizontalAlignment alignment = HORIZONTAL_ALIGNMENT_LEFT;
			Control::TextDirection dir = Control::TEXT_DIRECTION_INHERITED;
			String lang;
			PackedFloat32Array tab_stops;
			TextServer::StructuredTextParser st_parser_type = TextServer::STRUCTURED_TEXT_DEFAULT;
			BitField<TextServer::JustificationFlag> jst_flags = default_jst_flags;
			for (int i = 0; i < subtag.size(); i++) {
				Vector<String> subtag_a = subtag[i].split("=");
				_normalize_subtags(subtag_a);

				if (subtag_a.size() == 2) {
					if (subtag_a[0] == "justification_flags" || subtag_a[0] == "jst") {
						Vector<String> subtag_b = subtag_a[1].split(",");
						for (const String &E : subtag_b) {
							if (E == "kashida" || E == "k") {
								jst_flags.set_flag(TextServer::JUSTIFICATION_KASHIDA);
							} else if (E == "word" || E == "w") {
								jst_flags.set_flag(TextServer::JUSTIFICATION_WORD_BOUND);
							} else if (E == "trim" || E == "tr") {
								jst_flags.set_flag(TextServer::JUSTIFICATION_TRIM_EDGE_SPACES);
							} else if (E == "after_last_tab" || E == "lt") {
								jst_flags.set_flag(TextServer::JUSTIFICATION_AFTER_LAST_TAB);
							} else if (E == "skip_last" || E == "sl") {
								jst_flags.set_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE);
							} else if (E == "skip_last_with_chars" || E == "sv") {
								jst_flags.set_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE_WITH_VISIBLE_CHARS);
							} else if (E == "do_not_skip_singe" || E == "ns") {
								jst_flags.set_flag(TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE);
							}
						}
					} else if (subtag_a[0] == "tab_stops") {
						Vector<String> splitters;
						splitters.push_back(",");
						splitters.push_back(";");
						tab_stops = subtag_a[1].split_floats_mk(splitters);
					} else if (subtag_a[0] == "align") {
						if (subtag_a[1] == "l" || subtag_a[1] == "left") {
							alignment = HORIZONTAL_ALIGNMENT_LEFT;
						} else if (subtag_a[1] == "c" || subtag_a[1] == "center") {
							alignment = HORIZONTAL_ALIGNMENT_CENTER;
						} else if (subtag_a[1] == "r" || subtag_a[1] == "right") {
							alignment = HORIZONTAL_ALIGNMENT_RIGHT;
						} else if (subtag_a[1] == "f" || subtag_a[1] == "fill") {
							alignment = HORIZONTAL_ALIGNMENT_FILL;
						}
					} else if (subtag_a[0] == "dir" || subtag_a[0] == "direction") {
						if (subtag_a[1] == "a" || subtag_a[1] == "auto") {
							dir = Control::TEXT_DIRECTION_AUTO;
						} else if (subtag_a[1] == "l" || subtag_a[1] == "ltr") {
							dir = Control::TEXT_DIRECTION_LTR;
						} else if (subtag_a[1] == "r" || subtag_a[1] == "rtl") {
							dir = Control::TEXT_DIRECTION_RTL;
						}
					} else if (subtag_a[0] == "lang" || subtag_a[0] == "language") {
						lang = subtag_a[1];
					} else if (subtag_a[0] == "st" || subtag_a[0] == "bidi_override") {
						if (subtag_a[1] == "d" || subtag_a[1] == "default") {
							st_parser_type = TextServer::STRUCTURED_TEXT_DEFAULT;
						} else if (subtag_a[1] == "u" || subtag_a[1] == "uri") {
							st_parser_type = TextServer::STRUCTURED_TEXT_URI;
						} else if (subtag_a[1] == "f" || subtag_a[1] == "file") {
							st_parser_type = TextServer::STRUCTURED_TEXT_FILE;
						} else if (subtag_a[1] == "e" || subtag_a[1] == "email") {
							st_parser_type = TextServer::STRUCTURED_TEXT_EMAIL;
						} else if (subtag_a[1] == "l" || subtag_a[1] == "list") {
							st_parser_type = TextServer::STRUCTURED_TEXT_LIST;
						} else if (subtag_a[1] == "n" || subtag_a[1] == "gdscript") {
							st_parser_type = TextServer::STRUCTURED_TEXT_GDSCRIPT;
						} else if (subtag_a[1] == "c" || subtag_a[1] == "custom") {
							st_parser_type = TextServer::STRUCTURED_TEXT_CUSTOM;
						}
					}
				}
			}
			push_paragraph(alignment, dir, lang, st_parser_type, jst_flags, tab_stops);
			pos = brk_end + 1;
			tag_stack.push_front("p");
		} else if (tag == "url") {
			int end = p_bbcode.find("[", brk_end);
			if (end == -1) {
				end = p_bbcode.length();
			}
			String url = p_bbcode.substr(brk_end + 1, end - brk_end - 1).unquote();
			push_meta(url);

			pos = brk_end + 1;
			tag_stack.push_front(tag);

		} else if (tag.begins_with("url=")) {
			String url = tag.substr(4, tag.length()).unquote();
			push_meta(url);
			pos = brk_end + 1;
			tag_stack.push_front("url");
		} else if (tag.begins_with("hint=")) {
			String description = tag.substr(5, tag.length()).unquote();
			push_hint(description);
			pos = brk_end + 1;
			tag_stack.push_front("hint");
		} else if (tag.begins_with("dropcap")) {
			Vector<String> subtag = tag.substr(5, tag.length()).split(" ");
			_normalize_subtags(subtag);

			int fs = theme_cache.normal_font_size * 3;
			Ref<Font> f = theme_cache.normal_font;
			Color color = theme_cache.default_color;
			Color outline_color = theme_cache.font_outline_color;
			int outline_size = theme_cache.outline_size;
			Rect2 dropcap_margins;

			for (int i = 0; i < subtag.size(); i++) {
				Vector<String> subtag_a = subtag[i].split("=");
				_normalize_subtags(subtag_a);

				if (subtag_a.size() == 2) {
					if (subtag_a[0] == "font" || subtag_a[0] == "f") {
						String fnt = subtag_a[1];
						Ref<Font> font = ResourceLoader::load(fnt, "Font");
						if (font.is_valid()) {
							f = font;
						}
					} else if (subtag_a[0] == "font_size") {
						fs = subtag_a[1].to_int();
					} else if (subtag_a[0] == "margins") {
						Vector<String> subtag_b = subtag_a[1].split(",");
						_normalize_subtags(subtag_b);

						if (subtag_b.size() == 4) {
							dropcap_margins.position.x = subtag_b[0].to_float();
							dropcap_margins.position.y = subtag_b[1].to_float();
							dropcap_margins.size.x = subtag_b[2].to_float();
							dropcap_margins.size.y = subtag_b[3].to_float();
						}
					} else if (subtag_a[0] == "outline_size") {
						outline_size = subtag_a[1].to_int();
					} else if (subtag_a[0] == "color") {
						color = Color::from_string(subtag_a[1], color);
					} else if (subtag_a[0] == "outline_color") {
						outline_color = Color::from_string(subtag_a[1], outline_color);
					}
				}
			}
			int end = p_bbcode.find("[", brk_end);
			if (end == -1) {
				end = p_bbcode.length();
			}

			String dc_txt = p_bbcode.substr(brk_end + 1, end - brk_end - 1);

			push_dropcap(dc_txt, f, fs, dropcap_margins, color, outline_size, outline_color);

			pos = end;
			tag_stack.push_front(bbcode_name);
		} else if (tag.begins_with("img")) {
			int alignment = INLINE_ALIGNMENT_CENTER;
			if (tag.begins_with("img=")) {
				Vector<String> subtag = tag.substr(4, tag.length()).split(",");
				_normalize_subtags(subtag);

				if (subtag.size() > 1) {
					if (subtag[0] == "top" || subtag[0] == "t") {
						alignment = INLINE_ALIGNMENT_TOP_TO;
					} else if (subtag[0] == "center" || subtag[0] == "c") {
						alignment = INLINE_ALIGNMENT_CENTER_TO;
					} else if (subtag[0] == "bottom" || subtag[0] == "b") {
						alignment = INLINE_ALIGNMENT_BOTTOM_TO;
					}
					if (subtag[1] == "top" || subtag[1] == "t") {
						alignment |= INLINE_ALIGNMENT_TO_TOP;
					} else if (subtag[1] == "center" || subtag[1] == "c") {
						alignment |= INLINE_ALIGNMENT_TO_CENTER;
					} else if (subtag[1] == "baseline" || subtag[1] == "l") {
						alignment |= INLINE_ALIGNMENT_TO_BASELINE;
					} else if (subtag[1] == "bottom" || subtag[1] == "b") {
						alignment |= INLINE_ALIGNMENT_TO_BOTTOM;
					}
				} else if (subtag.size() > 0) {
					if (subtag[0] == "top" || subtag[0] == "t") {
						alignment = INLINE_ALIGNMENT_TOP;
					} else if (subtag[0] == "center" || subtag[0] == "c") {
						alignment = INLINE_ALIGNMENT_CENTER;
					} else if (subtag[0] == "bottom" || subtag[0] == "b") {
						alignment = INLINE_ALIGNMENT_BOTTOM;
					}
				}
			}

			int end = p_bbcode.find("[", brk_end);
			if (end == -1) {
				end = p_bbcode.length();
			}

			String image = p_bbcode.substr(brk_end + 1, end - brk_end - 1);

			Ref<Texture2D> texture = ResourceLoader::load(image, "Texture2D");
			if (texture.is_valid()) {
				Rect2 region;
				OptionMap::Iterator region_option = bbcode_options.find("region");
				if (region_option) {
					Vector<String> region_values = region_option->value.split(",", false);
					if (region_values.size() == 4) {
						region.position.x = region_values[0].to_float();
						region.position.y = region_values[1].to_float();
						region.size.x = region_values[2].to_float();
						region.size.y = region_values[3].to_float();
					}
				}

				Color color = Color(1.0, 1.0, 1.0);
				OptionMap::Iterator color_option = bbcode_options.find("color");
				if (color_option) {
					color = Color::from_string(color_option->value, color);
				}

				int width = 0;
				int height = 0;
				if (!bbcode_value.is_empty()) {
					int sep = bbcode_value.find("x");
					if (sep == -1) {
						width = bbcode_value.to_int();
					} else {
						width = bbcode_value.substr(0, sep).to_int();
						height = bbcode_value.substr(sep + 1).to_int();
					}
				} else {
					OptionMap::Iterator width_option = bbcode_options.find("width");
					if (width_option) {
						width = width_option->value.to_int();
					}

					OptionMap::Iterator height_option = bbcode_options.find("height");
					if (height_option) {
						height = height_option->value.to_int();
					}
				}

				add_image(texture, width, height, color, (InlineAlignment)alignment, region);
			}

			pos = end;
			tag_stack.push_front(bbcode_name);
		} else if (tag.begins_with("color=")) {
			String color_str = tag.substr(6, tag.length()).unquote();
			Color color = Color::from_string(color_str, theme_cache.default_color);
			push_color(color);
			pos = brk_end + 1;
			tag_stack.push_front("color");

		} else if (tag.begins_with("outline_color=")) {
			String color_str = tag.substr(14, tag.length()).unquote();
			Color color = Color::from_string(color_str, theme_cache.default_color);
			push_outline_color(color);
			pos = brk_end + 1;
			tag_stack.push_front("outline_color");

		} else if (tag.begins_with("font_size=")) {
			int fnt_size = tag.substr(10, tag.length()).to_int();
			push_font_size(fnt_size);
			pos = brk_end + 1;
			tag_stack.push_front("font_size");

		} else if (tag.begins_with("opentype_features=") || tag.begins_with("otf=")) {
			int value_pos = tag.find("=");
			String fnt_ftr = tag.substr(value_pos + 1);
			Vector<String> subtag = fnt_ftr.split(",");
			_normalize_subtags(subtag);

			if (subtag.size() > 0) {
				Ref<Font> font = theme_cache.normal_font;
				DefaultFont def_font = NORMAL_FONT;

				ItemFont *font_it = _find_font(current);
				if (font_it) {
					if (font_it->font.is_valid()) {
						font = font_it->font;
						def_font = font_it->def_font;
					}
				}
				Dictionary features;
				for (int i = 0; i < subtag.size(); i++) {
					Vector<String> subtag_a = subtag[i].split("=");
					_normalize_subtags(subtag_a);

					if (subtag_a.size() == 2) {
						features[TS->name_to_tag(subtag_a[0])] = subtag_a[1].to_int();
					} else if (subtag_a.size() == 1) {
						features[TS->name_to_tag(subtag_a[0])] = 1;
					}
				}

				Ref<FontVariation> fc;
				fc.instantiate();

				fc->set_base_font(font);
				fc->set_opentype_features(features);

				if (def_font != CUSTOM_FONT) {
					_push_def_font_var(def_font, fc);
				} else {
					push_font(fc);
				}
			}
			pos = brk_end + 1;
			tag_stack.push_front(tag.substr(0, value_pos));

		} else if (tag.begins_with("font=")) {
			String fnt = tag.substr(5, tag.length()).unquote();

			Ref<Font> fc = ResourceLoader::load(fnt, "Font");
			if (fc.is_valid()) {
				push_font(fc);
			}

			pos = brk_end + 1;
			tag_stack.push_front("font");

		} else if (tag.begins_with("font ")) {
			Vector<String> subtag = tag.substr(2, tag.length()).split(" ");
			_normalize_subtags(subtag);

			Ref<Font> font = theme_cache.normal_font;
			DefaultFont def_font = NORMAL_FONT;

			ItemFont *font_it = _find_font(current);
			if (font_it) {
				if (font_it->font.is_valid()) {
					font = font_it->font;
					def_font = font_it->def_font;
				}
			}

			Ref<FontVariation> fc;
			fc.instantiate();

			int fnt_size = -1;
			for (int i = 1; i < subtag.size(); i++) {
				Vector<String> subtag_a = subtag[i].split("=", true, 1);
				_normalize_subtags(subtag_a);

				if (subtag_a.size() == 2) {
					if (subtag_a[0] == "name" || subtag_a[0] == "n") {
						String fnt = subtag_a[1];
						Ref<Font> font_data = ResourceLoader::load(fnt, "Font");
						if (font_data.is_valid()) {
							font = font_data;
							def_font = CUSTOM_FONT;
						}
					} else if (subtag_a[0] == "size" || subtag_a[0] == "s") {
						fnt_size = subtag_a[1].to_int();
					} else if (subtag_a[0] == "glyph_spacing" || subtag_a[0] == "gl") {
						int spacing = subtag_a[1].to_int();
						fc->set_spacing(TextServer::SPACING_GLYPH, spacing);
					} else if (subtag_a[0] == "space_spacing" || subtag_a[0] == "sp") {
						int spacing = subtag_a[1].to_int();
						fc->set_spacing(TextServer::SPACING_SPACE, spacing);
					} else if (subtag_a[0] == "top_spacing" || subtag_a[0] == "top") {
						int spacing = subtag_a[1].to_int();
						fc->set_spacing(TextServer::SPACING_TOP, spacing);
					} else if (subtag_a[0] == "bottom_spacing" || subtag_a[0] == "bt") {
						int spacing = subtag_a[1].to_int();
						fc->set_spacing(TextServer::SPACING_BOTTOM, spacing);
					} else if (subtag_a[0] == "embolden" || subtag_a[0] == "emb") {
						float emb = subtag_a[1].to_float();
						fc->set_variation_embolden(emb);
					} else if (subtag_a[0] == "face_index" || subtag_a[0] == "fi") {
						int fi = subtag_a[1].to_int();
						fc->set_variation_face_index(fi);
					} else if (subtag_a[0] == "slant" || subtag_a[0] == "sln") {
						float slant = subtag_a[1].to_float();
						fc->set_variation_transform(Transform2D(1.0, slant, 0.0, 1.0, 0.0, 0.0));
					} else if (subtag_a[0] == "opentype_variation" || subtag_a[0] == "otv") {
						Dictionary variations;
						if (!subtag_a[1].is_empty()) {
							Vector<String> variation_tags = subtag_a[1].split(",");
							for (int j = 0; j < variation_tags.size(); j++) {
								Vector<String> subtag_b = variation_tags[j].split("=");
								_normalize_subtags(subtag_b);

								if (subtag_b.size() == 2) {
									variations[TS->name_to_tag(subtag_b[0])] = subtag_b[1].to_float();
								}
							}
							fc->set_variation_opentype(variations);
						}
					} else if (subtag_a[0] == "opentype_features" || subtag_a[0] == "otf") {
						Dictionary features;
						if (!subtag_a[1].is_empty()) {
							Vector<String> feature_tags = subtag_a[1].split(",");
							for (int j = 0; j < feature_tags.size(); j++) {
								Vector<String> subtag_b = feature_tags[j].split("=");
								_normalize_subtags(subtag_b);

								if (subtag_b.size() == 2) {
									features[TS->name_to_tag(subtag_b[0])] = subtag_b[1].to_float();
								} else if (subtag_b.size() == 1) {
									features[TS->name_to_tag(subtag_b[0])] = 1;
								}
							}
							fc->set_opentype_features(features);
						}
					}
				}
			}
			fc->set_base_font(font);

			if (def_font != CUSTOM_FONT) {
				_push_def_font_var(def_font, fc, fnt_size);
			} else {
				push_font(fc, fnt_size);
			}

			pos = brk_end + 1;
			tag_stack.push_front("font");

		} else if (tag.begins_with("outline_size=")) {
			int fnt_size = tag.substr(13, tag.length()).to_int();
			if (fnt_size > 0) {
				push_outline_size(fnt_size);
			}
			pos = brk_end + 1;
			tag_stack.push_front("outline_size");

		} else if (bbcode_name == "fade") {
			int start_index = 0;
			OptionMap::Iterator start_option = bbcode_options.find("start");
			if (start_option) {
				start_index = start_option->value.to_int();
			}

			int length = 10;
			OptionMap::Iterator length_option = bbcode_options.find("length");
			if (length_option) {
				length = length_option->value.to_int();
			}

			push_fade(start_index, length);
			pos = brk_end + 1;
			tag_stack.push_front("fade");
		} else if (bbcode_name == "shake") {
			int strength = 5;
			OptionMap::Iterator strength_option = bbcode_options.find("level");
			if (strength_option) {
				strength = strength_option->value.to_int();
			}

			float rate = 20.0f;
			OptionMap::Iterator rate_option = bbcode_options.find("rate");
			if (rate_option) {
				rate = rate_option->value.to_float();
			}

			bool connected = true;
			OptionMap::Iterator connected_option = bbcode_options.find("connected");
			if (connected_option) {
				connected = connected_option->value.to_int();
			}

			push_shake(strength, rate, connected);
			pos = brk_end + 1;
			tag_stack.push_front("shake");
			set_process_internal(true);
		} else if (bbcode_name == "wave") {
			float amplitude = 20.0f;
			OptionMap::Iterator amplitude_option = bbcode_options.find("amp");
			if (amplitude_option) {
				amplitude = amplitude_option->value.to_float();
			}

			float period = 5.0f;
			OptionMap::Iterator period_option = bbcode_options.find("freq");
			if (period_option) {
				period = period_option->value.to_float();
			}

			bool connected = true;
			OptionMap::Iterator connected_option = bbcode_options.find("connected");
			if (connected_option) {
				connected = connected_option->value.to_int();
			}

			push_wave(period, amplitude, connected);
			pos = brk_end + 1;
			tag_stack.push_front("wave");
			set_process_internal(true);
		} else if (bbcode_name == "tornado") {
			float radius = 10.0f;
			OptionMap::Iterator radius_option = bbcode_options.find("radius");
			if (radius_option) {
				radius = radius_option->value.to_float();
			}

			float frequency = 1.0f;
			OptionMap::Iterator frequency_option = bbcode_options.find("freq");
			if (frequency_option) {
				frequency = frequency_option->value.to_float();
			}

			bool connected = true;
			OptionMap::Iterator connected_option = bbcode_options.find("connected");
			if (connected_option) {
				connected = connected_option->value.to_int();
			}

			push_tornado(frequency, radius, connected);
			pos = brk_end + 1;
			tag_stack.push_front("tornado");
			set_process_internal(true);
		} else if (bbcode_name == "rainbow") {
			float saturation = 0.8f;
			OptionMap::Iterator saturation_option = bbcode_options.find("sat");
			if (saturation_option) {
				saturation = saturation_option->value.to_float();
			}

			float value = 0.8f;
			OptionMap::Iterator value_option = bbcode_options.find("val");
			if (value_option) {
				value = value_option->value.to_float();
			}

			float frequency = 1.0f;
			OptionMap::Iterator frequency_option = bbcode_options.find("freq");
			if (frequency_option) {
				frequency = frequency_option->value.to_float();
			}

			push_rainbow(saturation, value, frequency);
			pos = brk_end + 1;
			tag_stack.push_front("rainbow");
			set_process_internal(true);

		} else if (tag.begins_with("bgcolor=")) {
			String color_str = tag.substr(8, tag.length()).unquote();
			Color color = Color::from_string(color_str, theme_cache.default_color);

			push_bgcolor(color);
			pos = brk_end + 1;
			tag_stack.push_front("bgcolor");

		} else if (tag.begins_with("fgcolor=")) {
			String color_str = tag.substr(8, tag.length()).unquote();
			Color color = Color::from_string(color_str, theme_cache.default_color);

			push_fgcolor(color);
			pos = brk_end + 1;
			tag_stack.push_front("fgcolor");

		} else {
			Vector<String> &expr = split_tag_block;
			if (expr.size() < 1) {
				add_text("[");
				pos = brk_pos + 1;
			} else {
				String identifier = expr[0];
				expr.remove_at(0);
				Dictionary properties = parse_expressions_for_values(expr);
				Ref<RichTextEffect> effect = _get_custom_effect_by_code(identifier);

				if (!effect.is_null()) {
					push_customfx(effect, properties);
					pos = brk_end + 1;
					tag_stack.push_front(identifier);
				} else {
					add_text("["); //ignore
					pos = brk_pos + 1;
				}
			}
		}
	}

	Vector<ItemFX *> fx_items;
	for (Item *E : main->subitems) {
		Item *subitem = static_cast<Item *>(E);
		_fetch_item_fx_stack(subitem, fx_items);

		if (fx_items.size()) {
			set_process_internal(true);
			break;
		}
	}
}

void SelectableRichTextLabel::scroll_to_selection() {
	if (selection.active && selection.from_frame && selection.from_line >= 0 && selection.from_line < (int)selection.from_frame->lines.size()) {
		// Selected frame paragraph offset.
		float line_offset = selection.from_frame->lines[selection.from_line].offset.y;

		// Add wrapped line offset.
		for (int i = 0; i < selection.from_frame->lines[selection.from_line].text_buf->get_line_count(); i++) {
			Vector2i range = selection.from_frame->lines[selection.from_line].text_buf->get_line_range(i);
			if (range.x <= selection.from_char && range.y >= selection.from_char) {
				break;
			}
			line_offset += selection.from_frame->lines[selection.from_line].text_buf->get_line_size(i).y + theme_cache.line_separation;
		}

		// Add nested frame (e.g. table cell) offset.
		ItemFrame *it = selection.from_frame;
		while (it->parent_frame != nullptr) {
			line_offset += it->parent_frame->lines[it->line].offset.y;
			it = it->parent_frame;
		}
		vscroll->set_value(line_offset);
	}
}

void SelectableRichTextLabel::scroll_to_selection_centered() {
	if (
		selection.active &&
		selection.from_frame &&
		selection.from_line >= 0 &&
		selection.from_line < (int)selection.from_frame->lines.size() &&
		selection.to_frame &&
		selection.to_line >= 0 &&
		selection.to_line < (int)selection.to_frame->lines.size())
	{
		_validate_line_caches();

		// Selected frame paragraph offset.
		float from_line_offset = selection.from_frame->lines[selection.from_line].offset.y;

		// Add wrapped line offset.
		for (int i = 0; i < selection.from_frame->lines[selection.from_line].text_buf->get_line_count(); i++) {
			Vector2i range = selection.from_frame->lines[selection.from_line].text_buf->get_line_range(i);
			if (range.x <= selection.from_char && range.y >= selection.from_char) {
				break;
			}
			from_line_offset += selection.from_frame->lines[selection.from_line].text_buf->get_line_size(i).y + theme_cache.line_separation;
		}

		// Add nested frame (e.g. table cell) offset.
		ItemFrame *it = selection.from_frame;
		while (it->parent_frame != nullptr) {
			from_line_offset += it->parent_frame->lines[it->line].offset.y;
			it = it->parent_frame;
		}

		// Selected frame paragraph offset.
		float to_line_offset = selection.to_frame->lines[selection.to_line].offset.y;

		// Add wrapped line offset.
		for (int i = 0; i < selection.to_frame->lines[selection.to_line].text_buf->get_line_count(); i++) {
			Vector2i range = selection.to_frame->lines[selection.to_line].text_buf->get_line_range(i);
			if (range.x <= selection.to_char && range.y >= selection.to_char) {
				break;
			}
			to_line_offset += selection.to_frame->lines[selection.to_line].text_buf->get_line_size(i).y + theme_cache.line_separation;
		}

		// Add nested frame (e.g. table cell) offset.
		it = selection.to_frame;
		while (it->parent_frame != nullptr) {
			to_line_offset += it->parent_frame->lines[it->line].offset.y;
			it = it->parent_frame;
		}

		float selection_height = to_line_offset - from_line_offset;
		float visible_height = get_size().y;

		if (selection_height >= visible_height) {
			scroll_to_selection();
			return;
		}

		float delta = (visible_height - selection_height) / 2.0;
		vscroll->set_value(from_line_offset - delta);
	}
}

void SelectableRichTextLabel::scroll_to_paragraph(int p_paragraph) {
	_validate_line_caches();

	if (p_paragraph <= 0) {
		vscroll->set_value(0);
	} else if (p_paragraph >= main->first_invalid_line.load()) {
		vscroll->set_value(vscroll->get_max());
	} else {
		vscroll->set_value(main->lines[p_paragraph].offset.y);
	}
}

int SelectableRichTextLabel::get_paragraph_count() const {
	return current_frame->lines.size();
}

int SelectableRichTextLabel::get_visible_paragraph_count() const {
	if (!is_visible()) {
		return 0;
	}

	const_cast<SelectableRichTextLabel *>(this)->_validate_line_caches();
	return visible_paragraph_count;
}

void SelectableRichTextLabel::scroll_to_line(int p_line) {
	if (p_line <= 0) {
		vscroll->set_value(0);
		return;
	}
	_validate_line_caches();

	int line_count = 0;
	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		MutexLock lock(main->lines[i].text_buf->get_mutex());
		if ((line_count <= p_line) && (line_count + main->lines[i].text_buf->get_line_count() >= p_line)) {
			float line_offset = 0.f;
			for (int j = 0; j < p_line - line_count; j++) {
				line_offset += main->lines[i].text_buf->get_line_size(j).y + theme_cache.line_separation;
			}
			vscroll->set_value(main->lines[i].offset.y + line_offset);
			return;
		}
		line_count += main->lines[i].text_buf->get_line_count();
	}
	vscroll->set_value(vscroll->get_max());
}

float SelectableRichTextLabel::get_line_offset(int p_line) {
	_validate_line_caches();

	int line_count = 0;
	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		MutexLock lock(main->lines[i].text_buf->get_mutex());
		if ((line_count <= p_line) && (p_line <= line_count + main->lines[i].text_buf->get_line_count())) {
			float line_offset = 0.f;
			for (int j = 0; j < p_line - line_count; j++) {
				line_offset += main->lines[i].text_buf->get_line_size(j).y + theme_cache.line_separation;
			}
			return main->lines[i].offset.y + line_offset;
		}
		line_count += main->lines[i].text_buf->get_line_count();
	}
	return 0;
}

float SelectableRichTextLabel::get_paragraph_offset(int p_paragraph) {
	_validate_line_caches();

	int to_line = main->first_invalid_line.load();
	if (0 <= p_paragraph && p_paragraph < to_line) {
		return main->lines[p_paragraph].offset.y;
	}
	return 0;
}

int SelectableRichTextLabel::get_line_count() const {
	const_cast<SelectableRichTextLabel *>(this)->_validate_line_caches();

	int line_count = 0;
	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		MutexLock lock(main->lines[i].text_buf->get_mutex());
		line_count += main->lines[i].text_buf->get_line_count();
	}
	return line_count;
}

int SelectableRichTextLabel::get_visible_line_count() const {
	if (!is_visible()) {
		return 0;
	}
	const_cast<SelectableRichTextLabel *>(this)->_validate_line_caches();

	return visible_line_count;
}

void SelectableRichTextLabel::set_selection_enabled(bool p_enabled) {
	if (selection.enabled == p_enabled) {
		return;
	}

	selection.enabled = p_enabled;
	if (!p_enabled) {
		if (selection.active) {
			deselect();
		}
		set_focus_mode(FOCUS_NONE);
	} else {
		set_focus_mode(FOCUS_ALL);
	}
}

void SelectableRichTextLabel::set_deselect_on_focus_loss_enabled(const bool p_enabled) {
	if (deselect_on_focus_loss_enabled == p_enabled) {
		return;
	}

	deselect_on_focus_loss_enabled = p_enabled;
	if (p_enabled && selection.active && !has_focus()) {
		deselect();
	}
}

Variant SelectableRichTextLabel::get_drag_data(const Point2 &p_point) {
	Variant ret = Control::get_drag_data(p_point);
	if (ret != Variant()) {
		return ret;
	}

	if (selection.drag_attempt && selection.enabled) {
		String t = get_selected_text();
		Label *l = memnew(Label);
		l->set_text(t);
		set_drag_preview(l);
		return t;
	}

	return Variant();
}

bool SelectableRichTextLabel::_is_click_inside_selection() const {
	if (selection.active && selection.enabled && selection.click_frame && selection.from_frame && selection.to_frame) {
		const Line &l_click = selection.click_frame->lines[selection.click_line];
		const Line &l_from = selection.from_frame->lines[selection.from_line];
		const Line &l_to = selection.to_frame->lines[selection.to_line];
		return (l_click.char_offset + selection.click_char >= l_from.char_offset + selection.from_char) && (l_click.char_offset + selection.click_char <= l_to.char_offset + selection.to_char);
	} else {
		return false;
	}
}

bool SelectableRichTextLabel::_search_table(ItemTable *p_table, List<Item *>::Element *p_from, const String &p_string, bool p_reverse_search) {
	List<Item *>::Element *E = p_from;
	while (E != nullptr) {
		ERR_CONTINUE(E->get()->type != ITEM_FRAME); // Children should all be frames.
		ItemFrame *frame = static_cast<ItemFrame *>(E->get());
		if (p_reverse_search) {
			for (int i = (int)frame->lines.size() - 1; i >= 0; i--) {
				if (_search_line(frame, i, p_string, -1, p_reverse_search)) {
					return true;
				}
			}
		} else {
			for (int i = 0; i < (int)frame->lines.size(); i++) {
				if (_search_line(frame, i, p_string, 0, p_reverse_search)) {
					return true;
				}
			}
		}
		E = p_reverse_search ? E->prev() : E->next();
	}
	return false;
}

bool SelectableRichTextLabel::_search_line(ItemFrame *p_frame, int p_line, const String &p_string, int p_char_idx, bool p_reverse_search) {
	ERR_FAIL_COND_V(p_frame == nullptr, false);
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)p_frame->lines.size(), false);

	Line &l = p_frame->lines[p_line];

	String txt;
	Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;
	for (Item *it = l.from; it && it != it_to; it = _get_next_item(it)) {
		switch (it->type) {
			case ITEM_NEWLINE: {
				txt += "\n";
			} break;
			case ITEM_TEXT: {
				ItemText *t = static_cast<ItemText *>(it);
				txt += t->text;
			} break;
			case ITEM_IMAGE: {
				txt += " ";
			} break;
			case ITEM_TABLE: {
				ItemTable *table = static_cast<ItemTable *>(it);
				List<Item *>::Element *E = p_reverse_search ? table->subitems.back() : table->subitems.front();
				if (_search_table(table, E, p_string, p_reverse_search)) {
					return true;
				}
			} break;
			default:
				break;
		}
	}

	int sp = -1;
	if (p_reverse_search) {
		sp = txt.rfindn(p_string, p_char_idx);
	} else {
		sp = txt.findn(p_string, p_char_idx);
	}

	if (sp != -1) {
		selection.from_frame = p_frame;
		selection.from_line = p_line;
		selection.from_item = _get_item_at_pos(l.from, it_to, sp);
		selection.from_char = sp;
		selection.to_frame = p_frame;
		selection.to_line = p_line;
		selection.to_item = _get_item_at_pos(l.from, it_to, sp + p_string.length());
		selection.to_char = sp + p_string.length();
		selection.active = true;
		emit_signal("selection_active", true);
		return true;
	}

	return false;
}

bool SelectableRichTextLabel::search(const String &p_string, bool p_from_selection, bool p_search_previous) {
	ERR_FAIL_COND_V(!selection.enabled, false);

	if (p_string.size() == 0) {
		selection.active = false;
		emit_signal("selection_active", false);
		return false;
	}

	int char_idx = p_search_previous ? -1 : 0;
	int current_line = 0;
	int to_line = main->first_invalid_line.load();
	int ending_line = to_line - 1;
	if (p_from_selection && selection.active) {
		// First check to see if other results exist in current line
		char_idx = p_search_previous ? selection.from_char - 1 : selection.to_char;
		if (!(p_search_previous && char_idx < 0) &&
				_search_line(selection.from_frame, selection.from_line, p_string, char_idx, p_search_previous)) {
			scroll_to_selection();
			queue_redraw();
			return true;
		}
		char_idx = p_search_previous ? -1 : 0;

		// Next, check to see if the current search result is in a table
		if (selection.from_frame->parent != nullptr && selection.from_frame->parent->type == ITEM_TABLE) {
			// Find last search result in table
			ItemTable *parent_table = static_cast<ItemTable *>(selection.from_frame->parent);
			List<Item *>::Element *parent_element = p_search_previous ? parent_table->subitems.back() : parent_table->subitems.front();

			while (parent_element->get() != selection.from_frame) {
				parent_element = p_search_previous ? parent_element->prev() : parent_element->next();
				ERR_FAIL_COND_V(parent_element == nullptr, false);
			}

			// Search remainder of table
			if (!(p_search_previous && parent_element == parent_table->subitems.front()) &&
					parent_element != parent_table->subitems.back()) {
				parent_element = p_search_previous ? parent_element->prev() : parent_element->next(); // Don't want to search current item
				ERR_FAIL_COND_V(parent_element == nullptr, false);

				// Search for next element
				if (_search_table(parent_table, parent_element, p_string, p_search_previous)) {
					scroll_to_selection();
					queue_redraw();
					return true;
				}
			}
		}

		ending_line = selection.from_frame->line + selection.from_line;
		current_line = p_search_previous ? ending_line - 1 : ending_line + 1;
	} else if (p_search_previous) {
		current_line = ending_line;
		ending_line = 0;
	}

	// Search remainder of the file
	while (current_line != ending_line) {
		// Wrap around
		if (current_line < 0) {
			current_line = to_line - 1;
		} else if (current_line >= to_line) {
			current_line = 0;
		}

		if (_search_line(main, current_line, p_string, char_idx, p_search_previous)) {
			scroll_to_selection();
			queue_redraw();
			return true;
		}

		if (current_line != ending_line) {
			p_search_previous ? current_line-- : current_line++;
		}
	}

	if (p_from_selection && selection.active) {
		// Check contents of selection
		return _search_line(main, current_line, p_string, char_idx, p_search_previous);
	} else {
		return false;
	}
}

String SelectableRichTextLabel::_get_line_text(ItemFrame *p_frame, int p_line, Selection p_selection) const {
	String txt;

	ERR_FAIL_COND_V(p_frame == nullptr, txt);
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)p_frame->lines.size(), txt);

	Line &l = p_frame->lines[p_line];

	Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;
	int end_idx = 0;
	if (it_to != nullptr) {
		end_idx = it_to->index;
	} else {
		for (Item *it = l.from; it; it = _get_next_item(it)) {
			end_idx = it->index + 1;
		}
	}
	for (Item *it = l.from; it && it != it_to; it = _get_next_item(it)) {
		if (it->type == ITEM_TABLE) {
			ItemTable *table = static_cast<ItemTable *>(it);
			for (Item *E : table->subitems) {
				ERR_CONTINUE(E->type != ITEM_FRAME); // Children should all be frames.
				ItemFrame *frame = static_cast<ItemFrame *>(E);
				for (int i = 0; i < (int)frame->lines.size(); i++) {
					txt += _get_line_text(frame, i, p_selection);
				}
			}
		}
		if ((p_selection.to_item != nullptr) && (p_selection.to_item->index < l.from->index)) {
			continue;
		}
		if ((p_selection.from_item != nullptr) && (p_selection.from_item->index >= end_idx)) {
			continue;
		}
		if (it->type == ITEM_DROPCAP) {
			const ItemDropcap *dc = static_cast<ItemDropcap *>(it);
			txt += dc->text;
		} else if (it->type == ITEM_TEXT) {
			const ItemText *t = static_cast<ItemText *>(it);
			txt += t->text;
		} else if (it->type == ITEM_NEWLINE) {
			txt += "\n";
		} else if (it->type == ITEM_IMAGE) {
			txt += " ";
		}
	}
	if ((l.from != nullptr) && (p_frame == p_selection.to_frame) && (p_selection.to_item != nullptr) && (p_selection.to_item->index >= l.from->index) && (p_selection.to_item->index < end_idx)) {
		txt = txt.substr(0, p_selection.to_char);
	}
	if ((l.from != nullptr) && (p_frame == p_selection.from_frame) && (p_selection.from_item != nullptr) && (p_selection.from_item->index >= l.from->index) && (p_selection.from_item->index < end_idx)) {
		txt = txt.substr(p_selection.from_char, -1);
	}
	return txt;
}

void SelectableRichTextLabel::set_context_menu_enabled(bool p_enabled) {
	context_menu_enabled = p_enabled;
}

bool SelectableRichTextLabel::is_context_menu_enabled() const {
	return context_menu_enabled;
}

void SelectableRichTextLabel::set_shortcut_keys_enabled(bool p_enabled) {
	shortcut_keys_enabled = p_enabled;
}

bool SelectableRichTextLabel::is_shortcut_keys_enabled() const {
	return shortcut_keys_enabled;
}

// Context menu.
PopupMenu *SelectableRichTextLabel::get_menu() const {
	if (!menu) {
		const_cast<SelectableRichTextLabel *>(this)->_generate_context_menu();
	}
	return menu;
}

bool SelectableRichTextLabel::is_menu_visible() const {
	return menu && menu->is_visible();
}


String SelectableRichTextLabel::get_before_selected_text() const {
	int selection_from = get_selection_from();
	if (selection_from == -1) { return ""; }
	return get_parsed_text().substr(0, selection_from);
}

String SelectableRichTextLabel::get_selected_text() const {
	int selection_from = get_selection_from();
	if (selection_from == -1) { return ""; }

	int selection_to = get_selection_to();
	if (selection_to == -1) { return ""; }

	return get_parsed_text().substr(selection_from, selection_to - selection_from + 1);
}

String SelectableRichTextLabel::get_after_selected_text() const {
	int selection_to = get_selection_to();
	if (selection_to == -1) { return ""; }
	return get_parsed_text().substr(selection_to + 1);
}


void SelectableRichTextLabel::deselect() {
	selection.active = false;
	emit_signal("selection_active", false);
	queue_redraw();
}

void SelectableRichTextLabel::selection_copy() {
	String txt = get_selected_text();

	if (!txt.is_empty()) {
		DisplayServer::get_singleton()->clipboard_set(txt);
	}
}

void SelectableRichTextLabel::select_all() {
	if (!selection.enabled) {
		return;
	}

	Item *it = main;
	Item *from_item = nullptr;
	Item *to_item = nullptr;

	while (it) {
		if (it->type != ITEM_FRAME) {
			if (!from_item) {
				from_item = it;
			} else {
				to_item = it;
			}
		}
		it = _get_next_item(it, true);
	}
	if (!from_item || !to_item) {
		return;
	}

	ItemFrame *from_frame = nullptr;
	int from_line = 0;
	_find_frame(from_item, &from_frame, &from_line);
	if (!from_frame) {
		return;
	}
	ItemFrame *to_frame = nullptr;
	int to_line = 0;
	_find_frame(to_item, &to_frame, &to_line);
	if (!to_frame) {
		return;
	}
	selection.from_line = from_line;
	selection.from_frame = from_frame;
	selection.from_char = 0;
	selection.from_item = from_item;
	selection.to_line = to_line;
	selection.to_frame = to_frame;
	selection.to_char = to_frame->lines[to_line].char_count;
	selection.to_item = to_item;
	selection.active = true;
	emit_signal("selection_active", true);
	queue_redraw();
}

bool SelectableRichTextLabel::is_selection_enabled() const {
	return selection.enabled;
}

bool SelectableRichTextLabel::is_deselect_on_focus_loss_enabled() const {
	return deselect_on_focus_loss_enabled;
}


bool SelectableRichTextLabel::_set_selection_from(ItemFrame* p_frame, int p_from) {
	if (!p_frame->subitems.is_empty()) {
		for (Item* p_item : p_frame->subitems) {
			if (p_item->type == ITEM_FRAME) {
				if (_set_selection_from((ItemFrame*)p_item, p_from)) {
					return true;
				}
			}
		}
	}

	for (uint32_t line_idx = 0; line_idx < p_frame->first_invalid_line.load(); ++line_idx) {
		Line& line = p_frame->lines[line_idx];

		if ((p_from >= line.char_offset) && (p_from <= (line.char_offset + line.char_count))) {
			selection.from_frame = p_frame;
			selection.from_line = line_idx;
			selection.from_char = p_from - line.char_offset;
			return true;
		}
	}

	return false;
}

void SelectableRichTextLabel::set_selection_from(int p_from) {
	if (!selection.enabled) {
		return;
	}

	_validate_line_caches();

	if (_set_selection_from(current_frame, p_from)) {
		selection.active = (selection.from_frame != nullptr) && (selection.to_frame != nullptr);
		emit_signal("selection_active", selection.active);
	}
	else {
		selection.from_frame = nullptr;
		selection.from_line = 0;
		selection.from_char = 0;
	}
}

int SelectableRichTextLabel::get_selection_from() const {
	if (!selection.active || !selection.enabled) {
		return -1;
	}

	return selection.from_frame->lines[selection.from_line].char_offset + selection.from_char;
}


bool SelectableRichTextLabel::_set_selection_to(ItemFrame* p_frame, int p_to) {
	if (!p_frame->subitems.is_empty()) {
		for (Item* p_item : p_frame->subitems) {
			if (p_item->type == ITEM_FRAME) {
				if (_set_selection_to((ItemFrame*)p_item, p_to)) {
					return true;
				}
			}
		}
	}

	for (uint32_t line_idx = 0; line_idx < p_frame->first_invalid_line.load(); ++line_idx) {
		Line& line = p_frame->lines[line_idx];

		if ((p_to >= line.char_offset) && (p_to <= (line.char_offset + line.char_count))) {
			selection.to_frame = p_frame;
			selection.to_line = line_idx;
			selection.to_char = p_to - line.char_offset;
			return true;
		}
	}

	return false;
}

void SelectableRichTextLabel::set_selection_to(int p_to) {
	if (!selection.enabled) {
		return;
	}

	_validate_line_caches();

	if (_set_selection_to(current_frame, p_to)) {
		selection.active = (selection.from_frame != nullptr) && (selection.to_frame != nullptr);
		emit_signal("selection_active", selection.active);
	}
	else {
		selection.to_frame = nullptr;
		selection.to_line = 0;
		selection.to_char = 0;
	}
}

int SelectableRichTextLabel::get_selection_to() const {
	if (!selection.active || !selection.enabled) {
		return -1;
	}

	return selection.to_frame->lines[selection.to_line].char_offset + selection.to_char - 1;
}

void SelectableRichTextLabel::set_text(const String &p_bbcode) {
	if (text == p_bbcode) {
		return;
	}
	text = p_bbcode;
	_apply_translation();
}

void SelectableRichTextLabel::_apply_translation() {
	String xl_text = atr(text);
	if (use_bbcode) {
		parse_bbcode(xl_text);
	} else { // raw text
		clear();
		add_text(xl_text);
	}
}

String SelectableRichTextLabel::get_text() const {
	return text;
}

void SelectableRichTextLabel::set_use_bbcode(bool p_enable) {
	if (use_bbcode == p_enable) {
		return;
	}
	use_bbcode = p_enable;
	notify_property_list_changed();

	_apply_translation();
}

bool SelectableRichTextLabel::is_using_bbcode() const {
	return use_bbcode;
}

String SelectableRichTextLabel::get_parsed_text() const {
	String txt = "";
	Item *it = main;
	while (it) {
		if (it->type == ITEM_DROPCAP) {
			ItemDropcap *dc = static_cast<ItemDropcap *>(it);
			txt += dc->text;
		} else if (it->type == ITEM_TEXT) {
			ItemText *t = static_cast<ItemText *>(it);
			txt += t->text;
		} else if (it->type == ITEM_NEWLINE) {
			txt += "\n";
		} else if (it->type == ITEM_IMAGE) {
			txt += " ";
		} else if (it->type == ITEM_INDENT || it->type == ITEM_LIST) {
			txt += "\t";
		}
		it = _get_next_item(it, true);
	}
	return txt;
}

void SelectableRichTextLabel::set_text_direction(Control::TextDirection p_text_direction) {
	ERR_FAIL_COND((int)p_text_direction < -1 || (int)p_text_direction > 3);
	_stop_thread();

	if (text_direction != p_text_direction) {
		text_direction = p_text_direction;
		main->first_invalid_line.store(0); //invalidate ALL
		_validate_line_caches();
		queue_redraw();
	}
}

void SelectableRichTextLabel::set_structured_text_bidi_override(TextServer::StructuredTextParser p_parser) {
	if (st_parser != p_parser) {
		_stop_thread();

		st_parser = p_parser;
		main->first_invalid_line.store(0); //invalidate ALL
		_validate_line_caches();
		queue_redraw();
	}
}

TextServer::StructuredTextParser SelectableRichTextLabel::get_structured_text_bidi_override() const {
	return st_parser;
}

void SelectableRichTextLabel::set_structured_text_bidi_override_options(Array p_args) {
	if (st_args != p_args) {
		_stop_thread();

		st_args = p_args;
		main->first_invalid_line.store(0); //invalidate ALL
		_validate_line_caches();
		queue_redraw();
	}
}

Array SelectableRichTextLabel::get_structured_text_bidi_override_options() const {
	return st_args;
}

Control::TextDirection SelectableRichTextLabel::get_text_direction() const {
	return text_direction;
}

void SelectableRichTextLabel::set_language(const String &p_language) {
	if (language != p_language) {
		_stop_thread();

		language = p_language;
		main->first_invalid_line.store(0); //invalidate ALL
		_validate_line_caches();
		queue_redraw();
	}
}

String SelectableRichTextLabel::get_language() const {
	return language;
}

void SelectableRichTextLabel::set_autowrap_mode(TextServer::AutowrapMode p_mode) {
	if (autowrap_mode != p_mode) {
		_stop_thread();

		autowrap_mode = p_mode;
		main->first_invalid_line = 0; //invalidate ALL
		_validate_line_caches();
		queue_redraw();
	}
}

TextServer::AutowrapMode SelectableRichTextLabel::get_autowrap_mode() const {
	return autowrap_mode;
}

void SelectableRichTextLabel::set_visible_ratio(float p_ratio) {
	if (visible_ratio != p_ratio) {
		_stop_thread();

		if (p_ratio >= 1.0) {
			visible_characters = -1;
			visible_ratio = 1.0;
		} else if (p_ratio < 0.0) {
			visible_characters = 0;
			visible_ratio = 0.0;
		} else {
			visible_characters = get_total_character_count() * p_ratio;
			visible_ratio = p_ratio;
		}

		if (visible_chars_behavior == TextServer::VC_CHARS_BEFORE_SHAPING) {
			main->first_invalid_line.store(0); // Invalidate ALL.
			_validate_line_caches();
		}
		queue_redraw();
	}
}

float SelectableRichTextLabel::get_visible_ratio() const {
	return visible_ratio;
}

void SelectableRichTextLabel::set_effects(Array p_effects) {
	custom_effects = p_effects;
	if ((!text.is_empty()) && use_bbcode) {
		parse_bbcode(atr(text));
	}
}

Array SelectableRichTextLabel::get_effects() {
	return custom_effects;
}

void SelectableRichTextLabel::install_effect(const Variant effect) {
	Ref<RichTextEffect> rteffect;
	rteffect = effect;

	ERR_FAIL_COND_MSG(rteffect.is_null(), "Invalid RichTextEffect resource.");
	custom_effects.push_back(effect);
	if ((!text.is_empty()) && use_bbcode) {
		parse_bbcode(atr(text));
	}
}

int SelectableRichTextLabel::get_content_height() const {
	const_cast<SelectableRichTextLabel *>(this)->_validate_line_caches();

	int total_height = 0;
	int to_line = main->first_invalid_line.load();
	if (to_line) {
		MutexLock lock(main->lines[to_line - 1].text_buf->get_mutex());
		if (theme_cache.line_separation < 0) {
			// Do not apply to the last line to avoid cutting text.
			total_height = main->lines[to_line - 1].offset.y + main->lines[to_line - 1].text_buf->get_size().y + (main->lines[to_line - 1].text_buf->get_line_count() - 1) * theme_cache.line_separation;
		} else {
			total_height = main->lines[to_line - 1].offset.y + main->lines[to_line - 1].text_buf->get_size().y + main->lines[to_line - 1].text_buf->get_line_count() * theme_cache.line_separation;
		}
	}
	return total_height;
}

int SelectableRichTextLabel::get_content_width() const {
	const_cast<SelectableRichTextLabel *>(this)->_validate_line_caches();

	int total_width = 0;
	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		MutexLock lock(main->lines[i].text_buf->get_mutex());
		total_width = MAX(total_width, main->lines[i].offset.x + main->lines[i].text_buf->get_size().x);
	}
	return total_width;
}

#ifndef DISABLE_DEPRECATED
// People will be very angry, if their texts get erased, because of #39148. (3.x -> 4.0)
// Although some people may not used bbcode_text, so we only overwrite, if bbcode_text is not empty.
bool SelectableRichTextLabel::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "bbcode_text" && !((String)p_value).is_empty()) {
		set_text(p_value);
		return true;
	}
	return false;
}
#endif

void SelectableRichTextLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_parsed_text"), &SelectableRichTextLabel::get_parsed_text);
	ClassDB::bind_method(D_METHOD("add_text", "text"), &SelectableRichTextLabel::add_text);
	ClassDB::bind_method(D_METHOD("set_text", "text"), &SelectableRichTextLabel::set_text);
	ClassDB::bind_method(D_METHOD("add_image", "image", "width", "height", "color", "inline_align", "region"), &SelectableRichTextLabel::add_image, DEFVAL(0), DEFVAL(0), DEFVAL(Color(1.0, 1.0, 1.0)), DEFVAL(INLINE_ALIGNMENT_CENTER), DEFVAL(Rect2(0, 0, 0, 0)));
	ClassDB::bind_method(D_METHOD("newline"), &SelectableRichTextLabel::add_newline);
	ClassDB::bind_method(D_METHOD("remove_paragraph", "paragraph"), &SelectableRichTextLabel::remove_paragraph);
	ClassDB::bind_method(D_METHOD("push_font", "font", "font_size"), &SelectableRichTextLabel::push_font);
	ClassDB::bind_method(D_METHOD("push_font_size", "font_size"), &SelectableRichTextLabel::push_font_size);
	ClassDB::bind_method(D_METHOD("push_normal"), &SelectableRichTextLabel::push_normal);
	ClassDB::bind_method(D_METHOD("push_bold"), &SelectableRichTextLabel::push_bold);
	ClassDB::bind_method(D_METHOD("push_bold_italics"), &SelectableRichTextLabel::push_bold_italics);
	ClassDB::bind_method(D_METHOD("push_italics"), &SelectableRichTextLabel::push_italics);
	ClassDB::bind_method(D_METHOD("push_mono"), &SelectableRichTextLabel::push_mono);
	ClassDB::bind_method(D_METHOD("push_color", "color"), &SelectableRichTextLabel::push_color);
	ClassDB::bind_method(D_METHOD("push_outline_size", "outline_size"), &SelectableRichTextLabel::push_outline_size);
	ClassDB::bind_method(D_METHOD("push_outline_color", "color"), &SelectableRichTextLabel::push_outline_color);
	ClassDB::bind_method(D_METHOD("push_paragraph", "alignment", "base_direction", "language", "st_parser", "justification_flags", "tab_stops"), &SelectableRichTextLabel::push_paragraph, DEFVAL(TextServer::DIRECTION_AUTO), DEFVAL(""), DEFVAL(TextServer::STRUCTURED_TEXT_DEFAULT), DEFVAL(TextServer::JUSTIFICATION_WORD_BOUND | TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_SKIP_LAST_LINE | TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE), DEFVAL(PackedFloat32Array()));
	ClassDB::bind_method(D_METHOD("push_indent", "level"), &SelectableRichTextLabel::push_indent);
	ClassDB::bind_method(D_METHOD("push_list", "level", "type", "capitalize", "bullet"), &SelectableRichTextLabel::push_list, DEFVAL(String::utf8("•")));
	ClassDB::bind_method(D_METHOD("push_meta", "data"), &SelectableRichTextLabel::push_meta);
	ClassDB::bind_method(D_METHOD("push_hint", "description"), &SelectableRichTextLabel::push_hint);
	ClassDB::bind_method(D_METHOD("push_underline"), &SelectableRichTextLabel::push_underline);
	ClassDB::bind_method(D_METHOD("push_strikethrough"), &SelectableRichTextLabel::push_strikethrough);
	ClassDB::bind_method(D_METHOD("push_table", "columns", "inline_align", "align_to_row"), &SelectableRichTextLabel::push_table, DEFVAL(INLINE_ALIGNMENT_TOP), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("push_dropcap", "string", "font", "size", "dropcap_margins", "color", "outline_size", "outline_color"), &SelectableRichTextLabel::push_dropcap, DEFVAL(Rect2()), DEFVAL(Color(1, 1, 1)), DEFVAL(0), DEFVAL(Color(0, 0, 0, 0)));
	ClassDB::bind_method(D_METHOD("set_table_column_expand", "column", "expand", "ratio"), &SelectableRichTextLabel::set_table_column_expand);
	ClassDB::bind_method(D_METHOD("set_cell_row_background_color", "odd_row_bg", "even_row_bg"), &SelectableRichTextLabel::set_cell_row_background_color);
	ClassDB::bind_method(D_METHOD("set_cell_border_color", "color"), &SelectableRichTextLabel::set_cell_border_color);
	ClassDB::bind_method(D_METHOD("set_cell_size_override", "min_size", "max_size"), &SelectableRichTextLabel::set_cell_size_override);
	ClassDB::bind_method(D_METHOD("set_cell_padding", "padding"), &SelectableRichTextLabel::set_cell_padding);
	ClassDB::bind_method(D_METHOD("push_cell"), &SelectableRichTextLabel::push_cell);
	ClassDB::bind_method(D_METHOD("push_fgcolor", "fgcolor"), &SelectableRichTextLabel::push_fgcolor);
	ClassDB::bind_method(D_METHOD("push_bgcolor", "bgcolor"), &SelectableRichTextLabel::push_bgcolor);
	ClassDB::bind_method(D_METHOD("push_customfx", "effect", "env"), &SelectableRichTextLabel::push_customfx);
	ClassDB::bind_method(D_METHOD("pop"), &SelectableRichTextLabel::pop);

	ClassDB::bind_method(D_METHOD("clear"), &SelectableRichTextLabel::clear);

	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override", "parser"), &SelectableRichTextLabel::set_structured_text_bidi_override);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override"), &SelectableRichTextLabel::get_structured_text_bidi_override);
	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override_options", "args"), &SelectableRichTextLabel::set_structured_text_bidi_override_options);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override_options"), &SelectableRichTextLabel::get_structured_text_bidi_override_options);
	ClassDB::bind_method(D_METHOD("set_text_direction", "direction"), &SelectableRichTextLabel::set_text_direction);
	ClassDB::bind_method(D_METHOD("get_text_direction"), &SelectableRichTextLabel::get_text_direction);
	ClassDB::bind_method(D_METHOD("set_language", "language"), &SelectableRichTextLabel::set_language);
	ClassDB::bind_method(D_METHOD("get_language"), &SelectableRichTextLabel::get_language);

	ClassDB::bind_method(D_METHOD("set_autowrap_mode", "autowrap_mode"), &SelectableRichTextLabel::set_autowrap_mode);
	ClassDB::bind_method(D_METHOD("get_autowrap_mode"), &SelectableRichTextLabel::get_autowrap_mode);

	ClassDB::bind_method(D_METHOD("set_meta_underline", "enable"), &SelectableRichTextLabel::set_meta_underline);
	ClassDB::bind_method(D_METHOD("is_meta_underlined"), &SelectableRichTextLabel::is_meta_underlined);

	ClassDB::bind_method(D_METHOD("set_hint_underline", "enable"), &SelectableRichTextLabel::set_hint_underline);
	ClassDB::bind_method(D_METHOD("is_hint_underlined"), &SelectableRichTextLabel::is_hint_underlined);

	ClassDB::bind_method(D_METHOD("set_scroll_active", "active"), &SelectableRichTextLabel::set_scroll_active);
	ClassDB::bind_method(D_METHOD("is_scroll_active"), &SelectableRichTextLabel::is_scroll_active);

	ClassDB::bind_method(D_METHOD("set_scroll_follow", "follow"), &SelectableRichTextLabel::set_scroll_follow);
	ClassDB::bind_method(D_METHOD("is_scroll_following"), &SelectableRichTextLabel::is_scroll_following);

	ClassDB::bind_method(D_METHOD("get_v_scroll_bar"), &SelectableRichTextLabel::get_v_scroll_bar);

	ClassDB::bind_method(D_METHOD("scroll_to_line", "line"), &SelectableRichTextLabel::scroll_to_line);
	ClassDB::bind_method(D_METHOD("scroll_to_paragraph", "paragraph"), &SelectableRichTextLabel::scroll_to_paragraph);
	ClassDB::bind_method(D_METHOD("scroll_to_selection"), &SelectableRichTextLabel::scroll_to_selection);
	ClassDB::bind_method(D_METHOD("scroll_to_selection_centered"), &SelectableRichTextLabel::scroll_to_selection_centered);

	ClassDB::bind_method(D_METHOD("set_tab_size", "spaces"), &SelectableRichTextLabel::set_tab_size);
	ClassDB::bind_method(D_METHOD("get_tab_size"), &SelectableRichTextLabel::get_tab_size);

	ClassDB::bind_method(D_METHOD("set_fit_content", "enabled"), &SelectableRichTextLabel::set_fit_content);
	ClassDB::bind_method(D_METHOD("is_fit_content_enabled"), &SelectableRichTextLabel::is_fit_content_enabled);

	ClassDB::bind_method(D_METHOD("set_selection_enabled", "enabled"), &SelectableRichTextLabel::set_selection_enabled);
	ClassDB::bind_method(D_METHOD("is_selection_enabled"), &SelectableRichTextLabel::is_selection_enabled);

	ClassDB::bind_method(D_METHOD("set_context_menu_enabled", "enabled"), &SelectableRichTextLabel::set_context_menu_enabled);
	ClassDB::bind_method(D_METHOD("is_context_menu_enabled"), &SelectableRichTextLabel::is_context_menu_enabled);

	ClassDB::bind_method(D_METHOD("set_shortcut_keys_enabled", "enabled"), &SelectableRichTextLabel::set_shortcut_keys_enabled);
	ClassDB::bind_method(D_METHOD("is_shortcut_keys_enabled"), &SelectableRichTextLabel::is_shortcut_keys_enabled);

	ClassDB::bind_method(D_METHOD("set_deselect_on_focus_loss_enabled", "enable"), &SelectableRichTextLabel::set_deselect_on_focus_loss_enabled);
	ClassDB::bind_method(D_METHOD("is_deselect_on_focus_loss_enabled"), &SelectableRichTextLabel::is_deselect_on_focus_loss_enabled);

	ClassDB::bind_method(D_METHOD("set_selection_from", "from"), &SelectableRichTextLabel::set_selection_from);
	ClassDB::bind_method(D_METHOD("get_selection_from"), &SelectableRichTextLabel::get_selection_from);

	ClassDB::bind_method(D_METHOD("set_selection_to", "to"), &SelectableRichTextLabel::set_selection_to);
	ClassDB::bind_method(D_METHOD("get_selection_to"), &SelectableRichTextLabel::get_selection_to);

	ClassDB::bind_method(D_METHOD("get_before_selected_text"), &SelectableRichTextLabel::get_before_selected_text);
	ClassDB::bind_method(D_METHOD("get_selected_text"), &SelectableRichTextLabel::get_selected_text);
	ClassDB::bind_method(D_METHOD("get_after_selected_text"), &SelectableRichTextLabel::get_after_selected_text);

	ClassDB::bind_method(D_METHOD("select_all"), &SelectableRichTextLabel::select_all);
	ClassDB::bind_method(D_METHOD("deselect"), &SelectableRichTextLabel::deselect);

	ClassDB::bind_method(D_METHOD("parse_bbcode", "bbcode"), &SelectableRichTextLabel::parse_bbcode);
	ClassDB::bind_method(D_METHOD("append_text", "bbcode"), &SelectableRichTextLabel::append_text);

	ClassDB::bind_method(D_METHOD("get_text"), &SelectableRichTextLabel::get_text);

	ClassDB::bind_method(D_METHOD("is_ready"), &SelectableRichTextLabel::is_ready);

	ClassDB::bind_method(D_METHOD("set_threaded", "threaded"), &SelectableRichTextLabel::set_threaded);
	ClassDB::bind_method(D_METHOD("is_threaded"), &SelectableRichTextLabel::is_threaded);

	ClassDB::bind_method(D_METHOD("set_progress_bar_delay", "delay_ms"), &SelectableRichTextLabel::set_progress_bar_delay);
	ClassDB::bind_method(D_METHOD("get_progress_bar_delay"), &SelectableRichTextLabel::get_progress_bar_delay);

	ClassDB::bind_method(D_METHOD("set_visible_characters", "amount"), &SelectableRichTextLabel::set_visible_characters);
	ClassDB::bind_method(D_METHOD("get_visible_characters"), &SelectableRichTextLabel::get_visible_characters);

	ClassDB::bind_method(D_METHOD("get_visible_characters_behavior"), &SelectableRichTextLabel::get_visible_characters_behavior);
	ClassDB::bind_method(D_METHOD("set_visible_characters_behavior", "behavior"), &SelectableRichTextLabel::set_visible_characters_behavior);

	ClassDB::bind_method(D_METHOD("set_visible_ratio", "ratio"), &SelectableRichTextLabel::set_visible_ratio);
	ClassDB::bind_method(D_METHOD("get_visible_ratio"), &SelectableRichTextLabel::get_visible_ratio);

	ClassDB::bind_method(D_METHOD("get_character_line", "character"), &SelectableRichTextLabel::get_character_line);
	ClassDB::bind_method(D_METHOD("get_character_paragraph", "character"), &SelectableRichTextLabel::get_character_paragraph);
	ClassDB::bind_method(D_METHOD("get_total_character_count"), &SelectableRichTextLabel::get_total_character_count);

	ClassDB::bind_method(D_METHOD("set_use_bbcode", "enable"), &SelectableRichTextLabel::set_use_bbcode);
	ClassDB::bind_method(D_METHOD("is_using_bbcode"), &SelectableRichTextLabel::is_using_bbcode);

	ClassDB::bind_method(D_METHOD("get_line_count"), &SelectableRichTextLabel::get_line_count);
	ClassDB::bind_method(D_METHOD("get_visible_line_count"), &SelectableRichTextLabel::get_visible_line_count);

	ClassDB::bind_method(D_METHOD("get_paragraph_count"), &SelectableRichTextLabel::get_paragraph_count);
	ClassDB::bind_method(D_METHOD("get_visible_paragraph_count"), &SelectableRichTextLabel::get_visible_paragraph_count);

	ClassDB::bind_method(D_METHOD("get_content_height"), &SelectableRichTextLabel::get_content_height);
	ClassDB::bind_method(D_METHOD("get_content_width"), &SelectableRichTextLabel::get_content_width);

	ClassDB::bind_method(D_METHOD("get_line_offset", "line"), &SelectableRichTextLabel::get_line_offset);
	ClassDB::bind_method(D_METHOD("get_paragraph_offset", "paragraph"), &SelectableRichTextLabel::get_paragraph_offset);

	ClassDB::bind_method(D_METHOD("parse_expressions_for_values", "expressions"), &SelectableRichTextLabel::parse_expressions_for_values);

	ClassDB::bind_method(D_METHOD("set_effects", "effects"), &SelectableRichTextLabel::set_effects);
	ClassDB::bind_method(D_METHOD("get_effects"), &SelectableRichTextLabel::get_effects);
	ClassDB::bind_method(D_METHOD("install_effect", "effect"), &SelectableRichTextLabel::install_effect);

	ClassDB::bind_method(D_METHOD("get_menu"), &SelectableRichTextLabel::get_menu);
	ClassDB::bind_method(D_METHOD("is_menu_visible"), &SelectableRichTextLabel::is_menu_visible);
	ClassDB::bind_method(D_METHOD("menu_option", "option"), &SelectableRichTextLabel::menu_option);

	ClassDB::bind_method(D_METHOD("_thread_end"), &SelectableRichTextLabel::_thread_end);

	// Note: set "bbcode_enabled" first, to avoid unnecessary "text" resets.
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bbcode_enabled"), "set_use_bbcode", "is_using_bbcode");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fit_content"), "set_fit_content", "is_fit_content_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_active"), "set_scroll_active", "is_scroll_active");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_following"), "set_scroll_follow", "is_scroll_following");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "autowrap_mode", PROPERTY_HINT_ENUM, "Off,Arbitrary,Word,Word (Smart)"), "set_autowrap_mode", "get_autowrap_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tab_size", PROPERTY_HINT_RANGE, "0,24,1"), "set_tab_size", "get_tab_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "context_menu_enabled"), "set_context_menu_enabled", "is_context_menu_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shortcut_keys_enabled"), "set_shortcut_keys_enabled", "is_shortcut_keys_enabled");

	ADD_GROUP("Markup", "");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "custom_effects", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("RichTextEffect"), (PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SCRIPT_VARIABLE)), "set_effects", "get_effects");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "meta_underlined"), "set_meta_underline", "is_meta_underlined");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hint_underlined"), "set_hint_underline", "is_hint_underlined");

	ADD_GROUP("Threading", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "threaded"), "set_threaded", "is_threaded");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "progress_bar_delay", PROPERTY_HINT_NONE, "suffix:ms"), "set_progress_bar_delay", "get_progress_bar_delay");

	ADD_GROUP("Text Selection", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selection_enabled"), "set_selection_enabled", "is_selection_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "deselect_on_focus_loss_enabled"), "set_deselect_on_focus_loss_enabled", "is_deselect_on_focus_loss_enabled");

	ADD_GROUP("Displayed Text", "");
	// Note: "visible_characters" and "visible_ratio" should be set after "text" to be correctly applied.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "visible_characters", PROPERTY_HINT_RANGE, "-1,128000,1"), "set_visible_characters", "get_visible_characters");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "visible_characters_behavior", PROPERTY_HINT_ENUM, "Characters Before Shaping,Characters After Shaping,Glyphs (Layout Direction),Glyphs (Left-to-Right),Glyphs (Right-to-Left)"), "set_visible_characters_behavior", "get_visible_characters_behavior");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "visible_ratio", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_visible_ratio", "get_visible_ratio");

	ADD_GROUP("BiDi", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "text_direction", PROPERTY_HINT_ENUM, "Auto,Left-to-Right,Right-to-Left,Inherited"), "set_text_direction", "get_text_direction");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language", PROPERTY_HINT_LOCALE_ID, ""), "set_language", "get_language");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "structured_text_bidi_override", PROPERTY_HINT_ENUM, "Default,URI,File,Email,List,None,Custom"), "set_structured_text_bidi_override", "get_structured_text_bidi_override");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "structured_text_bidi_override_options"), "set_structured_text_bidi_override_options", "get_structured_text_bidi_override_options");

	ADD_SIGNAL(MethodInfo("meta_clicked", PropertyInfo(Variant::NIL, "meta", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("meta_hover_started", PropertyInfo(Variant::NIL, "meta", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("meta_hover_ended", PropertyInfo(Variant::NIL, "meta", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));

	ADD_SIGNAL(MethodInfo("finished"));
	ADD_SIGNAL(MethodInfo("selection_active", PropertyInfo(Variant::BOOL, "valid")));

	BIND_ENUM_CONSTANT(LIST_NUMBERS);
	BIND_ENUM_CONSTANT(LIST_LETTERS);
	BIND_ENUM_CONSTANT(LIST_ROMAN);
	BIND_ENUM_CONSTANT(LIST_DOTS);

	BIND_ENUM_CONSTANT(MENU_COPY);
	BIND_ENUM_CONSTANT(MENU_SELECT_ALL);
	BIND_ENUM_CONSTANT(MENU_MAX);
}

TextServer::VisibleCharactersBehavior SelectableRichTextLabel::get_visible_characters_behavior() const {
	return visible_chars_behavior;
}

void SelectableRichTextLabel::set_visible_characters_behavior(TextServer::VisibleCharactersBehavior p_behavior) {
	if (visible_chars_behavior != p_behavior) {
		_stop_thread();

		visible_chars_behavior = p_behavior;
		main->first_invalid_line.store(0); //invalidate ALL
		_validate_line_caches();
		queue_redraw();
	}
}

void SelectableRichTextLabel::set_visible_characters(int p_visible) {
	if (visible_characters != p_visible) {
		_stop_thread();

		visible_characters = p_visible;
		if (p_visible == -1) {
			visible_ratio = 1;
		} else {
			int total_char_count = get_total_character_count();
			if (total_char_count > 0) {
				visible_ratio = (float)p_visible / (float)total_char_count;
			}
		}
		if (visible_chars_behavior == TextServer::VC_CHARS_BEFORE_SHAPING) {
			main->first_invalid_line.store(0); //invalidate ALL
			_validate_line_caches();
		}
		queue_redraw();
	}
}

int SelectableRichTextLabel::get_visible_characters() const {
	return visible_characters;
}

int SelectableRichTextLabel::get_character_line(int p_char) {
	_validate_line_caches();

	int line_count = 0;
	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		MutexLock lock(main->lines[i].text_buf->get_mutex());
		int char_offset = main->lines[i].char_offset;
		int char_count = main->lines[i].char_count;
		if (char_offset <= p_char && p_char < char_offset + char_count) {
			for (int j = 0; j < main->lines[i].text_buf->get_line_count(); j++) {
				Vector2i range = main->lines[i].text_buf->get_line_range(j);
				if (char_offset + range.x <= p_char && p_char <= char_offset + range.y) {
					return line_count;
				}
				line_count++;
			}
		} else {
			line_count += main->lines[i].text_buf->get_line_count();
		}
	}
	return -1;
}

int SelectableRichTextLabel::get_character_paragraph(int p_char) {
	_validate_line_caches();

	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		int char_offset = main->lines[i].char_offset;
		if (char_offset <= p_char && p_char < char_offset + main->lines[i].char_count) {
			return i;
		}
	}
	return -1;
}

int SelectableRichTextLabel::get_total_character_count() const {
	// Note: Do not use line buffer "char_count", it includes only visible characters.
	int tc = 0;
	Item *it = main;
	while (it) {
		if (it->type == ITEM_TEXT) {
			ItemText *t = static_cast<ItemText *>(it);
			tc += t->text.length();
		} else if (it->type == ITEM_NEWLINE) {
			tc++;
		} else if (it->type == ITEM_IMAGE) {
			tc++;
		}
		it = _get_next_item(it, true);
	}
	return tc;
}

int SelectableRichTextLabel::get_total_glyph_count() const {
	const_cast<SelectableRichTextLabel *>(this)->_validate_line_caches();

	int tg = 0;
	Item *it = main;
	while (it) {
		if (it->type == ITEM_FRAME) {
			ItemFrame *f = static_cast<ItemFrame *>(it);
			for (int i = 0; i < (int)f->lines.size(); i++) {
				MutexLock lock(f->lines[i].text_buf->get_mutex());
				tg += TS->shaped_text_get_glyph_count(f->lines[i].text_buf->get_rid());
			}
		}
		it = _get_next_item(it, true);
	}

	return tg;
}

Size2 SelectableRichTextLabel::get_minimum_size() const {
	Size2 sb_min_size = theme_cache.normal_style->get_minimum_size();
	Size2 min_size;

	if (fit_content) {
		min_size.x = get_content_width();
		min_size.y = get_content_height();
	}

	return sb_min_size +
			((autowrap_mode != TextServer::AUTOWRAP_OFF) ? Size2(1, min_size.height) : min_size);
}

// Context menu.
void SelectableRichTextLabel::_generate_context_menu() {
	menu = memnew(PopupMenu);
	add_child(menu, false, INTERNAL_MODE_FRONT);
	menu->connect("id_pressed", callable_mp(this, &SelectableRichTextLabel::menu_option));

	menu->add_item(RTR("Copy"), MENU_COPY);
	menu->add_item(RTR("Select All"), MENU_SELECT_ALL);
}

void SelectableRichTextLabel::_update_context_menu() {
	if (!menu) {
		_generate_context_menu();
	}

	int idx = -1;

#define MENU_ITEM_ACTION_DISABLED(m_menu, m_id, m_action, m_disabled)                                                  \
	idx = m_menu->get_item_index(m_id);                                                                                \
	if (idx >= 0) {                                                                                                    \
		m_menu->set_item_accelerator(idx, shortcut_keys_enabled ? _get_menu_action_accelerator(m_action) : Key::NONE); \
		m_menu->set_item_disabled(idx, m_disabled);                                                                    \
	}

	MENU_ITEM_ACTION_DISABLED(menu, MENU_COPY, "ui_copy", !selection.enabled)
	MENU_ITEM_ACTION_DISABLED(menu, MENU_SELECT_ALL, "ui_text_select_all", !selection.enabled)

#undef MENU_ITEM_ACTION_DISABLED
}

Key SelectableRichTextLabel::_get_menu_action_accelerator(const String &p_action) {
	const List<Ref<InputEvent>> *events = InputMap::get_singleton()->action_get_events(p_action);
	if (!events) {
		return Key::NONE;
	}

	// Use first event in the list for the accelerator.
	const List<Ref<InputEvent>>::Element *first_event = events->front();
	if (!first_event) {
		return Key::NONE;
	}

	const Ref<InputEventKey> event = first_event->get();
	if (event.is_null()) {
		return Key::NONE;
	}

	// Use physical keycode if non-zero
	if (event->get_physical_keycode() != Key::NONE) {
		return event->get_physical_keycode_with_modifiers();
	} else {
		return event->get_keycode_with_modifiers();
	}
}

void SelectableRichTextLabel::menu_option(int p_option) {
	switch (p_option) {
		case MENU_COPY: {
			selection_copy();
		} break;
		case MENU_SELECT_ALL: {
			select_all();
		} break;
	}
}

void SelectableRichTextLabel::_draw_fbg_boxes(RID p_ci, RID p_rid, Vector2 line_off, Item *it_from, Item *it_to, int start, int end, int fbg_flag) {
	Vector2i fbg_index = Vector2i(end, start);
	Color last_color = Color(0, 0, 0, 0);
	bool draw_box = false;
	int hpad = get_theme_constant(SNAME("text_highlight_h_padding"));
	int vpad = get_theme_constant(SNAME("text_highlight_v_padding"));
	// Draw a box based on color tags associated with glyphs
	for (int i = start; i < end; i++) {
		Item *it = _get_item_at_pos(it_from, it_to, i);
		Color color;

		if (fbg_flag == 0) {
			color = _find_bgcolor(it);
		} else {
			color = _find_fgcolor(it);
		}

		bool change_to_color = ((color.a > 0) && ((last_color.a - 0.0) < 0.01));
		bool change_from_color = (((color.a - 0.0) < 0.01) && (last_color.a > 0.0));
		bool change_color = (((color.a > 0) == (last_color.a > 0)) && (color != last_color));

		if (change_to_color) {
			fbg_index.x = MIN(i, fbg_index.x);
			fbg_index.y = MAX(i, fbg_index.y);
		}

		if (change_from_color || change_color) {
			fbg_index.x = MIN(i, fbg_index.x);
			fbg_index.y = MAX(i, fbg_index.y);
			draw_box = true;
		}

		if (draw_box) {
			Vector<Vector2> sel = TS->shaped_text_get_selection(p_rid, fbg_index.x, fbg_index.y);
			for (int j = 0; j < sel.size(); j++) {
				Vector2 rect_off = line_off + Vector2(sel[j].x - hpad, -TS->shaped_text_get_ascent(p_rid) - vpad);
				Vector2 rect_size = Vector2(sel[j].y - sel[j].x + 2 * hpad, TS->shaped_text_get_size(p_rid).y + 2 * vpad);
				RenderingServer::get_singleton()->canvas_item_add_rect(p_ci, Rect2(rect_off, rect_size), last_color);
			}
			fbg_index = Vector2i(end, start);
			draw_box = false;
		}

		if (change_color) {
			fbg_index.x = MIN(i, fbg_index.x);
			fbg_index.y = MAX(i, fbg_index.y);
		}

		last_color = color;
	}

	if (last_color.a > 0) {
		Vector<Vector2> sel = TS->shaped_text_get_selection(p_rid, fbg_index.x, end);
		for (int i = 0; i < sel.size(); i++) {
			Vector2 rect_off = line_off + Vector2(sel[i].x - hpad, -TS->shaped_text_get_ascent(p_rid) - vpad);
			Vector2 rect_size = Vector2(sel[i].y - sel[i].x + 2 * hpad, TS->shaped_text_get_size(p_rid).y + 2 * vpad);
			RenderingServer::get_singleton()->canvas_item_add_rect(p_ci, Rect2(rect_off, rect_size), last_color);
		}
	}
}

Ref<RichTextEffect> SelectableRichTextLabel::_get_custom_effect_by_code(String p_bbcode_identifier) {
	for (int i = 0; i < custom_effects.size(); i++) {
		Ref<RichTextEffect> effect = custom_effects[i];
		if (!effect.is_valid()) {
			continue;
		}

		if (effect->get_bbcode() == p_bbcode_identifier) {
			return effect;
		}
	}

	return Ref<RichTextEffect>();
}

Dictionary SelectableRichTextLabel::parse_expressions_for_values(Vector<String> p_expressions) {
	Dictionary d;
	for (int i = 0; i < p_expressions.size(); i++) {
		String expression = p_expressions[i];

		Array a;
		Vector<String> parts = expression.split("=", true);
		String key = parts[0];
		if (parts.size() != 2) {
			return d;
		}

		Vector<String> values = parts[1].split(",", false);

#ifdef MODULE_REGEX_ENABLED
		RegEx color = RegEx();
		color.compile("^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$");
		RegEx nodepath = RegEx();
		nodepath.compile("^\\$");
		RegEx boolean = RegEx();
		boolean.compile("^(true|false)$");
		RegEx decimal = RegEx();
		decimal.compile("^-?^.?\\d+(\\.\\d+?)?$");
		RegEx numerical = RegEx();
		numerical.compile("^\\d+$");

		for (int j = 0; j < values.size(); j++) {
			if (!color.search(values[j]).is_null()) {
				a.append(Color::html(values[j]));
			} else if (!nodepath.search(values[j]).is_null()) {
				if (values[j].begins_with("$")) {
					String v = values[j].substr(1, values[j].length());
					a.append(NodePath(v));
				}
			} else if (!boolean.search(values[j]).is_null()) {
				if (values[j] == "true") {
					a.append(true);
				} else if (values[j] == "false") {
					a.append(false);
				}
			} else if (!decimal.search(values[j]).is_null()) {
				a.append(values[j].to_float());
			} else if (!numerical.search(values[j]).is_null()) {
				a.append(values[j].to_int());
			} else {
				a.append(values[j]);
			}
		}
#endif

		if (values.size() > 1) {
			d[key] = a;
		} else if (values.size() == 1) {
			d[key] = a[0];
		}
	}
	return d;
}

SelectableRichTextLabel::SelectableRichTextLabel(const String &p_text) {
	main = memnew(ItemFrame);
	main->index = 0;
	current = main;
	main->lines.resize(1);
	main->lines[0].from = main;
	main->first_invalid_line.store(0);
	main->first_resized_line.store(0);
	main->first_invalid_font_line.store(0);
	current_frame = main;

	vscroll = memnew(VScrollBar);
	add_child(vscroll, false, INTERNAL_MODE_FRONT);
	vscroll->set_drag_node(String(".."));
	vscroll->set_step(1);
	vscroll->set_anchor_and_offset(SIDE_TOP, ANCHOR_BEGIN, 0);
	vscroll->set_anchor_and_offset(SIDE_BOTTOM, ANCHOR_END, 0);
	vscroll->set_anchor_and_offset(SIDE_RIGHT, ANCHOR_END, 0);
	vscroll->connect("value_changed", callable_mp(this, &SelectableRichTextLabel::_scroll_changed));
	vscroll->set_step(1);
	vscroll->hide();

	set_text(p_text);
	updating.store(false);
	validating.store(false);
	stop_thread.store(false);

	set_clip_contents(true);
}

SelectableRichTextLabel::~SelectableRichTextLabel() {
	_stop_thread();
	memdelete(main);
}
