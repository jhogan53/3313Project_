// File: src/pages/Header/Header.js
import React, { useContext, useEffect } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import UserContext from '../../UserContext';
import './Header.css';

function Header() {
  const { user, setUser } = useContext(UserContext);
  const navigate = useNavigate();

  // Poll profile every 10 seconds to refresh the balance
  useEffect(() => {
    const fetchProfile = async () => {
      if (!user.token) return;
      try {
        const response = await fetch('/profile', {
          method: 'GET',
          headers: {
            "Authorization": `Bearer ${user.token}`
          }
        });
        if (response.ok) {
          const data = await response.json();
          // Update the context and localStorage with the new balance
          setUser(prev => ({ ...prev, balance: data.balance }));
          localStorage.setItem("balance", data.balance);
        }
      } catch (err) {
        console.error("Failed to refresh profile", err);
      }
    };

    const intervalId = setInterval(fetchProfile, 10000); // Poll every 10 seconds

    return () => clearInterval(intervalId);
  }, [user.token, setUser]);

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
              <li><Link to="/my_listings">My Listings</Link></li>
              <li><Link to="/upcoming_auctions">Upcoming Auctions</Link></li>
              <li><Link to="/live_auctions">Live Auctions</Link></li>
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
