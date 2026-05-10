#include "_FastWcUtil.h"

size_t util::intWidth(size_t n)
{
	if (n == 0) {
		return 1;
	}

	// Use binary search on powers of 10
	// It's faster than log10 for small numbers.
	if (n < 10) {
		return 1;
	}

	if (n < 100) {
		return 2;
	}

	if (n < 1000) {
		return 3;
	}

	if (n < 10000) {
		return 4;
	}

	if (n < 100000) {
		return 5;
	}

	if (n < 1000000) {
		return 6;
	}

	if (n < 10000000) {
		return 7;
	}

	if (n < 100000000) {
		return 8;
	}

	if (n < 1000000000) {
		return 9;
	}

	if (n < 10000000000ULL) {
		return 10;
	}

	if (n < 100000000000ULL) {
		return 11;
	}

	if (n < 1000000000000ULL) {
		return 12;
	}

	if (n < 10000000000000ULL) {
		return 13;
	}

	if (n < 100000000000000ULL) {
		return 14;
	}

	if (n < 1000000000000000ULL) {
		return 15;
	}

	if (n < 10000000000000000ULL) {
		return 16;
	}

	if (n < 100000000000000000ULL) {
		return 17;
	}

	if (n < 1000000000000000000ULL) {
		return 18;
	}

	if (n < 10000000000000000000ULL) {
		return 19;
	}

	return 20;
}

