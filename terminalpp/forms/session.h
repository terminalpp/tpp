#pragma once 

#include "../config.h"

#include "ui/widgets/panel.h"
#include "ui/widgets/modal_pane.h"
#include "ui/widgets/dialog.h"
#include "ui/layouts/maximize.h"
#include "ui/layouts/column.h"
#include "ui/layouts/row.h"
#include "ui-terminal/ansi_terminal.h"
#include "ui-terminal/bypass_pty.h"
#include "ui-terminal/local_pty.h"


namespace tpp {

    /** Paste confirmation dialog. 
     */
    class PasteDialog : public ui::Dialog {
    public:
        PasteDialog() {
            setHeightHint(SizeHint::Percentage(25));
        }


    protected:
        void paint(Canvas & canvas) override {
            Dialog::paint(canvas);
            canvas.textOut(Point{0,0}, "Are you sure you want to paste?");
        }
    }; 

    /** The terminal session. 
     */

    // TODO fix container's add method
    class Session : public ui::Panel, public ui::AutoScroller<Session> {
    public:

        Session(Window * window):
            window_{window},
            palette_{AnsiTerminal::Palette::XTerm256()} {
        	Config const & config = Config::Instance();
            window_->setRootWidget(this);
            terminal_ = new AnsiTerminal{& palette_, width(), height()};
            //terminal_->setHeightHint(SizeHint::Percentage(75));
            terminal_->onPTYTerminated.setHandler(&Session::terminalPTYTerminated, this);
            terminal_->onTitleChange.setHandler(&Session::terminalTitleChanged, this);
            terminal_->onNotification.setHandler(&Session::terminalNotification, this);
            terminal_->onKeyDown.setHandler(&Session::terminalKeyDown, this);
            terminal_->onMouseMove.setHandler(&Session::terminalMouseMove, this);
            terminal_->onMouseDown.setHandler(&Session::terminalMouseDown, this);
            terminal_->onMouseUp.setHandler(&Session::terminalMouseUp, this);
            terminal_->onMouseWheel.setHandler(&Session::terminalMouseWheel, this);
            terminal_->onSetClipboard.setHandler(&Session::terminalSetClipboard, this);
            setLayout(new MaximizeLayout());
            //setLayout(new ColumnLayout(VerticalAlign::Bottom));
            setBorder(Border{Color::Blue}.setAll(Border::Kind::Thick));
            add(terminal_);

            modalPane_ = new ModalPane();
            modalPane_->setLayout(new ColumnLayout(VerticalAlign::Bottom));
            add(modalPane_);
    

#if (ARCH_WINDOWS)
            pty_ = new ui::BypassPTY{terminal_, config.session.command()};
//            pty_ = new ui::LocalPTY{terminal_, helpers::Command{"cmd.exe", {}}};
#else
            pty_ = new ui::LocalPTY{terminal_, config.session.command()};
#endif
            //window_->setKeyboardFocus(terminal_);
        }

        ~Session() override {
            delete pty_;
        }

    protected:

        bool autoScrollStep(Point by) override {
            return terminal_->scrollBy(by);
        }

        void terminalPTYTerminated(Event<ExitCode>::Payload & e) {
            window_->setIcon(Window::Icon::Notification);
            window_->setTitle(STR("Terminated, exit code " << *e));
            Config & config = Config::Instance();
            //if (! config.session.waitAfterPtyTerminated())
            //    window_->requestClose();
            // TODO perform the wait for keypress here
        }        

        void terminalTitleChanged(Event<std::string>::Payload & e) {
            window_->setTitle(*e);
        }

        void terminalNotification(Event<void>::Payload & e) {
            MARK_AS_UNUSED(e);
            window_->setIcon(Window::Icon::Notification);
        }

        void terminalKeyDown(Event<Key>::Payload & e) {
            if (window_->icon() != Window::Icon::Default)
                window_->setIcon(Window::Icon::Default);
            // trigger paste event for ourselves so that the paste can be intercepted
            if (*e == (Key::V + Key::Ctrl + Key::Shift)) {
                requestClipboard();
                e.stop();
            }
        }

        void terminalMouseMove(Event<MouseMoveEvent>::Payload & event) {
            if (terminal_->mouseCaptured())
                return;
            if (terminal_->updatingSelection()) {
                if (event->coords.y() < 0 || event->coords.y() >= terminal_->height())
                    startAutoScroll(Point{0, event->coords.y() < 0 ? -1 : 1});
                else
                    stopAutoScroll();
            }
        }

        void terminalMouseDown(Event<MouseButtonEvent>::Payload & event) {
            if (terminal_->mouseCaptured())
                return;
            if (event->modifiers == 0) {
                if (event->button == MouseButton::Left) {
                    terminal_->startSelectionUpdate(event->coords);
                } else if (event->button == MouseButton::Wheel) {
                    requestSelection(); 
                } else if (event->button == MouseButton::Right && ! terminal_->selection().empty()) {
                    setClipboard(terminal_->getSelectionContents());
                    terminal_->clearSelection();
                } else {
                    return;
                }
            }
            event.stop();
        }

        void terminalMouseUp(Event<MouseButtonEvent>::Payload & event) {
            if (terminal_->mouseCaptured())
                return;
            if (event->modifiers == 0) {
                if (event->button == MouseButton::Left) 
                    terminal_->endSelectionUpdate();
                else
                    return;
            }
            event.stop();
        }

        void terminalMouseWheel(Event<MouseWheelEvent>::Payload & event) {
            if (terminal_->mouseCaptured())
                return;
            //if (state_.buffer.historyRows() > 0) {
                if (event->by > 0)
                    terminal_->scrollBy(Point{0, -1});
                else 
                    terminal_->scrollBy(Point{0, 1});
                event.stop();
            //}
        }

        void terminalSetClipboard(Event<std::string>::Payload & event) {
            setClipboard(*event);
        }

        void paste(Event<std::string>::Payload & e) override {
            PasteDialog * pd = new PasteDialog();
            pd->onSuccess.setHandler([this, e](Event<void>::Payload) mutable {
                terminal_->paste(e);
            });
            modalPane_->show(pd);
        }

    private:

        /** The window in which the session is rendered.
         */
        Window * window_;

        AnsiTerminal::Palette palette_;
        AnsiTerminal * terminal_;
        PTY * pty_;
        ModalPane * modalPane_;
    }; 

}
