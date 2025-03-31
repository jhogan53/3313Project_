// File: server.cpp
// This HTTP server uses Boost.Beast, libpqxx, nlohmann/json, and jwt-cpp
// to implement user registration, login, and profile management endpoints.
// Endpoints:
//   POST /register: expects JSON { "username": "...", "password": "..." }
//   POST /login:    expects JSON { "username": "...", "password": "..." }
//                   On success returns a JWT token (expires in 1 hour),
//                   username, and balance.
//   GET  /profile:  requires header "Authorization: Bearer <token>"
//                   Returns username and balance.
//   POST /deposit:  requires header "Authorization: Bearer <token>",
//                   JSON { "amount": <number> }
//   POST /withdraw: requires header "Authorization: Bearer <token>",
//                   JSON { "amount": <number> }
// NOTE: Passwords are stored in plaintext for demonstration purposes only.

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <random>
#include <sstream>
#include <chrono>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include <jwt-cpp/jwt.h> // jwt-cpp header

using json = nlohmann::json;
namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

// Connection string to your YugabyteDB (database "yugabyte")
std::string db_connection_str =
    "postgresql://admin:hmUFdhyZfSQb_aOwiv8KwXXr7XpUtn@"
    "ca-central-1.0043d35e-0abb-460d-8940-1948fd1bba9e.aws.yugabyte.cloud:5433/"
    "yugabyte?ssl=true&sslmode=verify-full&sslrootcert=certs/root.crt";

// Define a secret key for JWT signing (store securely in production)
const std::string jwt_secret = "my_super_secret_key";

// Helper: generate a JWT token with a 1-hour expiration.
std::string generate_jwt_token(const std::string &username)
{
    auto token = jwt::create()
                     .set_issuer("auction_server")
                     .set_payload_claim("username", jwt::claim(username))
                     .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(1))
                     .sign(jwt::algorithm::hs256{jwt_secret});
    return token;
}

// Helper: verify JWT token; return username if valid, or empty string if invalid.
std::string verify_jwt_token(const std::string &token)
{
    try
    {
        auto decoded = jwt::decode(token);
        jwt::verifier<jwt::default_clock> verifier(jwt::algorithm::hs256{jwt_secret});
        verifier.verify(decoded);
        return decoded.get_payload_claim("username").as_string();
    }
    catch (...)
    {
        return "";
    }
}

// Helper: Create a JSON error response with CORS header.
http::response<http::string_body> make_response(
    http::request<http::string_body> const &req,
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

// Helper: Extract token from "Authorization: Bearer <token>" header.
std::string extract_token(http::request<http::string_body> const &req)
{
    auto auth = req[http::field::authorization];
    if (auth.empty())
        return "";
    std::string authStr(auth.data(), auth.size());
    const std::string prefix = "Bearer ";
    if (authStr.compare(0, prefix.size(), prefix) == 0)
        return authStr.substr(prefix.size());
    return "";
}

// Handle /register endpoint.
http::response<http::string_body> handle_register(http::request<http::string_body> const &req)
{
    try
    {
        auto j = json::parse(req.body());
        std::string username = j.at("username").get<std::string>();
        std::string password = j.at("password").get<std::string>();

        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");

        pqxx::work W(C);
        auto result = W.exec("SELECT user_id FROM users WHERE username = $1", pqxx::params(username));
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

// Handle /login endpoint.
http::response<http::string_body> handle_login(http::request<http::string_body> const &req)
{
    try
    {
        auto j = json::parse(req.body());
        std::string username = j.at("username").get<std::string>();
        std::string password = j.at("password").get<std::string>();

        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");

        pqxx::work W(C);
        auto result = W.exec("SELECT password, balance FROM users WHERE username = $1", pqxx::params(username));
        if (result.empty())
            return make_response(req, 400, "Invalid username or password");

        std::string stored_password = result[0]["password"].as<std::string>();
        if (stored_password != password)
            return make_response(req, 400, "Invalid username or password");

        std::string token = generate_jwt_token(username);

        json res_json;
        res_json["message"] = "Login successful";
        res_json["token"] = token;
        res_json["username"] = username;
        res_json["balance"] = result[0]["balance"].as<std::string>();
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

// Handle /profile endpoint (GET): returns username and balance.
http::response<http::string_body> handle_profile(http::request<http::string_body> const &req)
{
    std::string token = extract_token(req);
    if (token.empty())
        return make_response(req, 401, "Missing token");

    std::string username = verify_jwt_token(token);
    if (username.empty())
        return make_response(req, 401, "Invalid or expired token");

    try
    {
        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");
        pqxx::work W(C);
        auto result = W.exec("SELECT balance FROM users WHERE username = $1", pqxx::params(username));
        if (result.empty())
            return make_response(req, 404, "User not found");

        json res_json;
        res_json["username"] = username;
        res_json["balance"] = result[0]["balance"].as<std::string>();
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

// Handle /deposit endpoint.
http::response<http::string_body> handle_deposit(http::request<http::string_body> const &req)
{
    std::string token = extract_token(req);
    if (token.empty())
        return make_response(req, 401, "Missing token");

    std::string username = verify_jwt_token(token);
    if (username.empty())
        return make_response(req, 401, "Invalid or expired token");

    try
    {
        auto j = json::parse(req.body());
        double amount = j.at("amount").template get<double>();
        if (amount <= 0)
            return make_response(req, 400, "Deposit amount must be positive");

        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");
        pqxx::work W(C);
        W.exec("UPDATE users SET balance = balance + $1 WHERE username = $2", pqxx::params(amount, username));
        W.commit();

        json res_json;
        res_json["message"] = "Deposit successful";
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

// Handle /withdraw endpoint.
http::response<http::string_body> handle_withdraw(http::request<http::string_body> const &req)
{
    std::string token = extract_token(req);
    if (token.empty())
        return make_response(req, 401, "Missing token");

    std::string username = verify_jwt_token(token);
    if (username.empty())
        return make_response(req, 401, "Invalid or expired token");

    try
    {
        auto j = json::parse(req.body());
        double amount = j.at("amount").template get<double>();
        if (amount <= 0)
            return make_response(req, 400, "Withdrawal amount must be positive");

        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");
        pqxx::work W(C);
        auto result = W.exec("SELECT balance FROM users WHERE username = $1", pqxx::params(username));
        if (result.empty())
            return make_response(req, 404, "User not found");

        double balance = std::stod(result[0]["balance"].as<std::string>());
        if (balance < amount)
            return make_response(req, 400, "Insufficient funds");

        W.exec("UPDATE users SET balance = balance - $1 WHERE username = $2", pqxx::params(amount, username));
        W.commit();

        json res_json;
        res_json["message"] = "Withdrawal successful";
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
void do_session(beast::tcp_stream &stream)
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
        else if (req.target() == "/deposit")
            res = handle_deposit(req);
        else if (req.target() == "/withdraw")
            res = handle_withdraw(req);
        else
            res = make_response(req, 404, "Not Found");
    }
    else if (req.method() == http::verb::get)
    {
        if (req.target() == "/profile")
            res = handle_profile(req);
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

    auto &sock = boost::beast::get_lowest_layer(stream);
    sock.close();
}

// Main server: Listens on port 9002 and spawns a session per connection.
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
