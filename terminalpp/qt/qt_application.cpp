#if (defined RENDERER_QT)

#include "../directwrite/windows.h"

#include "qt_application.h"
#include "qt_window.h"

namespace tpp2 {

    QtApplication::QtApplication(int & argc, char ** argv):
        QApplication{argc, argv},
        //selectionOwner_{nullptr},
        iconDefault_{":/icon_32x32.png"},
        iconNotification_{":/icon-notification_32x32.png"} {
#if (defined ARCH_WINDOWS)
        // on windows, the console must be attached and its window disabled so that later executions of WSL programs won't spawn new console window
        AttachConsole();
#endif

        //connect(clipboard(), &QClipboard::selectionChanged, this, &QtApplication::selectionChanged);
        connect(this, &QtApplication::tppUserEvent, this, static_cast<void (QtApplication::*)()>(&QtApplication::userEvent), Qt::ConnectionType::QueuedConnection);

        iconDefault_.addFile(":/icon_16x16.png");
        iconDefault_.addFile(":/icon_48x48.png");
        iconDefault_.addFile(":/icon_64x64.png");
        iconDefault_.addFile(":/icon_128x128.png");
        iconDefault_.addFile(":/icon_256x256.png");
        iconNotification_.addFile(":/icon-notification_16x16.png");
        iconNotification_.addFile(":/icon-notification_48x48.png");
        iconNotification_.addFile(":/icon-notification_64_64.png");
        iconNotification_.addFile(":/icon-notification_128x128.png");
        iconNotification_.addFile(":/icon-notification_256x256.png");
        // assertions to verify that the qt resources were built properly
        ASSERT(QFile::exists(":/icon_32x32.png"));
        ASSERT(QFile::exists(":/icon-notification_32x32.png"));



        Renderer::Initialize([this](){
            emit tppUserEvent();
        });

		QtWindow::StartBlinkerThread();
    }

    Window * QtApplication::createWindow(std::string const & title, int cols, int rows) {
        return new QtWindow{title, cols, rows};
    }

    void QtApplication::alert(std::string const & message) {
        QMessageBox msgBox{QMessageBox::Icon::Warning, "Error", message.c_str()};
        msgBox.exec();
    }

    void QtApplication::openLocalFile(std::string const & filename, bool edit) {
        MARK_AS_UNUSED(edit);
        QDesktopServices::openUrl(QUrl::fromLocalFile(filename.c_str()));
    }

    void QtApplication::userEvent() {
        Renderer::ExecuteUserEvent();
    }

} // namespace tpp

namespace tpp {

    Window * QtApplication::createWindow(std::string const & title, int cols, int rows, unsigned cellHeightPx) {
        return new QtWindow{title, cols, rows, cellHeightPx};
    }

    void QtApplication::clearSelection(QtWindow * owner) {
        if (owner == selectionOwner_) {
            selectionOwner_ = nullptr;
            if (clipboard()->supportsSelection()) 
                clipboard()->clear(QClipboard::Mode::Selection);
            else
                selection_.clear();
        } else {
            LOG() << "Window renderer clear selection does not match stored selection owner";
        }
    }

    void QtApplication::setSelection(QString value, QtWindow * owner) {
        if (selectionOwner_ != owner && selectionOwner_ != nullptr) 
            selectionOwner_->selectionInvalidated();
        selectionOwner_ = owner;
        if (clipboard()->supportsSelection()) 
            clipboard()->setText(value, QClipboard::Mode::Selection);
        else
            selection_ = value.toStdString();
    }

    void QtApplication::getSelectionContents(QtWindow * sender) {
        if (clipboard()->supportsSelection()) {
            QString text{clipboard()->text(QClipboard::Mode::Selection)};
            if (! text.isEmpty())
                sender->paste(text.toStdString());
        } else {
            if (!selection_.empty())
                sender->paste(selection_);
        }
    }

    void QtApplication::getClipboardContents(QtWindow * sender) {
        QString text{clipboard()->text(QClipboard::Mode::Clipboard)};
        if (! text.isEmpty())
            sender->paste(text.toStdString());
    }

    void QtApplication::selectionChanged() {
        if (! clipboard()->ownsSelection() && selectionOwner_ != nullptr) {
            selectionOwner_->selectionInvalidated();
            selectionOwner_ = nullptr;
        }
    }

    QtApplication::QtApplication(int & argc, char ** argv):
        QApplication{argc, argv},
        selectionOwner_{nullptr},
        iconDefault_{":/icon_32x32.png"},
        iconNotification_{":/icon-notification_32x32.png"} {
#if (defined ARCH_WINDOWS)
        // on windows, the console must be attached and its window disabled so that later executions of WSL programs won't spawn new console window
        AttachConsole();
#endif

        connect(clipboard(), &QClipboard::selectionChanged, this, &QtApplication::selectionChanged);

        QtWindow::StartBlinkerThread();
        iconDefault_.addFile(":/icon_16x16.png");
        iconDefault_.addFile(":/icon_48x48.png");
        iconDefault_.addFile(":/icon_64x64.png");
        iconDefault_.addFile(":/icon_128x128.png");
        iconDefault_.addFile(":/icon_256x256.png");
        iconNotification_.addFile(":/icon-notification_16x16.png");
        iconNotification_.addFile(":/icon-notification_48x48.png");
        iconNotification_.addFile(":/icon-notification_64_64.png");
        iconNotification_.addFile(":/icon-notification_128x128.png");
        iconNotification_.addFile(":/icon-notification_256x256.png");
        // assertions to verify that the qt resources were built properly
        ASSERT(QFile::exists(":/icon_32x32.png"));
        ASSERT(QFile::exists(":/icon-notification_32x32.png"));
    }

    QtApplication::~QtApplication() {
    }

    void QtApplication::alert(std::string const & message) {
        QMessageBox msgBox{QMessageBox::Icon::Warning, "Error", message.c_str()};
        msgBox.exec();
    }

    void QtApplication::openLocalFile(std::string const & filename, bool edit) {
        MARK_AS_UNUSED(edit);
        QDesktopServices::openUrl(QUrl::fromLocalFile(filename.c_str()));
    }

} // namespace tpp

#endif
