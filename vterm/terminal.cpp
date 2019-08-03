#include "terminal.h"

namespace vterm {

    // Terminal::Buffer

    Terminal::Buffer::Buffer(int cols, int rows):
        cols_{cols},
        rows_{rows},
        cells_{new Cell*[rows]} {
        for (size_t i = 0; i < rows; ++i)
            cells_[i] = new Cell[cols];
    }

    Terminal::Buffer::Buffer(Buffer const & from):
        cols_{from.cols_},
        rows_{from.rows_},
        cells_{new Cell*[from.rows_]} {
        for (size_t i = 0; i < rows_; ++i) {
            cells_[i] = new Cell[cols_];
            memcpy(cells_[i], from.cells_[i], sizeof(Cell) * cols_);
        }
    }

    Terminal::Buffer & Terminal::Buffer::operator = (Buffer const & other) {
        if (cols_ != other.cols_ || rows_ != other.rows_) {
            for (size_t i = 0; i < rows_; ++i)
                delete [] cells_[i];
            delete[] cells_;
            cols_ = other.cols_;
            rows_ = other.rows_;
            cells_ = new Cell*[rows_];
            for (size_t i = 0; i < rows_; ++i) 
                cells_[i] = new Cell[cols_];
        }
        for (size_t i = 0; i < rows_; ++i) 
            memcpy(cells_[i], other.cells_[i], sizeof(Cell) * cols_);
        return *this;
    }

    Terminal::Buffer::~Buffer() {
        for (size_t i = 0; i < rows_; ++i)
            delete [] cells_[i];
        delete [] cells_;
    }

    void Terminal::Buffer::resize(int cols, int rows) {
        if (cols_ != cols || rows_ != rows) {
            resizeCells(cols, rows);
            cols_ = cols;
            rows_ = rows;
        }
    }

    void Terminal::Buffer::insertLines(int lines, int top, int bottom, Cell const & fill) {
        ASSERT(bottom <= rows_);
        for (size_t i = 0; i < lines; ++i) {
            Cell* row = cells_[bottom - 1];
            memmove(cells_ + top + 1, cells_ + top, sizeof(Cell*) * (bottom - top - 1));
            cells_[top] = row;
            fillRow(row, fill, cols_);
        }
    }

    void Terminal::Buffer::deleteLines(int lines, int top, int bottom, Cell const & fill) {
        ASSERT(bottom <= rows_);
        ASSERT(lines <= bottom - top);
        for (size_t i = 0; i < lines; ++i) {
            Cell* row = cells_[top];
            memmove(cells_ + top, cells_ + top + 1, sizeof(Cell*) * (bottom - top - 1));
            cells_[bottom - 1] = row;
            fillRow(row, fill, cols_);
        }
    }

    void Terminal::Buffer::fillRow(Cell* row, Cell const& fill, unsigned cols) {
        row[0] = fill;
        size_t i = 1;
        size_t next = 2;
        while (next < cols) {
            memcpy(row + i, row, sizeof(Cell) * i);
            i = next;
            next *= 2;
        }
        memcpy(row + i, row, sizeof(Cell) * (cols - i));
    }

	void Terminal::Buffer::resizeCells(int newCols, int newRows) {
		// create the new cells 
		Cell** newCells = new Cell * [newRows];
		for (size_t i = 0; i < newRows; ++i)
			newCells[i] = new Cell[newCols];
		// now determine the row at which we should stop - this is done by going back from cursor's position until we hit end of line, that would be the last line we will use
		int stopRow = cursor_.row - 1;
		while (stopRow < rows_) {
			Cell* row = cells_[stopRow];
			int i = 0;
			for (; i < cols_; ++i)
				if (row[i].attributes().endOfLine())
					break;
			// we have found the line end
			if (i < cols_) {
				++stopRow; // stop after current row
				break;
			}
			// otherwise try the above line
			--stopRow;
		}
		// if not found, or out of bounds, we'll stop immediately
		if (stopRow > rows_)
			stopRow = 0;
		// now transfer the contents
		int oldCursorRow = cursor_.row;
		cursor_.col = 0;
		cursor_.row = 0;
		for (int y = 0; y < stopRow; ++y) {
			for (int x = 0; x < cols_; ++x) {
				Cell& cell = cells_[y][x];
				newCells[cursor_.row][cursor_.col] = cell;
				// if the copied cell is end of line, or if we are at the end of new line, increase the cursor row
				if (cell.attributes().endOfLine() || ++cursor_.col == newCols) {
					++cursor_.row;
					cursor_.col = 0;
				}
				// scroll the new lines if necessary
				if (cursor_.row == newRows) {
					Cell* r = newCells[0];
					memmove(newCells, newCells + 1, sizeof(Cell*) * (newRows - 1));
					newCells[newRows - 1] = r;
					fillRow(r, Cell(), newCols);
					--cursor_.row;
				}
				// if it was new line, skip whatever was afterwards
				if (cell.attributes().endOfLine())
					break;
			}
		}
		// the contents was transferred, delete the old cells, replace them with the new ones
		for (size_t i = 0; i < rows_; ++i)
			delete[] cells_[i];
		delete[] cells_;
		cells_ = newCells;
		// because the first thing the app will do after resize is to adjust the cursor position if the current line span more than 1 terminal line, we must account for this and update cursor position
		cursor_.row += (oldCursorRow - stopRow);
	}




    // Terminal 

    Terminal::Terminal(int x, int y, int width, int height, PTY * pty, size_t ptyBufferSize):
        ui::Widget(x, y, width, height),
        buffer_(width, height),
        pty_(pty) {
        pty_->resize(width, height);
        ptyReader_ = std::thread([this, ptyBufferSize](){
            std::unique_ptr<char> holder(new char[ptyBufferSize]);
            char * ptyBuffer = holder.get();
            char * writeStart = holder.get();
            while (true) {
                size_t read = pty_->receive(writeStart, ptyBufferSize - (writeStart - ptyBuffer));
				// if 0 bytes were read, terminate the thread
                if (read == 0)
                    break;
				// otherwise add any pending data from previous cycle
                read += (writeStart - ptyBuffer);
                // process the input
                size_t processed = processInput(ptyBuffer, read);
                // if not everything was processed, copy the unprocessed part at the beginning and set writeStart_ accordingly
                if (processed != read) {
                    memcpy(ptyBuffer, ptyBuffer + processed, read - processed);
                    writeStart = ptyBuffer + read - processed;
                } else {
                    writeStart = ptyBuffer;
                }
            }
        });
        ptyListener_ = std::thread([this](){
            helpers::ExitCode ec = pty_->waitFor();
            this->ptyTerminated(ec);
        });
    }

    void Terminal::paint(ui::Canvas & canvas) {
        Buffer::Ptr buffer = this->buffer(/* priority */true);
        MARK_AS_UNUSED(canvas);
        NOT_IMPLEMENTED;

    }

} // namespace vterm