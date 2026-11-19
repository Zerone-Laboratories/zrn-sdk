/**
 * zrnui - classes.h
 * Every class there is, for the registry of core.c. A class another source derives from is also declared by the header of its module.
 */

#pragma once

#include "zrnui/element.h"
#include "zrnui/internal/class.h"

/** One class object per ZClass, each defined in the source of its module. The registry (zrn_class_of in core.c) must list them in ZClass order. */
extern zrn_class_t zrn_class_element, zrn_class_panel, zrn_class_stack_panel, zrn_class_virtualizing_stack_panel,
	zrn_class_wrap_panel, zrn_class_grid, zrn_class_canvas, zrn_class_border, zrn_class_viewbox, zrn_class_text_block,
	zrn_class_image, zrn_class_popup, zrn_class_control, zrn_class_user_control, zrn_class_content_control,
	zrn_class_frame, zrn_class_page, zrn_class_pivot, zrn_class_tooltip, zrn_class_button_base, zrn_class_button, zrn_class_hyperlink_button, zrn_class_repeat_button,
	zrn_class_toggle_button, zrn_class_checkbox, zrn_class_radio_button, zrn_class_range_base, zrn_class_slider,
	zrn_class_progress_bar, zrn_class_scrollbar, zrn_class_thumb, zrn_class_separator, zrn_class_scroll_viewer,
	zrn_class_text_box, zrn_class_password_box, zrn_class_toggle_switch, zrn_class_progress_ring, zrn_class_items_control, zrn_class_selector, zrn_class_list_box,
	zrn_class_list_box_item, zrn_class_combo_box, zrn_class_combo_box_item, zrn_class_auto_suggest_box, zrn_class_selector_item,
	zrn_class_list_view_base, zrn_class_list_view, zrn_class_grid_view, zrn_class_list_view_item, zrn_class_grid_view_item,
	zrn_class_list_view_base_header_item, zrn_class_list_view_header_item, zrn_class_grid_view_header_item, zrn_class_flip_view,
	zrn_class_flip_view_item, zrn_class_items_stack_panel, zrn_class_items_wrap_grid, zrn_class_semantic_zoom,
	zrn_class_icon_element, zrn_class_symbol_icon, zrn_class_font_icon, zrn_class_path_icon, zrn_class_bitmap_icon, zrn_class_app_bar,
	zrn_class_command_bar, zrn_class_app_bar_button, zrn_class_app_bar_toggle_button, zrn_class_app_bar_separator, zrn_class_flyout_base,
	zrn_class_flyout, zrn_class_menu_flyout, zrn_class_menu_flyout_item_base, zrn_class_menu_flyout_item,
	zrn_class_toggle_menu_flyout_item, zrn_class_menu_flyout_separator, zrn_class_content_dialog, zrn_class_pivot_item, zrn_class_hub,
	zrn_class_hub_section, zrn_class_looping_selector, zrn_class_date_picker, zrn_class_time_picker, zrn_class_picker_flyout_base,
	zrn_class_date_picker_flyout, zrn_class_time_picker_flyout, zrn_class_rich_text_block;
