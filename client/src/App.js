// File: src/App.js
// This file sets up the main application routes and provides the shared user context.
import React, { useState } from 'react';
import { BrowserRouter as Router, Route, Routes } from 'react-router-dom';
import Header from './components/Header/Header';
import Home from './pages/Home/Home';
import Login from './pages/Login/Login';
import Register from './pages/Register/Register';
import Profile from './pages/Profile/Profile';
import UserContext from './UserContext';
import './App.css';

function App() {
  // Initialize user state from localStorage.
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
            </Routes>
          </div>
        </div>
      </Router>
    </UserContext.Provider>
  );
}

export default App;
