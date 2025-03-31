// File: src/pages/Login/Login.js
// Login page with username and password form.
// On success, it shows an embedded green message and then reloads to update header.

import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import './Login.css';

function Login() {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [msg, setMsg] = useState({ text: '', type: '' });
  const navigate = useNavigate();

  const handleSubmit = async (e) => {
    e.preventDefault();
    setMsg({ text: '', type: '' });
    try {
      const response = await fetch('/login', {
        method: 'POST',
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ username, password })
      });
      const data = await response.json();
      if (!response.ok) {
        setMsg({ text: `Error: ${data.error}`, type: 'error' });
      } else {
        // Save token, username, and balance in localStorage
        localStorage.setItem("token", data.token);
        localStorage.setItem("username", data.username);
        localStorage.setItem("balance", data.balance);
        setMsg({ text: "Login successful!", type: 'success' });
        setTimeout(() => {
          // Force full reload so Header updates immediately
          window.location.href = "/";
        }, 2000);
      }
    } catch (err) {
      setMsg({ text: "Network error", type: 'error' });
    }
  };

  return (
    <div className="login">
      <h2>Login</h2>
      {msg.text && (
        <div className={`msg ${msg.type}`}>
          {msg.text}
        </div>
      )}
      <form onSubmit={handleSubmit}>
        <div className="form-group">
          <label>Username:</label>
          <input 
            type="text"
            value={username}
            onChange={(e) => setUsername(e.target.value)}
            required 
          />
        </div>
        <div className="form-group">
          <label>Password:</label>
          <input 
            type="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            required 
          />
        </div>
        <button type="submit">Login</button>
      </form>
    </div>
  );
}

export default Login;
