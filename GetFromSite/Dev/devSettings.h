///////////////////////////////////////////////////////////////////////////////////////////////////
// devSettings.h
//
// Standard ISO/IEC 114882, C++11
//
// |   version  |    release    | Description
// |------------|---------------|---------------------------------
// |      1     |   2020 04 29  |
// |            |               | 
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <devConfig.h>

#include <utilsBase.h>

#include <string>

namespace dev
{

struct tSettings
{
	struct
	{
		std::string Host;
		std::string User;
		std::string Passwd;
		std::string DB;
		std::uint16_t Port = 0;
	}DB;

private:
	std::string m_ConfigFileName;

public:
	tSettings() {}
	explicit tSettings(const std::string& fileName);

	std::string GetConfigFileName() const { return m_ConfigFileName; }
};

extern tSettings g_Settings;

}