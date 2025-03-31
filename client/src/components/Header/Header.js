// File: src/components/Header/Header.js
// This component displays the navigation links and current user info.
// It uses the shared user context to update the balance automatically.
import React, { useContext } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import UserContext from '../../UserContext';
import './Header.css';

function Header() {
  const { user, setUser } = useContext(UserContext);
  const navigate = useNavigate();

  const handleLogout = () => {
    localStorage.removeItem("token");
    localStorage.removeItem("username");
    localStorage.removeItem("balance");
    setUser({ token: "", username: "", balance: "" });
    navigate("/");
  };

  return (
    <header className="header">
      <h1>Online Auction System</h1>
      <nav>
        <ul>
          <li><Link to="/">Home</Link></li>
          {user.token ? (
            <>
              <li><Link to="/profile">Profile</Link></li>
              <li>
                <span className="user-info">
                  {user.username} (Balance: ${user.balance})
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
