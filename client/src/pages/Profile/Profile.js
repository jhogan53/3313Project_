// File: src/pages/Profile/Profile.js
// This file implements the Profile page which shows the user’s details
// and provides forms for depositing and withdrawing funds.
// Success messages are now shown inline and the updated balance is reflected immediately.
import React, { useState, useEffect, useContext } from 'react';
import UserContext from '../../UserContext';
import './Profile.css';

function Profile() {
  const { user, setUser } = useContext(UserContext);
  const [profile, setProfile] = useState({ username: user.username, balance: user.balance });
  const [depositAmount, setDepositAmount] = useState('');
  const [withdrawAmount, setWithdrawAmount] = useState('');
  const [msg, setMsg] = useState({ text: '', type: '' });

  const token = user.token;

  useEffect(() => {
    // Fetch profile info from the backend.
    const fetchProfile = async () => {
      try {
        const response = await fetch('/profile', {
          method: 'GET',
          headers: { "Authorization": `Bearer ${token}` }
        });
        const data = await response.json();
        if (response.ok) {
          setProfile({ username: data.username, balance: data.balance });
          // Update both the context and localStorage.
          setUser({ token, username: data.username, balance: data.balance });
          localStorage.setItem("balance", data.balance);
        } else {
          setMsg({ text: `Error: ${data.error}`, type: 'error' });
        }
      } catch (err) {
        setMsg({ text: "Network error", type: 'error' });
      }
    };

    if (token) {
      fetchProfile();
    }
  }, [token, setUser]);

  const handleDeposit = async (e) => {
    e.preventDefault();
    setMsg({ text: '', type: '' });
    try {
      const response = await fetch('/deposit', {
        method: 'POST',
        headers: {
          "Content-Type": "application/json",
          "Authorization": `Bearer ${token}`
        },
        body: JSON.stringify({ amount: parseFloat(depositAmount) })
      });
      const data = await response.json();
      if (response.ok) {
        setMsg({ text: data.message, type: 'success' });
        // Refresh profile info.
        const res = await fetch('/profile', {
          method: 'GET',
          headers: { "Authorization": `Bearer ${token}` }
        });
        const profileData = await res.json();
        if (res.ok) {
          setProfile({ username: profileData.username, balance: profileData.balance });
          setUser({ token, username: profileData.username, balance: profileData.balance });
          localStorage.setItem("balance", profileData.balance);
        }
        setDepositAmount('');
      } else {
        setMsg({ text: `Error: ${data.error}`, type: 'error' });
      }
    } catch (err) {
      setMsg({ text: "Network error", type: 'error' });
    }
  };

  const handleWithdraw = async (e) => {
    e.preventDefault();
    setMsg({ text: '', type: '' });
    try {
      const response = await fetch('/withdraw', {
        method: 'POST',
        headers: {
          "Content-Type": "application/json",
          "Authorization": `Bearer ${token}`
        },
        body: JSON.stringify({ amount: parseFloat(withdrawAmount) })
      });
      const data = await response.json();
      if (response.ok) {
        setMsg({ text: data.message, type: 'success' });
        // Refresh profile info.
        const res = await fetch('/profile', {
          method: 'GET',
          headers: { "Authorization": `Bearer ${token}` }
        });
        const profileData = await res.json();
        if (res.ok) {
          setProfile({ username: profileData.username, balance: profileData.balance });
          setUser({ token, username: profileData.username, balance: profileData.balance });
          localStorage.setItem("balance", profileData.balance);
        }
        setWithdrawAmount('');
      } else {
        setMsg({ text: `Error: ${data.error}`, type: 'error' });
      }
    } catch (err) {
      setMsg({ text: "Network error", type: 'error' });
    }
  };

  return (
    <div className="profile">
      <h2>Profile</h2>
      {msg.text && (
        <div className={`msg ${msg.type}`}>
          {msg.text}
        </div>
      )}
      <p><strong>Username:</strong> {profile.username}</p>
      <p><strong>Balance:</strong> ${profile.balance}</p>
      <div className="transactions">
        <form onSubmit={handleDeposit}>
          <h3>Deposit Funds</h3>
          <input 
            type="number" 
            placeholder="Amount" 
            value={depositAmount} 
            onChange={(e) => setDepositAmount(e.target.value)}
            required 
          />
          <button type="submit">Deposit</button>
        </form>
        <form onSubmit={handleWithdraw}>
          <h3>Withdraw Funds</h3>
          <input 
            type="number" 
            placeholder="Amount" 
            value={withdrawAmount} 
            onChange={(e) => setWithdrawAmount(e.target.value)}
            required 
          />
          <button type="submit">Withdraw</button>
        </form>
      </div>
    </div>
  );
}

export default Profile;
