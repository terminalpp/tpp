#include "helpers/helpers.h"
#ifdef ARCH_UNIX

#include "helpers/helpers.h"
#include "helpers/log.h"

#include "../session.h"

#include "x11_application.h"

#include "x11_terminal_window.h"

namespace tpp {

	/** The statically generated icon description stored in an array so that in can be part of the executable. 
	
	    To change its contents, run the `icons` build target. 
	 */
	extern unsigned long tppIcon[];

    std::unordered_map<Window, X11TerminalWindow *> X11TerminalWindow::Windows_;

	// http://math.msu.su/~vvb/2course/Borisenko/CppProjects/GWindow/xintro.html
	// https://keithp.com/~keithp/talks/xtc2001/paper/
    // https://en.wikibooks.org/wiki/Guide_to_X11/Fonts
	// https://keithp.com/~keithp/render/Xft.tutorial


	X11TerminalWindow::X11TerminalWindow(Session * session, Properties const& properties, std::string const& title) :
		TerminalWindow(session, properties, title),
		display_(app()->xDisplay()),
		screen_(app()->xScreen()),
	    visual_(DefaultVisual(display_, screen_)),
	    colorMap_(DefaultColormap(display_, screen_)),
		ic_(nullptr),
	    buffer_(0),
	    draw_(nullptr),
        text_(nullptr),
        textSize_(0),
        textBlink_(false),
        textUnderline_(false),
        textStrikethrough_(false) {
		unsigned long black = BlackPixel(display_, screen_);	/* get color black */
		unsigned long white = WhitePixel(display_, screen_);  /* get color white */
        Window parent = RootWindow(display_, screen_);

		window_ = XCreateSimpleWindow(display_, parent, 0, 0, widthPx_, heightPx_, 1, white, black);

		// from http://math.msu.su/~vvb/2course/Borisenko/CppProjects/GWindow/xintro.html

		/* here is where some properties of the window can be set.
			   The third and fourth items indicate the name which appears
			   at the top of the window and the name of the minimized window
			   respectively.
			*/
		XSetStandardProperties(display_, window_, title_.c_str(), nullptr, x11::None, nullptr, 0, nullptr);

		/* this routine determines which types of input are allowed in
		   the input.  see the appropriate section for details...
		*/
		XSelectInput(display_, window_, ButtonPressMask | ButtonReleaseMask | PointerMotionMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask | VisibilityChangeMask | ExposureMask | FocusChangeMask);

		/* X11 in itself does not deal with window close requests, but this enables sending of the WM_DELETE_WINDOW message when the close button is send and the application can decide what to do instead. 

		   The message is received as a client message with the wmDeleteMessage_ atom in its first long payload.
		 */
		XSetWMProtocols(display_, window_, & app()->wmDeleteMessage_, 1);

        XGCValues gcv;
        memset(&gcv, 0, sizeof(XGCValues));
    	gcv.graphics_exposures = False;
        gc_ = XCreateGC(display_, parent, GCGraphicsExposures, &gcv);

		// only create input context if XIM is present
		if (app()->xIm_ != nullptr) {
			// create input context for the window... The extra arguments to the XCreateIC are c-c c-v from the internet and for now are a mystery to me
			ic_ = XCreateIC(app()->xIm_, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
				XNClientWindow, window_, XNFocusWindow, window_, nullptr);
		}

        updateTextStructures(widthPx_, cellWidthPx_);

        // set the icon
		setIcon(tppIcon);

		// register the window
        Windows_[window_] = this;
	}

	void X11TerminalWindow::show() {
        XMapWindow(display_, window_);
		//XMapRaised(display_, window_);
	}

	X11TerminalWindow::~X11TerminalWindow() {
        Windows_.erase(window_);
		XFreeGC(display_, gc_);
	}

