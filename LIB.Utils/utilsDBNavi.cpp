#include "utilsDBNavi.h"

#include <mysqld_error.h>

#include <exception>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <utility>

namespace utils
{
	namespace db
	{

#define LIB_UTILS_DB_DBNAVI_VERSION "DBNavi_1"

tDBNavi::tDBNavi(const std::string& host, const std::string& user, const std::string& passwd, const std::string& db, std::uint16_t port)
	:m_DB(db)
{
	mysql_init(&m_MySQL);

	if (&m_MySQL == nullptr)
		throw std::runtime_error{ GetErrorMsg(&m_MySQL) };

	mysql_real_connect(&m_MySQL, host.c_str(), user.c_str(), passwd.c_str(), "", port, "", 0);//DB is not specified intentionally

	if (mysql_errno(&m_MySQL))
		throw std::runtime_error{ GetErrorMsg(&m_MySQL) };

	if (mysql_query(&m_MySQL, std::string("USE " + m_DB).c_str()))
	{
		const unsigned int Cerr = mysql_errno(&m_MySQL);

		if (Cerr != ER_BAD_DB_ERROR)
			throw std::runtime_error{ GetErrorMsg(&m_MySQL) };
	}

	SetTimestamp();

	Create();

	const tTable TableSys = GetTableSys();

	if (TableSys.size() != 1 || TableSys.front().size() != 2 || TableSys.front()[1] != LIB_UTILS_DB_DBNAVI_VERSION)
		throw std::runtime_error{ "Wrong DEV_DB_VERSION" };

	if (mysql_errno(&m_MySQL))
		throw std::runtime_error{ GetErrorMsg(&m_MySQL) };
}

tDBNavi::tDBNavi(const std::string& host, const std::string& user, const std::string& passwd, const std::string& db, std::uint16_t port,
				const std::string& rcvModel, const std::string& rcvId)
	:tDBNavi(host, user, passwd, db, port)
{
	for (int i = 0; i < 2; ++i)//if the receiver is not on the list - insert it and repeate the search
	{
		tTable TableRCV = GetTableRcv();

		for (tTableRow& row : TableRCV)
		{
			if (row.size() != 4)
				throw std::runtime_error{ "Wrong TableRCV rows qty" };

			if (row[2] == rcvModel && row[3] == rcvId)
			{
				m_RcvID = utils::Read<std::uint8_t>(row[0].cbegin(), row[0].cend(), tRadix::dec);
				return;
			}
		}

		Insert(rcvModel, rcvId);
	}

	if (mysql_errno(&m_MySQL))
		throw std::runtime_error{ GetErrorMsg(&m_MySQL) };
}

tDBNavi::~tDBNavi()
{
	mysql_close(&m_MySQL);
}

my_ulonglong tDBNavi::Insert(const std::string& model, const std::string& id)
{
	const tSQLQueryParam Query
	{
		{"timestamp", ToString(m_Timestamp)},
		{"model", model},
		{"id", id},
	};

	return Insert("rcv", Query);
}

my_ulonglong tDBNavi::Insert(char gnss, const std::time_t dateTime, bool valid, double latitude, double longitude, double altitude, double speed, double course)
{
	const tSQLQueryParam Query
	{
		{"timestamp", ToString(m_Timestamp)},
		{"gnss", ToString(gnss)},//[TBD] - that's GNSS_State - pos can from two or more GNSSs
		{"datetime", ToString(dateTime)},
		{"valid", ToString(valid)},
		{"latitude", ToString(latitude)},
		{"longitude", ToString(longitude)},
		{"altitude", ToString(altitude)},
		{"speed", ToString(speed)},
		{"course", ToString(course)},
		{"rcv_id", ToString(m_RcvID)},
	};

	return Insert("pos", Query);
}

void tDBNavi::Insert(tTablePosSatBulk& table)
{
	SetTimestamp();

	tLockGuard Lock(m_MySQLMtx);

	MYSQL_STMT* Stmt = mysql_stmt_init(&m_MySQL);

	if (Stmt == nullptr)
		throw std::runtime_error{ GetErrorMsg(Stmt) };

	const unsigned int ColumnQty = 5;

	if (mysql_stmt_prepare(Stmt, "INSERT INTO pos_sat VALUES (?,?,?,?,?)", -1))//[TBD] - columns
		throw std::runtime_error{ GetErrorMsg(Stmt) };

	std::unique_ptr<MYSQL_BIND> Bind{ new MYSQL_BIND[ColumnQty] };

	auto InsertBulk = [=, &table](MYSQL_BIND* bind, std::size_t& count) -> void
	{
		if (count >= table.size())
			return;//No Error at least from DB

		const std::size_t RowSize = sizeof(tTablePosSatBulkRow);
		const std::size_t ArraySize = table.size();
		//std::size_t ArraySize = table.size() - count;
		//if (ArraySize > 16)
		//{
		//	ArraySize = 16;
		//}

		memset(bind, 0, sizeof(MYSQL_BIND) * ColumnQty);

		bind[0].buffer = &table[count].pos_id;
		bind[0].buffer_type = MYSQL_TYPE_LONG;
		bind[0].u.indicator = &table[count].pos_id_ind;

		bind[1].buffer = &table[count].norad;
		bind[1].buffer_type = MYSQL_TYPE_LONG;
		bind[1].u.indicator = &table[count].norad_ind;

		bind[2].buffer = &table[count].elevation;
		bind[2].buffer_type = MYSQL_TYPE_LONG;
		bind[2].u.indicator = &table[count].elevation_ind;

		bind[3].buffer = &table[count].azimuth;
		bind[3].buffer_type = MYSQL_TYPE_LONG;
		bind[3].u.indicator = &table[count].azimuth_ind;

		bind[4].buffer = &table[count].snr;
		bind[4].buffer_type = MYSQL_TYPE_LONG;
		bind[4].u.indicator = &table[count].snr_ind;

		if (mysql_stmt_attr_set(Stmt, STMT_ATTR_ARRAY_SIZE, &ArraySize))
			throw std::runtime_error{ GetErrorMsg(Stmt) };

		if (mysql_stmt_attr_set(Stmt, STMT_ATTR_ROW_SIZE, &RowSize))
			throw std::runtime_error{ GetErrorMsg(Stmt) };

		if (mysql_stmt_bind_param(Stmt, bind))
			throw std::runtime_error{ GetErrorMsg(Stmt) };

		if (mysql_stmt_execute(Stmt))
			throw std::runtime_error{ GetErrorMsg(Stmt) };

		count += ArraySize;

		return;
	};

	std::size_t Count = 0;
	while (Count < table.size())
	{
		InsertBulk(Bind.get(), Count);
	}

	mysql_stmt_close(Stmt);
}

void tDBNavi::Update(const tTableSat& value)
{
	SetTimestamp();

	for (const auto& i : value)
	{
		const utils::db::tDBNavi::tTableSatRow satRow = GetTableSatLatest(i.NORAD);
		if (satRow != i)
		{
			Insert(i);
		}
	}

	CleanTableSat(LIB_UTILS_DB_DBNAVI_TABLE_SAT_MAX);//[TBD] set it with define in config
}

tDBNavi::tTable tDBNavi::GetTablePos() const
{
	return Select("SELECT * FROM pos");
}

tDBNavi::tTable tDBNavi::GetTableRcv() const
{
	return Select("SELECT * FROM rcv");
}

tDBNavi::tTableSatRow tDBNavi::GetTableSatLatest(std::uint16_t norad) const
{
	const tDBNavi::tTable Rows = Select("SELECT * FROM sat WHERE norad=" + std::to_string(norad) + " ORDER BY timestamp DESC LIMIT 1");

	for (const auto& i : Rows)
	{
		assert(i.size() == 18);

		tTableSatRow row{};

		int Index = 0;
		row.NORAD = utils::Read<std::uint16_t>(i[++Index].c_str(), tRadix::dec);
		row.GNSS = utils::Read<tGNSSCode>(i[++Index].c_str(), tRadix::dec);
		row.PRN = utils::Read<std::uint16_t>(i[++Index].c_str(), tRadix::dec);
		row.Slot = utils::Read<std::uint8_t>(i[++Index].c_str(), tRadix::dec);
		row.Plane = i[++Index];
		row.Channel = utils::Read<std::int8_t>(i[++Index].c_str(), tRadix::dec);
		row.SVN = utils::Read<std::uint16_t>(i[++Index].c_str(), tRadix::dec);
		row.TypeKA = i[++Index];
		row.HSA = utils::Read<bool>(i[++Index].c_str(), tRadix::dec);
		row.HSE = utils::Read<bool>(i[++Index].c_str(), tRadix::dec);
		row.HSE_DateTime = ToDateTime(i[++Index]);
		row.Status = utils::Read<std::uint8_t>(i[++Index].c_str(), tRadix::dec);
		row.COSMOS = utils::Read<std::uint16_t>(i[++Index].c_str(), tRadix::dec);
		row.Launch_Date = ToDate(i[++Index]);
		row.Intro_Date = ToDate(i[++Index]);
		row.Outage_Date = ToDate(i[++Index]);
		row.AESV = std::strtof(i[++Index].c_str(), nullptr);

		return row;
	}

	return {};
}

tDBNavi::tTable tDBNavi::GetTableSys() const
{
	return Select("SELECT * FROM sys");
}

void tDBNavi::Drop()
{
	tLockGuard Lock(m_MySQLMtx);

	if (mysql_query(&m_MySQL, std::string("DROP DATABASE IF EXISTS " + m_DB).c_str()))
		throw std::runtime_error{ GetErrorMsg(&m_MySQL) };
}

void tDBNavi::Create()
{
	tLockGuard Lock(m_MySQLMtx);

	if (mysql_query(&m_MySQL, std::string("CREATE DATABASE IF NOT EXISTS " + m_DB).c_str()))
		throw std::runtime_error{ GetErrorMsg(&m_MySQL) };

	if (mysql_query(&m_MySQL, std::string("USE " + m_DB).c_str()))
		throw std::runtime_error{ GetErrorMsg(&m_MySQL) };

	const std::vector<std::string> ReqList
	{
		"CREATE TABLE sys (timestamp DATETIME NOT NULL, version VARCHAR(20) NOT NULL DEFAULT '', PRIMARY KEY(timestamp, version));",
		"INSERT INTO sys (timestamp, version) VALUE('" + ToString(m_Timestamp) + "','" LIB_UTILS_DB_DBNAVI_VERSION "');",
		"CREATE TABLE rcv (rcv_id INT(2) NOT NULL AUTO_INCREMENT, timestamp DATETIME NOT NULL, model VARCHAR(50) NOT NULL DEFAULT '', id VARCHAR(50) NOT NULL DEFAULT '', PRIMARY KEY(rcv_id), UNIQUE INDEX(id));",
		"CREATE TABLE pos (pos_id INT(10) NOT NULL AUTO_INCREMENT, timestamp DATETIME NOT NULL, gnss INT(2), datetime DATETIME, valid BOOLEAN, latitude DOUBLE, longitude DOUBLE, altitude DOUBLE, speed FLOAT, course FLOAT, rcv_id INT(2) NOT NULL, PRIMARY KEY(pos_id), INDEX(timestamp));",
		"CREATE TABLE pos_sat (pos_id INT(10) NOT NULL, norad SMALLINT UNSIGNED NOT NULL, elevation INT(2), azimuth INT(3), snr INT(2), PRIMARY KEY(pos_id, norad));",
		"CREATE TABLE sat(timestamp DATETIME NOT NULL, norad SMALLINT UNSIGNED NOT NULL, gnss TINYINT UNSIGNED NOT NULL, prn SMALLINT UNSIGNED, slot TINYINT UNSIGNED NOT NULL, plane VARCHAR(3), channel TINYINT SIGNED, svn SMALLINT UNSIGNED, type_ka VARCHAR(10), hsa BOOL NOT NULL DEFAULT FALSE, hse BOOL NOT NULL DEFAULT FALSE, hse_datetime DATETIME, status TINYINT UNSIGNED, cosmos SMALLINT UNSIGNED, launch_date DATE, intro_date DATE, outage_date DATE, aesv FLOAT, PRIMARY KEY(timestamp, norad), INDEX(slot));"
	};

	for (auto& i : ReqList)
	{
		if (mysql_query(&m_MySQL, i.c_str()))
		{
			if (mysql_errno(&m_MySQL) == ER_TABLE_EXISTS_ERROR)
				return;

			throw std::runtime_error{ GetErrorMsg(&m_MySQL) };
		}
	}
}

my_ulonglong tDBNavi::Insert(const std::string& table, const tSQLQueryParam& prm)
{
	std::stringstream SStream;

	SStream << "INSERT INTO " << table << " (";

	for (std::size_t i = 0; i < prm.size(); ++i)
	{
		SStream << prm[i].first;

		if (i < prm.size() - 1)
		{
			SStream << ',';
		}
	}

	SStream << ") VALUE(";

	for (std::size_t i = 0; i < prm.size(); ++i)
	{
		SStream << "'" << prm[i].second << "'";

		if (i < prm.size() - 1)
		{
			SStream << ',';
		}
	}

	SStream << ");";

	const std::string ReqStr = SStream.str();

	tLockGuard Lock(m_MySQLMtx);

	if (mysql_real_query(&m_MySQL, ReqStr.c_str(), static_cast<unsigned long>(ReqStr.size())))//Contrary to the mysql_query() function, mysql_real_query is binary safe.
		throw std::runtime_error{ GetErrorMsg(&m_MySQL) };

	return mysql_insert_id(&m_MySQL);
}

my_ulonglong tDBNavi::Insert(const tTableSatRow& value)
{
	const tSQLQueryParam Query
	{
		{"timestamp", ToString(m_Timestamp)},
		{"norad", ToString(value.NORAD)},
		{"gnss", ToString(value.GNSS)},
		{"prn", ToString(value.PRN)},
		{"slot", ToString(value.Slot)},
		{"plane", value.Plane},
		{"channel", ToString(value.Channel)},
		{"svn", ToString(value.SVN)},
		{"type_ka", value.TypeKA},
		{"hsa", ToString(value.HSA)},
		{"hse", ToString(value.HSE)},
		{"hse_datetime", ToString(value.HSE_DateTime)},
		{"status", ToString(value.Status)},
		{"cosmos", ToString(value.COSMOS)},
		{"launch_date", ToString(value.Launch_Date, false)},
		{"intro_date", ToString(value.Intro_Date, false)},
		{"outage_date", ToString(value.Outage_Date, false)},
		{"aesv", ToString(value.AESV)},
	};

	return Insert("sat", Query);
}

tDBNavi::tTable tDBNavi::Select(const std::string& query) const
{
	tTable List;

	tLockGuard Lock(m_MySQLMtx);

	if (mysql_query(&m_MySQL, query.c_str()))
		throw std::runtime_error{ GetErrorMsg(&m_MySQL) };

	MYSQL_RES* Res = mysql_use_result(&m_MySQL);

	if (Res == nullptr)
		throw std::runtime_error{ GetErrorMsg(&m_MySQL) };

	unsigned int Count = mysql_num_fields(Res);

	while (true)//[#] true or [limit]
	{
		tTableRow Row;

		MYSQL_ROW AB = mysql_fetch_row(Res);

		if (AB == nullptr)
			break;

		for (unsigned int i = 0; i < Count; ++i)
		{
			Row.push_back(AB[i]);
		}

		List.push_back(std::forward<tTableRow>(Row));
	}

	mysql_free_result(Res);

	return List;
}

void tDBNavi::Delete(const std::string& query) const
{
	tLockGuard Lock(m_MySQLMtx);

	if (mysql_query(&m_MySQL, query.c_str()))
		throw std::runtime_error{ GetErrorMsg(&m_MySQL) };
}

void tDBNavi::SetTimestamp()
{
	m_Timestamp = std::time(nullptr);
}

void tDBNavi::CleanTableSat(std::size_t maxQty)
{
	const tTable tblSat = Select("SELECT norad FROM sat GROUP BY norad");
	for (const auto& sat : tblSat)
	{
		const tTable tblSatQty = Select("SELECT COUNT(*) FROM sat WHERE norad=" + sat[0] + " GROUP BY timestamp");

		if (tblSatQty.size() <= maxQty)
			continue;

		const size_t delDateQty = static_cast<int>(tblSatQty.size() - maxQty);
		int delRowQty = 0;
		size_t i = 0;
		for (const auto& row : tblSatQty)
		{
			delRowQty += utils::Read<int>(row[0].c_str(), utils::tRadix::dec);

			if (delDateQty <= ++i)
				break;
		}

		Delete("DELETE FROM sat WHERE norad=" + sat[0] + " ORDER BY timestamp LIMIT " + std::to_string(delRowQty));
	}
}

std::string tDBNavi::GetErrorMsg(MYSQL* mysql) const
{
	std::stringstream SStream;
	SStream << "MYSQL Error(" << mysql_errno(mysql) << ") [" << mysql_sqlstate(mysql) << "] \"" << mysql_error(mysql) << "\"";
	return SStream.str();
}

std::string tDBNavi::GetErrorMsg(MYSQL_STMT* stmt) const
{
	std::stringstream SStream;
	SStream << "MYSQL Error(" << mysql_stmt_errno(stmt) << ") [" << mysql_stmt_sqlstate(stmt) << "] \"" << mysql_stmt_error(stmt) << "\"";
	return SStream.str();
}

std::string tDBNavi::ToString(const std::tm& time, bool showTime)
{
	std::stringstream Stream;
	if (showTime)
	{
		Stream << std::put_time(&time, "%F %T");
	}
	else
	{
		Stream << std::put_time(&time, "%F");
	}
	return Stream.str();
}

std::string tDBNavi::ToString(const std::time_t& timestamp, bool showTime)
{
	return ToString(*std::localtime(&timestamp), showTime);
}

std::time_t tDBNavi::ToDateTime(const std::string& format, const std::string& value)
{
	std::tm time{};
	std::istringstream SStream(value);
	SStream.imbue(std::locale("en_US.utf-8"));
	SStream >> std::get_time(&time, format.c_str());
	if (SStream.fail())
		return {};

	std::time_t posixTime = std::mktime(&time);//1970-01-01 is returned as -1
	if (posixTime < 0)
		posixTime = 0;

	return posixTime;
}

std::time_t tDBNavi::ToDateTime(const std::string& value)
{
	return ToDateTime("%Y-%m-%d %H:%M:%S", value);
}

std::time_t tDBNavi::ToDate(const std::string& value)
{
	return ToDateTime("%Y-%m-%d", value);
}

bool tDBNavi::tTableSatRow::operator==(const tTableSatRow& value) const
{
	return
		NORAD == value.NORAD &&
		GNSS == value.GNSS &&
		PRN == value.PRN &&
		Slot == value.Slot &&
		Plane == value.Plane &&
		Channel == value.Channel &&
		SVN == value.SVN &&
		TypeKA == value.TypeKA &&
		HSA == value.HSA &&
		HSE == value.HSE &&
		//HSE_DateTime == value.HSE_DateTime &&	// not interesting
		Status == value.Status &&
		COSMOS == value.COSMOS &&
		Launch_Date == value.Launch_Date &&
		Intro_Date == value.Intro_Date &&
		Outage_Date == value.Outage_Date;
		//AESV == value.AESV;					// not interesting
}

bool tDBNavi::tTableSatRow::operator!=(const tTableSatRow& value) const
{
	return !(*this == value);
}

	}
}
