#pragma once
#include <cctype>
#include <string>

// Ordering functor for case-insensitive std::map/set keys. Pascal identifiers
// (and therefore preprocessor symbols) compare without regard to case.
struct CILess {
	bool operator()(const std::string& a, const std::string& b) const {
		size_t n = std::min(a.size(), b.size());
		for (size_t i = 0; i < n; i++) {
			int ca = std::tolower((unsigned char)a[i]);
			int cb = std::tolower((unsigned char)b[i]);
			if (ca != cb) {
				return ca < cb;
			}
		}
		return a.size() < b.size();
	}
};
