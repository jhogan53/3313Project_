import React, { useState } from 'react';
import { BrowserRouter as Router, Route, Routes } from 'react-router-dom';
import Header from './components/Header/Header';
import Home from './pages/Home/Home';
import Login from './pages/Login/Login';
import Register from './pages/Register/Register';
import Profile from './pages/Profile/Profile';
import MyListings from './pages/MyListings/MyListings';
import UpcomingAuctions from './pages/UpcomingAuctions/UpcomingAuctions';
import LiveAuctions from './pages/LiveAuctions/LiveAuctions';
import UserContext from './UserContext';
import './App.css';

function App() {
  const [user, setUser] = useState({
    token: localStorage.getItem("token") || "",
    username: localStorage.getItem("username") || "",
    balance: localStorage.getItem("balance") || ""
  });

  return (
    <UserContext.Provider value={{ user, setUser }}>
      <Router>
        <div className="app-container">
          <Header />
          <div className="content">
            <Routes>
              <Route path="/" element={<Home />} />
              <Route path="/login" element={<Login />} />
              <Route path="/register" element={<Register />} />
              <Route path="/profile" element={<Profile />} />
              <Route path="/my_listings" element={<MyListings />} />
              <Route path="/upcoming_auctions" element={<UpcomingAuctions />} />
              <Route path="/live_auctions" element={<LiveAuctions />} />
            </Routes>
          </div>
        </div>
      </Router>
    </UserContext.Provider>
  );
}

export default App;