    /* From: https://www.tonyobryan.com//index.php?article=9
     */
	void X11TerminalWindow::doSetFullscreen(bool value) {
		MotifHints hints;
		hints.flags = 2;
		if (value == true) {
            // get window size & position
            XWindowAttributes attrs;
            Window childW; 
			XGetWindowAttributes(display_, window_, & attrs);
            XTranslateCoordinates(display_, window_, DefaultRootWindow(display_), 0, 0, & fullscreenRestore_.x, & fullscreenRestore_.y, &childW);
			hints.decorations = 0;
            fullscreenRestore_.width = attrs.width;
            fullscreenRestore_.height = attrs.height;
            fullscreenRestore_.x -= attrs.x;
            fullscreenRestore_.y -= attrs.y;
            // remove the decorations
            XChangeProperty(display_, window_, app()->motifWmHints_, app()->motifWmHints_, 32, PropModeReplace, (unsigned char*)& hints, 5);
            // update window size and position
            Screen * screen = XScreenOfDisplay(display_, XDefaultScreen(display_));
    		XMoveResizeWindow(display_, window_, 0, 0, XWidthOfScreen(screen), XHeightOfScreen(screen));
		} else {
			hints.decorations = 1;
            XChangeProperty(display_, window_, app()->motifWmHints_, app()->motifWmHints_, 32, PropModeReplace, (unsigned char*)& hints, 5);
    		XMoveResizeWindow(
                display_,
                window_,
                fullscreenRestore_.x,
                fullscreenRestore_.y,
                fullscreenRestore_.width,
                fullscreenRestore_.height
            );
		}
		XMapWindow(display_, window_);
	}

	void X11TerminalWindow::titleChange(vterm::Terminal::TitleChangeEvent & e) {
		XSetStandardProperties(display_, window_, e->c_str(), nullptr, x11::None, nullptr, 0, nullptr);
        TerminalWindow::titleChange(e);
	}

    void X11TerminalWindow::clipboardUpdate(vterm::Terminal::ClipboardUpdateEvent& e) {
		Application::Instance<X11Application>()->clipboard_ = *e;
		XSetSelectionOwner(display_, app()->clipboardName_, window_, CurrentTime);
	}

    void X11TerminalWindow::selectionClear(bool manual) {
        TerminalWindow::selectionClear();
        // only set selection owner to none if the selection is cleared manually
        if (manual)
            XSetSelectionOwner(display_, app()->primaryName_, x11::None, CurrentTime);
    }

    void X11TerminalWindow::selectionSet() {
        TerminalWindow::selectionSet();
        // notify X server that we hold primary selection now
        XSetSelectionOwner(display_, app()->primaryName_, window_, CurrentTime);
    }

    bool X11TerminalWindow::selectionPaste() {
        // if the selection belongs to the current window, there is no need to consult X, otherwise obtain the PRIMARY selection from the X server
        if (! TerminalWindow::selectionPaste()) 
            XConvertSelection(display_, app()->primaryName_, app()->formatStringUTF8_, app()->primaryName_, window_, CurrentTime);
        return true;
    }

	void X11TerminalWindow::clipboardPaste() {
		XConvertSelection(display_, app()->clipboardName_, app()->formatStringUTF8_, app()->clipboardName_, window_, CurrentTime);
	}

	unsigned X11TerminalWindow::doPaint() {
        std::lock_guard<std::mutex> g(drawGuard_);
		ASSERT(draw_ == nullptr);
		bool forceDirty = false;
		if (buffer_ == 0) {
			buffer_ = XCreatePixmap(display_, window_, widthPx_, heightPx_, DefaultDepth(display_, screen_));
			ASSERT(buffer_ != 0);
			forceDirty = true;
		}
		draw_ = XftDrawCreate(display_, buffer_, visual_, colorMap_);
		unsigned numCells = drawBuffer(forceDirty);
        // draw any remaining cells
        drawText();
		// first clear the borders that won't be used (don't clear the whole window to prevent flicker)
        unsigned marginRight = widthPx_ % cellWidthPx_;
        unsigned marginBottom = heightPx_ % cellHeightPx_;
        if (marginRight != 0) 
            XClearArea(display_, window_, widthPx_ - marginRight, 0, marginRight, heightPx_, false);
        if (marginBottom != 0)
            XClearArea(display_, window_, 0, heightPx_ - marginBottom, widthPx_, marginBottom, false);
        // now bitblt the buffer
		XCopyArea(display_, buffer_, window_, gc_, 0, 0, widthPx_, heightPx_, 0, 0);
		XftDrawDestroy(draw_);
		draw_ = nullptr;
        XFlush(display_);
		return numCells;
	}

