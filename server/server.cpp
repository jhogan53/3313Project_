// File: server.cpp
// This HTTP server uses Boost.Beast, libpqxx, and nlohmann/json to implement
// user registration and login endpoints (/register and /login).
// It expects POST requests with JSON bodies containing "username" and "password".
// NOTE: Passwords are stored in plaintext for demonstration purposes only.
// In production, always store hashed passwords.

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

// Connection string to your YugabyteDB.
// Make sure the certs folder (with root.crt) is in your server folder.
std::string db_connection_str =
    "postgresql://admin:hmUFdhyZfSQb_aOwiv8KwXXr7XpUtn@"
    "ca-central-1.0043d35e-0abb-460d-8940-1948fd1bba9e.aws.yugabyte.cloud:5433/"
    "yugabyte?ssl=true&sslmode=verify-full&sslrootcert=certs/root.crt";

// Helper: Create a JSON error response with CORS header.
template <class Body, class Allocator>
http::response<http::string_body> make_response(
    http::request<Body, http::basic_fields<Allocator>> const &req,
    int code,
    const std::string &message)
{
    http::response<http::string_body> res{static_cast<http::status>(code), req.version()};
    res.set(http::field::content_type, "application/json");
    res.set(http::field::access_control_allow_origin, "*");
    res.keep_alive(req.keep_alive());
    json j;
    j["error"] = message;
    res.body() = j.dump();
    res.prepare_payload();
    return res;
}

// Handle the /register endpoint.
template <class Body, class Allocator>
http::response<http::string_body> handle_register(
    http::request<Body, http::basic_fields<Allocator>> const &req)
{
    try
    {
        auto j = json::parse(req.body());
        std::string username = j.at("username").template get<std::string>();
        std::string password = j.at("password").template get<std::string>();

        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");

        pqxx::work W(C);
        auto result = W.exec("SELECT id FROM users WHERE username = $1", pqxx::params(username));
        if (!result.empty())
            return make_response(req, 400, "Username already exists");

        W.exec("INSERT INTO users (username, password) VALUES ($1, $2)", pqxx::params(username, password));
        W.commit();

        json res_json;
        res_json["message"] = "User registered successfully";
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.set(http::field::access_control_allow_origin, "*");
        res.keep_alive(req.keep_alive());
        res.body() = res_json.dump();
        res.prepare_payload();
        return res;
    }
    catch (const std::exception &e)
    {
        return make_response(req, 500, e.what());
    }
}

// Handle the /login endpoint.
template <class Body, class Allocator>
http::response<http::string_body> handle_login(
    http::request<Body, http::basic_fields<Allocator>> const &req)
{
    try
    {
        auto j = json::parse(req.body());
        std::string username = j.at("username").template get<std::string>();
        std::string password = j.at("password").template get<std::string>();

        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");

        pqxx::work W(C);
        auto result = W.exec("SELECT password FROM users WHERE username = $1", pqxx::params(username));
        if (result.empty())
            return make_response(req, 400, "Invalid username or password");

        std::string stored_password = result[0]["password"].as<std::string>();
        if (stored_password != password)
            return make_response(req, 400, "Invalid username or password");

        json res_json;
        res_json["message"] = "Login successful";
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.set(http::field::access_control_allow_origin, "*");
        res.keep_alive(req.keep_alive());
        res.body() = res_json.dump();
        res.prepare_payload();
        return res;
    }
    catch (const std::exception &e)
    {
        return make_response(req, 500, e.what());
    }
}

// Session handler: Reads a request, routes it, and writes a response.
template <class Stream>
void do_session(Stream &stream)
{
    beast::error_code ec;
    beast::flat_buffer buffer;

    http::request<http::string_body> req;
    http::read(stream, buffer, req, ec);
    if (ec == http::error::end_of_stream)
        return;
    if (ec)
    {
        std::cerr << "read: " << ec.message() << "\n";
        return;
    }

    http::response<http::string_body> res;
    if (req.method() == http::verb::post)
    {
        if (req.target() == "/register")
            res = handle_register(req);
        else if (req.target() == "/login")
            res = handle_login(req);
        else
            res = make_response(req, 404, "Not Found");
    }
    else
    {
        res = make_response(req, 405, "Method Not Allowed");
    }

    http::write(stream, res, ec);
    if (ec)
        std::cerr << "write: " << ec.message() << "\n";

    // Close the underlying socket.
    auto &sock = boost::beast::get_lowest_layer(stream);
    sock.close();
}

// Main server: Listens on port 9002 and spawns a session for each connection.
int main()
{
    try
    {
        auto const address = net::ip::make_address("0.0.0.0");
        unsigned short port = 9002;
        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {address, port}};
        std::cout << "HTTP server started on port " << port << std::endl;

        for (;;)
        {
            tcp::socket socket{ioc};
            acceptor.accept(socket);
            beast::tcp_stream stream(std::move(socket));
            std::thread([s = std::move(stream)]() mutable
                        { do_session(s); })
                .detach();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
