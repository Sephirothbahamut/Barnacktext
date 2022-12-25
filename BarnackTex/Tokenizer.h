#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>

#include <vector>
#include <deque>

#include "Base_classes.h"


namespace bartex
	{

	class Reader
		{
		public:
			Reader(const std::filesystem::path& fname) : in{fname}
				{
				if (!in.is_open()) { throw reader_error{"Failed to open file \"" + fname.string() + "\"."}; }
				tok.value = get_codepoint_from_stream();
				}

			bool has_next() 
				{ 
				//return has_back() || !file.eof();
				bool a = has_back();
				bool b = !in.eof();
				return a || b;
				}
			Token<UChar32> next()
				{
				Token<UChar32> ret;
				do { ret = has_back() ? _next_back() : _next_input(); }
				while (ret.value == U'\r');

				//if (ret.value != U'\n' && is_control(utils::codepoint_to_utf8(ret.value)[0])) { throw reader_error{"Unexpected control character \"" + std::to_string(utils::char_as_number(ret.value)) + "\".", ret.line, ret.pos}; }
				
				return ret;
				}
			void back(Token<UChar32> c) { _back = c; }

			void skip_whitespace(bool skip_newline = true)
				{
				Token<UChar32> tmp;
				if (has_next())
					{
					do 
						{
						tmp = next(); 
						if (!skip_newline && tmp.value == U'\n') { break; }
						}
					while ((u_isUWhiteSpace(tmp.value) || tmp.value == U'\n') && has_next());
					}
				back(tmp);
				}

		private:
			Token<UChar32> _next_back()  { Token<UChar32> ret = _back; back({U'\0'}); return ret; }
			Token<UChar32> _next_input() 
				{ 
				Token<UChar32> ret = tok; 
				tok.value = get_codepoint_from_stream(); 

				if (tok.value == U'\n') { tok.line++; tok.pos = 0; }
				else if (tok.value != U'\r') { tok.pos++; }

				return ret; 
				}

			UChar32 get_codepoint_from_stream()
				{
				std::string str{""};

				char c;
				unsigned char debugger_please;

				in.read(&c, 1);
				debugger_please = utils::char_as_number(c);
				str += c;

				unsigned length = 0;

				//0xxxxxxx > Only byte of a 1-byte character encoding
				     if (!(c & 0b10000000))                     { length = 1; }
				//110xxxxx > First byte of a 2-byte character encoding
				else if ((c & 0b11000000) && !(c & 0b00100000)) { length = 2; }
				//1110xxxx > First byte of a 3-byte character encoding
				else if ((c & 0b11100000) && !(c & 0b00010000)) { length = 3; }
				//11110xxx > First byte of a 4-byte character encoding
				else if ((c & 0b11110000) && !(c & 0b00001000)) { length = 4; }

				for (unsigned i = 1; i < length; i++)
					{
					in.read(&c, 1);
					debugger_please = utils::char_as_number(c);

					if (!((c & 0b10000000) && !(c & 0b01000000))) { return {}; }//throw reader_error{"Malformed unicode code point.", tok.line, tok.pos}; } //Cannot throw, this can happen on the last get at eof.
					str += c;
					}
				
				return utils::utf8_to_codepoint(str);
				}

			bool has_back() { return _back.value != U'\0'; }
			Token<UChar32> _back{U'\0'};

			Token<UChar32> tok;

			std::ifstream in;
		};

	class Tokenizer
		{
		public:
			Tokenizer(Reader& reader) : reader{reader} {}

			bool has_next() { return has_back() || reader.has_next(); }

			Token<std::string> next() 
				{
				auto ret = has_back() ? _next_back() : _next_input(); 
				return ret;
				}

			void back(Token<std::string> b) 
				{
				queue_back.push_back(b);
				}

			void skip_whitespace(bool skip_newline = true)
				{
				BEGIN:
				if (has_back())
					{
					Token<std::string> tok = queue_back.front();

					if (!skip_newline && tok.value == "\n") { return; }
					else
						{
						icu::UnicodeString ustr{tok.value.data(), "utf-8"};

						if (ustr.countChar32() > 1) { return; } //More than one codepoint, must be a word.
						else
							{
							UChar32 tmp = ustr.char32At(0);
							if (u_isUWhiteSpace(tmp) || (tmp == U'\n')) { next(); goto BEGIN; }
							//else {} //Single byte non-space codepoint.
							}
						}
					}
				else { reader.skip_whitespace(skip_newline); }
				}

		private:
			Token<std::string> _next_back() { Token<std::string> ret = queue_back.back(); queue_back.pop_back(); return ret; }
			Token<std::string> _next_input()
				{
				Token<UChar32> in_tok = reader.next();
				Token<std::string> ret{"", in_tok.line, in_tok.pos};

				// Abusing of lazy evaluation:
				// Each read_ operation returns true if its internal conditions are satisfied; in that case it will have populated the token; lazy evaluation prevents
				// the other read_ operations to be called, so we return the proper token.
				// If a read_ operation condation fails, it won't have progressed in reading the input stream, so "in_tok" will still be the first codepoint to check, 
				// and we pass it to the next read_operation.

				if (read_sequence(in_tok, ret) || read_whitespace(in_tok, ret) || read_symbol(in_tok, ret)) { return ret; }
				else { throw tokenizer_error{"Unknown character, unicode codepoint " + std::to_string(in_tok.value), in_tok.line, in_tok.pos}; }
				}

			bool read_sequence(Token<UChar32>& first, Token<std::string>& ret)
				{

				if (u_isUAlphabetic(first.value) || (first.value >= U'0' && first.value <= U'9'))
					{
					while (u_isUAlphabetic(first.value) || (first.value >= U'0' && first.value <= U'9'))
						{
						ret.value += utils::codepoint_to_utf8(first.value);

						if (!reader.has_next()) { goto EOF_REACHED; }

						first = reader.next();
						}

					reader.back(first);

				EOF_REACHED:
					return true;
					}
				return false;
				}

			bool read_whitespace(Token<UChar32>& first, Token<std::string>& ret)
				{
				if (u_isUWhiteSpace(first.value))
					{
					bool found_newline{false};

					while (u_isUWhiteSpace(first.value))
						{
						if (first.value == U'\n')
							{
							if (found_newline) { break; } //A newline was already met; break to push this newline back to the reader for the next token.
							else { found_newline = true; }
							}

						if (!reader.has_next()) { goto EOF_REACHED; }
						first = reader.next();
						}

					reader.back(first);

					EOF_REACHED:

					if (found_newline) { ret.value = utils::codepoint_to_utf8(U'\n'); }
					else { ret.value = utils::codepoint_to_utf8(U' '); }

					return true;
					}
				return false;
				}

			bool read_symbol(Token<UChar32>& first, Token<std::string>& ret)
				{
				ret.value = utils::codepoint_to_utf8(first.value);
				return true;
				}

			bool has_back() { return queue_back.size(); }
			std::deque<Token<std::string>> queue_back;
			Reader& reader;
		};
	}