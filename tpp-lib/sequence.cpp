#if (defined ARCH_LINUX)
#include <unistd.h>
#endif

#include <iostream>

#include <thread>

#include "helpers/char.h"
#include "helpers/string.h"

#include "terminal.h"

#include "sequence.h"

namespace tpp {


    Sequence Sequence::Parse(char * & start, char const * end) {
        Sequence result{};
        char * x = start;
        if (x == end) {
            result.id_ = Kind::Incomplete;
            return result;
        }
        // read the id
        if (helpers::IsDecimalDigit(*x)) {
            int id = 0;
            do {
                id = id * 10 + helpers::DecCharToNumber(*x++);
            } while (x != end && helpers::IsDecimalDigit(*x));
            // skip the ';' after the id if present
            if (x != end && *x == ';')
                ++x;
            result.id_ = static_cast<Kind>(id);
        }
        // read the contents
        char * valueStart = x;
        while (x != end) {
            // ESC character cannot appear in the payload, if it does, we are leaking real escape codes and should stop.
            if (*x == helpers::Char::ESC) {
                result.id_ = Kind::Invalid;
                start = x;
                return result;
            }
            if (*x == helpers::Char::BEL) {
                result.payload_ = std::string{valueStart, static_cast<size_t>(x - valueStart)};
                start = ++x;
                return result;
            }
            ++x;
        }
        result.id_ = Kind::Incomplete;
        return result;
    }

    Sequence::AckResponse::AckResponse(Sequence && seq) {
        if (seq.id_ != Kind::Ack || !seq.payload_.empty())
            THROW(SequenceError()) << "Invalid ack response " << seq;
    }

    Sequence::CapabilitiesRequest::CapabilitiesRequest(Sequence && seq) {
        if (seq.id_ != Kind::Capabilities || !seq.payload_.empty())
            THROW(SequenceError()) << "Invalid capabilities request sequence " << seq;
    }

    Sequence::CapabilitiesResponse::CapabilitiesResponse(Sequence && seq) {
        if (seq.id_ != Kind::Capabilities)
            THROW(SequenceError()) << "Invalid capabilities response sequence " << seq;
        ContentsParser cp(std::move(seq));
        version = cp.parseInt();
        cp.checkEoF();
    }

    Sequence::NewFileRequest::NewFileRequest(Sequence && seq) {
        if (seq.id_ != Kind::NewFile)
            THROW(SequenceError()) << "Invalid new file request sequence " << seq;
        ContentsParser cp(std::move(seq));
        size = cp.parseUnsigned();
        hostname = cp.parseString();
        filename = cp.parseString();
        remotePath = cp.parseString();
        cp.checkEoF();
    }

    Sequence::NewFileResponse::NewFileResponse(Sequence && seq) {
        if (seq.id_ != Kind::NewFile)
            THROW(SequenceError()) << "Invalid new file response sequence " << seq;
        ContentsParser cp(std::move(seq));
        fileId = cp.parseInt();
        cp.checkEoF();
    }

    Sequence::DataRequest::DataRequest(Sequence && seq) {
        if (seq.id_ != Kind::Data)
            THROW(SequenceError()) << "Invalid data request sequence " << seq;
        ContentsParser cp(std::move(seq));
        fileId = cp.parseInt();
        size_t size = cp.parseUnsigned();
        offset = cp.parseUnsigned();
        data = cp.parseEncodedData();
        if (size != data.size())
            THROW(SequenceError()) << "Data packet size mismatch";
        cp.checkEoF();
    }

    Sequence::TransferStatusRequest::TransferStatusRequest(Sequence && seq) {
        if (seq.id_ != Kind::TransferStatus)
            THROW(SequenceError()) << "Invalid transfer status request sequence " << seq;
        ContentsParser cp(std::move(seq));
        fileId = cp.parseInt();
        cp.checkEoF();
    }

    Sequence::TransferStatusResponse::TransferStatusResponse(Sequence && seq) {
        if (seq.id_ != Kind::TransferStatus)
            THROW(SequenceError()) << "Invalid transfer status response sequence " << seq;
        ContentsParser cp(std::move(seq));
        fileId = cp.parseInt();
        transmittedBytes = cp.parseUnsigned();
        cp.checkEoF();
    }

    Sequence::OpenFileRequest::OpenFileRequest(Sequence && seq) {
        if (seq.id_ != Kind::OpenFile)
            THROW(SequenceError()) << "Invalid open file request sequence " << seq;
        ContentsParser cp(std::move(seq));
        fileId = cp.parseInt();
        cp.checkEoF();
    }

    int Sequence::ContentsParser::parseInt() {
        if (i_ >= payload_.size())
            THROW(SequenceError()) << "Integer expected, but EoF found in sequence " << payload_;
        int result = 0;
        int multiplier = 1;
        if (payload_[i_] == '-') {
            multiplier = -1;
            ++i_;
            if (i_ >= payload_.size())
                THROW(SequenceError()) << "Integer expected, but EoF found in sequence " << payload_;
            if (payload_[i_] == ';')
                THROW(SequenceError()) << "Integer expected, but only sign found " << payload_;
        }
        bool ok = false;
        while (i_ < payload_.size()) {
            if (payload_[i_] == ';') {
                ++i_;
                break;
            }
            if (! helpers::IsDecimalDigit(payload_[i_]))
                THROW(SequenceError()) << "Expected number, but " << payload_[i_] << " found in sequence " << payload_;
            result = result * 10 + helpers::DecCharToNumber(payload_[i_++]);
            ok = true;
        }
        if (! ok)
            THROW(SequenceError()) << "Empty integer value not allowed " << payload_;
        return result * multiplier;
    }

    size_t Sequence::ContentsParser::parseUnsigned() {
        if (i_ >= payload_.size())
            THROW(SequenceError()) << "Unsigned expected, but EoF found in sequence " << payload_;
        size_t result = 0;
        bool ok = false;
        while (i_ < payload_.size()) {
            if (payload_[i_] == ';') {
                ++i_;
                break;
            }
            if (! helpers::IsDecimalDigit(payload_[i_]))
                THROW(SequenceError()) << "Expected number, but " << payload_[i_] << " found in sequence " << payload_;
            result = result * 10 + helpers::DecCharToNumber(payload_[i_++]);
            ok = true;
        }
        if (! ok)
            THROW(SequenceError()) << "Empty integer value not allowed " << payload_;
        return result;
    }

    std::string Sequence::ContentsParser::parseString() {
        if (i_ >= payload_.size())
            THROW(SequenceError()) << "string expected, but EoF found in sequence " << payload_;
        size_t nextSemicolon = payload_.find(';', i_);
        if (nextSemicolon == std::string::npos) {
            std::string result = payload_.substr(i_);
            i_ = payload_.size();
            return result;
        } else {
            std::string result = payload_.substr(i_, nextSemicolon - i_);
            i_ = nextSemicolon + 1;
            return result;
        }
    }

    std::string Sequence::ContentsParser::parseEncodedData() {
        std::string result{std::move(payload_)};
        char const * r = result.c_str() + i_;
        size_t w = 0;
        char const * e = result.c_str() + result.size();
        while (r < e) {
            result[w++] = Terminal::Decode(r, e);
        }
        result.resize(w);
        i_ = 0;
        return result;
    }

    void Sequence::ContentsParser::checkEoF() {
        if (i_ != payload_.size())
            THROW(SequenceError()) << "expected end of sequence"; 
    }

} // namespace tpp