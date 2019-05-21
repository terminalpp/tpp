#pragma once
#ifdef __linux___

#include "vterm/vt100.h"


namespace tpp {

	class Terminal : public vterm::VT100 {
	public:
		Terminal(std::string const& cmd, std::initializer_list<std::string> args, unsigned cols, unsigned rows, vterm::Palette const& palette, unsigned defaultFg, unsigned defaultBg) :
			IOTerminal(cols, rows),
			VT100(cols, rows, palette, defaultFg, defaultBg),
			PTYTerminal(cmd, args, cols, rows) {
		}

	protected:

		void doResize(unsigned cols, unsigned rows) override {
			IOTerminal::doResize(cols, rows);
			VT100::doResize(cols, rows);
			PTYTerminal::doResize(cols, rows);
		}


	}; // tpp::Terminal


} // namespace tpp

#endif