#pragma once
#ifdef __linux__

#include "../terminal.h"

namespace vterm {
	 
	class PTYTerminal : public virtual IOTerminal {
	public:
		PTYTerminal(std::string const & command, unsigned cols, unsigned rows);

		~PTYTerminal() {

		}

		void execute();

	protected:

		bool readInputStream(char* buffer, size_t& size) override;

		void doResize(unsigned cols, unsigned rows) override;

		bool write(char const* buffer, size_t size) override;

	private:
		std::string command_;

		int pipe_;

		pid_t pid_;

	};
} // namespace vterm


#endif 
