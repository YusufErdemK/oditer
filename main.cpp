#include <gtkmm.h>

int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create(argc, argv, "org.erdamn.oditer");

    Gtk::Window window;
    window.set_title("oditer");
    window.set_default_size(1200, 800);

    Gtk::Box main_box(Gtk::ORIENTATION_VERTICAL);
    window.add(main_box);

    // creating a menu bar
    Gtk::MenuBar menu_bar;

    // our little menus 😭
    Gtk::Menu* file_menu = Gtk::manage(new Gtk::Menu());
    Gtk::Menu* edit_menu = Gtk::manage(new Gtk::Menu());
    Gtk::Menu* window_menu = Gtk::manage(new Gtk::Menu());
    Gtk::Menu* pen_menu = Gtk::manage(new Gtk::Menu());

    // file menu items
    file_menu->append(*Gtk::manage(new Gtk::MenuItem("New")));
    file_menu->append(*Gtk::manage(new Gtk::MenuItem("Open")));
    file_menu->append(*Gtk::manage(new Gtk::MenuItem("Quit")));

    // edit menu items
    edit_menu->append(*Gtk::manage(new Gtk::MenuItem("Undo")));
    edit_menu->append(*Gtk::manage(new Gtk::MenuItem("Redo")));
    edit_menu->append(*Gtk::manage(new Gtk::MenuItem("Cut")));
    edit_menu->append(*Gtk::manage(new Gtk::MenuItem("Copy")));
    edit_menu->append(*Gtk::manage(new Gtk::MenuItem("Paste")));

    // window menu items
    window_menu->append(*Gtk::manage(new Gtk::MenuItem("Maximize")));
    window_menu->append(*Gtk::manage(new Gtk::MenuItem("Minimize")));

    //

    Gtk::MenuItem* file_item = Gtk::manage(new Gtk::MenuItem("File"));
    file_item->set_submenu(*file_menu);

    Gtk::MenuItem* edit_item = Gtk::manage(new Gtk::MenuItem("Edit"));
    edit_item->set_submenu(*edit_menu);

    Gtk::MenuItem* window_item = Gtk::manage(new Gtk::MenuItem("Window"));
    window_item->set_submenu(*window_menu);

    menu_bar.append(*file_item);
    menu_bar.append(*edit_item);
    menu_bar.append(*window_item);

    main_box.pack_start(menu_bar, Gtk::PACK_SHRINK);

    window.show_all();
    return app->run(window);
}