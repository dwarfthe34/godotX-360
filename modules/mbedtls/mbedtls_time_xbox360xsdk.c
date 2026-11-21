#include <stdint.h>
#include <time.h>

#include <mbedtls/platform_time.h>

mbedtls_ms_time_t mbedtls_ms_time(void)
{
	struct timespec ts = {};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (mbedtls_ms_time_t)((uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL);
}
