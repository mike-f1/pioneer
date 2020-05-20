#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <string>
#include <vector>
#include "vector3.h"
#include "matrix3x3.h"
#include "matrix4x4.h"

namespace stringUtils {

	// find string in bigger string, ignoring case
	const char *pi_strcasestr(const char *haystack, const char *needle);

	inline bool starts_with(const char *s, const char *t)
	{
		assert(s && t);
		while ((*s == *t) && *t) {
			++s;
			++t;
		}
		return (*t == '\0');
	}

	inline bool starts_with(const std::string &s, const char *t)
	{
		assert(t);
		return starts_with(s.c_str(), t);
	}

	inline bool starts_with(const std::string &s, const std::string &t)
	{
		return starts_with(s.c_str(), t.c_str());
	}

	inline bool ends_with(const char *s, size_t ns, const char *t, size_t nt)
	{
		return (ns >= nt) && (memcmp(s + (ns - nt), t, nt) == 0);
	}

	inline bool ends_with(const char *s, const char *t)
	{
		return ends_with(s, strlen(s), t, strlen(t));
	}

	inline bool ends_with(const std::string &s, const char *t)
	{
		return ends_with(s.c_str(), s.size(), t, strlen(t));
	}

	inline bool ends_with(const std::string &s, const std::string &t)
	{
		return ends_with(s.c_str(), s.size(), t.c_str(), t.size());
	}

	inline bool ends_with_ci(const char *s, size_t ns, const char *t, size_t nt)
	{
		if (ns < nt) return false;
		s += (ns - nt);
		for (size_t i = 0; i < nt; i++)
			if (tolower(*s++) != tolower(*t++)) return false;
		return true;
	}

	inline bool ends_with_ci(const char *s, const char *t)
	{
		return ends_with_ci(s, strlen(s), t, strlen(t));
	}

	inline bool ends_with_ci(const std::string &s, const char *t)
	{
		return ends_with_ci(s.c_str(), s.size(), t, strlen(t));
	}

	inline bool ends_with_ci(const std::string &s, const std::string &t)
	{
		return ends_with_ci(s.c_str(), s.size(), t.c_str(), t.size());
	}

	static inline size_t SplitSpec(const std::string &spec, std::vector<int> &output)
	{
		static const std::string delim(",");

		size_t num = 0, start = 0, end = 0;
		while (end != std::string::npos) {
			// get to the first non-delim char
			start = spec.find_first_not_of(delim, end);

			// read the end, no more to do
			if (start == std::string::npos)
				break;

			// find the end - next delim or end of string
			end = spec.find_first_of(delim, start);

			// extract the fragment and remember it
			output[num++] = atoi(spec.substr(start, (end == std::string::npos) ? std::string::npos : end - start).c_str());
		}

		return num;
	}

	static inline size_t SplitSpec(const std::string &spec, std::vector<float> &output)
	{
		static const std::string delim(",");

		size_t num = 0, start = 0, end = 0;
		while (end != std::string::npos) {
			// get to the first non-delim char
			start = spec.find_first_not_of(delim, end);

			// read the end, no more to do
			if (start == std::string::npos)
				break;

			// find the end - next delim or end of string
			end = spec.find_first_of(delim, start);

			// extract the fragment and remember it
			output[num++] = std::stof(spec.substr(start, (end == std::string::npos) ? std::string::npos : end - start));
		}

		return num;
	}

	std::vector<std::string> SplitString(const std::string &source, const std::string &delim);

	std::string string_join(std::vector<std::string> &v, std::string sep);
	std::string format_date(double time);
	std::string format_date_only(double time);
	std::string format_distance(double dist, int precision = 2);
	std::string format_money(double cents, bool showCents = true);
	std::string format_duration(double seconds);
	std::string format_latitude(double decimal_degree);
	std::string format_longitude(double decimal_degree);
	std::string format_speed(double speed, int precision = 2);

	// 'Numeric type' to string conversions.
	std::string FloatToStr(float val);
	std::string DoubleToStr(double val);
	std::string AutoToStr(int32_t val);
	std::string AutoToStr(int64_t val);
	std::string AutoToStr(float val);
	std::string AutoToStr(double val);

	void Vector3fToStr(const vector3f &val, char *out, size_t size);
	void Vector3dToStr(const vector3d &val, char *out, size_t size);
	void Matrix3x3fToStr(const matrix3x3f &val, char *out, size_t size);
	void Matrix3x3dToStr(const matrix3x3d &val, char *out, size_t size);
	void Matrix4x4fToStr(const matrix4x4f &val, char *out, size_t size);
	void Matrix4x4dToStr(const matrix4x4d &val, char *out, size_t size);

	// String to 'Numeric type' conversions.
	int64_t StrToSInt64(const std::string &str);
	uint64_t StrToUInt64(const std::string &str);
	float StrToFloat(const std::string &str);
	double StrToDouble(const std::string &str);
	void StrToAuto(int32_t *pVal, const std::string &str);
	void StrToAuto(int64_t *pVal, const std::string &str);
	void StrToAuto(float *pVal, const std::string &str);
	void StrToAuto(double *pVal, const std::string &str);

	void StrToVector3f(const char *str, vector3f &val);
	void StrToVector3d(const char *str, vector3d &val);
	void StrToMatrix3x3f(const char *str, matrix3x3f &val);
	void StrToMatrix3x3d(const char *str, matrix3x3d &val);
	void StrToMatrix4x4f(const char *str, matrix4x4f &val);
	void StrToMatrix4x4d(const char *str, matrix4x4d &val);

	// Convert decimal coordinates to degree/minute/second format and return as string
	std::string DecimalToDegMinSec(float dec);

}

#endif // STRING_UTILS_H
