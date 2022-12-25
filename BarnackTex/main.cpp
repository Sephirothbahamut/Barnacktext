#include <iostream>
#include <iterator>
#include <fstream>

/*template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T> vector)
	{
	os << "[ ";
	for (const auto& element : vector) { os << element << " "; }
	return os << "]";
	}*/

#include "Compiler.h"

#include <Windows.h>


int main(int argc, char** argv)
	{
	// Remove on Linux, sets up windows console to accept unicode
	// Set console code page to UTF-8 so console known how to interpret string data
	SetConsoleOutputCP(65001);
	// Enable buffering to prevent VS from chopping up UTF-8 byte sequences
	setvbuf(stdout, nullptr, _IOFBF, 1000);

	std::vector<std::string> arguments(argv + 1, argv + argc);
	if (arguments.size() == 0 || arguments.size() > 2) { std::cout << "Usage: [compiler] [source root] [output root]" << std::endl; return 0; }
	
	bartex::compiler::compile_project(arguments[0], arguments[1]);
	}