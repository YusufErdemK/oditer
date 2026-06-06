#include <gtkmm.h>
#include <gdk/gdkkeysyms.h>
#include <cmath>
#include <vector>
#include <poppler.h>
#include <giomm/file.h>

class PDF
{
protected:
    Cairo::RefPtr<Cairo::ImageSurface> pdf_surface;

public:
    bool PDFloader(const std::string &path)
    {
        PopplerDocument *doc = poppler_document_new_from_file(("file://" + path).c_str(), nullptr, nullptr);
        if (!doc)
            return false;

        PopplerPage *page = poppler_document_get_page(doc, 0);
        double w, h;
        poppler_page_get_size(page, &w, &h);

        pdf_surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, (int)w, (int)h);
        auto cr = Cairo::Context::create(pdf_surface);
        poppler_page_render(page, cr->cobj());

        g_object_unref(page);
        g_object_unref(doc);
        return true;
    }
};

class NotebookArea : public Gtk::DrawingArea, public PDF
{
public:
    bool show_new_pen_dialog = false;
    bool beautify_enabled = false;

    enum class Tool
    {
        PAN,
        BRUSH
    };
    Tool current_tool = Tool::PAN;

    NotebookArea()
    {
        add_events(Gdk::BUTTON_PRESS_MASK |
                   Gdk::BUTTON_RELEASE_MASK |
                   Gdk::POINTER_MOTION_MASK |
                   Gdk::SCROLL_MASK);

        signal_button_press_event().connect(
            sigc::mem_fun(*this, &NotebookArea::on_button_press), false);

        signal_button_release_event().connect(
            sigc::mem_fun(*this, &NotebookArea::on_button_release), false);

        signal_motion_notify_event().connect(
            sigc::mem_fun(*this, &NotebookArea::on_mouse_move), false);

        signal_scroll_event().connect(
            sigc::mem_fun(*this, &NotebookArea::on_scroll), false);
    }

    void toggle_beautifier() { beautify_enabled = !beautify_enabled; }
    void toggle_pen_dialog()
    {
        show_new_pen_dialog = !show_new_pen_dialog;
        queue_draw();
    }

    void set_tool_brush()
    {
        current_tool = Tool::BRUSH;
        if (auto win = get_window())
            win->set_cursor(Gdk::Cursor::create(Gdk::PENCIL));
    }

    void set_tool_pan()
    {
        current_tool = Tool::PAN;
        if (auto win = get_window())
            win->set_cursor();
    }

    void clear_document()
    {
        strokes.clear();
        redo_strokes.clear();
        current_stroke.clear();
        queue_draw();
    }

    void undo()
    {
        if (!strokes.empty())
        {
            redo_strokes.push_back(strokes.back());
            strokes.pop_back();
            queue_draw();
        }
    }

    void redo()
    {
        if (!redo_strokes.empty())
        {
            strokes.push_back(redo_strokes.back());
            redo_strokes.pop_back();
            queue_draw();
        }
    }

protected:
    struct Point
    {
        double x, y;
    };

    std::vector<std::vector<Point>> strokes;
    std::vector<std::vector<Point>> redo_strokes;
    std::vector<Point> current_stroke;

    double offset_x = 0;
    double offset_y = 0;
    double zoom = 1.0;

    double last_x = 0;
    double last_y = 0;
    bool dragging = false;

    double r = 0.1, g = 0.1, b = 0.15;

    Point screen_to_world(double x, double y)
    {
        return {
            (x / zoom) + offset_x,
            (y / zoom) + offset_y};
    }

    bool on_scroll(GdkEventScroll *event)
    {
        double old_zoom = zoom;

        if (event->direction == GDK_SCROLL_UP)
            zoom *= 1.1;
        else if (event->direction == GDK_SCROLL_DOWN)
            zoom /= 1.1;

        zoom = std::clamp(zoom, 0.2, 5.0);

        double mx = event->x;
        double my = event->y;

        offset_x += (mx / old_zoom - mx / zoom);
        offset_y += (my / old_zoom - my / zoom);

        queue_draw();
        return true;
    }

