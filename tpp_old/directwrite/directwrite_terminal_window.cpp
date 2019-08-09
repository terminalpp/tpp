#ifdef ARCH_WINDOWS

#include "helpers/string.h"

#include "../session.h"

#include "directwrite_application.h"

#include "directwrite_terminal_window.h"



namespace tpp {

	std::unordered_map<HWND, DirectWriteTerminalWindow*> DirectWriteTerminalWindow::Windows_;

	DirectWriteTerminalWindow::DirectWriteTerminalWindow(Session* session, Properties const& properties, std::string const& title) :
		TerminalWindow(session, properties, title),
		glyphIndices_(nullptr),
		glyphAdvances_(nullptr),
		glyphOffsets_(nullptr),
		dwFont_(nullptr),
		wndPlacement_{ sizeof(wndPlacement_) },
		frameWidth_{ 0 },
		frameHeight_{ 0 },
        grBlink_(false),
	    grUnderline_(false),
	    grStrikethrough_(false) {
		// all win32 windows start focused since they receive the setfocus message first
		focused_ = true;
		helpers::utf16_string t = helpers::UTF8toUTF16(title_);
		hWnd_ = CreateWindowExW(
			WS_EX_LEFT, // the default
			app()->TerminalWindowClassName_, // window class
			// ok, on windows wchar_t and char16_t are the same (see helpers/char.h)
			t.c_str(), // window name (all start as terminal++)
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, // x position
			CW_USEDEFAULT, // y position
			widthPx_,
			heightPx_,
			nullptr, // handle to parent
			nullptr, // handle to menu 
			app()->hInstance_, // module handle
			this // lParam for WM_CREATE message
		);
		ASSERT(hWnd_ != 0) << "Cannot create window : " << GetLastError();

		D2D1_SIZE_U size = D2D1::SizeU(widthPx_, heightPx_);
		OSCHECK(SUCCEEDED(app()->d2dFactory_->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(
				hWnd_,
				size
			),
			&rt_
		)));
		rt_->SetTransform(D2D1::IdentityMatrix());

