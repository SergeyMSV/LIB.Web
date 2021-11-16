#pragma once

#include <optional>
#include <string>

namespace utils
{
	namespace web
	{

std::optional<std::string> HttpsReqSync(const std::string& a_host, const std::string& a_target);

	}
}