    bool on_button_press(GdkEventButton *event)
    {
        if (event->button != 1)
            return true;

        dragging = true;
        last_x = event->x;
        last_y = event->y;

        if (current_tool == Tool::BRUSH)
        {
            current_stroke.clear();
            current_stroke.push_back(screen_to_world(event->x, event->y));
        }

        return true;
    }

    bool on_button_release(GdkEventButton *event)
    {
        if (event->button != 1)
            return true;

        dragging = false;

        if (current_tool == Tool::BRUSH && !current_stroke.empty())
        {
            strokes.push_back(current_stroke);
            current_stroke.clear();
            redo_strokes.clear();
        }

        queue_draw();
        return true;
    }

    bool on_mouse_move(GdkEventMotion *event)
    {
        if (!dragging)
            return true;

        if (current_tool == Tool::PAN)
        {
            double dx = event->x - last_x;
            double dy = event->y - last_y;

            offset_x -= dx / zoom;
            offset_y -= dy / zoom;

            last_x = event->x;
            last_y = event->y;

            queue_draw();
        }
        else if (current_tool == Tool::BRUSH)
        {
            current_stroke.push_back(screen_to_world(event->x, event->y));
            queue_draw();
        }

        return true;
    }

    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override
    {
        auto alloc = get_allocation();
        int w = alloc.get_width();
        int h = alloc.get_height();

        cr->set_source_rgb(0.965, 0.965, 0.975);
        cr->paint();

        cr->save();
        cr->scale(zoom, zoom);
        cr->translate(-offset_x, -offset_y);

        if (pdf_surface)
        {
            cr->set_source(pdf_surface, 0, 0);
            cr->paint();
        }

        const int grid = 28;
        cr->set_source_rgba(0.82, 0.84, 0.88, 0.75);
        cr->set_line_width(1.0);

        for (int x = -10000; x < 10000; x += grid)
        {
            cr->move_to(x, -10000);
            cr->line_to(x, 10000);
        }

        for (int y = -10000; y < 10000; y += grid)
        {
            cr->move_to(-10000, y);
            cr->line_to(10000, y);
        }

        cr->stroke();

        cr->set_line_width(2.5);
        cr->set_source_rgba(r, g, b, 0.9);
        cr->set_line_cap(Cairo::LINE_CAP_ROUND);
        cr->set_line_join(Cairo::LINE_JOIN_ROUND);

        auto draw = [&](const std::vector<Point> &s)
        {
            if (s.empty())
                return;
            cr->move_to(s[0].x, s[0].y);
            for (auto &p : s)
                cr->line_to(p.x, p.y);
            cr->stroke();
        };

        for (auto &s : strokes)
            draw(s);
        draw(current_stroke);

        cr->restore();

        if (show_new_pen_dialog)
        {
            show_new_pen_dialog = false;

            auto dialog = new Gtk::Window();
            dialog->set_title("Create Pen");
            dialog->set_default_size(270, 300);

            auto box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL, 12);
            dialog->add(*box);

            auto r_scale = Gtk::make_managed<Gtk::Scale>(Gtk::ORIENTATION_HORIZONTAL);
            auto g_scale = Gtk::make_managed<Gtk::Scale>(Gtk::ORIENTATION_HORIZONTAL);
            auto b_scale = Gtk::make_managed<Gtk::Scale>(Gtk::ORIENTATION_HORIZONTAL);

            r_scale->set_range(0, 1);
            g_scale->set_range(0, 1);
            b_scale->set_range(0, 1);

            r_scale->set_value(r);
            g_scale->set_value(g);
            b_scale->set_value(b);

            r_scale->signal_value_changed().connect([this, r_scale]()
                                                    { r = r_scale->get_value(); });
            g_scale->signal_value_changed().connect([this, g_scale]()
                                                    { g = g_scale->get_value(); });
            b_scale->signal_value_changed().connect([this, b_scale]()
                                                    { b = b_scale->get_value(); });

            box->pack_start(*r_scale);
            box->pack_start(*g_scale);
            box->pack_start(*b_scale);

            dialog->show_all();
        }

