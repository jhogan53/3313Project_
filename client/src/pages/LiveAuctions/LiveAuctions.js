// File: src/pages/LiveAuctions/LiveAuctions.js
import React, { useState, useEffect, useContext } from 'react';
import UserContext from '../../UserContext';
import './LiveAuctions.css';

function LiveAuctions() {
  const { user } = useContext(UserContext);
  const [live, setLive] = useState([]);
  const [msg, setMsg] = useState({ text: '', type: '' });

  // Fetch live auctions from the backend
  const fetchLive = async () => {
    try {
      const response = await fetch('/live', {
        method: 'GET',
        headers: {
          'Authorization': `Bearer ${user.token}`
        }
      });
      const data = await response.json();
      if (response.ok) {
        setLive(data.live_auctions);
      } else {
        setMsg({ text: `Error: ${data.error}`, type: 'error' });
      }
    } catch (err) {
      setMsg({ text: 'Network error', type: 'error' });
    }
  };

  useEffect(() => {
    fetchLive();
  }, [user.token]);

  return (
    <div className="live-auctions">
      <h2>Live Auctions</h2>
      {msg.text && (
        <div className={`msg ${msg.type}`}>
          {msg.text}
        </div>
      )}
      {live.length === 0 ? (
        <p>No live auctions found.</p>
      ) : (
        live.map(auction => (
          <div key={auction.auction_id} className="auction-card">
            <h3>{auction.item_name}</h3>
            {auction.image_url && (
              <div className="auction-image">
                <img src={auction.image_url} alt={auction.item_name} />
              </div>
            )}
            <p><strong>Description:</strong> {auction.description}</p>
            <p><strong>Base Price:</strong> ${auction.base_price}</p>
            <p><strong>Posted:</strong> {new Date(auction.posted_time).toLocaleString()}</p>
            <p><strong>Start Delay:</strong> {auction.start_delay} minutes</p>
            <p><strong>Live Duration:</strong> {auction.live_duration} minutes</p>
            <p><strong>Status:</strong> Live</p>
          </div>
        ))
      )}
    </div>
  );
}

export default LiveAuctions;
