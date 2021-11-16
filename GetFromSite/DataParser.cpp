#include <devConfig.h>
#include <devSettings.h>

#include <utilsDBNavi.h>

#include <cstdlib>
#include <iostream>//TEST
#include <string>

#include <boost/filesystem.hpp>
#include <boost/json.hpp>

std::time_t ToTime(const std::string& format, const std::string& value)
{
	std::tm time{};
	std::istringstream SStream(value);
	SStream.imbue(std::locale("en_US.utf-8"));
	SStream >> std::get_time(&time, format.c_str());
	if (SStream.fail())
		return {};

	return std::mktime(&time);
}

void StoreGLONASS(utils::db::tDBNavi& a_dbNavi, const boost::json::value& a_value)
{
	utils::db::tDBNavi::tTableSat tblSat;

	for (auto& sat : a_value.as_array())
	{
		std::cout
			<< "----------------------------"
			<< boost::json::value_to<std::string>(sat.at("point")) << " "
			<< boost::json::value_to<std::string>(sat.at("datetime")) << " "
			<< boost::json::value_to<std::string>(sat.at("SVN")) << " "
			<< boost::json::value_to<std::string>(sat.at("plane")) << " "
			<< boost::json::value_to<std::string>(sat.at("slot")) << " "
			<< boost::json::value_to<std::string>(sat.at("DL")) << " "
			<< boost::json::value_to<std::string>(sat.at("DC")) << " "
			<< boost::json::value_to<std::string>(sat.at("AESV")) << " "
			<< boost::json::value_to<std::string>(sat.at("typeID")) << " "
			<< boost::json::value_to<std::string>(sat.at("typeKA")) << " "
			<< boost::json::value_to<std::string>(sat.at("HSA")) << " "
			<< boost::json::value_to<std::string>(sat.at("NORAD")) << " "
			<< boost::json::value_to<std::string>(sat.at("COSMOS")) << " "
			<< boost::json::value_to<std::string>(sat.at("status")) << '\n';

		utils::db::tDBNavi::tTableSatRow satRow{};

		satRow.GNSS = utils::tGNSSCode::GLONASS;
		satRow.Slot = utils::Read<uint8_t>(boost::json::value_to<std::string>(sat.at("point")).c_str(), utils::tRadix::dec);
		satRow.Plane = boost::json::value_to<std::string>(sat.at("plane"));
		satRow.SVN = utils::Read<uint16_t>(boost::json::value_to<std::string>(sat.at("SVN")).c_str(), utils::tRadix::dec);
		satRow.Channel = utils::Read<int8_t>(boost::json::value_to<std::string>(sat.at("slot")).c_str(), utils::tRadix::dec);
		satRow.NORAD = utils::Read<uint16_t>(boost::json::value_to<std::string>(sat.at("NORAD")).c_str(), utils::tRadix::dec);
		satRow.COSMOS = utils::Read<uint16_t>(boost::json::value_to<std::string>(sat.at("COSMOS")).c_str(), utils::tRadix::dec);
		satRow.TypeKA = boost::json::value_to<std::string>(sat.at("typeKA"));
		satRow.HSA = boost::json::value_to<std::string>(sat.at("HSA")) == "+";

		const std::string strHSE = boost::json::value_to<std::string>(sat.at("HSE"));
		if (strHSE.size() == 16)
		{
			satRow.HSE = strHSE[0] == '+';
			satRow.HSE_DateTime = ToTime("%H:%M %d.%m.%y", strHSE.c_str() + 2);
		}

		const std::string strDL = boost::json::value_to<std::string>(sat.at("DL"));
		satRow.Launch_Date = ToTime("%d.%m.%y", strDL.c_str());

		const std::string strDC = boost::json::value_to<std::string>(sat.at("DC"));
		satRow.Intro_Date = ToTime("%d.%m.%y", strDC.c_str());

		const std::string strDDC = boost::json::value_to<std::string>(sat.at("DDC"));
		satRow.Outage_Date = ToTime("%d.%m.%y", strDDC.c_str());

		satRow.AESV = strtof(boost::json::value_to<std::string>(sat.at("AESV")).c_str(), nullptr);
		satRow.Status = utils::Read<uint8_t>(boost::json::value_to<std::string>(sat.at("status")).c_str(), utils::tRadix::dec);

		tblSat.push_back(satRow);
	}

	a_dbNavi.Update(tblSat);
}

void StoreGPS(utils::db::tDBNavi& a_dbNavi, const boost::json::value& a_value)
{
	utils::db::tDBNavi::tTableSat tblSat;

	for (auto& sat : a_value.as_array())
	{

		//"point":"1",
		//"datetime":"28.09.21",
		//"PRN":"24",
		//"plane":"A",
		//"DL":"04.10.12",
		//"DC":"14.11.12",
		//"DDC":" ",
		//"AESV":"106.5",
		//"typeID":"1",
		//"comment":"\u0418\u0441\u043f\u043e\u043b\u044c\u0437\u0443\u0435\u0442\u0441\u044f \u043f\u043e \u0426\u041d",
		//"typeKA":"II-F",
		//"NORAD":"38833",
		//"color":"#ffffff"

		std::cout
			<< "----------------------------"
			<< boost::json::value_to<std::string>(sat.at("point")) << " "
			<< boost::json::value_to<std::string>(sat.at("datetime")) << " "
			<< boost::json::value_to<std::string>(sat.at("PRN")) << " "
			<< boost::json::value_to<std::string>(sat.at("plane")) << " "
			<< boost::json::value_to<std::string>(sat.at("DL")) << " "
			<< boost::json::value_to<std::string>(sat.at("DC")) << " "
			<< boost::json::value_to<std::string>(sat.at("AESV")) << " "
			<< boost::json::value_to<std::string>(sat.at("typeID")) << " "
			<< boost::json::value_to<std::string>(sat.at("typeKA")) << " "
			<< boost::json::value_to<std::string>(sat.at("NORAD")) << '\n';


		utils::db::tDBNavi::tTableSatRow satRow{};

		satRow.GNSS = utils::tGNSSCode::GPS;

		//Channel may be wrong
		satRow.Channel = utils::Read<uint8_t>(boost::json::value_to<std::string>(sat.at("point")).c_str(), utils::tRadix::dec);

		satRow.PRN = utils::Read<uint16_t>(boost::json::value_to<std::string>(sat.at("PRN")).c_str(), utils::tRadix::dec);
		satRow.Plane = boost::json::value_to<std::string>(sat.at("plane"));
		satRow.NORAD = utils::Read<uint16_t>(boost::json::value_to<std::string>(sat.at("NORAD")).c_str(), utils::tRadix::dec);
		satRow.TypeKA = boost::json::value_to<std::string>(sat.at("typeKA"));

		const std::string strDL = boost::json::value_to<std::string>(sat.at("DL"));
		satRow.Launch_Date = ToTime("%d.%m.%y", strDL.c_str());

		const std::string strDC = boost::json::value_to<std::string>(sat.at("DC"));
		satRow.Intro_Date = ToTime("%d.%m.%y", strDC.c_str());

		const std::string strDDC = boost::json::value_to<std::string>(sat.at("DDC"));
		satRow.Outage_Date = ToTime("%d.%m.%y", strDDC.c_str());

		satRow.AESV = strtof(boost::json::value_to<std::string>(sat.at("AESV")).c_str(), nullptr);

		tblSat.push_back(satRow);
	}

	a_dbNavi.Update(tblSat);
}