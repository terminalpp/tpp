#pragma once
#if (defined RENDERER_QT)

#include <QtWidgets>

#include "../application.h"

namespace tpp {

    class QtWindow;

    class QtApplication : public Application, QApplication {
    public:

        static void Initialize(int & argc, char ** argv) {
            new QtApplication{argc, argv};
        }

        static QtApplication * Instance() {
            return dynamic_cast<QtApplication*>(Application::Instance());
        }

        Window * createWindow(std::string const & title, int cols, int rows, unsigned cellHeightPx) override;

        void mainLoop() override {
            exec();
        }


    protected:

        friend class QtWindow;

        QtApplication(int & argc, char ** argv);

        ~QtApplication() override;

        void alert(std::string const & message) override;

        void openLocalFile(std::string const & filename, bool edit) override;

    }; // tpp::QtApplication

} // tpp

#endif