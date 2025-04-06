// File: server/server.cpp
// This HTTP server uses Boost.Beast, libpqxx, nlohmann/json, and jwt-cpp
// to implement user registration, login, profile management, and auction listing endpoints.
//
// Endpoints:
//   POST /register: expects JSON { "username": "...", "password": "..." }
//   POST /login:    expects JSON { "username": "...", "password": "..." }
//                   On success returns a JWT token (expires in 1 hour), username, and balance.
//   GET  /profile:  requires header "Authorization: Bearer <token>"
//                   Returns username and balance.
//   POST /deposit:  requires header "Authorization: Bearer <token>",
//                   JSON { "amount": <number> }
//   POST /withdraw: requires header "Authorization: Bearer <token>",
//                   JSON { "amount": <number> }
//   POST /create_listing: requires header "Authorization: Bearer <token>",
//                   JSON { "item_name": "...", "description": "...", "image_url": "...",
//                          "base_price": <number>, "start_delay": <number>, "live_duration": <number> }
//   POST /edit_listing: requires header "Authorization: Bearer <token>",
//                   JSON { "auction_id": <number>, "description": "new description" }
//   POST /make_live: requires header "Authorization: Bearer <token>",
//                   JSON { "auction_id": <number> }
//   POST /place_bid: requires header "Authorization: Bearer <token>",
//                   JSON { "auction_id": <number>, "bid_amount": <number> }
//   POST /end_auction: requires header "Authorization: Bearer <token>",
//                   JSON { "auction_id": <number> }  (ends the auction immediately and finalizes it)
//   POST /delete_listing: requires header "Authorization: Bearer <token>",
//                   JSON { "auction_id": <number> }
//   GET  /my_listings: requires header "Authorization: Bearer <token>"
//                   Returns all auctions created by the authenticated user (with highest bid info).
//   GET  /upcoming:  Returns auctions that have not yet gone live.
//   GET  /live:  Returns auctions that are currently live (with highest bid info).
//   GET  /current_time: Returns the server's current time in ISO 8601 format.
//   GET  /auction_result?auction_id=xxx: Returns the highest bid and bidder's username for the specified auction.
//
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
        jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{jwt_secret})
            .with_issuer("auction_server")
            .verify(decoded);
        return decoded.get_payload_claim("username").as_string();
    }
    catch (...)
    {
        return "";
    }
}

// Helper: Create a JSON error response with CORS header.
http::response<http::string_body> make_response(http::request<http::string_body> const &req,
                                                int code, const std::string &message)
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

// --------------------
// Endpoint Definitions
// --------------------

// POST /register
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

// POST /login
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

// GET /profile
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

// POST /deposit
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
        W.exec("UPDATE Users SET balance = balance + $1 WHERE username = $2", pqxx::params(amount, username));
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

// POST /withdraw
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
        auto result = W.exec("SELECT balance FROM Users WHERE username = $1", pqxx::params(username));
        if (result.empty())
            return make_response(req, 404, "User not found");
        double balance = std::stod(result[0]["balance"].as<std::string>());
        if (balance < amount)
            return make_response(req, 400, "Insufficient funds");
        W.exec("UPDATE Users SET balance = balance - $1 WHERE username = $2", pqxx::params(amount, username));
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

// POST /create_listing
http::response<http::string_body> handle_create_listing(http::request<http::string_body> const &req)
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
        std::string item_name = j.at("item_name").get<std::string>();
        std::string description = j.at("description").get<std::string>();
        std::string image_url = "";
        if (j.find("image_url") != j.end())
            image_url = j.at("image_url").get<std::string>();
        double base_price = j.at("base_price").get<double>();
        int start_delay = j.at("start_delay").get<int>();
        int live_duration = j.at("live_duration").get<int>();
        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");
        pqxx::work W(C);
        auto user_res = W.exec("SELECT user_id FROM Users WHERE username = $1", pqxx::params(username));
        if (user_res.empty())
            return make_response(req, 404, "User not found");
        int seller_id = user_res[0]["user_id"].as<int>();
        W.exec("INSERT INTO Auctions (seller_id, item_name, description, image_url, base_price, start_delay, live_duration) "
               "VALUES ($1, $2, $3, $4, $5, $6, $7)",
               pqxx::params(seller_id, item_name, description, image_url, base_price, start_delay, live_duration));
        W.commit();
        json res_json;
        res_json["message"] = "Auction listing created successfully";
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