        return true;
    }
};

int main(int argc, char *argv[])
{
    auto app = Gtk::Application::create(argc, argv, "org.erdamn.oditer");

    Gtk::Window window;
    window.set_title("oditer");
    window.set_default_size(1200, 800);

    Glib::RefPtr<Gtk::AccelGroup> accel_group = Gtk::AccelGroup::create();
    window.add_accel_group(accel_group);

    Gtk::Box main_box(Gtk::ORIENTATION_VERTICAL);
    window.add(main_box);

    NotebookArea notebook;

    Gtk::MenuBar menu_bar;

    Gtk::Menu *file_menu = Gtk::manage(new Gtk::Menu());
    Gtk::Menu *edit_menu = Gtk::manage(new Gtk::Menu());
    Gtk::Menu *window_menu = Gtk::manage(new Gtk::Menu());
    Gtk::Menu *pen_menu = Gtk::manage(new Gtk::Menu());

    Gtk::MenuItem *new_item = Gtk::manage(new Gtk::MenuItem("New"));
    new_item->add_accelerator("activate", accel_group, GDK_KEY_n, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    file_menu->append(*new_item);

    new_item->signal_activate().connect([&]()
                                        {
    notebook.clear_document();
    notebook.queue_draw(); });

    Gtk::MenuItem *open_item = Gtk::manage(new Gtk::MenuItem("Open"));
    open_item->add_accelerator("activate", accel_group, GDK_KEY_o, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    file_menu->append(*open_item);

    open_item->signal_activate().connect([&window, &notebook]()
                                         {
        Gtk::FileChooserDialog dialog(window, "Please select a PDF file", Gtk::FILE_CHOOSER_ACTION_OPEN);
        dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("Open", Gtk::RESPONSE_OK);

        if (dialog.run() == Gtk::RESPONSE_OK) {
            if (notebook.PDFloader(dialog.get_filename())) {
                notebook.queue_draw();
            }
        } });

    Gtk::MenuItem *save_item = Gtk::manage(new Gtk::MenuItem("Save"));
    save_item->add_accelerator("activate", accel_group, GDK_KEY_S, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    file_menu->append(*save_item);

    save_item->signal_activate().connect([&window, &notebook]()
                                         {
        Gtk::FileChooserDialog dialog(window, "Please select a location to save", Gtk::FILE_CHOOSER_ACTION_SAVE);
        dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("Save", Gtk::RESPONSE_OK);

        if (dialog.run() == Gtk::RESPONSE_OK) {
        } });

    Gtk::MenuItem *quit_item = Gtk::manage(new Gtk::MenuItem("Quit"));
    quit_item->add_accelerator("activate", accel_group, GDK_KEY_F4, Gdk::MOD1_MASK, Gtk::ACCEL_VISIBLE);
    file_menu->append(*quit_item);

    quit_item->signal_activate().connect(sigc::ptr_fun(&Gtk::Main::quit));

    Gtk::MenuItem *undo_item = Gtk::manage(new Gtk::MenuItem("Undo"));
    undo_item->add_accelerator("activate", accel_group, GDK_KEY_z, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    undo_item->signal_activate().connect(sigc::mem_fun(notebook, &NotebookArea::undo));
    edit_menu->append(*undo_item);

    Gtk::MenuItem *redo_item = Gtk::manage(new Gtk::MenuItem("Redo"));
    redo_item->add_accelerator("activate", accel_group, GDK_KEY_y, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    redo_item->signal_activate().connect(sigc::mem_fun(notebook, &NotebookArea::redo));
    edit_menu->append(*redo_item);

    Gtk::MenuItem *toggle_beautify_item = Gtk::manage(new Gtk::CheckMenuItem("Beautifier"));
    toggle_beautify_item->add_accelerator("activate", accel_group, GDK_KEY_b, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    toggle_beautify_item->signal_activate().connect(sigc::mem_fun(notebook, &NotebookArea::toggle_beautifier));
    edit_menu->append(*toggle_beautify_item);

    Gtk::MenuItem *cut_item = Gtk::manage(new Gtk::MenuItem("Cut"));
    cut_item->add_accelerator("activate", accel_group, GDK_KEY_x, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    edit_menu->append(*cut_item);

    Gtk::MenuItem *copy_item = Gtk::manage(new Gtk::MenuItem("Copy"));
    copy_item->add_accelerator("activate", accel_group, GDK_KEY_c, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    edit_menu->append(*copy_item);

    Gtk::MenuItem *paste_item = Gtk::manage(new Gtk::MenuItem("Paste"));
    paste_item->add_accelerator("activate", accel_group, GDK_KEY_v, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    edit_menu->append(*paste_item);

    Gtk::MenuItem *maximize_item = Gtk::manage(new Gtk::MenuItem("Maximize"));
    maximize_item->signal_activate().connect([&window]()
                                             { window.maximize(); });
    window_menu->append(*maximize_item);

    Gtk::MenuItem *minimize_item = Gtk::manage(new Gtk::MenuItem("Minimize"));
    minimize_item->signal_activate().connect([&window]()
                                             { window.iconify(); });
    window_menu->append(*minimize_item);

    // window_menu->append(*Gtk::manage(new Gtk::MenuItem("Maximize")));
    // window_menu->append(*Gtk::manage(new Gtk::MenuItem("Minimize")));

    Gtk::MenuItem *create_pen_item = Gtk::manage(new Gtk::MenuItem("Create Pen"));
    pen_menu->append(*create_pen_item);
    pen_menu->append(*Gtk::manage(new Gtk::MenuItem("Import Pen")));

    create_pen_item->signal_activate().connect(sigc::mem_fun(notebook, &NotebookArea::toggle_pen_dialog));

    Gtk::MenuItem *file_item = Gtk::manage(new Gtk::MenuItem("File"));
    file_item->set_submenu(*file_menu);

    Gtk::MenuItem *edit_item = Gtk::manage(new Gtk::MenuItem("Edit"));
    edit_item->set_submenu(*edit_menu);

    Gtk::MenuItem *window_item = Gtk::manage(new Gtk::MenuItem("Window"));
    window_item->set_submenu(*window_menu);

    Gtk::MenuItem *pen_item = Gtk::manage(new Gtk::MenuItem("Pen"));
    pen_item->set_submenu(*pen_menu);

    Gtk::MenuItem *pens_sub_item = Gtk::manage(new Gtk::MenuItem("Pens"));

    Gtk::Menu *pens_nested_menu = Gtk::manage(new Gtk::Menu());

    Gtk::MenuItem *mouse_item = Gtk::manage(new Gtk::MenuItem("Mouse"));
    mouse_item->add_accelerator("activate", accel_group, GDK_KEY_1, (Gdk::ModifierType)0, Gtk::ACCEL_VISIBLE);
    pens_nested_menu->append(*mouse_item);
    mouse_item->signal_activate().connect(sigc::mem_fun(notebook, &NotebookArea::set_tool_pan));

    Gtk::MenuItem *brush_item = Gtk::manage(new Gtk::MenuItem("Brush"));
    brush_item->add_accelerator("activate", accel_group, GDK_KEY_2, (Gdk::ModifierType)0, Gtk::ACCEL_VISIBLE);
    pens_nested_menu->append(*brush_item);
    brush_item->signal_activate().connect(sigc::mem_fun(notebook, &NotebookArea::set_tool_brush));

    pens_nested_menu->append(*Gtk::manage(new Gtk::MenuItem("Marker")));

    pens_sub_item->set_submenu(*pens_nested_menu);

    pen_menu->append(*pens_sub_item);
    menu_bar.append(*file_item);
    menu_bar.append(*edit_item);
    menu_bar.append(*window_item);
    menu_bar.append(*pen_item);

    main_box.pack_start(menu_bar, Gtk::PACK_SHRINK);
    main_box.pack_start(notebook);

    window.show_all();
    return app->run(window);
}