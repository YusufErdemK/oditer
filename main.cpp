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

    void toggle_beautifier()
    {
        beautify_enabled = !beautify_enabled;
    }

    void toggle_pen_dialog()
    {
        show_new_pen_dialog = !show_new_pen_dialog;
        queue_draw();
    }

    void set_tool_brush()
    {
        current_tool = Tool::BRUSH;
        auto window = get_window();
        if (window)
            window->set_cursor(Gdk::Cursor::create(Gdk::PENCIL));
    }

    void set_tool_pan()
    {
        current_tool = Tool::PAN;
        auto window = get_window();
        if (window)
            window->set_cursor();
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

    NotebookArea()
    {
        add_events(Gdk::BUTTON_PRESS_MASK |
                   Gdk::BUTTON_RELEASE_MASK |
                   Gdk::POINTER_MOTION_MASK);

        signal_button_press_event().connect(
            sigc::mem_fun(*this, &NotebookArea::on_button_press), false);

        signal_button_release_event().connect(
            sigc::mem_fun(*this, &NotebookArea::on_button_release), false);

        signal_motion_notify_event().connect(
            sigc::mem_fun(*this, &NotebookArea::on_mouse_move), false);
    }

protected:
    double offset_x = 0;
    double offset_y = 0;

    double last_x = 0;
    double last_y = 0;

    bool dragging = false;

    struct Point
    {
        double x, y;
    };

    std::vector<Point> beautify_stroke(const std::vector<Point> &input)
    {
        if (input.size() < 3)
            return input;

        double total_length = 0;
        for (size_t i = 1; i < input.size(); ++i)
        {
            double dx = input[i].x - input[i - 1].x;
            double dy = input[i].y - input[i - 1].y;
            total_length += std::sqrt(dx * dx + dy * dy);
        }

        Point start = input.front();
        Point end = input.back();
        double dx_end = end.x - start.x;
        double dy_end = end.y - start.y;
        double straight_distance = std::sqrt(dx_end * dx_end + dy_end * dy_end);

        if (total_length > 0 && (straight_distance / total_length) > 0.85)
        {
            if (std::abs(dx_end) < 14.0)
            {
                end.x = start.x;
            }
            else if (std::abs(dy_end) < 14.0)
            {
                end.y = start.y;
            }
            return {start, end};
        }

        std::vector<Point> clean_stroke;
        size_t start_idx = (input.size() > 6) ? 2 : 0;
        size_t end_idx = (input.size() > 6) ? input.size() - 2 : input.size();
        for (size_t i = start_idx; i < end_idx; ++i)
        {
            clean_stroke.push_back(input[i]);
        }
        if (clean_stroke.size() < 3)
            return input;

        std::vector<Point> filtered;
        filtered.push_back(clean_stroke.front());
        for (size_t i = 1; i < clean_stroke.size(); ++i)
        {
            double dx = clean_stroke[i].x - filtered.back().x;
            double dy = clean_stroke[i].y - filtered.back().y;
            if (std::sqrt(dx * dx + dy * dy) > 3.0)
            {
                filtered.push_back(clean_stroke[i]);
            }
        }
        if (filtered.size() < 3)
            return filtered;

        std::vector<Point> current = filtered;
        for (int iter = 0; iter < 3; ++iter)
        {
            std::vector<Point> next;
            next.push_back(current.front());
            for (size_t i = 0; i < current.size() - 1; ++i)
            {
                Point p0 = current[i];
                Point p1 = current[i + 1];
                next.push_back({0.75 * p0.x + 0.25 * p1.x, 0.75 * p0.y + 0.25 * p1.y});
                next.push_back({0.25 * p0.x + 0.75 * p1.x, 0.25 * p0.y + 0.75 * p1.y});
            }
            next.push_back(current.back());
            current = next;
        }

        return current;
    }

    std::vector<std::vector<Point>> strokes;
    std::vector<std::vector<Point>> redo_strokes;
    std::vector<Point> current_stroke;

    bool on_button_press(GdkEventButton *event)
    {
        if (event->button == 1)
        {
            dragging = true;
            last_x = event->x;
            last_y = event->y;

            if (current_tool == Tool::PAN)
            {
                auto window = get_window();
                if (window)
                    window->set_cursor(Gdk::Cursor::create(Gdk::HAND1));
            }
            else if (current_tool == Tool::BRUSH)
            {
                current_stroke.clear();
                current_stroke.push_back({event->x + offset_x, event->y + offset_y});
            }
        }
        return true;
    }

    bool on_button_release(GdkEventButton *event)
    {
        if (event->button == 1)
        {
            dragging = false;

            if (current_tool == Tool::PAN)
            {
                auto window = get_window();
                if (window)
                    window->set_cursor();
            }
            else if (current_tool == Tool::BRUSH)
            {
                if (!current_stroke.empty())
                {
                    if (beautify_enabled)
                    {
                        strokes.push_back(beautify_stroke(current_stroke));
                    }
                    else
                    {
                        strokes.push_back(current_stroke);
                    }
                    current_stroke.clear();
                    redo_strokes.clear();
                }
                queue_draw();
            }
        }
        return true;
    }

    bool on_mouse_move(GdkEventMotion *event)
    {
        if (dragging)
        {
            if (current_tool == Tool::PAN)
            {
                double dx = event->x - last_x;
                double dy = event->y - last_y;

                offset_x -= dx;
                offset_y -= dy;

                last_x = event->x;
                last_y = event->y;

                queue_draw();
            }
            else if (current_tool == Tool::BRUSH)
            {
                current_stroke.push_back({event->x + offset_x, event->y + offset_y});
                queue_draw();
            }
        }
        return true;
    }

    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override
    {
        Gtk::Allocation allocation = get_allocation();

        const int width = allocation.get_width();
        const int height = allocation.get_height();

        cr->set_source_rgb(0.965, 0.965, 0.975);
        cr->paint();

        if (pdf_surface)
        {
            cr->set_source(pdf_surface, -offset_x, -offset_y);
            cr->paint();
        }

        const int grid = 28;

        cr->set_source_rgba(0.82, 0.84, 0.88, 0.75);
        cr->set_line_width(1.0);

        int start_x = (int)(-offset_x) % grid;
        int start_y = (int)(-offset_y) % grid;

        if (start_x > 0)
            start_x -= grid;
        if (start_y > 0)
            start_y -= grid;

        for (int x = start_x; x < width; x += grid)
        {
            cr->move_to(x + 0.5, 0);
            cr->line_to(x + 0.5, height);
        }

        for (int y = start_y; y < height; y += grid)
        {
            cr->move_to(0, y + 0.5);
            cr->line_to(width, y + 0.5);
        }

        cr->stroke();

        cr->set_line_width(2.5);
        cr->set_source_rgba(0.1, 0.1, 0.15, 0.9);
        cr->set_line_cap(Cairo::LINE_CAP_ROUND);
        cr->set_line_join(Cairo::LINE_JOIN_ROUND);

        auto draw_stroke = [&](const std::vector<Point> &stroke)
        {
            if (stroke.empty())
                return;
            cr->move_to(stroke[0].x - offset_x, stroke[0].y - offset_y);
            for (size_t i = 1; i < stroke.size(); ++i)
            {
                cr->line_to(stroke[i].x - offset_x, stroke[i].y - offset_y);
            }
            cr->stroke();
        };

        for (const auto &stroke : strokes)
        {
            draw_stroke(stroke);
        }

        draw_stroke(current_stroke);

        if (show_new_pen_dialog)
        {
            double box_w = 300, box_h = 400;
            double x = (width - box_w) / 2;
            double y = (height - box_h) / 2;
            double radius = 24;

            auto draw_rounded_rect = [&](double rx, double ry, double rw, double rh, double r)
            {
                cr->begin_new_sub_path();
                cr->arc(rx + rw - r, ry + r, r, -M_PI / 2, 0);
                cr->arc(rx + rw - r, ry + rh - r, r, 0, M_PI / 2);
                cr->arc(rx + r, ry + rh - r, r, M_PI / 2, M_PI);
                cr->arc(rx + r, ry + r, r, M_PI, 3 * M_PI / 2);
                cr->close_path();
            };

            for (int s = 1; s <= 12; s++)
            {
                double shadow_opacity = 0.03 * (13 - s) / 12.0;
                cr->set_source_rgba(0.0, 0.0, 0.0, shadow_opacity);
                draw_rounded_rect(x - s / 2.0, y - s / 2.0 + 4, box_w + s, box_h + s, radius + s / 2.0);
                cr->fill();
            }

            draw_rounded_rect(x, y, box_w, box_h, radius);
            cr->set_source_rgba(0.98, 0.98, 0.98, 0.90);
            cr->fill_preserve();

            cr->set_line_width(1.0);
            cr->set_source_rgba(1.0, 1.0, 1.0, 1.0);
            cr->stroke_preserve();

            cr->set_line_width(0.5);
            cr->set_source_rgba(0.0, 0.0, 0.0, 0.15);
            cr->stroke();

            double cx = x + box_w / 2;
            double cy = y + 150;
            double r_outer = 65, r_inner = 45;

            for (int i = 0; i < 360; i++)
            {
                double angle1 = i * M_PI / 180.0;
                double angle2 = (i + 1.5) * M_PI / 180.0;

                cr->set_source_rgb(
                    0.5 + 0.5 * sin(angle1),
                    0.5 + 0.5 * sin(angle1 + 2.09439),
                    0.5 + 0.5 * sin(angle1 + 4.18879));
                cr->arc(cx, cy, r_outer, angle1, angle2);
                cr->arc_negative(cx, cy, r_inner, angle2, angle1);
                cr->fill();
            }

            cr->set_line_width(0.5);
            cr->set_source_rgba(0.0, 0.0, 0.0, 0.1);

            cr->arc(cx, cy, r_outer, 0, 2 * M_PI);
            cr->stroke();

            cr->arc(cx, cy, r_inner, 0, 2 * M_PI);
            cr->stroke();
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

    Gtk::MenuItem *quit_item = Gtk::manage(new Gtk::MenuItem("Quit"));
    quit_item->add_accelerator("activate", accel_group, GDK_KEY_F4, Gdk::MOD1_MASK, Gtk::ACCEL_VISIBLE);
    file_menu->append(*quit_item);

    Gtk::MenuItem *undo_item = Gtk::manage(new Gtk::MenuItem("Undo"));
    undo_item->add_accelerator("activate", accel_group, GDK_KEY_z, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    undo_item->signal_activate().connect(sigc::mem_fun(notebook, &NotebookArea::undo));
    edit_menu->append(*undo_item);

    Gtk::MenuItem *redo_item = Gtk::manage(new Gtk::MenuItem("Redo"));
    redo_item->add_accelerator("activate", accel_group, GDK_KEY_y, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    redo_item->signal_activate().connect(sigc::mem_fun(notebook, &NotebookArea::redo));
    edit_menu->append(*redo_item);

    Gtk::MenuItem *toggle_beautify_item = Gtk::manage(new Gtk::CheckMenuItem("Toggle Beautifier"));
    toggle_beautify_item->add_accelerator("activate", accel_group, GDK_KEY_b, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    toggle_beautify_item->signal_activate().connect(sigc::mem_fun(notebook, &NotebookArea::toggle_beautifier));
    edit_menu->append(*toggle_beautify_item);

    edit_menu->append(*Gtk::manage(new Gtk::MenuItem("Cut")));
    edit_menu->append(*Gtk::manage(new Gtk::MenuItem("Copy")));
    edit_menu->append(*Gtk::manage(new Gtk::MenuItem("Paste")));

    window_menu->append(*Gtk::manage(new Gtk::MenuItem("Maximize")));
    window_menu->append(*Gtk::manage(new Gtk::MenuItem("Minimize")));

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
    pens_nested_menu->append(*mouse_item);
    mouse_item->signal_activate().connect(sigc::mem_fun(notebook, &NotebookArea::set_tool_pan));

    Gtk::MenuItem *brush_item = Gtk::manage(new Gtk::MenuItem("Brush"));
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