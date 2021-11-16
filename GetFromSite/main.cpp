#include <devConfig.h>
#include <devSettings.h>

#include <utilsDBNavi.h>
#include <utilsWebHttpsReqSync.h>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/json.hpp>

//https://www.glonass-iac.ru/glonass/sostavOG/sostavOG_calc.php?lang=ru&sort=point
//https://www.glonass-iac.ru/gps/sostavOG/sostavOG_calc.php?lang=ru
// 
// PREFERABLE
//https://www.glonass-iac.ru/glonass/sostavOG/sostavOG_calc.php?lang=en&sort=point
//https://www.glonass-iac.ru/gps/sostavOG/sostavOG_calc.php?lang=en

void StoreGLONASS(utils::db::tDBNavi& a_dbNavi, const boost::json::value& value);
void StoreGPS(utils::db::tDBNavi& a_dbNavi, const boost::json::value& a_value);

utils::tExitCode RetrieveData(const std::string& a_target, std::function<void(utils::db::tDBNavi&, const boost::json::value&)> a_handler);

int main(int argc, char** argv)
{
	try
	{
		const boost::filesystem::path Path{ argv[0] };
		boost::filesystem::path PathFile = Path.filename();
		if (PathFile.has_extension())
			PathFile.replace_extension();

		const std::string FileNameConf = PathFile.string() + ".conf";
		dev::g_Settings = dev::tSettings(FileNameConf);
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";

		return static_cast<int>(utils::tExitCode::EX_CONFIG);
	}

	utils::tExitCode CErr = RetrieveData("/glonass/sostavOG/sostavOG_calc.php?lang=en&sort=point", StoreGLONASS);
	if (CErr != utils::tExitCode::EX_OK)
		return static_cast<int>(CErr);

	CErr = RetrieveData("/gps/sostavOG/sostavOG_calc.php?lang=en", StoreGPS);
	if (CErr != utils::tExitCode::EX_OK)
		return static_cast<int>(CErr);

	return static_cast<int>(utils::tExitCode::EX_OK);
}

utils::tExitCode RetrieveData(const std::string& a_target, std::function<void(utils::db::tDBNavi&, const boost::json::value&)> a_handler)
{
	std::optional<std::string> resData = utils::web::HttpsReqSync("glonass-iac.ru", a_target);

	if (!resData.has_value())
		return utils::tExitCode::EX_UNAVAILABLE;

	std::string resDataText(std::move(resData.value()));

	try
	{
		boost::json::value jsonData = boost::json::parse(resDataText);

		utils::db::tDBNavi dbNavi(dev::g_Settings.DB.Host, dev::g_Settings.DB.User, dev::g_Settings.DB.Passwd, dev::g_Settings.DB.DB, dev::g_Settings.DB.Port);

		for (const auto& i : jsonData.as_array())
		{
			if (boost::json::value_to<std::string>(i.at("name")) == "OG")
			{
				const auto& satellites = i.at("data");

				a_handler(dbNavi, satellites);
			}
		}
	}
	catch (std::exception const& exJson)
	{
		std::cerr << "Error: " << exJson.what() << std::endl;

		return utils::tExitCode::EX_DATAERR;
	}

	return utils::tExitCode::EX_OK;
}