	void X11TerminalWindow::setIcon(unsigned long* icon) {
		XChangeProperty(
			display_,
			window_,
			Application::Instance<X11Application>()->netWmIcon_,
			XA_CARDINAL,
			32,
			PropModeReplace,
			reinterpret_cast<unsigned char*>(&icon[1]),
			icon[0]
		);
	}

	unsigned X11TerminalWindow::GetStateModifiers(int state) {
		unsigned modifiers = 0;
		if (state & 1)
			modifiers += vterm::Key::Shift;
		if (state & 4)
			modifiers += vterm::Key::Ctrl;
		if (state & 8)
			modifiers += vterm::Key::Alt;
		if (state & 64)
			modifiers += vterm::Key::Win;
		return modifiers;
	}
	
	vterm::Key X11TerminalWindow::GetKey(KeySym k, unsigned modifiers, bool pressed) {
        if (k >= 'a' && k <= 'z') 
            return vterm::Key(k - 32, modifiers);
        if (k >= 'A' && k <= 'Z')
            return vterm::Key(k, modifiers);
        if (k >= '0' && k <= '9')
            return vterm::Key(k, modifiers);
        // numpad
        if (k >= XK_0 && k <= XK_9)
            return vterm::Key(vterm::Key::Numpad0 + k - XK_0, modifiers);
        // fn keys
        if (k >= XK_F1 && k <= XK_F12)
            return vterm::Key(vterm::Key::F1 + k - XK_F1, modifiers);
        // others
        switch (k) {
            case XK_BackSpace:
                return vterm::Key(vterm::Key::Backspace, modifiers);
            case XK_Tab:
                return vterm::Key(vterm::Key::Tab, modifiers);
            case XK_Return:
                return vterm::Key(vterm::Key::Enter, modifiers);
            case XK_Caps_Lock:
                return vterm::Key(vterm::Key::CapsLock, modifiers);
            case XK_Escape:
                return vterm::Key(vterm::Key::Esc, modifiers);
            case XK_space:
                return vterm::Key(vterm::Key::Space, modifiers);
            case XK_Page_Up:
                return vterm::Key(vterm::Key::PageUp, modifiers);
            case XK_Page_Down:
                return vterm::Key(vterm::Key::PageDown, modifiers);
            case XK_End:
                return vterm::Key(vterm::Key::End, modifiers);
            case XK_Home:
                return vterm::Key(vterm::Key::Home, modifiers);
            case XK_Left:
                return vterm::Key(vterm::Key::Left, modifiers);
            case XK_Up:
                return vterm::Key(vterm::Key::Up, modifiers);
            case XK_Right:
                return vterm::Key(vterm::Key::Right, modifiers);
            case XK_Down:
                return vterm::Key(vterm::Key::Down, modifiers);
            case XK_Insert:
                return vterm::Key(vterm::Key::Insert, modifiers);
            case XK_Delete:
                return vterm::Key(vterm::Key::Delete, modifiers);
            case XK_Menu:
                return vterm::Key(vterm::Key::Menu, modifiers);
            case XK_KP_Multiply:
                return vterm::Key(vterm::Key::NumpadMul, modifiers);
            case XK_KP_Add:
                return vterm::Key(vterm::Key::NumpadAdd, modifiers);
            case XK_KP_Separator:
                return vterm::Key(vterm::Key::NumpadComma, modifiers);
            case XK_KP_Subtract:
                return vterm::Key(vterm::Key::NumpadSub, modifiers);
            case XK_KP_Decimal:
                return vterm::Key(vterm::Key::NumpadDot, modifiers);
            case XK_KP_Divide:
                return vterm::Key(vterm::Key::NumpadDiv, modifiers);
            case XK_Num_Lock:
                return vterm::Key(vterm::Key::NumLock, modifiers);
            case XK_Scroll_Lock:
                return vterm::Key(vterm::Key::ScrollLock, modifiers);
            case XK_semicolon:
                return vterm::Key(vterm::Key::Semicolon, modifiers);
            case XK_equal:
                return vterm::Key(vterm::Key::Equals, modifiers);
            case XK_comma:
                return vterm::Key(vterm::Key::Comma, modifiers);
            case XK_minus:
                return vterm::Key(vterm::Key::Minus, modifiers);
            case XK_period: // .
                return vterm::Key(vterm::Key::Dot, modifiers);
            case XK_slash:
                return vterm::Key(vterm::Key::Slash, modifiers);
            case XK_grave: // `
                return vterm::Key(vterm::Key::Tick, modifiers);
            case XK_bracketleft: // [
                return vterm::Key(vterm::Key::SquareOpen, modifiers);
            case XK_backslash:  
                return vterm::Key(vterm::Key::Backslash, modifiers);
            case XK_bracketright: // ]
                return vterm::Key(vterm::Key::SquareClose, modifiers);
            case XK_apostrophe: // '
                return vterm::Key(vterm::Key::Quote, modifiers);
			case XK_Shift_L:
			case XK_Shift_R:
				if (pressed)
					modifiers |= vterm::Key::Shift;
				else
					modifiers &= ~vterm::Key::Shift;
				return vterm::Key(vterm::Key::ShiftKey, modifiers);
			case XK_Control_L:
			case XK_Control_R:
				if (pressed)
					modifiers |= vterm::Key::Ctrl;
				else
					modifiers &= ~vterm::Key::Ctrl;
				return vterm::Key(vterm::Key::CtrlKey, modifiers);
			case XK_Alt_L:
			case XK_Alt_R:
				if (pressed)
					modifiers |= vterm::Key::Alt;
				else
					modifiers &= ~vterm::Key::Alt;
				return vterm::Key(vterm::Key::AltKey, modifiers);
			case XK_Meta_L:
			case XK_Meta_R:
				if (pressed)
					modifiers |= vterm::Key::Win;
				else
					modifiers &= ~vterm::Key::Win;
				return vterm::Key(vterm::Key::WinKey, modifiers);
			default:
                return vterm::Key(vterm::Key::Invalid, 0);
        }
    }