		OSCHECK(SUCCEEDED(rt_->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF::White),
			&fg_
		)));
		OSCHECK(SUCCEEDED(rt_->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF::Black),
			&bg_
		)));
		ZeroMemory(&glyphRun_, sizeof(DWRITE_GLYPH_RUN));
		updateGlyphRunStructures(widthPx_, cellWidthPx_);

		Windows_.insert(std::make_pair(hWnd_, this));
	}

	DirectWriteTerminalWindow::~DirectWriteTerminalWindow() {
		Windows_.erase(hWnd_);
		delete[] glyphIndices_;
		delete[] glyphAdvances_;
		delete[] glyphOffsets_;
	}

	/** Basically taken from:

		https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
	 */
	void DirectWriteTerminalWindow::doSetFullscreen(bool value) {
		DWORD style = GetWindowLong(hWnd_, GWL_STYLE);
		if (value == true) {
			MONITORINFO mInfo = { sizeof(mInfo) };
			if (GetWindowPlacement(hWnd_, &wndPlacement_) &&
				GetMonitorInfo(MonitorFromWindow(hWnd_, MONITOR_DEFAULTTOPRIMARY), &mInfo)) {
				SetWindowLong(hWnd_, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
				int width = mInfo.rcMonitor.right - mInfo.rcMonitor.left;
				int height = mInfo.rcMonitor.bottom - mInfo.rcMonitor.top;
				SetWindowPos(hWnd_, HWND_TOP, mInfo.rcMonitor.left, mInfo.rcMonitor.top, width, height, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			} else {
				// we are not actually fullscreen
				fullscreen_ = false;
				LOG("Win32") << "Unable to enter fullscreen mode";
			}
		} else {
			SetWindowLong(hWnd_, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
			SetWindowPlacement(hWnd_, &wndPlacement_);
			SetWindowPos(hWnd_, nullptr, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	}

	void DirectWriteTerminalWindow::titleChange(vterm::Terminal::TitleChangeEvent& e) {
		title_ = *e;
		PostMessage(hWnd_, WM_USER, DirectWriteApplication::MSG_TITLE_CHANGE, 0);
	}

	void DirectWriteTerminalWindow::focusChanged(bool focused) {
		// if the window has been focused, we need to update the activeModifiers. 
		if (focused) {
			unsigned shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? vterm::Key::Shift : 0;
			unsigned ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? vterm::Key::Ctrl : 0;
			unsigned alt = (GetAsyncKeyState(VK_MENU) & 0x8000) ? vterm::Key::Alt : 0;
			unsigned win = (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000) ? vterm::Key::Win : 0;
			activeModifiers_ = vterm::Key(vterm::Key::Invalid, shift | ctrl | alt | win);
		}
		TerminalWindow::focusChanged(focused);
	}


	void DirectWriteTerminalWindow::clipboardUpdate(vterm::Terminal::ClipboardUpdateEvent& e) {
		if (OpenClipboard(nullptr)) {
			EmptyClipboard();
			// encode the string into UTF16 and get the size of the data we need
			// ok, on windows wchar_t and char16_t are the same (see helpers/char.h)
			helpers::utf16_string str = helpers::UTF8toUTF16(*e);
			// the str is null-terminated
			size_t size = (str.size() + 1) * 2;
			HGLOBAL clipboard = GlobalAlloc(0, size);
			if (clipboard) {
				WCHAR* data = reinterpret_cast<WCHAR*>(GlobalLock(clipboard));
				if (data) {
					memcpy(data, str.c_str(), size);
					GlobalUnlock(clipboard);
					SetClipboardData(CF_UNICODETEXT, clipboard);
				}
			}
			CloseClipboard();
		}
	}

	void DirectWriteTerminalWindow::clipboardPaste() {
		if (OpenClipboard(nullptr)) {
			HANDLE clipboard = GetClipboardData(CF_UNICODETEXT);
			if (clipboard) {
				// ok, on windows wchar_t and char16_t are the same (see helpers/char.h)
				helpers::utf16_char* data = reinterpret_cast<helpers::utf16_char*>(GlobalLock(clipboard));
				if (data) {
					std::string str(helpers::UTF16toUTF8(data));
					GlobalUnlock(clipboard);
					if (!str.empty())
						terminal()->paste(str);
				}
			}
			CloseClipboard();
		}
	}

	unsigned DirectWriteTerminalWindow::doPaint() {
		rt_->BeginDraw();
		unsigned numCells = drawBuffer(false);
		drawGlyphRun();
		rt_->EndDraw();
		return numCells;
	}

	// https://docs.microsoft.com/en-us/windows/desktop/inputdev/virtual-key-codes
	vterm::Key DirectWriteTerminalWindow::GetKey(unsigned vk) {
		// we don't distinguish between left and right win keys
		if (vk == VK_RWIN)
			vk = VK_LWIN;
		if (!vterm::Key::IsValidCode(vk))
			return vterm::Key(vterm::Key::Invalid);
		// MSB == pressed, LSB state since last time
		unsigned shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? vterm::Key::Shift : 0;
		unsigned ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? vterm::Key::Ctrl : 0;
		unsigned alt = (GetAsyncKeyState(VK_MENU) & 0x8000) ? vterm::Key::Alt : 0;
		unsigned win = (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000) ? vterm::Key::Win : 0;

		return vterm::Key(vk, shift | ctrl | alt | win);
	}

	LRESULT CALLBACK DirectWriteTerminalWindow::EventHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		// determine terminal window corresponding to the handle given with the message
		auto i = Windows_.find(hWnd);
		DirectWriteTerminalWindow* tw = i == Windows_.end() ? nullptr : i->second;
		// do the message
		switch (msg) {
			/** Closes the current window. */
			case WM_CLOSE: {
				ASSERT(tw != nullptr) << "Unknown window";
				Session::Close(tw->session());
				break;
			}
			/** Destroys the window, if it is the last window, quits the app. */
			case WM_DESTROY: {
				ASSERT(tw != nullptr) << "Attempt to destroy unknown window";
				// delete the window object and remove it from the list of active windows
				delete i->second;
				// if it was last window, terminate the application
				if (Windows_.empty())
					PostQuitMessage(0);
				break;
			}
			/* When the window is created, the border width and height of a terminal window is determined and the window's size is updated to adjust for it. */
			case WM_CREATE: {
				// now calculate the border and actually update the window size to account for it
				CREATESTRUCT& cs = *reinterpret_cast<CREATESTRUCT*>(lParam);
				// get the tw member from the create struct argument
				ASSERT(tw == nullptr);
				tw = reinterpret_cast<DirectWriteTerminalWindow*>(cs.lpCreateParams);
				ASSERT(tw != nullptr);
				RECT r;
				r.left = cs.x;
				r.right = cs.x + cs.cx;
				r.top = cs.y;
				r.bottom = cs.y + cs.cy;
				AdjustWindowRectEx(&r, cs.style, false, cs.dwExStyle);
				unsigned fw = r.right - r.left - cs.cx;
				unsigned fh = r.bottom - r.top - cs.cy;
				if (fw != 0 || fh != 0) {
					tw->frameWidth_ = fw;
					tw->frameHeight_ = fh;
					SetWindowPos(hWnd, HWND_TOP, cs.x, cs.y, cs.cx + fw, cs.cy + fh, SWP_NOZORDER);
				}
				break;
			}
			/** Window gains focus. 
			 */
			case WM_SETFOCUS:
				if (tw != nullptr)
    				tw->focusChangeMessageReceived(true);
				break;
			/** Window loses focus. 
			 */
			case WM_KILLFOCUS:
				ASSERT(tw != nullptr);
				tw->focusChangeMessageReceived(false);
				break;
			/* Called when the window is resized interactively by the user. Makes sure that the window size snaps to discrete terminal sizes. */
			case WM_SIZING: {
				RECT* winRect = reinterpret_cast<RECT*>(lParam);
				switch (wParam) {
				case WMSZ_BOTTOM:
				case WMSZ_BOTTOMRIGHT:
				case WMSZ_BOTTOMLEFT:
					winRect->bottom -= (winRect->bottom - winRect->top - tw->frameHeight_) % tw->cellHeightPx_;
					break;
				default:
					winRect->top += (winRect->bottom - winRect->top - tw->frameHeight_) % tw->cellHeightPx_;
					break;
				}
				switch (wParam) {
				case WMSZ_RIGHT:
				case WMSZ_TOPRIGHT:
				case WMSZ_BOTTOMRIGHT:
					winRect->right -= (winRect->right - winRect->left - tw->frameWidth_) % tw->cellWidthPx_;
					break;
				default:
					winRect->left += (winRect->right - winRect->left - tw->frameWidth_) % tw->cellWidthPx_;
					break;
				}
				break;
			}
			/* Called when the window is resized to given values.

			   No resize is performed if the window is minimized (we would have terminal size of length 0).

			   It is ok if no terminal window is associated with the handle as the message can be sent from the WM_CREATE when window is resized to account for the window border which has to be calculated.
			 */
			case WM_SIZE: {
				if (wParam == SIZE_MINIMIZED)
					break;
				if (tw != nullptr) {
					RECT rect;
					GetClientRect(hWnd, &rect);
					tw->windowResized(rect.right, rect.bottom);
				}
				break;
			}
			/* Repaint of the window is requested. */
			case WM_PAINT: {
				ASSERT(tw != nullptr) << "Attempt to paint unknown window";
				tw->paint();
				break;
			}
			/* No need to use WM_UNICHAR since WM_CHAR is already unicode aware */
			case WM_UNICHAR:
				UNREACHABLE;
				break;
			case WM_CHAR:
				if (wParam >= 0x20)
					tw->keyChar(helpers::Char::FromCodepoint(static_cast<unsigned>(wParam)));
				break;
			/* Processes special key events.
			
			   TODO perhaps all the syskeydown & syskeyup events should be stopped? 
			 */
			case WM_SYSKEYDOWN:
			case WM_KEYDOWN: {
				vterm::Key k = GetKey(static_cast<unsigned>(wParam));
				if (k != vterm::Key::Invalid)
					tw->keyDown(k);
				// returning w/o calling the default window proc means that the OS will not interfere by interpreting own shortcuts
				// NOTE add other interfering shortcuts as necessary
				if (k == vterm::Key::F10 || k.code() == vterm::Key::AltKey)
				    return 0;
				break;
			}
			case WM_SYSKEYUP:
			case WM_KEYUP: {
				vterm::Key k = GetKey(static_cast<unsigned>(wParam));
				tw->keyUp(k);
				break;
			}
			/* Mouse events which simply obtain the mouse coordinates, convert the buttons and wheel values to vterm standards and then calls the DirectWriteTerminalWindow's events, which perform the pixels to cols & rows translation and then call the terminal itself.
			 */
#define MOUSE_X static_cast<unsigned>(lParam & 0xffff)
#define MOUSE_Y static_cast<unsigned>((lParam >> 16) & 0xffff)
			case WM_LBUTTONDOWN:
				tw->mouseDown(MOUSE_X, MOUSE_Y, vterm::MouseButton::Left);
				break;
			case WM_LBUTTONUP:
				tw->mouseUp(MOUSE_X, MOUSE_Y, vterm::MouseButton::Left);
				break;
			case WM_RBUTTONDOWN:
				tw->mouseDown(MOUSE_X, MOUSE_Y, vterm::MouseButton::Right);
				break;
			case WM_RBUTTONUP:
				tw->mouseUp(MOUSE_X, MOUSE_Y, vterm::MouseButton::Right);
				break;
			case WM_MBUTTONDOWN:
				tw->mouseDown(MOUSE_X, MOUSE_Y, vterm::MouseButton::Wheel);
				break;
			case WM_MBUTTONUP:
				tw->mouseUp(MOUSE_X, MOUSE_Y, vterm::MouseButton::Wheel);
				break;
			/* Mouse wheel contains the position relative to screen top/left, so we must first translate it to window coordinates. 
			 */
			case WM_MOUSEWHEEL: {
				POINT pos{ MOUSE_X, MOUSE_Y };
				ScreenToClient(hWnd, &pos);
				tw->mouseWheel(pos.x, pos.y, GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
				break;
			}
			case WM_MOUSEMOVE:
				tw->mouseMove(MOUSE_X, MOUSE_Y);
				break;
			/* User specified messages for various events that we want to be handled in the app thread.
			 */
			case WM_USER:
				switch (wParam) {
				case DirectWriteApplication::MSG_TITLE_CHANGE: {
					helpers::utf16_string t = helpers::UTF8toUTF16(tw->terminal()->title());
					// ok, on windows wchar_t and char16_t are the same (see helpers/char.h)
					SetWindowTextW(hWnd, t.c_str());
					break;
				}
				default:
					LOG("Win32") << "Invalid user message " << wParam;
				}
				break;
			} // end of switch
			return DefWindowProc(hWnd, msg, wParam, lParam);
	}

} // namespace tpp

#endif