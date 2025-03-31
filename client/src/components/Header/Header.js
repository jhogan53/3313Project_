// File: src/components/Header/Header.js
// Header component with navigation links and user info (if logged in).
import React from 'react';
import { Link, useNavigate } from 'react-router-dom';
import './Header.css';

function Header() {
  const navigate = useNavigate();
  const token = localStorage.getItem("token");
  const username = localStorage.getItem("username");
  const balance = localStorage.getItem("balance");

  const handleLogout = () => {
    localStorage.removeItem("token");
    localStorage.removeItem("username");
    localStorage.removeItem("balance");
    navigate("/");
  };

  return (
    <header className="header">
      <h1>Online Auction System</h1>
      <nav>
        <ul>
          <li><Link to="/">Home</Link></li>
          { token ? (
            <>
              <li><Link to="/profile">Profile</Link></li>
              <li>
                <span className="user-info">
                  {username} (Balance: ${balance})
                </span>
              </li>
              <li><button onClick={handleLogout} className="logout-btn">Logout</button></li>
            </>
          ) : (
            <>
              <li><Link to="/login">Login</Link></li>
              <li><Link to="/register">Register</Link></li>
            </>
          )}
        </ul>
      </nav>
    </header>
  );
}

export default Header;
