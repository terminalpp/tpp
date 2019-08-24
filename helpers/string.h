#pragma once

#include <vector>

#include "helpers.h"

#include "char.h"

namespace helpers {

	inline std::string ConvertNonPrintableCharacters(std::string const& from) {
		std::stringstream str;
		for (char c : from) {
			switch (c) {
			case '\a': // BEL, 0x07
				str << "\\a";
				break;
			case '\b': // backspace, 0x08
				str << "\\b";
				break;
			case '\t': // tab, 0x09
				str << "\\t";
				break;
			case '\n': // new line, 0x0a
				str << "\\n";
				break;
			case '\v': // vertical tab, 0x1b
				str << "\\v";
				break;
			case '\f': // form feed, 0x1c
				str << "\\f";
				break;
			case '\r': // carriage return, 0x1d
				str << "\\r";
				break;
			default:
				if (c >= 0 && c < ' ') {
					str << "\\x" << ToHexDigit(c / 16) << ToHexDigit(c % 16);
				} else {
					str << c;
				}
			}
		}
		return str.str();
	}

	/** Removes any whitespace characters from the beginning and end of the given string. 

	    Uses the `IsWhitespace` function from `char.h`.
	 */
	inline std::string Trim(std::string const& from) {
		if (from.empty())
			return from;
		size_t start = 0;
		while (IsWhitespace(from[start]))
			++start;
		// if we got to the end of the string, the string is empty
		if (start == from.size() - 1)
			return "";
		// otherwise there is at least one non-whitespace character in the string
		size_t end = from.size() - 1;
		while (IsWhitespace(from[end]))
			--end;
		return from.substr(start, end - start + 1);
	}

	inline std::string TrimRight(std::string const & from) {
		if (from.empty())
		    return from;
		int end = static_cast<int>(from.size() - 1);
		while (end > 0) {
		    if (! IsWhitespace(from[end]))
			    break;
			--end;
		}
		++end;
		return from.substr(0, end);
	}

	/** Splits the given string by given delimiter. 
	 */
	inline std::vector<std::string> Split(std::string const& what, std::string const& delimiter) {
		std::vector<std::string> result;
		for (size_t start = 0, e = what.size(); start < e; ) {
			size_t next = what.find(delimiter, start);
			if (next == std::string::npos)
				next = what.size();
			result.push_back(what.substr(start, next - start));
			start = next + delimiter.size();
		}
		return result;
	}

	/** Quotes the current string. 
	 
	    TODO the quoting should be more robust. 
 	 */
    inline std::string Quote(std::string const & what, char quote = '"') {
		std::stringstream result;
		result << quote;
		for (char c : what) {
			switch (c) {
				case '\\':
				case '\'':
				case '"':
				    result << '\\' << c;
					break;
				case '\n':
				    result << "\\n";
					break;
				default:
				    result << c;
					break;
			}
		}
		result << quote;
		return result.str();
	}

	// Conversions ------------------------------------------------------------------------------------

	/** Converts a null terminated wide string in UTF-16 encoding into an std::string encoded in UTF-8.
	  */
	inline std::string UTF16toUTF8(utf16_char const* str) {
		std::stringstream result;
		while (*str != 0) {
			result << Char::FromUTF16(str, str + 2); // null terminated so we assume there is enough space
		}
		return result.str();
	}

// macOS does not really support the C++ standard wrt char16_t. Fportunately we only need it on Windows for now
#ifndef ARCH_MACOS
	inline utf16_string UTF8toUTF16(char const* str) {
		utf16_stringstream result;
		while (*str != 0) {
			Char const * c = Char::At(str, str + 4); // null terminated, so we assume there is enough space
			// just stop if the character looks invalid
			if (c == nullptr)
				break;
			c->toUTF16(result);
		}
		return result.str();
	}

	inline utf16_string UTF8toUTF16(std::string const& from) {
		return UTF8toUTF16(from.c_str());
	}
#endif


} // namespace helpers