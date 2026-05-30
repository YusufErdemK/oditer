#include <gtkmm.h>
#include <cmath>

class NotebookArea : public Gtk::DrawingArea
{
public:
    bool show_new_pen_dialog = false;

    void toggle_pen_dialog()
    {
        show_new_pen_dialog = !show_new_pen_dialog;
        queue_draw();
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

    bool on_button_press(GdkEventButton *event)
    {
        if (event->button == 1)
        {
            dragging = true;
            last_x = event->x;
            last_y = event->y;

            auto window = get_window();
            if (window)
            {
                window->set_cursor(Gdk::Cursor::create(Gdk::HAND1));
            }
        }
        return true;
    }

    bool on_button_release(GdkEventButton *event)
    {
        if (event->button == 1)
        {
            dragging = false;

            auto window = get_window();
            if (window)
            {
                window->set_cursor();
            }
        }
        return true;
    }

    bool on_mouse_move(GdkEventMotion *event)
    {
        if (dragging)
        {
            double dx = event->x - last_x;
            double dy = event->y - last_y;

            offset_x -= dx;
            offset_y -= dy;

            last_x = event->x;
            last_y = event->y;

            queue_draw();
        }
        return true;
    }

    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override
    {
        Gtk::Allocation allocation = get_allocation();

        const int width = allocation.get_width();
        const int height = allocation.get_height();

        // background
        cr->set_source_rgb(0.965, 0.965, 0.975);
        cr->paint();

        const int grid = 28;

        cr->set_source_rgba(0.82, 0.84, 0.88, 0.75);
        cr->set_line_width(1.0);

        int start_x = (int)(-offset_x) % grid;
        int start_y = (int)(-offset_y) % grid;

        if (start_x > 0)
            start_x -= grid;
        if (start_y > 0)
            start_y -= grid;

        // vertical lines
        for (int x = start_x; x < width; x += grid)
        {
            cr->move_to(x + 0.5, 0);
            cr->line_to(x + 0.5, height);
        }

        // horizontal lines
        for (int y = start_y; y < height; y += grid)
        {
            cr->move_to(0, y + 0.5);
            cr->line_to(width, y + 0.5);
        }

        cr->stroke();

        if (show_new_pen_dialog)
        {
            double box_w = 300, box_h = 400;
            double x = (width - box_w) / 2;
            double y = (height - box_h) / 2;
            double radius = 24; // i love 24px br twin

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
                // shadow effect (this is ai sorry)
                draw_rounded_rect(x - s / 2.0, y - s / 2.0 + 4, box_w + s, box_h + s, radius + s / 2.0);
                cr->fill();
            }

            // LIQUID GLASSSSSS YEAAAH
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

    Gtk::Box main_box(Gtk::ORIENTATION_VERTICAL);
    window.add(main_box);

    NotebookArea notebook;

    // menu bar
    Gtk::MenuBar menu_bar;

    Gtk::Menu *file_menu = Gtk::manage(new Gtk::Menu());
    Gtk::Menu *edit_menu = Gtk::manage(new Gtk::Menu());
    Gtk::Menu *window_menu = Gtk::manage(new Gtk::Menu());
    Gtk::Menu *pen_menu = Gtk::manage(new Gtk::Menu());

    file_menu->append(*Gtk::manage(new Gtk::MenuItem("New")));
    file_menu->append(*Gtk::manage(new Gtk::MenuItem("Open")));
    file_menu->append(*Gtk::manage(new Gtk::MenuItem("Quit")));

    edit_menu->append(*Gtk::manage(new Gtk::MenuItem("Undo")));
    edit_menu->append(*Gtk::manage(new Gtk::MenuItem("Redo")));
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

    pens_nested_menu->append(*Gtk::manage(new Gtk::MenuItem("Brush")));
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