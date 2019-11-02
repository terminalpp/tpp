#pragma once
#include <type_traits>
#include <thread>
#include <deque>

#include "helpers/process.h"

#include "../canvas.h"
#include "../widget.h"
#include "../selection.h"
#include "../builders.h"
#include "scrollbox.h"


/** \page vterm Terminal Widget
 
    \brief Widget capable of displaying terminal contents via attached PTY. 

    \section vtermHistory Terminal History
*/

namespace ui {

    /** The pseudoterminal connection interface. 

        A pseudoterminal connection provides the simplest possible interface to the target process. 
     */
    class PTY {
    public:
        /** Sends the given buffer to the target process. 
         */
        virtual void send(char const * buffer, size_t bufferSize) = 0;

        /** Receives up to bufferSize bytes which are stored in the provided buffer.
         
            Returns the number of bytes received. If there are no data available, blocks until the data is ready and then returns them. 

            If the attached process is terminated, should return immediately with 0 bytes read. 
         */
        virtual size_t receive(char * buffer, size_t bufferSize) = 0;

        /** Terminates the target process and returns immediately. 
         
            If the target process has already terminated, simply returns. 
         */
        virtual void terminate() = 0;

        /** Waits for the attached process to terminate and returns its exit code. 
         */
        virtual helpers::ExitCode waitFor() = 0;

        virtual void resize(int cols, int rows) = 0;

		/** Virtual destructor. 
		 */
		virtual ~PTY() {
		}

    }; // ui::PTY


    typedef helpers::EventPayload<helpers::ExitCode, Widget> ExitCodeEvent;

    /** The input buffer of the terminal and its length. 
     
        TODO this looks very generic and perhaps can exist in some generic place too.
     */
    struct InputBuffer {
        char const * buffer;
        size_t size;
    };

    struct InputError : public InputBuffer {
        std::string error;

        InputError(char const * buffer, size_t size, std::string const &error):
            InputBuffer{buffer, size},
            error(error) {
        }
    };

    typedef helpers::EventPayload<InputBuffer, Widget> InputProcessedEvent;
    typedef helpers::EventPayload<InputError, Widget> InputErrorEvent;

    class Terminal : public ScrollBox, public SelectionOwner {
    public:

        using Widget::setFocused;
        using Widget::setFocusStop;
        using Widget::setFocusIndex;
        using Widget::setEnabled;

        /** The terminal buffer holds information about the cells and cursor. 

        */
        class Buffer {
        public:

            typedef helpers::SmartRAIIPtr<Buffer> Ptr;

            Buffer(int cols, int rows);

            Buffer(Buffer const & from);

            Buffer & operator = (Buffer const & other);

            ~Buffer();

            int cols() const {
                return cols_;
            }

            int rows() const {
                return rows_;
            }

            Cell const & at(int x, int y) const {
                ASSERT(x >= 0 && x <= cols_ && y >= 0 && y < rows_);
                return cells_[y][x];
            }

            Cell & at(int x, int y) {
                ASSERT(x >= 0 && x <= cols_ && y >= 0 && y < rows_);
                return cells_[y][x];
            }

            Cell const & at(Point pos) const {
                return at(pos.x, pos.y);
            }

            Cell & at(Point pos) {
                return at(pos.x, pos.y);
            }

            Cursor const & cursor() const {
                return cursor_;
            }

            Cursor & cursor() {
                return cursor_;
            }

            void lock() {
                lock_.lock();
            }

            void priorityLock() {
                lock_.priorityLock();
            }

            void unlock() {
                lock_.unlock();
            }

            /** Resizes the buffer. 
             
                The terminal to which the buffer belongs can be provided as an optional argument, in which case any lines that would be deleted after the resize would be added to the history of the specified terminal. 
             */
            void resize(int cols, int rows, Terminal * terminal);

            /** Inserts given number of lines at given top row.
                
                Scrolls down all lines between top and bottom accordingly. Fills the new lines with the provided cell.
                */
            void insertLines(int lines, int top, int bottom, Cell const & fill);

            /** Deletes given number of lines at given top row.
                
                Scrolls up all lines between top and bottom accordingly. Fills the new lines at the bottom with the provided cell.
                */
            void deleteLines(int lines, int top, int bottom, Cell const & fill);

            /** Returns the given line as a string.
             
                TODO This should preserve the attributes somehow.
             */
            std::string getLine(int line);

        private:

            /** Fills the specified row with given character. 

                Copies larger and larger number of cells at once to be more efficient than simple linear copy. 
                */
            void fillRow(Cell* row, Cell const& fill, unsigned cols);

            /** Resizes the cell buffer. 

                First creates the new buffer and clears it. Then we determine the latest line to be copied (since the client app is supposed to rewrite the current line). We also calculate the offset of this line to the current cursor line, which is important if the last line spans multiple terminal lines since we must adjust the cursor position accordingly then. 

                The line is the copied. 
            */
            void resizeCells(int newCols, int newRows, Terminal * terminal);


