#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <stdexcept>


namespace utils::unicode
	{
	struct u8codepoint : std::string
		{

		template<typename T>
		std::vector<T> to_vector()
			{
			std::vector<T> ret;
			for (const char& c : *this) { ret.push_back(static_cast<T>(static_cast<unsigned char>(c))); }
			return ret;
			}

		bool is_valid()
			{
			if (size() == 0) 
				{ return false; }
			char c = operator[](0);
			auto debugger_have_mercy = to_vector<unsigned int>();

			//0xxxxxxx > Only byte of a 1-byte character encoding
			     if (!(c & 0b10000000))
					 { return size() == 1; }
			//110xxxxx > First byte of a 2-byte character encoding
			else if ((c & 0b11000000) && !(c & 0b00100000)) { if (size() != 2) 
					 { return false; } }
			//1110xxxx > First byte of a 3-byte character encoding
			else if ((c & 0b11100000) && !(c & 0b00010000)) { if (size() != 3) 
					 { return false; } }
			//11110xxx > First byte of a 4-byte character encoding
			else if ((c & 0b11110000) && !(c & 0b00001000)) { if (size() != 4) 
					 { return false; } }
			
			for (unsigned i = 1; i < size(); i++)
				{
				c = operator[](i);

				//10xxxxxx  0x80..0xBF   Continuation byte: one of 1-3 bytes following the first
				if (!((c & 0b10000000) && !(c & 0b01000000))) 
					{ return false; }
				}
			return true;
			}
		};

	//Alternative for std::istream; you can safely iterate over eof, and get will not have weird behaviour on Windows with multiple-bytes unicode characters.
	class u8cp_istream
		{
		public:
			u8cp_istream(std::istream& in) : in{in} { last_char = read(); }

			bool eof() const noexcept { return in.eof(); }

			u8codepoint get_except()
				{
				if (!last_char.is_valid() && false)
					{
					std::string msg{"Invalid unicode character in file.\nBytes sequence contains: "};

					auto tmp = last_char.to_vector<unsigned int>();
					for (const auto& value : tmp) { msg += value + " "; }
					std::cerr << msg << std::endl;
					throw std::runtime_error{msg};
					}
				return get();
				}

			u8codepoint get() noexcept
				{
				u8codepoint ret = std::move(last_char);
				last_char = read();
				return ret;
				}

		private:
			u8codepoint last_char{};

			u8codepoint read() noexcept
				{
				u8codepoint ret{""};

				char c;
				in.read(&c, 1);
				ret += c;

				//0xxxxxxx > Only byte of a 1-byte character encoding
				if (!(c & 0b10000000)) { return ret; }

				unsigned length = 0;

				//110xxxxx > First byte of a 2-byte character encoding
				     if ((c & 0b11000000) && !(c & 0b00100000)) { length = 2; }
				//1110xxxx > First byte of a 3-byte character encoding
				else if ((c & 0b11100000) && !(c & 0b00010000)) { length = 3; }
				//11110xxx > First byte of a 4-byte character encoding
				else if ((c & 0b11110000) && !(c & 0b00001000)) { length = 4; }

				for (unsigned i = 1; i < length; i++)
					{
					in.read(&c, 1);
					ret += c;
					}
				return ret;
				}

			std::istream& in;
		};

	/*class unicode_view
		{
		public:
			unicode_view(std::stirng& str
		private:
		};*/
	}