import React, { useState } from 'react';

export default function Lobby({ onJoinRoom }) {
  const [roomIdInput, setRoomIdInput] = useState('');
  const [roleInput, setRoleInput] = useState('candidate');
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
      setError('Please enter a valid Room ID.');
      return;
    }
    onJoinRoom(roomIdInput.trim(), roleInput);
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

        <form onSubmit={handleJoinRoom} className="join-form">
          <div className="form-group">
            <label htmlFor="room-id">Session ID</label>
            <input 
              id="room-id"
              type="text" 
              placeholder="e.g. AbC123"
              value={roomIdInput}
              onChange={(e) => setRoomIdInput(e.target.value)}
              disabled={isLoading}
            />
          </div>

          <div className="form-group">
            <label htmlFor="role">Your Role</label>
            <select 
              id="role"
              value={roleInput}
              onChange={(e) => setRoleInput(e.target.value)}
              disabled={isLoading}
            >
              <option value="candidate">Candidate</option>
              <option value="interviewer">Interviewer</option>
            </select>
          </div>

          <button 
            type="submit" 
            className="secondary"
            disabled={isLoading}
            style={{ justifyContent: 'center', width: '100%', padding: '12px' }}
          >
            Join Existing Session
          </button>
        </form>
      </div>
    </div>
  );
}
