#pragma once

#include "helpers/char.h"

#include "common.h"
#include "geometry.h"

namespace ui2 {


    /** Single cell of the buffer. 
     
        The cell represents the codepoint to be displayed and its graphic properties. 
     */
    class Cell { 
    public:

        /** \name Codepoint
          
            The unencoded Unicode codepoint  to be displayed in the cell. 
         */
        //@{
        char32_t const & codepoint() const {
            return codepoint_;
        }

        char32_t & codepoint() {
            return codepoint_;
        }
        //@}

        /** \name Foreground Color
         
            The color of the character displayed in the cell.
         */
        //@{
        Color const & fg() const {
            return fg_;
        }

        Color & fg() {
            return fg_;
        }
        //@}

        /** \name Background Color
         
            The background color of the cell. 
         */
        //@{
        Color const & bg() const {
            return bg_;
        }

        Color & bg() {
            return bg_;
        }
        //@}

        /** \name Decoration Color. 
         
            The color of font decorations, such as underline or strikethrough. 
         */
        //@{
        Color const & decor() const {
            return decor_;
        }

        Color & decor() {
            return decor_;
        }
        //@}

        /** \name Font
         
            The font used to render the cell. 
         */
        //@{
        Font const & font() const {
            return font_;
        }

        Font & font() {
            return font_;
        }
        //@}

        /** \name Border
         
            The border of the cell. 
         */
        //@{
        Border const & border() const {
            return border_;
        }

        Border & border() {
            return border_;
        }
        //@}

    private:
        char32_t codepoint_;

        Color fg_;
        Color bg_;
        Color decor_;

        Font font_;

        Border border_;

    }; // ui::Cell

    /** The UI backing buffer. 
     
        The buffer contains a 2D array of cells describing the physical screen and allows their basic access. 

        For performance reasons, the 2D array is organized on per row basis so that scrolling rows is a simple pointer swap and does not have to involve any complex memory copying, which in case of fast scrolling terminal commands can be quite expensive. 
     */
    class Buffer {
    public:

        Buffer(int width, int height):
            width_{width},
            height_{height},
            rows_{new Cell*[height]} {
            for (int i = 0; i < height; ++i)
                rows_[i] = new Cell[width];
        }

        Buffer(Buffer && from):
            width_{from.width_},
            height_{from.height_},
            rows_{from.rows_} {
            from.width_ = 0;
            from.height_ = 0;
            from.rows_ = nullptr;
        }

        Buffer & operator = (Buffer && from) {
            clear_();
            width_ = from.width_;
            height_ = from.height_;
            rows_ = from.rows_;
            from.width_ = 0;
            from.height_ = 0;
            from.rows_ = nullptr;
            return *this;
        }

        ~Buffer() {
            clear_();
        }

        int width() const {
            return width_;
        }

        int height() const {
            return height_;
        }

        Cell const & at(int x, int y) const {
            ASSERT(x >= 0 && x < width_);
            ASSERT(y >= 0 && y < height_);
            return rows_[y][x];
        }

        Cell const & at(Point p) const {
            return at(p.x(), p.y());
        }

        Cell & at(int x, int y) {
            ASSERT(x >= 0 && x < width_);
            ASSERT(y >= 0 && y < height_);
            return rows_[y][x];
        }

        Cell & at(Point p) {
            return at(p.x(), p.y());
        }

        void swapRows(int first, int second) {
            ASSERT(first >= 0 && first < height_);
            ASSERT(second>= 0 && second < height_);
            std::swap(rows_[first], rows_[second]);
        }

        /** Resizes the buffer. 
         */
        void resize(int width, int height) {
            MARK_AS_UNUSED(width);
            MARK_AS_UNUSED(height);
            NOT_IMPLEMENTED;
        }

    private:

        void clear_() {
            for (int i = 0; i < height_; ++i)
                delete rows_[i];
            delete rows_;
        }

        int width_;
        int height_;
        Cell ** rows_;
    };

} // namespace ui2