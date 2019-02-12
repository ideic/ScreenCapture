#pragma once
#include <string>
#include <chrono>
class TimeUtility
{
public:
	TimeUtility();
	~TimeUtility();

	std::string Now();

	std::string Since(std::chrono::time_point<std::chrono::system_clock> since);
};

