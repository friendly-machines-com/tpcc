#include "rtl.h"

#include <cstdlib>
#include <ctime>

struct system_time {
	::u_system::t_word p_year;
	::u_system::t_word p_month;
	::u_system::t_word p_dayofweek;
	::u_system::t_word p_day;
	::u_system::t_word p_hour;
	::u_system::t_word p_minute;
	::u_system::t_word p_second;
	::u_system::t_word p_millisecond;
};

static bool matches(const system_time& value, const ::timespec& snapshot) {
	std::tm local{};
	if (!::localtime_r(&snapshot.tv_sec, &local)) {
		return false;
	}
	return value.p_year == local.tm_year + 1900 && value.p_month == local.tm_mon + 1 && value.p_dayofweek == local.tm_wday && value.p_day == local.tm_mday && value.p_hour == local.tm_hour && value.p_minute == local.tm_min && value.p_second == local.tm_sec;
}

int main() {
	::timespec before{};
	::timespec after{};
	if (::clock_gettime(CLOCK_REALTIME, &before) != 0) {
		return EXIT_FAILURE;
	}

	system_time value{};
	::u_sysutils::p_getlocaltime(value);

	if (::clock_gettime(CLOCK_REALTIME, &after) != 0) {
		return EXIT_FAILURE;
	}
	if (!matches(value, before) && !matches(value, after)) {
		return EXIT_FAILURE;
	}
	if (value.p_millisecond > 999) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
