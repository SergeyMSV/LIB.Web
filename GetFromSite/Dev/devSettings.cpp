#include "devSettings.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace dev
{

tSettings g_Settings;

tSettings::tSettings(const std::string& fileName)
	:m_ConfigFileName(fileName)
{
	boost::property_tree::ptree PTree;
	boost::property_tree::xml_parser::read_xml(m_ConfigFileName, PTree);

	if (auto Value = PTree.get_child_optional("App.Settings.DB"))
	{
		auto ValueIter = (*Value).begin();

		if (ValueIter->first == "<xmlattr>")
		{
			DB.Host = ValueIter->second.get<std::string>("Host");
			DB.User = ValueIter->second.get<std::string>("User");
			DB.Passwd = ValueIter->second.get<std::string>("Passwd");
			DB.DB = ValueIter->second.get<std::string>("Name");
			DB.Port = ValueIter->second.get<std::uint16_t>("Port");
		}
	}
}

}