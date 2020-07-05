#include "helpers/char.h"
#include "helpers/ansi_sequences.h"

#include "ansi_renderer.h"

namespace ui3 {

    AnsiRenderer::AnsiRenderer(tpp::PTYSlave * pty):
        Renderer{pty->size()},
        tpp::TerminalClient{pty} {
    }

    AnsiRenderer::~AnsiRenderer() {
        
    }

    void AnsiRenderer::render(Buffer const & buffer, Rect const & rect) {
        std::stringstream s;
        // initialize the state
        Cell state = buffer.at(rect.topLeft());
        s << ansi::SGRReset() 
            << ansi::Fg(state.fg().r, state.fg().g, state.fg().b)
            << ansi::Bg(state.bg().r, state.bg().g, state.bg().b);
        if (state.font().bold())
            s << ansi::Bold();
        if (state.font().italic())
            s << ansi::Italic();
        if (state.font().underline())
            s << ansi::Underline();
        if (state.font().strikethrough())
            s << ansi::Strikethrough();
        if (state.font().blink())
            s << ansi::Blink();
        // actually output the buffer
        for (int y = rect.top(), ye = rect.bottom(); y < ye; ++y) {
            // for each row, first set the cursor properly
            int x = rect.left();
            s << ansi::SetCursor(x, y);
            // then for each cell update the attributes & colors if needs be and output the cell
            for (int xe = rect.right(); x < xe; ++x) {
                Cell const & c = buffer.at(x, y);
                if (c.fg() != state.fg()) {
                    state.fg() = c.fg();
                    s << ansi::Fg(state.fg().r, state.fg().g, state.fg().b);
                }
                if (c.bg() != state.bg()) {
                    state.bg() = c.bg();
                    s << ansi::Bg(state.bg().r, state.bg().g, state.bg().b);
                }
                if (c.font().bold() != state.font().bold()) {
                    state.font().setBold(c.font().bold());
                    s << ansi::Bold(state.font().bold());
                }
                if (c.font().italic() != state.font().italic()) {
                    state.font().setItalic(c.font().italic());
                    s << ansi::Italic(state.font().italic());
                }
                if (c.font().underline() != state.font().underline()) {
                    state.font().setUnderline(c.font().underline());
                    s << ansi::Underline(state.font().underline());
                }
                if (c.font().strikethrough() != state.font().strikethrough()) {
                    state.font().setStrikethrough(c.font().strikethrough());
                    s << ansi::Strikethrough(state.font().strikethrough());
                }
                if (c.font().blink() != state.font().blink()) {
                    state.font().setBlink(c.font().blink());
                    s << ansi::Blink(state.font().blink());
                }
                // finally output the codepoint
                s << Char{c.codepoint()};
            }
        }
        // TODO this is a very unnecessary copy, should be fixed
        std::string x{s.str()};
        send(x.c_str(), x.size());
    }

    size_t AnsiRenderer::received(char const * buffer, char const * bufferEnd) {
        // TODO this must be smarter detection and actually raise stuff and so on
        for (char const * i = buffer; i != bufferEnd; ++i)
            if (*i == '\003') // Ctrl + C
#if (defined ARCH_UNIX)
                raise(SIGINT);
#else
                exit(EXIT_FAILURE);
#endif
        return 0;
    }

    void AnsiRenderer::receivedSequence(tpp::Sequence::Kind, char const * buffer, char const * bufferEnd) {
        MARK_AS_UNUSED(buffer);
        MARK_AS_UNUSED(bufferEnd);
        NOT_IMPLEMENTED;
    }



} // namespace ui