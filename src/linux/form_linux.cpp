// run_form on Linux: a GtkDialog with the fields in a grid (a label, the control, the hint) and OK / Cancel. A number
// out of its range is said in a box and the window comes again with what was typed.
#include <gtk/gtk.h>

#include "../form.h"

#include <cmath>
#include <cwchar>
#include <string>

#include "../ini.h"
#include "../lang.h"
#include "linux_view.h"

namespace ng {

namespace {

struct Sync {
	GtkWidget *number, *slider;
	int lo, hi;
	bool busy = false;
};

void slider_moved(GtkRange *r, gpointer data)
{
	Sync *s = static_cast<Sync *>(data);
	if (s->busy) return;
	s->busy = true;
	gtk_entry_set_text(GTK_ENTRY(s->number), std::to_string(std::lround(gtk_range_get_value(r))).c_str());
	s->busy = false;
}

void number_typed(GtkEditable *e, gpointer data)
{
	Sync *s = static_cast<Sync *>(data);
	if (s->busy) return;
	const std::wstring t = widen(gtk_entry_get_text(GTK_ENTRY(e)));
	wchar_t *end = nullptr;
	const long v = std::wcstol(t.c_str(), &end, 10);
	if (t.empty() || *end || v < s->lo || v > s->hi) return;
	s->busy = true;
	gtk_range_set_value(GTK_RANGE(s->slider), static_cast<double>(v));
	s->busy = false;
}

GtkWidget *label(const std::wstring &text, bool dim = false, bool bold = false)
{
	GtkWidget *l = gtk_label_new(nullptr);
	gchar *esc = g_markup_escape_text(narrow(text).c_str(), -1);
	const std::string m = bold ? std::string("<b>") + esc + "</b>" : std::string(esc);
	g_free(esc);
	gtk_label_set_markup(GTK_LABEL(l), m.c_str());
	gtk_label_set_xalign(GTK_LABEL(l), 0);
	if (dim) gtk_widget_set_opacity(l, 0.65);
	return l;
}

}  // namespace

bool run_form(NativeWindow, const std::wstring &title, std::vector<FormField> &fields)
{
	const std::wstring &ok = lang::t("form.ok"), &cancel = lang::t("form.cancel"), &bad_number = lang::t("form.bad_number");
	for (;;) {
		GtkWidget *d = gtk_dialog_new_with_buttons(narrow(title).c_str(), dialog_parent(),
		                                           static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
		                                           narrow(cancel).c_str(), GTK_RESPONSE_CANCEL, narrow(ok).c_str(), GTK_RESPONSE_OK,
		                                           nullptr);
		gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
		GtkWidget *grid = gtk_grid_new();
		gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
		gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
		gtk_container_set_border_width(GTK_CONTAINER(grid), 14);
		gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(d))), grid);

		std::vector<GtkWidget *> controls(fields.size(), nullptr);
		std::vector<Sync> syncs(fields.size());  // (kept while the window is open)
		for (size_t i = 0; i < fields.size(); ++i) {
			FormField &f = fields[i];
			const int row = static_cast<int>(i);
			GtkWidget *control = nullptr;
			switch (f.kind) {
			case FormField::Heading: gtk_grid_attach(GTK_GRID(grid), label(f.label, false, true), 0, row, 2, 1); break;
			case FormField::Number:
			case FormField::Text: {
				control = gtk_entry_new();
				gtk_entry_set_text(GTK_ENTRY(control), narrow(f.text).c_str());
				gtk_entry_set_width_chars(GTK_ENTRY(control), f.kind == FormField::Text ? 32 : 9);
				gtk_entry_set_activates_default(GTK_ENTRY(control), TRUE);
				controls[i] = control;
				break;
			}
			case FormField::Slider: {
				GtkWidget *t = gtk_entry_new();
				gtk_entry_set_text(GTK_ENTRY(t), narrow(f.text).c_str());
				gtk_entry_set_width_chars(GTK_ENTRY(t), 6);
				gtk_entry_set_activates_default(GTK_ENTRY(t), TRUE);
				GtkWidget *s = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, f.lo, f.hi > f.lo ? f.hi : f.lo + 1, 1);
				gtk_scale_set_draw_value(GTK_SCALE(s), FALSE);
				gtk_widget_set_size_request(s, 180, -1);
				gtk_range_set_value(GTK_RANGE(s), static_cast<double>(std::wcstol(f.text.c_str(), nullptr, 10)));
				syncs[i] = Sync{t, s, f.lo, f.hi};
				g_signal_connect(s, "value-changed", G_CALLBACK(slider_moved), &syncs[i]);
				g_signal_connect(t, "changed", G_CALLBACK(number_typed), &syncs[i]);
				control = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
				gtk_box_pack_start(GTK_BOX(control), t, FALSE, FALSE, 0);
				gtk_box_pack_start(GTK_BOX(control), s, TRUE, TRUE, 0);
				controls[i] = t;
				break;
			}
			case FormField::Choice: {
				control = gtk_combo_box_text_new();
				for (const auto &c : f.choices) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(control), narrow(c).c_str());
				if (f.choice >= 0 && f.choice < static_cast<int>(f.choices.size())) gtk_combo_box_set_active(GTK_COMBO_BOX(control), f.choice);
				controls[i] = control;
				break;
			}
			case FormField::Check: {
				control = gtk_check_button_new_with_label(narrow(f.label).c_str());
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(control), f.checked);
				controls[i] = control;
				break;
			}
			}
			if (f.kind != FormField::Heading) {
				if (f.kind != FormField::Check) {
					GtkWidget *l = label(f.label);
					gtk_label_set_xalign(GTK_LABEL(l), 1);
					gtk_grid_attach(GTK_GRID(grid), l, 0, row, 1, 1);
				}
				gtk_grid_attach(GTK_GRID(grid), control, 1, row, 1, 1);
			}
			if (!f.note.empty()) gtk_grid_attach(GTK_GRID(grid), label(f.note, true), 2, row, 1, 1);
		}
		gtk_widget_show_all(d);

		modal_begin();
		const gint r = gtk_dialog_run(GTK_DIALOG(d));
		modal_end();

		// what is in the controls now (kept when the window comes again)
		std::wstring bad;
		for (size_t i = 0; i < fields.size(); ++i) {
			FormField &f = fields[i];
			GtkWidget *c = controls[i];
			if (!c) continue;
			switch (f.kind) {
			case FormField::Number:
			case FormField::Text:
			case FormField::Slider: {
				f.text = widen(gtk_entry_get_text(GTK_ENTRY(c)));
				if (f.kind == FormField::Text || !bad.empty()) break;
				wchar_t *end = nullptr;
				const long v = std::wcstol(f.text.c_str(), &end, 10);
				if (f.text.empty() || *end || v < f.lo || v > f.hi)
					bad = bad_number + L"\n" + f.label + L": " + std::to_wstring(f.lo) + L" .. " + std::to_wstring(f.hi);
				break;
			}
			case FormField::Choice: f.choice = gtk_combo_box_get_active(GTK_COMBO_BOX(c)); break;
			case FormField::Check: f.checked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c)) != FALSE; break;
			case FormField::Heading: break;
			}
		}
		gtk_widget_destroy(d);
		if (r != GTK_RESPONSE_OK) return false;
		if (bad.empty()) return true;
		message_box(nullptr, bad, title, MB_ICONWARNING);
	}
}

}  // namespace ng
