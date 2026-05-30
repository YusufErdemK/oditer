#include <gtkmm.h>

class NotebookArea : public Gtk::DrawingArea {
protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        Gtk::Allocation allocation = get_allocation();

        const int width = allocation.get_width();
        const int height = allocation.get_height();

        cr->set_source_rgb(0.965, 0.965, 0.975);
        cr->paint();

        const int grid = 28;

        cr->set_source_rgba(0.82, 0.84, 0.88, 0.35);
        cr->set_line_width(1.0);

        // vertical lines
        for (int x = 0; x < width; x += grid) {
            cr->move_to(x + 0.5, 0);
            cr->line_to(x + 0.5, height);
        }

        // horizontal lines
        for (int y = 0; y < height; y += grid) {
            cr->move_to(0, y + 0.5);
            cr->line_to(width, y + 0.5);
        }

        cr->stroke();

        // subtle top glow
        Cairo::RefPtr<Cairo::LinearGradient> gradient =
            Cairo::LinearGradient::create(0, 0, 0, 120);

        gradient->add_color_stop_rgba(0.0, 1, 1, 1, 0.18);
        gradient->add_color_stop_rgba(1.0, 1, 1, 1, 0.0);

        cr->set_source(gradient);
        cr->rectangle(0, 0, width, 120);
        cr->fill();

        return true;
    }
};

int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create(argc, argv, "org.erdamn.oditer");

    Gtk::Window window;
    window.set_title("oditer");
    window.set_default_size(1200, 800);

    Gtk::Box main_box(Gtk::ORIENTATION_VERTICAL);
    window.add(main_box);

    // menu bar
    Gtk::MenuBar menu_bar;

    Gtk::Menu* file_menu = Gtk::manage(new Gtk::Menu());
    Gtk::Menu* edit_menu = Gtk::manage(new Gtk::Menu());
    Gtk::Menu* window_menu = Gtk::manage(new Gtk::Menu());
    Gtk::Menu* pen_menu = Gtk::manage(new Gtk::Menu());

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

    pen_menu->append(*Gtk::manage(new Gtk::MenuItem("Create Pen")));
    pen_menu->append(*Gtk::manage(new Gtk::MenuItem("Import Pen")));
    pen_menu->append(*Gtk::manage(new Gtk::MenuItem("Pens")));

    Gtk::MenuItem* file_item = Gtk::manage(new Gtk::MenuItem("File"));
    file_item->set_submenu(*file_menu);

    Gtk::MenuItem* edit_item = Gtk::manage(new Gtk::MenuItem("Edit"));
    edit_item->set_submenu(*edit_menu);

    Gtk::MenuItem* window_item = Gtk::manage(new Gtk::MenuItem("Window"));
    window_item->set_submenu(*window_menu);

    Gtk::MenuItem* pen_item = Gtk::manage(new Gtk::MenuItem("Pen"));
    pen_item->set_submenu(*pen_menu);

    menu_bar.append(*file_item);
    menu_bar.append(*edit_item);
    menu_bar.append(*window_item);
    menu_bar.append(*pen_item);

    main_box.pack_start(menu_bar, Gtk::PACK_SHRINK);

    NotebookArea notebook;
    main_box.pack_start(notebook);

    window.show_all();
    return app->run(window);
}