#include "stdafx.h"
#include "TimeUtility.h"
#include <chrono>
#include <locale>
#include <codecvt>
#include <sstream>
#include <iomanip>

TimeUtility::TimeUtility()
{
}


TimeUtility::~TimeUtility()
{
}

std::string TimeUtility::Now()
{
	auto now = std::chrono::system_clock::now();
	auto now2 = std::chrono::system_clock::to_time_t(now);

	auto millisec = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

	auto fractional_seconds = millisec.count() % 1000;

	auto localTime = *std::localtime(&now2);

	std::ostringstream oss;
	oss << std::put_time(&localTime, "%Y.%m.%d %H:%M:%S.") << fractional_seconds;
	return oss.str();

}
