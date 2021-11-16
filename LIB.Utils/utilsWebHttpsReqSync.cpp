#include "utilsWebHttpsReqSync.h"

#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

//#include <libs/beast/example/common/root_certificates.hpp>

namespace utils
{
	namespace web
	{

std::optional<std::string> HttpsReqSync(const std::string& a_host, const std::string& a_target)
{
	const std::string RemotePort = "443";
	const int RemoteVersion = 11;//"1.0" -> 10; "1.1" -> 11

	std::string resDataText;

	try
	{
		boost::asio::io_context ioc;

		boost::asio::ssl::context ctx{ boost::asio::ssl::context::tlsv12_client };
		ctx.set_verify_mode(boost::asio::ssl::verify_none);//It's nothing to verify actually this sw sends nothing to the site.

		boost::asio::ip::tcp::resolver Resolver(ioc);
		boost::beast::ssl_stream<boost::beast::tcp_stream> TcpStream(ioc, ctx);

		//Set SNI Hostname (many hosts need this to handshake successfully)
		if (!SSL_set_tlsext_host_name(TcpStream.native_handle(), a_host.c_str()))
		{
			boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
			throw boost::beast::system_error{ ec };
		}

		boost::system::error_code cerrResolve;
		const auto results = Resolver.resolve(a_host, RemotePort, cerrResolve);//Look up the domain name
		
		boost::beast::get_lowest_layer(TcpStream).connect(results);//Make the connection on the IP address we get from a lookup

		TcpStream.handshake(boost::asio::ssl::stream_base::client);//Perform the SSL handshake

		// Set up an HTTP GET request message
		boost::beast::http::request<boost::beast::http::string_body> req{ boost::beast::http::verb::get, a_target, RemoteVersion };
		req.set(boost::beast::http::field::host, a_host);
		req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);

		boost::beast::http::write(TcpStream, req);//Send the HTTP request to the remote host

		boost::beast::flat_buffer buffer;//This buffer is used for reading and must be persisted

		boost::beast::http::response<boost::beast::http::dynamic_body> res;//Declare a container to hold the response

		boost::beast::http::read(TcpStream, buffer, res);//Receive the HTTP response

		const auto resData = res.body().data();
		resDataText = std::string(boost::asio::buffers_begin(resData), boost::asio::buffers_end(resData));

		boost::beast::error_code cerrBeast;
		TcpStream.shutdown(cerrBeast);
		if (cerrBeast == boost::asio::error::eof)
		{
			// Rationale:
			// http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
			cerrBeast = {};
		}

		if (cerrBeast == boost::asio::ssl::error::stream_truncated)
		{
			// Rationale:
			// https://github.com/boostorg/beast/issues/824
			cerrBeast = {};
		}

		if (cerrBeast)
		{
			throw boost::beast::system_error{ cerrBeast };
		}
	}
	catch (std::exception const& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return {};
	}

	return resDataText;
}

}
}