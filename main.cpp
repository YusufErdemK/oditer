// includes a gtk libary
#include <gtkmm.h>

// creates main function
int main(int argc, char* argv[]) {
    // gives unique id for our app
    auto app = Gtk::Application::create(argc, argv, "org.erdamn.oditer");

    // create the window
    Gtk::Window window;
    window.set_title("oditer");
    // set the size of our window
    window.set_default_size(1200, 800);
    // run that fucking app
    return app->run(window);
}