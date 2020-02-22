#pragma once

#include "../../build_stamp.h"
#include "helpers/stamp.h"

#include "ui/builders.h"

#include "ui/traits/box.h"
#include "ui/traits/modal.h"
//#include "ui/layout.h"

namespace tpp {

    /** A simple about box.

        Displays the terminal version and basic information. The about box dims the screen and then displays a centered about box with fixed width and height (60x10 cells).
     */
    class AboutBox : public ui::Widget, public ui::Modal<AboutBox>, public ui::Box<AboutBox> {
    public:
        AboutBox():
            Widget{},
            Modal{},
            Box{ui::Color::Blue, ui::Border::Thin(ui::Color::White)},
            lastKey_(ui::Key::Invalid) {
            setWidthHint(ui::Layout::SizeHint::Fixed());
            setHeightHint(ui::Layout::SizeHint::Fixed());
            resize(65,10);
        }

        using Widget::setVisible;

    protected:

        void mouseClick(int col, int row, ui::MouseButton button, ui::Key modifiers) override {
            MARK_AS_UNUSED(col);
            MARK_AS_UNUSED(row);
            MARK_AS_UNUSED(button);
            MARK_AS_UNUSED(modifiers);
            if (lastKey_ == ui::Key::Invalid)
                dismiss();
        }

        void keyDown(ui::Key k) override {
            if (lastKey_ == ui::Key::Invalid)
                lastKey_ = k.code();
        }

        void keyUp(ui::Key k) override {
            if (k.code() == lastKey_) {
                lastKey_ = ui::Key::Invalid;
                dismiss();
            }
        }

        void paint(ui::Canvas & canvas) override {
            using namespace ui;
            Box::paint(canvas);
            int x = 0; //(canvas.width() - 60) / 2;
            int y = 0; //(canvas.height() - 10) / 2;
            canvas.setFg(Color::White);
            canvas.setFont(ui::Font{}.setSize(2));
            canvas.lineOut(Point{x,y + 1}, "Terminal++", HorizontalAlign::Center);
            canvas.setFont(ui::Font{});

            helpers::Stamp stamp = helpers::Stamp::Stored();
            if (stamp.version().empty()) {
                canvas.lineOut(Point{x + 5, y + 3}, STR("commit:   " << stamp.commit() << (stamp.clean() ? "" : "*")));
                canvas.lineOut(Point{x + 15, y + 4}, stamp.time());
            } else {
                canvas.lineOut(Point{x + 5, y + 3}, STR("version:  " << stamp.version()));
                canvas.lineOut(Point{x + 15, y + 4}, STR(stamp.commit() << (stamp.clean() ? "" : "*")));
                canvas.lineOut(Point{x + 15, y + 5}, stamp.time());
            }
#if (defined RENDERER_QT)
            canvas.lineOut(Point{x + 5, y + 7}, STR("platform: " << ARCH << "(Qt) " << ARCH_SIZE << " " << ARCH_COMPILER << " " << ARCH_COMPILER_VERSION << " " << stamp.buildType()));
#else
            canvas.lineOut(Point{x + 5, y + 7}, STR("platform: " << ARCH << "(native) " << ARCH_SIZE << " " << ARCH_COMPILER << " " << ARCH_COMPILER_VERSION << " " << stamp.buildType()));
#endif
            canvas.setAttributes(Attributes{}.setBlink());
            canvas.lineOut(Point{x, y + 9}, "Hit a key to dismiss", HorizontalAlign::Center);
        }

    private:
        unsigned lastKey_;
    }; // tpp::AboutBox

} // namespace tpp