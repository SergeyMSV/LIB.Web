///////////////////////////////////////////////////////////////////////////////////////////////////
// utilsDBNav.h
//
// Standard ISO/IEC 114882, C++17
//
// MariaDB 10.4
// 
// |   version  |    release    | Description
// |------------|---------------|---------------------------------
// |      1     |   2020 03 31  |
// |            |               | 
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <libConfig.h>

#include "utilsBase.h"

#include <mysql.h>

#include <list>
#include <mutex>
#include <string>
#include <vector>
#include <utility>

#include <ctime>

namespace utils
{
	namespace db
	{

class tDBNavi
{
	using tLockGuard = std::lock_guard<std::recursive_mutex>;

	using tSQLQueryParamPair = std::pair<std::string, std::string>;
	using tSQLQueryParam = std::vector<tSQLQueryParamPair>;

	mutable std::recursive_mutex m_MySQLMtx;
	mutable MYSQL m_MySQL{};

	const std::string m_DB;

	std::uint8_t m_RcvID = 0xFF;

	std::time_t m_Timestamp{};

public:
	struct tTablePosSatBulkRow
	{
		my_ulonglong pos_id = 0;
		char pos_id_ind = STMT_INDICATOR_NONE;
		int norad = 0;
		char norad_ind = STMT_INDICATOR_NONE;
		int elevation = 0;
		char elevation_ind = STMT_INDICATOR_NONE;
		int azimuth = 0;
		char azimuth_ind = STMT_INDICATOR_NONE;
		int snr = 0;
		char snr_ind = STMT_INDICATOR_NONE;
	};

	using tTablePosSatBulk = std::vector<tTablePosSatBulkRow>;

	struct tTableSatRow
	{
		std::uint16_t NORAD = 0;
		tGNSSCode GNSS = tGNSSCode::GLONASS;
		std::uint16_t PRN = 0;
		std::uint8_t Slot = 0;
		std::string Plane;
		std::uint16_t SVN = 0;
		std::int8_t Channel = 0;
		std::string TypeKA;
		bool HSA = false;
		bool HSE = false;
		std::time_t HSE_DateTime = 0;
		std::uint8_t Status = 0;
		//All about satellite
		std::uint16_t COSMOS = 0;
		std::time_t Launch_Date = 0;
		std::time_t Intro_Date = 0;
		std::time_t Outage_Date = 0;
		float AESV = 0;		

		bool operator==(const tTableSatRow& value) const;
		bool operator!=(const tTableSatRow& value) const;
	};

	using tTableRow = std::vector<std::string>;
	using tTable = std::list<tTableRow>;
	using tTableSat = std::list<tTableSatRow>;

	tDBNavi() = delete;
	//It's in order to store data from third-party sources
	tDBNavi(const std::string& host, const std::string& user, const std::string& passwd, const std::string& db, std::uint16_t port);
	//Open in order to store positions from connected GNSS-receiver
	tDBNavi(const std::string& host, const std::string& user, const std::string& passwd, const std::string& db, std::uint16_t port,
		const std::string& rcvModel, const std::string& rcvId);
	tDBNavi(const tDBNavi&) = delete;
	tDBNavi(tDBNavi&&) = delete;
	~tDBNavi();

	my_ulonglong Insert(const std::string& model, const std::string& id);
	my_ulonglong Insert(char gnss, const std::time_t dateTime, bool valid, double latitude, double longitude, double altitude, double speed, double course);
	void Insert(tTablePosSatBulk& table);
	void Update(const tTableSat& value);//[TBD] all work with data is here

	tTable GetTablePos() const;
	tTable GetTableRcv() const;
	tTableSatRow GetTableSatLatest(std::uint16_t norad) const;//[!]
	tTable GetTableSys() const;

	void Drop();

private:
	void Create();
	my_ulonglong Insert(const std::string& table, const tSQLQueryParam& prm);
	my_ulonglong Insert(const tTableSatRow& value);
	tTable Select(const std::string& query) const;
	void Delete(const std::string& query) const;

	void SetTimestamp();
	void CleanTableSat(std::size_t maxQty);

	std::string GetErrorMsg(MYSQL* mysql) const;
	std::string GetErrorMsg(MYSQL_STMT* stmt) const;

	static std::string ToString(const std::tm& time, bool showTime = true);
	static std::string ToString(const std::time_t& timestamp, bool showTime = true);
	static std::time_t ToDateTime(const std::string& format, const std::string& value);
	static std::time_t ToDateTime(const std::string& value);
	static std::time_t ToDate(const std::string& value);

	template<typename T>
	std::string ToString(T value)
	{
		std::stringstream Stream;

		if (std::is_floating_point<T>::value)
		{
			Stream << value;
		}
		else if (std::numeric_limits<T>::is_integer)
		{
			if (std::numeric_limits<T>::is_signed)
			{
				Stream << static_cast<int>(value);
			}
			else
			{
				Stream << static_cast<unsigned int>(value);
			}
		}

		return Stream.str();
	}

	template<>
	std::string ToString(tGNSSCode value)
	{
		return std::to_string(static_cast<std::uint8_t>(value));
	}
};

	}
}