
#include <utility>
#include "../widget.h"
#include "../geometry.h"

namespace ui {

    class ScrollBar : public Widget {
    public:
        /** ScrollBar's position. 
         
            The left and right scrollbars are always vertical, while the top and bottom scrollbars are horizontal, if target widget is used. 
         */
        enum class Position {
            Left,
            Right,
            Top,
            Bottom,
        }; // ui::ScrollBar::Position

        Position position() const {
            return position_;
        }

        virtual void setPosition(Position value) {
            if (position_ != value) {
                position_ = value;
                requestRepaint();
            }
        }

    protected:

        /** Paints the scrollbar.
         */
        void paint(Canvas & canvas) override {
            switch (position_) {
                case Position::Left: {
                    Border b = Border::Empty(color_).setLeft(Border::Kind::Thin);
                    Border slider = Border::Empty(color_).setLeft(Border::Kind::Thick);
                    paintVertical(canvas, 0, b, slider);
                    break;
                }
                case Position::Right: {
                    Border b = Border::Empty(color_).setRight(Border::Kind::Thin);
                    Border slider = Border::Empty(color_).setRight(Border::Kind::Thick);
                    paintVertical(canvas, width() - 1, b, slider);
                    break;
                }
                case Position::Top: {
                    Border b = Border::Empty(color_).setTop(Border::Kind::Thin);
                    Border slider = Border::Empty(color_).setTop(Border::Kind::Thick);
                    paintHorizontal(canvas, 0, b, slider);
                    break;
                }
                case Position::Bottom: {
                    Border b = Border::Empty(color_).setBottom(Border::Kind::Thin);
                    Border slider = Border::Empty(color_).setBottom(Border::Kind::Thick);
                    paintHorizontal(canvas, height() - 1, b, slider);
                    break;
                }
            }
        }

        void paintHorizontal(Canvas & canvas, int row, Border & b, Border & slider) {
            std::pair<int, int> dim = sliderDimensions(size().height());
            canvas.setBorder(Point{0,row}, Point{dim.first, row}, b);
            canvas.setBorder(Point{dim.second, row}, Point{height(), row}, b);
            canvas.setBorder(Point{dim.first, row}, Point{dim.second, row}, slider);
        }

        void paintVertical(Canvas & canvas, int col, Border & b, Border & slider) {
            std::pair<int, int> dim = sliderDimensions(size().height());
            canvas.setBorder(Point{col, 0}, Point{col, dim.first}, b);
            canvas.setBorder(Point{col, dim.second}, Point{col, height()}, b);
            canvas.setBorder(Point{col, dim.first}, Point{col, dim.second}, slider);
        }

        /** Determines the size of the slider based on the scrollbar attributes and its size given as argument. 
         
            Returns the start (inclusive) and end (exclusive) of the slider for a scrollbar of given size. 
         */
        std::pair<int, int> sliderDimensions(int scrollBarSize) {
            // adjust the value and size so that they appear to start at 0
            int adjustedSize = max_ - min_;
            int adjustedValue = value_ - min_;
            // calculate slider size in cells, which must be at least one
            int sliderSize = std::max(1, sliderSize_ * scrollBarSize / adjustedSize);
            // calculate slider start and make sure the slider start and slider fall within the slider itself and that the slider is not at the top or bottom position unless value is in the extreme
            int sliderStart = adjustedValue * scrollBarSize / adjustedSize;
            if (sliderStart == 0 && adjustedValue != 0)
                sliderStart = 1;
            if (sliderStart + sliderSize > scrollBarSize)
                sliderStart = scrollBarSize - sliderSize;
            // and return 
            return std::make_pair(sliderStart, sliderStart + sliderSize);
        }	
        
    private:

        /** Position of the scrollbar's slider. */
        Position position_;
        /** Color of the slider and scrollbar. */
        Color color_ = Color::White.withAlpha(64);
        /** Minimal value of the scrolled value. */
        int min_ = 0;
        /** Maximal value of the scrolled value. */
        int max_ = 100;
        /** Current value of the scrollbar. */
        int value_ = 0;
        /** The size of the slider (in the min-max range). */
        int sliderSize_ = 1;

    }; // ui::Scrollbar
 

    /** Scrollable widget. 
     
     */
    class ScrollBox : public virtual Widget {
    public:

        /** Returns the horizontal scroolbar. 
         */
        ScrollBar * scrollBarHorizontal() const {
            return dynamic_cast<ScrollBar*>(children()[0]);
        }

        /** Returns the vertical scrollbar. 
         */
        ScrollBar * scrollBarVertical() const {
            return dynamic_cast<ScrollBar*>(children()[1]);
        }

        /** Returns the scrolled contents widget. 
         */
        Widget * contents() const {
            return children().size() == 2 ? nullptr : children()[2];
        }

        /** Sets the scrolled widget. 
         
            The widget is immediate child of the scrollbox. 
         */
        virtual void setContents(Widget * contents) {
            if (children().size() == 3 && children()[2] != contents)
                detach(children()[2]);
            if (contents != nullptr)
                attach(contents);
        }

    protected:

        /** Returns the contents canvas. 
         
            The contents canvas is created by offseting the widget's contents canvas by the scrollOffset(). 
         */
        Canvas getContentsCanvas(Canvas & widgetCanvas) const override {
            return Widget::getContentsCanvas(widgetCanvas).offsetBy(scrollOffset_);
        }

    private:


        Point scrollOffset_;

    }; // ui::ScrollBox

} // namespace ui