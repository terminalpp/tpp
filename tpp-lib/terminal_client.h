#pragma once

#include <thread>

#include "helpers/helpers.h"
#include "helpers/process.h"
#include "helpers/log.h"
#include "helpers/char.h"
#include "terminal_pty.h"
#include "sequence.h"

namespace tpp {

    class TimeoutError : public helpers::Exception {
    }; 

    /** t++ client for terminal. 
     
        Supports reading and writing both tpp sequences and normal input/output to the terminal.
     */
    class TerminalClient {
    public:

        TerminalClient(TerminalPTY & pty):
            pty_{pty} {
        }

    protected:

        /** Determines the capabilities of the attached terminal. 
         */
        Sequence::Capabilities getCapabilities();


        /** Starts the terminal client. 
         */
        virtual void start();

        /** Sends given buffer using the attached terminal. 
         */
        void send(char const * buffer, size_t numBytes) {
            pty_.send(buffer, numBytes);
        }

        /** Sends given t++ sequence. 
         */
        void send(Sequence const & seq) {
            pty_.send(seq);
        }

        /** Called when normal input is received from the terminal. 
         
            The implementation should process the received input and return the number of bytes processed. These will be removed from the buffer, while any unprocessed data will be prepended to data received next. 
         */
        virtual size_t receive(char const * buffer, char const * bufferEnd) = 0;

        /** Called when a t++ sequence has been received. 
         */
        virtual void receiveSequence(char const * buffer, char const * bufferEnd) = 0;

        /** Called when the terminal's input has reached end of file. 
         
            Contains any buffer, that has been previously received via the receive() method, but left unprocessed. The default implementation does nothing.
         */
        virtual void inputEof(char const * buffer, char const * bufferEnd) {
            MARK_AS_UNUSED(buffer);
            MARK_AS_UNUSED(bufferEnd);
        }

        static constexpr size_t DEFAULT_BUFFER_SIZE = 1024;

    private:

        void readerThread();

        char * parseTerminalInput(char * buffer, char const * bufferEnd);

        /** Finds the beginniong of a tpp sequence, or its prefix in the buffer. 
         
            Returns the beginning of the tpp sequence `"\033P+"`, or if the buffer terminates before the full sequence was read the beginning of possible tpp sequence start. 

            If not found, returns the bufferEnd. 
         */
        char * findTppStartPrefix(char * buffer, char const * bufferEnd);

        /** Given a start of the tpp sequence ("\033P+") or its prefix, calculates the range for the sequence's payload. 
         
            If the sequence is invalid, returns `(nullptr, nullptr)`. If the sequence seems valid, but the buffer does not conatin enough data, returns `(bufferEnd, bufferEnd)`. In other cases returns an std::pair where the first value is the first valid tpp sequence character and the second value is the sequence terminator. 
         */
        std::pair<char *, char*> findTppRange(char * tppStart, char const * bufferEnd);

        TerminalPTY & pty_;
        std::thread reader_;

    }; // tpp::TerminalClient

} // namespace tpp