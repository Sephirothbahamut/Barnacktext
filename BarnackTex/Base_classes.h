#pragma once

#include <string>
#include <stdexcept>
#include <iostream>

#include <unicode/utf8.h>
#include <unicode/uchar.h>
#include <unicode/utypes.h>
#include <unicode/unistr.h>
#include <unicode/ustream.h>
#include <unicode/stringpiece.h>

namespace utils
	{
	template<size_t N>
	struct Template_string
		{
		constexpr Template_string(const char(&str)[N])
			{
			std::copy_n(str, N, value);
			}

		char value[N];
		};

	inline unsigned int char_as_number(char c) { return static_cast<unsigned int>(static_cast<unsigned char>(c)); }

	inline std::string codepoint_to_utf8(UChar32 codepoint)
		{
		icu::UnicodeString ustr{codepoint};
		std::string ret;
		ustr.toUTF8String(ret);
		return ret;
		}

	inline UChar32 utf8_to_codepoint(const std::string& str)
		{
		icu::UnicodeString utf_cp_str{str.data(), "utf-8"};
		if (utf_cp_str.countChar32() > 1) { throw std::runtime_error{"This really shouldn't happen. Yay, you managed to break this :)"}; }
		return utf_cp_str.char32At(0);
		}
	}

namespace bartex
	{
	template<typename T>
	struct Token
		{
		T value{};
		size_t line{0};
		size_t pos{0};
		};

	template <utils::Template_string from>
	struct error : std::runtime_error
		{
		error(const std::string& msg, size_t line, size_t pos) : std::runtime_error{msg + "\nFrom " + from.value + " at line: " + std::to_string(line) + ", position: " + std::to_string(pos) + "."} {}
		error(const std::string& msg) : std::runtime_error{msg + "\nFrom " + from.value + " at EoF."} {}
		};

	using reader_error    = error<"Reader">;
	using tokenizer_error = error<"Tokenizer">;
	using parser_error    = error<"Parser">;
	}