            /* Size of the buffer and the array of the cells. */
            int cols_;
            int rows_;
            Cell ** cells_;

            Cursor cursor_;

            /* Priority lock on the buffer. */
            helpers::PriorityLock lock_;


        }; // Terminal::Buffer 

        // events

        /** Triggered when the attached pseudoterminal terminates. 
         
            Note that when the terminal is deleted, its PTY is forcibly terminated at first, so the event will fire precisely once in the terminal lifetime. 
         */
        helpers::Event<ExitCodeEvent> onPTYTerminated;

        helpers::Event<StringEvent> onTitleChange;

        helpers::Event<VoidEvent> onNotification;

        /** Triggered when a terminal line has been scrolled out. 
         
            It is assumed that the topmost line is always the line scrolled out. 
         */
        helpers::Event<VoidEvent> onLineScrolledOut;

        /** Triggered when new input has been processed by the terminal. 
         */
        helpers::Event<InputProcessedEvent> onInput;
        helpers::Event<InputErrorEvent> onInputError;

        // methods

        PTY * pty() {
            return pty_;
        }

        /** Returns the title of the terminal. 
         */
        std::string const & title() const {
            return title_;
        }

        /** Returns the limit of lines that are retained outside of the terminal window. 
         */
        size_t historySizeLimit() const {
            return historySizeLimit_;
        }

        /** Sets the maximum number of lines past the terminal window the terminal is allowed to remember. 
         
            If set to 0, no scrollback is allowed.
         */
        void setHistorySizeLimit(size_t value) {
            if (value != historySizeLimit_) {
                historySizeLimit_ = value;
                // remove excess history data
                while (history_.size() > historySizeLimit_)
                    popHistoryLine();
                // resize window properly
                updateClientRect();
            }
        }

        /** Makes sure the terminal is scrolled so that the prompt is visible. 
         
            For the terminal, it simply scrolls past the history. 
         */
        void scrollToPrompt() {
            if (scrollable_ && history_.size() > 0)
                setScrollOffset(Point{0, static_cast<int>(history_.size())});
        }

        /** Deletes the terminal and the attached PTY. 
         
            Waits for the reading and process exit terminating threads to finalize. 
         */
        ~Terminal() {
            fps_ = 0;
            delete pty_;
            ptyReader_.join();
            ptyListener_.join();
            repainter_.join();
            while (! history_.empty())
                popHistoryLine();
        }

        Point scrollOffset() const override {
            return ScrollBox::scrollOffset();
        }

        Rect clientRect() const override {
            return ScrollBox::clientRect();
        }

    protected:

        /** Creates the terminal with the given PTY. 

            Starts the process exit monitoring thread and the receiving thread to read input from the pty. 
         */
        Terminal(int width, int height, PTY * pty, unsigned fps, size_t ptyBufferSize = 10240);

        virtual Color defaultForeground() const {
            return Color::White();
        }

        virtual Color defaultBackground() const {
            return Color::Black();
        }

        /** Returns the locked buffer. 
         */
        Buffer::Ptr buffer(bool priority = false) {
            if (priority)
                buffer_.priorityLock();
            else
                buffer_.lock();
            return Buffer::Ptr(buffer_, false);
        }

        /** Copies the contents of the terminal's buffer to the ui canvas. 
         */
        void paint(Canvas & canvas) override;

        /** Updates the FPS ratio of the terminal. 
         
            Note that the FPS is not precise since triggering the repaint event itself is not accounted for in the wait. This should not be a problem since the terminal is not really hard real-time affair.
         */
        void setFPS(unsigned value) {
            ASSERT(value != 0);
            fps_ = value;
        }

        void setTitle(std::string const & value) {
            if (title_ != value) {
                title_ = value;
                updateTitle(value);
            }
        }

        void send(char const * buffer, size_t size) {
            pty_->send(buffer, size);
            // TODO check errors
        }

        void send(std::string const & str) {
            send(str.c_str(), str.size());
        }

        /** When terminal size is updated, we must resize the underlying buffer and the PTY.
         */
        void updateSize(int width, int height) override {
            {
                Buffer::Ptr b = buffer(true); // grab priority lock
                b->resize(width, height, this);
            }
            resizeHistory(width);
            pty_->resize(width, height);
            // resize the client canvas
            setClientArea(width, buffer_.rows() + static_cast<int>(history_.size())); 
            ScrollBox::updateSize(width, height);
            Widget::updateSize(width, height);
        }

        void updateFocused(bool value) override {
            Widget::updateFocused(value);
            repaint();
        }

        // mouse events to deal with the selection

        void mouseDown(int col, int row, MouseButton button, Key modifiers) override;
        void mouseUp(int col, int row, MouseButton button, Key modifiers) override;

