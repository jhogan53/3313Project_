// File: src/pages/MyListings/MyListings.js
import React, { useState, useEffect, useContext } from 'react';
import { useNavigate } from 'react-router-dom';
import UserContext from '../../UserContext';
import './MyListings.css';

function MyListings() {
  const { user } = useContext(UserContext);
  const [msg, setMsg] = useState({ text: '', type: '' });
  const [itemName, setItemName] = useState('');
  const [description, setDescription] = useState('');
  const [imageUrl, setImageUrl] = useState('');
  const [basePrice, setBasePrice] = useState('');
  const [startDelay, setStartDelay] = useState('');
  const [liveDuration, setLiveDuration] = useState('');
  const [listings, setListings] = useState([]);
  const [editingId, setEditingId] = useState(null);
  const [newDescription, setNewDescription] = useState('');
  const [auctionResults, setAuctionResults] = useState({}); // keyed by auction_id
  const [tick, setTick] = useState(0);
  const navigate = useNavigate();

  // Update tick every second to force re-render and update timers.
  useEffect(() => {
    const interval = setInterval(() => setTick(t => t + 1), 1000);
    return () => clearInterval(interval);
  }, []);

  // Helper: parse a timestamp string as UTC.
  const parsePostedTime = (timeStr) => {
    return timeStr.endsWith("Z") ? new Date(timeStr) : new Date(timeStr + "Z");
  };

  // Fetch listings posted by the user.
  const fetchListings = async () => {
    try {
      const response = await fetch('/my_listings', {
        method: 'GET',
        headers: { 'Authorization': `Bearer ${user.token}` }
      });
      const data = await response.json();
      if (response.ok) {
        setListings(data.my_listings);
      } else {
        setMsg({ text: `Error: ${data.error}`, type: 'error' });
      }
    } catch (err) {
      setMsg({ text: 'Network error', type: 'error' });
    }
  };

  useEffect(() => {
    if (user.token) {
      fetchListings();
    }
  }, [user.token, tick]);

  // Fetch auction result (highest bid and bidder) for a given auction.
  const fetchAuctionResult = async (auctionId) => {
    try {
      const response = await fetch(`/auction_result?auction_id=${auctionId}`, {
        method: 'GET',
        headers: { 'Authorization': `Bearer ${user.token}` }
      });
      const data = await response.json();
      if (response.ok) {
        setAuctionResults(prev => ({ ...prev, [auctionId]: data }));
      } else {
        setAuctionResults(prev => ({ ...prev, [auctionId]: { error: data.error } }));
      }
    } catch (err) {
      setAuctionResults(prev => ({ ...prev, [auctionId]: { error: 'Network error' } }));
    }
  };

  // When listings change, fetch auction results for each listing.
  useEffect(() => {
    listings.forEach(listing => {
      if (!auctionResults[listing.auction_id]) {
        fetchAuctionResult(listing.auction_id);
      }
    });
  }, [listings, user.token]);

  // Timer calculation:
  // For listings that are not live (start_delay > 0), effective start time = posted_time + start_delay.
  // For live listings (start_delay === 0), use the posted_time as the time the listing went live.
  const getEffectiveTimes = (listing) => {
    const posted = parsePostedTime(listing.posted_time).getTime();
    const sDelay = Number(listing.start_delay);
    const lDuration = Number(listing.live_duration);
    const effectiveStart = sDelay > 0 ? posted + sDelay * 60000 : posted;
    const effectiveEnd = effectiveStart + lDuration * 60000;
    return { effectiveStart, effectiveEnd };
  };

  // Helper to format milliseconds into HH:MM:SS.
  const formatTime = (ms) => {
    if (ms <= 0) return '00:00:00';
    const totalSeconds = Math.floor(ms / 1000);
    const hours = String(Math.floor(totalSeconds / 3600)).padStart(2, '0');
    const minutes = String(Math.floor((totalSeconds % 3600) / 60)).padStart(2, '0');
    const seconds = String(totalSeconds % 60).padStart(2, '0');
    return `${hours}:${minutes}:${seconds}`;
  };

  // Post a new listing.
  const handleSubmit = async (e) => {
    e.preventDefault();
    setMsg({ text: '', type: '' });
    try {
      const payload = {
        item_name: itemName,
        description: description,
        image_url: imageUrl,
        base_price: parseFloat(basePrice),
        start_delay: parseInt(startDelay, 10),
        live_duration: parseInt(liveDuration, 10)
      };

      const response = await fetch('/create_listing', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${user.token}`
        },
        body: JSON.stringify(payload)
      });
      const data = await response.json();
      if (!response.ok) {
        setMsg({ text: `Error: ${data.error}`, type: 'error' });
      } else {
        setMsg({ text: data.message, type: 'success' });
        setItemName('');
        setDescription('');
        setImageUrl('');
        setBasePrice('');
        setStartDelay('');
        setLiveDuration('');
        fetchListings();
      }
    } catch (err) {
      setMsg({ text: 'Network error', type: 'error' });
    }
  };

  // Edit listing description.
  const handleEditDescription = (auctionId, currentDescription) => {
    setEditingId(auctionId);
    setNewDescription(currentDescription);
  };

  const handleSaveDescription = async (auctionId) => {
    try {
      const response = await fetch('/edit_listing', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${user.token}`
        },
        body: JSON.stringify({ auction_id: auctionId, description: newDescription })
      });
      const data = await response.json();
      if (!response.ok) {
        setMsg({ text: `Error: ${data.error}`, type: 'error' });
      } else {
        setMsg({ text: data.message, type: 'success' });
        setEditingId(null);
        fetchListings();
      }
    } catch (err) {
      setMsg({ text: 'Network error', type: 'error' });
    }
  };

  // Make listing live immediately.
  const handleMakeLive = async (auctionId) => {
    try {
      const response = await fetch('/make_live', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${user.token}`
        },
        body: JSON.stringify({ auction_id: auctionId })
      });
      const data = await response.json();
      if (!response.ok) {
        setMsg({ text: `Error: ${data.error}`, type: 'error' });
      } else {
        setMsg({ text: data.message, type: 'success' });
        fetchListings();
      }
    } catch (err) {
      setMsg({ text: 'Network error', type: 'error' });
    }
  };

  // End a live auction immediately.
  const handleEndAuction = async (auctionId) => {
    try {
      const response = await fetch('/end_auction', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${user.token}`
        },
        body: JSON.stringify({ auction_id: auctionId })
      });
      const data = await response.json();
      if (!response.ok) {
        setMsg({ text: `Error: ${data.error}`, type: 'error' });
      } else {
        setMsg({ text: data.message, type: 'success' });
        fetchListings();
      }
    } catch (err) {
      setMsg({ text: 'Network error', type: 'error' });
    }
  };

  // Delete listing.
  const handleDelete = async (auctionId) => {
    try {
      const response = await fetch('/delete_listing', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${user.token}`
        },
        body: JSON.stringify({ auction_id: auctionId })
      });
      const data = await response.json();
      if (!response.ok) {
        setMsg({ text: `Error: ${data.error}`, type: 'error' });
      } else {
        setMsg({ text: data.message, type: 'success' });
        fetchListings();
      }
    } catch (err) {
      setMsg({ text: 'Network error', type: 'error' });
    }
  };

  return (
    <div className="my-listings">
      <h2>My Listings</h2>
      {msg.text && <div className={`msg ${msg.type}`}>{msg.text}</div>}
      <div className="post-listing">
        <h3>Post a New Listing</h3>
        <form onSubmit={handleSubmit}>
          <div className="form-group">
            <label>Item Name:</label>
            <input type="text" value={itemName} onChange={(e) => setItemName(e.target.value)} required />
          </div>
          <div className="form-group">
            <label>Description:</label>
            <textarea value={description} onChange={(e) => setDescription(e.target.value)} required></textarea>
          </div>
          <div className="form-group">
            <label>Image URL (optional):</label>
            <input type="text" value={imageUrl} onChange={(e) => setImageUrl(e.target.value)} />
          </div>
          <div className="form-group">
            <label>Base Price:</label>
            <input type="number" step="0.01" value={basePrice} onChange={(e) => setBasePrice(e.target.value)} required />
          </div>
          <div className="form-group">
            <label>Start Delay (minutes):</label>
            <input type="number" value={startDelay} onChange={(e) => setStartDelay(e.target.value)} required />
          </div>
          <div className="form-group">
            <label>Live Duration (minutes):</label>
            <input type="number" value={liveDuration} onChange={(e) => setLiveDuration(e.target.value)} required />
          </div>
          <button type="submit">Post Listing</button>
        </form>
      </div>
      <hr />
      <div className="current-listings">
        <h3>Current Listings</h3>
        {listings.length === 0 ? (
          <p>No listings found.</p>
        ) : (
          listings.map(listing => {
            const postedTime = parsePostedTime(listing.posted_time);
            // Compute effective start and end times.
            // If start_delay is 0, we trust the backend-updated posted_time as the live start time.
            const sDelay = Number(listing.start_delay);
            const lDuration = Number(listing.live_duration);
            const effectiveStart = sDelay > 0 
              ? postedTime.getTime() + sDelay * 60000 
              : postedTime.getTime();
            const effectiveEnd = effectiveStart + lDuration * 60000;
            const now = Date.now();
            let statusLabel = '';
            let actionButtons = null;
            let currentBidDisplay = null;

            if (now < effectiveStart) {
              statusLabel = `Starts in: ${formatTime(effectiveStart - now)}`;
              actionButtons = (
                <>
                  <button onClick={() => handleEditDescription(listing.auction_id, listing.description)}>
                    Edit Description
                  </button>
                  <button onClick={() => handleMakeLive(listing.auction_id)}>
                    Make Live Now
                  </button>
                </>
              );
            } else if (now >= effectiveStart && now < effectiveEnd) {
              statusLabel = `Live - Ends in: ${formatTime(effectiveEnd - now)}`;
              const highestBidDisplay =
                Number(listing.highest_bid) > 0
                  ? `Current Highest Bid: $${listing.highest_bid} by ${listing.highest_bidder}`
                  : "No bids yet";
              currentBidDisplay = (
                <div className="auction-result">
                  <p>{highestBidDisplay}</p>
                </div>
              );
              actionButtons = (
                <>
                  {currentBidDisplay}
                  <button onClick={() => handleEditDescription(listing.auction_id, listing.description)}>
                    Edit Description
                  </button>
                  <button onClick={() => handleEndAuction(listing.auction_id)}>
                    End Auction Now
                  </button>
                </>
              );
            } else {
              statusLabel = `Auction Ended`;
              const finalResult =
                Number(listing.highest_bid) > 0
                  ? `Highest Bid: $${listing.highest_bid} by ${listing.highest_bidder}`
                  : "No bids placed";
              actionButtons = (
                <>
                  <p><strong>Result:</strong></p>
                  <p>{finalResult}</p>
                </>
              );
            }

            return (
              <div key={listing.auction_id} className="listing-card">
                <h4>{listing.item_name}</h4>
                {listing.image_url && (
                  <div className="listing-image">
                    <img src={listing.image_url} alt={listing.item_name} />
                  </div>
                )}
                <p><strong>Description:</strong> {listing.description}</p>
                <p><strong>Base Price:</strong> ${listing.base_price}</p>
                <p><strong>Posted:</strong> {postedTime.toLocaleString()}</p>
                <p><strong>Start Delay:</strong> {listing.start_delay} minutes</p>
                <p><strong>Live Duration:</strong> {listing.live_duration} minutes</p>
                <p><strong>Status:</strong> {statusLabel}</p>
                <div className="listing-actions">
                  {actionButtons}
                  <button onClick={() => handleDelete(listing.auction_id)}>Delete Listing</button>
                </div>
              </div>
            );
          })
        )}
      </div>
    </div>
  );
}

export default MyListings;
