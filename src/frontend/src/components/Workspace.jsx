import React, { useEffect, useRef, useState } from 'react';
import Editor from '@monaco-editor/react';
import useCollaboration from '../hooks/useCollaboration';

export default function Workspace({ roomId, role, onLeave }) {
  const {
    code,
    problem,
    language,
    logs,
    isRunning,
    status,
    updateCode,
    updateLanguage,
    updateProblem,
    runCode
  } = useCollaboration(roomId, role);

  const [toastMessage, setToastMessage] = useState('');
  const consoleEndRef = useRef(null);

  // Map language backend value to Monaco editor value
  const getEditorLanguage = (lang) => {
    switch (lang.toLowerCase()) {
      case 'cpp':
      case 'c++':
        return 'cpp';
      case 'python':
      case 'py':
        return 'python';
      case 'javascript':
      case 'js':
      case 'node':
        return 'javascript';
      default:
        return 'cpp';
    }
  };

  // Auto-scroll console to bottom
  useEffect(() => {
    if (consoleEndRef.current) {
      consoleEndRef.current.scrollIntoView({ behavior: 'smooth' });
    }
  }, [logs]);

  const handleCopyLink = () => {
    // We copy the current URL with the roomId, but override role to candidate for sharing
    const shareUrl = `${window.location.origin}/?room=${roomId}&role=candidate`;
    navigator.clipboard.writeText(shareUrl)
      .then(() => {
        showToast('Share link copied to clipboard!');
      })
      .catch((err) => {
        console.error('Could not copy text: ', err);
      });
  };

  const showToast = (message) => {
    setToastMessage(message);
    setTimeout(() => {
      setToastMessage('');
    }, 3000);
  };

  return (
    <div className="workspace-container">
      {/* Navbar Header */}
      <header className="workspace-header">
        <div className="brand" onClick={onLeave} style={{ cursor: 'pointer' }}>
          CodePair <span>v1.0</span>
        </div>
        
        <div className="controls">
          <div className={`status-badge ${status}`}>
            <span className={`badge-dot ${status === 'connecting' ? 'pulse' : ''}`} />
            {status}
          </div>
          
          <span className={`role-badge ${role}`}>{role}</span>
          
          <button className="secondary" onClick={handleCopyLink} title="Copy invite link for candidate">
            Share Link
          </button>
          
          <button className="danger" onClick={onLeave}>
            Leave
          </button>
        </div>
      </header>

      {/* Main Workspace Body */}
      <main className="workspace-main">
        {/* Left Pane - Problem Statement */}
        <section className="pane pane-left">
          <div className="pane-header">
            <span>Problem Description</span>
            {role === 'interviewer' && <span style={{ fontSize: '0.75rem', color: 'var(--accent-purple)' }}>Editable</span>}
          </div>
          <div className="pane-content">
            {role === 'interviewer' ? (
              <textarea
                className="problem-editor-textarea"
                placeholder="Write the problem statement here. The candidate will see updates in real-time..."
                value={problem}
                onChange={(e) => updateProblem(e.target.value)}
              />
            ) : (
              <div className="problem-viewer">
                {problem.trim() ? problem : "The interviewer has not added a problem description yet."}
              </div>
            )}
          </div>
        </section>

        {/* Right Pane - Monaco Editor & Console */}
        <section className="pane pane-right">
          {/* Editor Header / Lang Selector */}
          <div className="pane-header" style={{ padding: '8px 20px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
              <span>Code Editor</span>
              <div className="select-wrapper">
                <select
                  className="theme-select"
                  value={language}
                  onChange={(e) => updateLanguage(e.target.value)}
                  disabled={status !== 'connected'}
                >
                  <option value="cpp">C++ (GCC)</option>
                  <option value="python">Python 3</option>
                  <option value="javascript">JavaScript (Node)</option>
                </select>
              </div>
            </div>

            <button
              className="success"
              onClick={runCode}
              disabled={isRunning || status !== 'connected'}
              style={{ padding: '4px 14px', fontSize: '0.8rem' }}
            >
              {isRunning ? 'Running...' : 'Run Code'}
            </button>
          </div>

          {/* Monaco Editor Container */}
          <div className="editor-section">
            <div className="editor-container-wrapper">
              <Editor
                height="100%"
                theme="vs-dark"
                language={getEditorLanguage(language)}
                value={code}
                onChange={(value) => updateCode(value || '')}
                options={{
                  minimap: { enabled: false },
                  fontSize: 14,
                  fontFamily: "'Fira Code', 'Courier New', Courier, monospace",
                  fontLigatures: true,
                  automaticLayout: true,
                  scrollbar: {
                    vertical: 'visible',
                    horizontal: 'visible',
                    useShadows: false,
                    verticalScrollbarSize: 8,
                    horizontalScrollbarSize: 8
                  }
                }}
              />
            </div>
          </div>

          {/* Execution Console Output */}
          <div className="console-section">
            <div className="pane-header" style={{ borderTop: '1px solid var(--border-light)' }}>
              Console Output
            </div>
            <div className="console-content">
              {logs.length === 0 ? (
                <div className="console-placeholder">
                  Run your code to see the stdout and stderr output streams.
                </div>
              ) : (
                logs.map((log, index) => (
                  <div key={index} className={`console-line ${log.type}`}>
                    {log.text}
                  </div>
                ))
              )}
              <div ref={consoleEndRef} />
            </div>
          </div>
        </section>
      </main>

      {/* Toast Notification popup */}
      {toastMessage && (
        <div className="toast">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
            <polyline points="20 6 9 17 4 12" />
          </svg>
          {toastMessage}
        </div>
      )}
    </div>
  );
}