        void mouseWheel(int col, int row, int by, Key modifiers) override {
            MARK_AS_UNUSED(col);
            MARK_AS_UNUSED(row);
            MARK_AS_UNUSED(modifiers);
            if (scrollable_ && ! history_.empty()) {
                scrollBy(Point{0,-by});
                repaint();
            } 
            Widget::mouseWheel(col, row, by, modifiers);
        }

        void mouseMove(int col, int row, Key modifiers) override;

        void mouseOut() override {
            if (scrollBarActive_) {
                scrollBarActive_ = false;
                repaint();
            }
            SelectionOwner::mouseOut();
        }

        /** When a keydown is pressed, the terminal prompt is scrolled in view automatically. 
         */
        void keyDown(Key key) override {
            MARK_AS_UNUSED(key);
            scrollToPrompt();
        }


        /** Updates the selection when autoscroll is activated. 
         */
        void autoScrollStep() override {
            Point m = getMouseCoordinates();
            selectionUpdate(m.x, m.y);
        }

        /** Returns the contents of the selection. 
         
            Trims the right of each line.
         */
        std::string getSelectionContents() override;

        void selectionUpdated() override {
            repaint();
        }

        void repaint() override {
            repaint_ = true;
        }

        // terminal interface


        /** Called by the terminal when new input data is available from the pty. 
         
            The method should obtain locked buffer and update it according to the received data. 
         */
        virtual size_t processInput(char * buffer, size_t bufferSize) = 0;

        /** Called when the attached PTY has terminated. 
         */
        virtual void ptyTerminated(helpers::ExitCode exitCode) {
            trigger(onPTYTerminated, exitCode);
        }

        virtual void updateTitle(std::string const & title) {
            trigger(onTitleChange, title);
        }

        virtual void notify() {
            trigger(onNotification);
        }

        /** Appends the selected top lines to the terminal history. 
         */
        virtual void lineScrolledOut(Cell * line, int cols);

        /** Updates the client rectangle based on the history size and terminal height. 
         */
        void updateClientRect() {
            setClientArea(width(), buffer_.rows() + static_cast<int>(history_.size())); 
        }

        /** Enables or disables showing the terminal history. 
         */
        void enableScrolling(bool value = true) {
            if (scrollable_ != value) {
                scrollable_ = value;
            }
        }

        /** Removes from the terminal history the oldest line.
         */
        void popHistoryLine();

        /** Adds new line to the terminal history. 
         
            The method assumes the buffer lock to be valid and that its argument is a pointer to the array of cells of the line, i.e. that its width is the buffer's number of cells. It then trims all empty cells on the right and copies the rest to the line.
         */
        void addHistoryLine(Cell const * line, int cols);

        void resizeHistory(int newCols);

        /* Cells and cursor. */
        Buffer buffer_;

    private:
    
        /* Pseudoterminal for communication */
        PTY * pty_;

        /* Thread which reads data from the attached pseudoterminal. */
        std::thread ptyReader_;

        /* Thread which waits for the attached pty to be terminated. */
        std::thread ptyListener_;

        /* The requested number of redraws */
        volatile unsigned fps_;
        std::thread repainter_;
        std::atomic<bool> repaint_;

        /* Terminal title */
        std::string title_;

        /** Determines whethet the terminal is scrollable, or not. 
         */
        bool scrollable_;

        /** Determines if the history scrollbar is active or not (mouse over it)
         */
        bool scrollBarActive_;

        size_t historySizeLimit_;

        /* Historic lines, for each line we remember its size and the actual cells.
         */
        std::deque<std::pair<int, Cell *>> history_;

    }; // ui::Terminal

    template<>
    inline void Canvas::copyBuffer<Terminal::Buffer>(int x, int y, Terminal::Buffer const & buffer) {
        int xe = std::min(x + buffer.cols(), width()) - x;
        int ye = std::min(y + buffer.rows(), height()) - y;
        for (int by = 0; by < ye; ++by)
            for (int bx = 0; bx < xe; ++bx)
                if (Cell * c = at(Point(x + bx, y + by))) 
                    *c = buffer.at(bx, by);
    }

    PROPERTY_BUILDER(HistorySizeLimit, size_t, setHistorySizeLimit, Terminal);

    EVENT_BUILDER(OnTitleChange, StringEvent, onTitleChange, Terminal);
    EVENT_BUILDER(OnNotification, VoidEvent, onNotification, Terminal);
    EVENT_BUILDER(OnPTYTerminated, ExitCodeEvent, onPTYTerminated, Terminal);
    EVENT_BUILDER(OnLineScrolledOut, VoidEvent, onLineScrolledOut, Terminal);
    EVENT_BUILDER(OnInput, InputProcessedEvent, onInput, Terminal);
    EVENT_BUILDER(OnInputError, InputErrorEvent, onInputError, Terminal);

} // namespace ui