    void X11TerminalWindow::EventHandler(XEvent & e) {
        X11TerminalWindow * tw = nullptr;
        auto i = Windows_.find(e.xany.window);
        if (i != Windows_.end())
            tw = i->second;
        switch(e.type) {
            /* Handles repaint event when window is shown or a repaint was triggered. 
             */
            case Expose: 
                if (e.xexpose.count != 0)
                    break;
                tw->paint();
                break;
			/** Handles when the window gets focus. 
			 */
			case FocusIn:
				if (e.xfocus.mode == NotifyGrab || e.xfocus.mode == NotifyUngrab)
					break;
				ASSERT(tw != nullptr);
				tw->focusChangeMessageReceived(true);
				break;
			/** Handles when the window loses focus. 
			 */
			case FocusOut:
				if (e.xfocus.mode == NotifyGrab || e.xfocus.mode == NotifyUngrab)
					break;
				ASSERT(tw != nullptr);
				tw->focusChangeMessageReceived(false);
				break;
            /* Handles window resize, which should change the terminal size accordingly. 
             */  
            case ConfigureNotify: {
                if (tw->widthPx_ != static_cast<unsigned>(e.xconfigure.width) || tw->heightPx_ != static_cast<unsigned>(e.xconfigure.height))
                    tw->windowResized(e.xconfigure.width, e.xconfigure.height);
                break;
            }
            case MapNotify:
                break;
            /* Unlike Win32 we have to determine whether we are dealing with sendChar, or keyDown. 
             */
            case KeyPress: {
				unsigned modifiers = GetStateModifiers(e.xkey.state);
				tw->activeModifiers_ = vterm::Key(vterm::Key::Invalid, modifiers);
				KeySym kSym;
                char str[32];
                Status status;
				int strLen = 0;
				if (tw->ic_ != nullptr)
					strLen = Xutf8LookupString(tw->ic_, &e.xkey, str, sizeof str, &kSym, &status);
				else
					strLen = XLookupString(&e.xkey, str, sizeof str, &kSym, nullptr);
                // if it is printable character and there were no modifiers other than shift pressed, we are dealing with printable character (backspace is not printable character)
                if (strLen > 0 && (str[0] < 0 || str[0] >= 0x20) && (e.xkey.state & 0x4c) == 0 && str[0] != 0x7f) {
                    char * x = reinterpret_cast<char*>(& str);
					helpers::Char const* c = helpers::Char::At(x, x + 32);
					if (c != nullptr) {
						tw->keyChar(*c);
					    break;
                    }
                }
                // otherwise if the keysym was recognized, it is a keyDown event
                vterm::Key key = GetKey(kSym, modifiers, true);
				// if the modifiers were updated (i.e. the key is Shift, Ctrl, Alt or Win, updated active modifiers
				if (modifiers != key.modifiers())
					tw->activeModifiers_ = vterm::Key(vterm::Key::Invalid, modifiers);
				if (key != vterm::Key::Invalid)
                    tw->keyDown(key);
                break;
            }
            case KeyRelease: {
				unsigned modifiers = GetStateModifiers(e.xkey.state);
				tw->activeModifiers_ = vterm::Key(vterm::Key::Invalid, modifiers);
				KeySym kSym = XLookupKeysym(& e.xkey, 0);
                vterm::Key key = GetKey(kSym, modifiers, false);
				// if the modifiers were updated (i.e. the key is Shift, Ctrl, Alt or Win, updated active modifiers
				if (modifiers != key.modifiers())
					tw->activeModifiers_ = vterm::Key(vterm::Key::Invalid, modifiers);
				if (key != vterm::Key::Invalid)
                    tw->keyUp(key);
                break;
            }
            case ButtonPress: 
				tw->activeModifiers_ = vterm::Key(vterm::Key::Invalid, GetStateModifiers(e.xbutton.state));
				switch (e.xbutton.button) {
                    case 1:
                        tw->mouseDown(e.xbutton.x, e.xbutton.y, vterm::MouseButton::Left);
                        break;
                    case 2:
                        tw->mouseDown(e.xbutton.x, e.xbutton.y, vterm::MouseButton::Wheel);
                        break;
                    case 3:
                        tw->mouseDown(e.xbutton.x, e.xbutton.y, vterm::MouseButton::Right);
                        break;
                    case 4:
                        tw->mouseWheel(e.xbutton.x, e.xbutton.y, 1);
                        break;
                    case 5:
                        tw->mouseWheel(e.xbutton.x, e.xbutton.y, -1);
                        break;
                    default:
                        break;
                }
                break;
            case ButtonRelease: 
				tw->activeModifiers_ = vterm::Key(vterm::Key::Invalid, GetStateModifiers(e.xbutton.state));
				switch (e.xbutton.button) {
                    case 1:
                        tw->mouseUp(e.xbutton.x, e.xbutton.y, vterm::MouseButton::Left);
                        break;
                    case 2:
                        tw->mouseUp(e.xbutton.x, e.xbutton.y, vterm::MouseButton::Wheel);
                        break;
                    case 3:
                        tw->mouseUp(e.xbutton.x, e.xbutton.y, vterm::MouseButton::Right);
                        break;
                    default:
                        break;
                }
                break;
            case MotionNotify:
				tw->activeModifiers_ = vterm::Key(vterm::Key::Invalid, GetStateModifiers(e.xbutton.state));
				tw->mouseMove(e.xmotion.x, e.xmotion.y);
                break;
			/** Called when we are notified that clipboard contents is available for previously requested paste.
			
			    Get the clipboard contents and call terminal's paste event. 
			 */
			case SelectionNotify:
				if (e.xselection.property) {
					char * result;
					unsigned long resSize, resTail;
					Atom type = x11::None;
					int format = 0;
					XGetWindowProperty(tw->display_, tw->window_, e.xselection.property, 0, LONG_MAX / 4, False, AnyPropertyType,
						&type, &format, &resSize, &resTail, (unsigned char**)& result);
					if (type == tw->app()->clipboardIncr_)
						// buffer too large, incremental reads must be implemented
						// https://stackoverflow.com/questions/27378318/c-get-string-from-clipboard-on-linux
						NOT_IMPLEMENTED;
					else
						tw->terminal()->paste(std::string(result, resSize));
					XFree(result);
                 }
				 break;
			/** Called when the clipboard contents is requested by an outside app. 
			 */
			case SelectionRequest: {
				X11Application* app = Application::Instance<X11Application>();
				XSelectionEvent response;
				response.type = SelectionNotify;
				response.requestor = e.xselectionrequest.requestor;
				response.selection = e.xselectionrequest.selection;
				response.target = e.xselectionrequest.target;
				response.time = e.xselectionrequest.time;
				// by default, the request is rejected
				response.property = x11::None; 
				// if the target is TARGETS, then all supported formats should be sent, in our case this is simple, only UTF8_STRING is supported
				if (response.target == app->formatTargets_) {
					XChangeProperty(
						tw->display_,
						e.xselectionrequest.requestor,
						e.xselectionrequest.property,
						e.xselectionrequest.target,
						32, // atoms are 4 bytes, so 32 bits
						PropModeReplace,
						reinterpret_cast<unsigned char const*>(&app->formatStringUTF8_),
						1
					);
					response.property = e.xselectionrequest.property;
				// otherwise, if UTF8_STRING, or a STRING is requested, we just send what we have 
				} else if (response.target == app->formatString_ || response.target == app->formatStringUTF8_) {
                    std::string clipboard = (response.selection == app->clipboardName_) ? app->clipboard_ : tw->terminal()->getText(tw->selectedArea());
					XChangeProperty(
						tw->display_,
						e.xselectionrequest.requestor,
						e.xselectionrequest.property,
						e.xselectionrequest.target,
						8, // utf-8 is encoded in chars, i.e. 8 bits
						PropModeReplace,
						reinterpret_cast<unsigned char const *>(clipboard.c_str()),
						clipboard.size()
					);
					response.property = e.xselectionrequest.property;
				}
				// send the event to the requestor
				if (!XSendEvent(
					e.xselectionrequest.display,
					e.xselectionrequest.requestor,
					1, // propagate
					0, // event mask
					reinterpret_cast<XEvent*>(&response)
				))
					LOG << "Error sending selection notify";
				break;
			}
			/** If we lose ownership, clear the clipboard contents with the application, or if we lose primary ownership, just clear the selection.   
			 */
			case SelectionClear: {
                X11Application * app = Application::Instance<X11Application>();
                if (e.xselectionclear.selection == app->clipboardName_)
    				app->clipboard_.clear();
                else 
                    tw->selectionClear(false);
				break;
            }
            case DestroyNotify:
                // delete the window object and remove it from the list of active windows
                delete i->second;
                // if it was last window, exit the terminal
                if (Windows_.empty()) {
                    throw X11Application::Terminate();
                }
                break;
			/* User-defined messages. 
			 */
			case ClientMessage:
			    if (static_cast<unsigned long>(e.xclient.data.l[0]) == tw->app()->wmDeleteMessage_) {
					ASSERT(tw != nullptr) << "Attempt to destroy unknown window";
					Session::Close(tw->session());
				}
				break;
            default:
                break;
        }
    }

} // namespace tpp

#endif