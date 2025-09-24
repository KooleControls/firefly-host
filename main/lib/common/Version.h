#pragma once
#include <stdint.h>

class Version
{
	uint64_t version = 0;
public:

	Version(uint8_t major, uint8_t minor, uint8_t beta)
		: version(((uint64_t)major << 16) | ((uint64_t)minor << 8) | (uint64_t)beta)
	{
	}


	uint8_t GetMajor() const { return (version >> 16) & 0xFF; }
	uint8_t GetMinor() const { return (version >> 8) & 0xFF; }
	uint8_t GetBeta()  const { return version & 0xFF; }

	void ToString(char* buffer, size_t size) const
	{
		snprintf(buffer, size, "%02d.%02d.%02d", GetMajor(), GetMinor(), GetBeta());
	}

	bool operator == (const Version& v) const { return version == v.version; }
	bool operator != (const Version& v) const { return version != v.version; }
	bool operator >  (const Version& v) const { return version >  v.version; }
	bool operator <  (const Version& v) const { return version <  v.version; }
	bool operator >= (const Version& v) const { return version >= v.version; }
	bool operator <= (const Version& v) const { return version <= v.version; }

};

