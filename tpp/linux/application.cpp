#ifdef __linux__

#include "helpers/helpers.h"
#include "helpers/log.h"

#include "application.h"
#include "terminal_window.h"

namespace tpp {

    namespace {

        int X11ErrorHandler(Display * display, XErrorEvent * e) {
            LOG << "X error: " << e->error_code;
            return 0;
        }

    } // anonymous namespace

	Display* Application::XDisplay_ = nullptr;
	int Application::XScreen_ = 0;
    XIM Application::XIm_;

	Application::Application() {
        XInitThreads();
		ASSERT(XDisplay_ == nullptr) << "Application is a singleton";
		XDisplay_ = XOpenDisplay(nullptr);
		if (XDisplay_ == nullptr)
			THROW(helpers::Exception("Unable to open X display"));
		XScreen_ = DefaultScreen(XDisplay_);
        //XSetErrorHandler(X11ErrorHandler);
        //XSynchronize(XDisplay_, true);


        // set the default machine locale instead of the "C" locale
        setlocale(LC_CTYPE, "");
        XSetLocaleModifiers("");

        // create X Input Method, each window then has its own context
        XIm_ = XOpenIM(XDisplay_, nullptr, nullptr, nullptr);
        ASSERT(XIm_ != nullptr);

	}

	Application::~Application() {
		XCloseDisplay(XDisplay_);
		XDisplay_ = nullptr;
	}

	void Application::mainLoop() {
		XEvent e;
		while (true) {
			XNextEvent(XDisplay_, &e);
            if (XFilterEvent(&e, None))
                continue;
            TerminalWindow::EventHandler(e);
       }
	}



} // namespace tpp

#endif