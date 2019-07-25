#include "widget.h"
#include "root_window.h"

namespace ui {


	void Widget::repaint() {
		// only repaint the control if it is visible
		if (visible_ && visibleRegion_.isValid()) {
			// if the widget is overlaid, the parent must be repainted
			if (overlay_) {
				ASSERT(parent_ != nullptr);
				parent_->repaint();
			// otherwise repainting the widget is enough
			} else {
				{
					vterm::Terminal::ScreenLock screenLock = visibleRegion_.root->lockScreen();
					Canvas canvas(visibleRegion_, *screenLock, width_, height_);
					paint(canvas);
				}
				// trigger repaint
				visibleRegion_.root->terminalRepaint();
			}
		} 
	}

	void Widget::paintChild(Widget * child, Canvas& canvas) {
		if (!child->visible_)
			return;
		if (!child->visibleRegion_.isValid()) {
			Canvas childCanvas(canvas, child->x_, child->y_, child->width_, child->height_);
			child->visibleRegion_ = childCanvas.visibleRegion_;
			child->paint(childCanvas);
		} else {
			Canvas childCanvas(child->visibleRegion_, canvas.screen_, child->width_, child->height_);
			child->paint(childCanvas);
		}
		// trigger repaint
		visibleRegion_.root->terminalRepaint();
	}

} // namespace ui