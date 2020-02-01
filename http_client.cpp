#include "http_client.h"

#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <iostream>

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

HttpClient::HttpClient(const std::string& host, const std::string& port):
    _host(host), _port(port) {}

HttpClient::~HttpClient()
{
}


http::response<http::dynamic_body> HttpClient::get(http::request<http::string_body>& req) const
{
    // Declare a container to hold the response
    http::response<http::dynamic_body> res;

    try {
        net::io_context ioc;
        // These objects perform our I/O
        tcp::resolver resolver{ioc};
        tcp::socket socket{ioc};

        // Look up the domain name
        auto const results = resolver.resolve(_host, _port);

        // Make the connection on the IP address we get from a lookup
        boost::asio::connect(socket, results.begin(), results.end());
        req.set(http::field::host, _host);

        // Send the HTTP request to the remote host
        http::write(socket, req);

        // This buffer is used for reading and must be persisted
        boost::beast::flat_buffer buffer;

        // Receive the HTTP response
        http::read(socket, buffer, res);

        // Gracefully close the socket
        boost::system::error_code ec;
        socket.shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if(ec && ec != boost::system::errc::not_connected)
            throw boost::system::system_error{ec};

        // If we get here then the connection is closed gracefully
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return res;
}
