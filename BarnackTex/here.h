#pragma once
#include <filesystem>
#include <Windows.h>

std::filesystem::path here()
	{
	typedef std::vector<char> char_vector;
	typedef std::vector<char>::size_type size_type;
	char_vector buf(1024, 0);
	size_type size = buf.size();
	bool havePath = false;
	bool shouldContinue = true;
	do
		{
		DWORD result = GetModuleFileNameA(nullptr, &buf[0], size);
		DWORD lastError = GetLastError();
		if (result == 0)
			{
			shouldContinue = false;
			}
		else if (result < size)
			{
			havePath = true;
			shouldContinue = false;
			}
		else if (
			result == size
			&& (lastError == ERROR_INSUFFICIENT_BUFFER || lastError == ERROR_SUCCESS)
			)
			{
			size *= 2;
			buf.resize(size);
			}
		else
			{
			shouldContinue = false;
			}
		}
	while (shouldContinue);

	if (!havePath)
		{
		throw std::runtime_error{"Could not get executable path."};
		}

	std::string ret = &buf[0];

	return {ret};
	}