// POST /edit_listing
http::response<http::string_body> handle_edit_listing(http::request<http::string_body> const &req)
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
        int auction_id = j.at("auction_id").get<int>();
        std::string new_description = j.at("description").get<std::string>();
        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");
        pqxx::work W(C);
        auto res_user = W.exec("SELECT user_id FROM Users WHERE username = $1", pqxx::params(username));
        if (res_user.empty())
            return make_response(req, 404, "User not found");
        int user_id = res_user[0]["user_id"].as<int>();
        auto res_auction = W.exec("SELECT seller_id FROM Auctions WHERE auction_id = $1", pqxx::params(auction_id));
        if (res_auction.empty())
            return make_response(req, 404, "Auction not found");
        int seller_id = res_auction[0]["seller_id"].as<int>();
        if (seller_id != user_id)
            return make_response(req, 403, "Not authorized to edit this listing");
        W.exec("UPDATE Auctions SET description = $1 WHERE auction_id = $2", pqxx::params(new_description, auction_id));
        W.commit();
        json res_json;
        res_json["message"] = "Listing description updated successfully";
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

// POST /make_live
// POST /make_live
http::response<http::string_body> handle_make_live(http::request<http::string_body> const &req)
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
        int auction_id = j.at("auction_id").get<int>();
        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");
        pqxx::work W(C);
        auto res_user = W.exec("SELECT user_id FROM Users WHERE username = $1", pqxx::params(username));
        if (res_user.empty())
            return make_response(req, 404, "User not found");
        int user_id = res_user[0]["user_id"].as<int>();
        auto res_auction = W.exec("SELECT seller_id FROM Auctions WHERE auction_id = $1", pqxx::params(auction_id));
        if (res_auction.empty())
            return make_response(req, 404, "Auction not found");
        int seller_id = res_auction[0]["seller_id"].as<int>();
        if (seller_id != user_id)
            return make_response(req, 403, "Not authorized to update this listing");
        // Update both start_delay and posted_time.
        W.exec("UPDATE Auctions SET start_delay = 0, posted_time = CURRENT_TIMESTAMP WHERE auction_id = $1", pqxx::params(auction_id));
        W.commit();
        json res_json;
        res_json["message"] = "Listing updated to live immediately";
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

