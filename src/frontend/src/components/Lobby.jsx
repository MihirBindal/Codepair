import React, { useState } from 'react';

export default function Lobby({ onJoinRoom }) {
  const [roomIdInput, setRoomIdInput] = useState('');
  const [showJoinForm, setShowJoinForm] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState('');

  const handleCreateRoom = async () => {
    setIsLoading(true);
    setError('');
    try {
      const response = await fetch('http://localhost:4001/create');
      if (!response.ok) {
        throw new Error('Failed to create room session');
      }
      const data = await response.json();
      if (data.room_id) {
        onJoinRoom(data.room_id, 'interviewer');
      } else {
        throw new Error('No room ID returned from server');
      }
    } catch (err) {
      console.error(err);
      setError('Could not connect to the C++ server. Make sure it is running on port 4001.');
    } finally {
      setIsLoading(false);
    }
  };

  const handleJoinRoom = (e) => {
    e.preventDefault();
    if (!roomIdInput.trim()) {
      setError('Please enter a valid Session ID.');
      return;
    }
    onJoinRoom(roomIdInput.trim(), 'candidate');
  };

  return (
    <div className="lobby-container">
      <div className="lobby-card glass-panel">
        <div>
          <h1 className="lobby-logo">CodePair</h1>
          <p className="lobby-subtitle">Real-time collaborative workspace for developer interviews</p>
        </div>

        {error && (
          <div style={{ color: 'var(--danger)', fontSize: '0.9rem', background: 'rgba(239, 68, 68, 0.1)', padding: '10px', borderRadius: '6px', border: '1px solid rgba(239, 68, 68, 0.2)' }}>
            {error}
          </div>
        )}

        <button 
          className="primary" 
          onClick={handleCreateRoom}
          disabled={isLoading}
          style={{ justifyContent: 'center', width: '100%', padding: '14px' }}
        >
          {isLoading ? 'Creating Room...' : 'Start a New Session (Interviewer)'}
        </button>

        <div className="lobby-divider">OR</div>

        {!showJoinForm ? (
          <button 
            type="button" 
            className="secondary"
            onClick={() => setShowJoinForm(true)}
            disabled={isLoading}
            style={{ justifyContent: 'center', width: '100%', padding: '14px' }}
          >
            Join Existing Session
          </button>
        ) : (
          <form onSubmit={handleJoinRoom} className="join-form">
            <div className="form-group">
              <label htmlFor="room-id">
                Session ID 
                <span style={{ textTransform: 'none', fontSize: '0.75rem', color: 'var(--text-muted)', marginLeft: '6px', fontWeight: 'normal' }}>
                  (case-sensitive)
                </span>
              </label>
              <input 
                id="room-id"
                type="text" 
                placeholder="e.g. AbC123"
                value={roomIdInput}
                onChange={(e) => setRoomIdInput(e.target.value)}
                disabled={isLoading}
                autoFocus
              />
            </div>

            <button 
              type="submit" 
              className="primary"
              disabled={isLoading}
              style={{ justifyContent: 'center', width: '100%', padding: '12px' }}
            >
              Join Session
            </button>
          </form>
        )}
      </div>
    </div>
  );
}