// POST /place_bid
http::response<http::string_body> handle_place_bid(http::request<http::string_body> const &req)
{
    std::string token = extract_token(req);
    if (token.empty())
        return make_response(req, 401, "Missing token");
    std::string bidderUsername = verify_jwt_token(token);
    if (bidderUsername.empty())
        return make_response(req, 401, "Invalid or expired token");
    try
    {
        auto j = json::parse(req.body());
        int auction_id = j.at("auction_id").get<int>();
        double bid_amount = j.at("bid_amount").get<double>();

        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");
        pqxx::work W(C);

        // Get auction details.
        auto auctionRes = W.exec_params("SELECT seller_id, base_price FROM Auctions WHERE auction_id = $1", auction_id);
        if (auctionRes.empty())
            return make_response(req, 404, "Auction not found");
        int seller_id = auctionRes[0]["seller_id"].as<int>();
        double base_price = auctionRes[0]["base_price"].as<double>();

        // Get bidder details.
        auto bidderRes = W.exec_params("SELECT user_id, balance FROM Users WHERE username = $1", bidderUsername);
        if (bidderRes.empty())
            return make_response(req, 404, "Bidder not found");
        int bidder_id = bidderRes[0]["user_id"].as<int>();
        double bidder_balance = std::stod(bidderRes[0]["balance"].as<std::string>());

        if (bidder_id == seller_id)
            return make_response(req, 403, "Cannot bid on your own auction");

        auto bidRes = W.exec_params("SELECT bid_amount FROM Bids WHERE auction_id = $1 ORDER BY bid_amount DESC LIMIT 1", auction_id);
        double current_highest = base_price;
        if (!bidRes.empty())
            current_highest = bidRes[0]["bid_amount"].as<double>();

        if (bid_amount <= current_highest)
            return make_response(req, 400, "Bid must be higher than the current highest bid");
        if (bid_amount > bidder_balance)
            return make_response(req, 400, "Insufficient wallet balance to place bid");

        W.exec_params("INSERT INTO Bids (auction_id, bidder_id, bid_amount) VALUES ($1, $2, $3)",
                      auction_id, bidder_id, bid_amount);
        W.commit();

        json res_json;
        res_json["message"] = "Bid placed successfully";
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

// POST /end_auction
http::response<http::string_body> handle_end_auction(http::request<http::string_body> const &req)
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
        int auction_id = j.at("auction_id").get<int>();

        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");
        pqxx::work W(C);

        auto auctionRes = W.exec_params("SELECT seller_id, base_price, finalized FROM Auctions WHERE auction_id = $1", auction_id);
        if (auctionRes.empty())
            return make_response(req, 404, "Auction not found");
        int seller_id = auctionRes[0]["seller_id"].as<int>();
        bool finalized = auctionRes[0]["finalized"].as<bool>();
        if (finalized)
            return make_response(req, 400, "Auction already finalized");

        auto bidRes = W.exec_params("SELECT bid_amount, bidder_id FROM Bids WHERE auction_id = $1 ORDER BY bid_amount DESC LIMIT 1", auction_id);
        double highest_bid = 0.0;
        int highest_bidder = 0;
        if (!bidRes.empty())
        {
            highest_bid = bidRes[0]["bid_amount"].as<double>();
            highest_bidder = bidRes[0]["bidder_id"].as<int>();
        }
        if (highest_bid == 0.0)
        {
            W.exec_params("UPDATE Auctions SET live_duration = 0, finalized = true WHERE auction_id = $1", auction_id);
            W.commit();
            json res_json;
            res_json["message"] = "Auction ended with no bids.";
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.set(http::field::access_control_allow_origin, "*");
            res.keep_alive(req.keep_alive());
            res.body() = res_json.dump();
            res.prepare_payload();
            return res;
        }

        auto sellerRes = W.exec_params("SELECT balance FROM Users WHERE user_id = $1", seller_id);
        if (sellerRes.empty())
            return make_response(req, 404, "Seller not found");
        double seller_balance = std::stod(sellerRes[0]["balance"].as<std::string>());

        auto bidderRes = W.exec_params("SELECT balance FROM Users WHERE user_id = $1", highest_bidder);
        if (bidderRes.empty())
            return make_response(req, 404, "Highest bidder not found");
        double bidder_balance = std::stod(bidderRes[0]["balance"].as<std::string>());

        if (bidder_balance < highest_bid)
            return make_response(req, 400, "Highest bidder has insufficient funds");

        W.exec_params("UPDATE Users SET balance = balance - $1 WHERE user_id = $2", highest_bid, highest_bidder);
        W.exec_params("UPDATE Users SET balance = balance + $1 WHERE user_id = $2", highest_bid, seller_id);
        W.exec_params("UPDATE Auctions SET live_duration = 0, finalized = true WHERE auction_id = $1", auction_id);
        W.commit();

        json res_json;
        res_json["message"] = "Auction ended and funds transferred successfully";
        res_json["bid_amount"] = highest_bid;
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

// POST /delete_listing
http::response<http::string_body> handle_delete_listing(http::request<http::string_body> const &req)
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
        int auction_id = j.at("auction_id").get<int>();
        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");
        pqxx::work W(C);
        auto res_user = W.exec("SELECT user_id FROM Users WHERE username = $1", pqxx::params(username));
        if (res_user.empty())
            return make_response(req, 404, "User not found");
        int user_id = res_user[0]["user_id"].as<int>();
        auto res_auction = W.exec("SELECT seller_id FROM Auctions WHERE auction_id = $1", pqxx::params(auction_id));
        if (res_auction.empty())
            return make_response(req, 404, "Auction not found");
        int seller_id = res_auction[0]["seller_id"].as<int>();
        if (seller_id != user_id)
            return make_response(req, 403, "Not authorized to delete this listing");
        // First, delete bids referencing this auction.
        W.exec("DELETE FROM Bids WHERE auction_id = $1", pqxx::params(auction_id));
        // Then, delete the auction.
        W.exec("DELETE FROM Auctions WHERE auction_id = $1", pqxx::params(auction_id));
        W.commit();
        json res_json;
        res_json["message"] = "Listing deleted successfully";
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

// GET /my_listings
http::response<http::string_body> handle_my_listings(http::request<http::string_body> const &req)
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
        auto user_res = W.exec("SELECT user_id FROM Users WHERE username = $1", pqxx::params(username));
        if (user_res.empty())
            return make_response(req, 404, "User not found");
        int seller_id = user_res[0]["user_id"].as<int>();
        // Modified query: include highest bid and highest bidder for each auction.
        auto result = W.exec_params(
            "SELECT a.auction_id, a.item_name, a.description, a.image_url, a.base_price, a.posted_time, a.start_delay, a.live_duration, "
            "COALESCE((SELECT b.bid_amount FROM Bids b WHERE b.auction_id = a.auction_id ORDER BY b.bid_amount DESC LIMIT 1), 0) AS highest_bid, "
            "COALESCE((SELECT u.username FROM Bids b JOIN Users u ON b.bidder_id = u.user_id WHERE b.auction_id = a.auction_id ORDER BY b.bid_amount DESC LIMIT 1), '') AS highest_bidder "
            "FROM Auctions a WHERE a.seller_id = $1",
            seller_id);
        json listings = json::array();
        for (auto row : result)
        {
            json auction;
            auction["auction_id"] = row["auction_id"].as<int>();
            auction["item_name"] = row["item_name"].as<std::string>();
            auction["description"] = row["description"].as<std::string>();
            auction["image_url"] = row["image_url"].as<std::string>();
            auction["base_price"] = row["base_price"].as<std::string>();
            auction["posted_time"] = row["posted_time"].c_str();
            auction["start_delay"] = row["start_delay"].as<int>();
            auction["live_duration"] = row["live_duration"].as<int>();
            auction["highest_bid"] = row["highest_bid"].as<double>();
            auction["highest_bidder"] = row["highest_bidder"].as<std::string>();
            listings.push_back(auction);
        }
        json res_json;
        res_json["my_listings"] = listings;
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

// GET /upcoming
http::response<http::string_body> handle_upcoming(http::request<http::string_body> const &req)
{
    try
    {
        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");
        pqxx::work W(C);
        auto result = W.exec("SELECT auction_id, seller_id, item_name, description, image_url, base_price, posted_time, start_delay, live_duration FROM Auctions WHERE (posted_time + (start_delay || ' minutes')::interval) > CURRENT_TIMESTAMP");
        json listings = json::array();
        for (auto row : result)
        {
            json auction;
            auction["auction_id"] = row["auction_id"].as<int>();
            auction["seller_id"] = row["seller_id"].as<int>();
            auction["item_name"] = row["item_name"].as<std::string>();
            auction["description"] = row["description"].as<std::string>();
            auction["image_url"] = row["image_url"].as<std::string>();
            auction["base_price"] = row["base_price"].as<std::string>();
            auction["posted_time"] = row["posted_time"].c_str();
            auction["start_delay"] = row["start_delay"].as<int>();
            auction["live_duration"] = row["live_duration"].as<int>();
            listings.push_back(auction);
        }
        json res_json;
        res_json["upcoming_auctions"] = listings;
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

// GET /live
http::response<http::string_body> handle_live(http::request<http::string_body> const &req)
{
    try
    {
        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");
        pqxx::work W(C);
        // Modified query: include highest bid and bidder info.
        auto result = W.exec(
            "SELECT a.auction_id, a.seller_id, a.item_name, a.description, a.image_url, a.base_price, a.posted_time, a.start_delay, a.live_duration, "
            "COALESCE((SELECT b.bid_amount FROM Bids b WHERE b.auction_id = a.auction_id ORDER BY b.bid_amount DESC LIMIT 1), 0) AS highest_bid, "
            "COALESCE((SELECT u.username FROM Bids b JOIN Users u ON b.bidder_id = u.user_id WHERE b.auction_id = a.auction_id ORDER BY b.bid_amount DESC LIMIT 1), '') AS highest_bidder "
            "FROM Auctions a "
            "WHERE (a.posted_time + (a.start_delay || ' minutes')::interval) <= CURRENT_TIMESTAMP "
            "  AND CURRENT_TIMESTAMP < (a.posted_time + (a.start_delay || ' minutes')::interval + (a.live_duration || ' minutes')::interval)");
        json listings = json::array();
        for (auto row : result)
        {
            json auction;
            auction["auction_id"] = row["auction_id"].as<int>();
            auction["seller_id"] = row["seller_id"].as<int>();
            auction["item_name"] = row["item_name"].as<std::string>();
            auction["description"] = row["description"].as<std::string>();
            auction["image_url"] = row["image_url"].as<std::string>();
            auction["base_price"] = row["base_price"].as<std::string>();
            auction["posted_time"] = row["posted_time"].c_str();
            auction["start_delay"] = row["start_delay"].as<int>();
            auction["live_duration"] = row["live_duration"].as<int>();
            auction["highest_bid"] = row["highest_bid"].as<double>();
            auction["highest_bidder"] = row["highest_bidder"].as<std::string>();
            listings.push_back(auction);
        }
        json res_json;
        res_json["live_auctions"] = listings;
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

// GET /current_time
http::response<http::string_body> handle_current_time(http::request<http::string_body> const &req)
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm *gmt = std::gmtime(&now_time);
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%FT%TZ", gmt);
    json res_json;
    res_json["current_time"] = buffer;
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/json");
    res.set(http::field::access_control_allow_origin, "*");
    res.keep_alive(req.keep_alive());
    res.body() = res_json.dump();
    res.prepare_payload();
    return res;
}

// GET /auction_result?auction_id=xxx
http::response<http::string_body> handle_auction_result(http::request<http::string_body> const &req)
{
    try
    {
        std::string target = std::string(req.target());
        auto pos = target.find("auction_id=");
        if (pos == std::string::npos)
            return make_response(req, 400, "auction_id not provided");
        std::string id_str = target.substr(pos + 11);
        int auction_id = std::stoi(id_str);

        pqxx::connection C(db_connection_str);
        if (!C.is_open())
            return make_response(req, 500, "Database connection failed");
        pqxx::work W(C);
        auto result = W.exec_params(
            "SELECT b.bid_amount, u.username FROM Bids b "
            "JOIN Users u ON b.bidder_id = u.user_id "
            "WHERE b.auction_id = $1 "
            "ORDER BY b.bid_amount DESC LIMIT 1",
            auction_id);
        json res_json;
        if (result.empty())
        {
            res_json["bid_amount"] = 0.0;
            res_json["bidder_username"] = "";
        }
        else
        {
            double bid = result[0]["bid_amount"].as<double>();
            res_json["bid_amount"] = bid;
            res_json["bidder_username"] = result[0]["username"].as<std::string>();
        }
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

// --------------------
// Session and Main
// --------------------
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
        std::string target = std::string(req.target());
        if (target == "/register")
            res = handle_register(req);
        else if (target == "/login")
            res = handle_login(req);
        else if (target == "/deposit")
            res = handle_deposit(req);
        else if (target == "/withdraw")
            res = handle_withdraw(req);
        else if (target == "/create_listing" || target == "/create_listing/")
            res = handle_create_listing(req);
        else if (target == "/edit_listing" || target == "/edit_listing/")
            res = handle_edit_listing(req);
        else if (target == "/make_live" || target == "/make_live/")
            res = handle_make_live(req);
        else if (target == "/place_bid" || target == "/place_bid/")
            res = handle_place_bid(req);
        else if (target == "/delete_listing" || target == "/delete_listing/")
            res = handle_delete_listing(req);
        else if (target == "/end_auction" || target == "/end_auction/")
            res = handle_end_auction(req);
        else if (req.target().find("/auction_result") != std::string::npos)
            res = handle_auction_result(req);
        else if (req.target().find("/current_time") != std::string::npos)
            res = handle_current_time(req);
        else
            res = make_response(req, 404, "Not Found");
    }
    else if (req.method() == http::verb::get)
    {
        if (req.target() == "/profile")
            res = handle_profile(req);
        else if (req.target() == "/my_listings")
            res = handle_my_listings(req);
        else if (req.target() == "/upcoming")
            res = handle_upcoming(req);
        else if (req.target() == "/live")
            res = handle_live(req);
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
