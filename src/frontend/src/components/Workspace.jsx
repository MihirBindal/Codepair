import React, { useEffect, useRef, useState } from 'react';
import Editor from '@monaco-editor/react';
import useCollaboration from '../hooks/useCollaboration';

export default function Workspace({ roomId, role, onLeave }) {
  const {
    code,
    problem,
    language,
    stdin,
    logs,
    isRunning,
    status,
    updateCode,
    updateLanguage,
    updateProblem,
    updateStdin,
    runCode
  } = useCollaboration(roomId, role);

  const [toastMessage, setToastMessage] = useState('');
  const [consoleTab, setConsoleTab] = useState('output'); // 'output' or 'stdin'
  const consoleEndRef = useRef(null);

  // Auto-switch to output console tab when code runs
  useEffect(() => {
    if (isRunning) {
      setConsoleTab('output');
    }
  }, [isRunning]);

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
          CodePair
        </div>
        
        <div className="controls">
          <div className="room-code-badge" style={{
            display: 'inline-flex',
            alignItems: 'center',
            gap: '8px',
            fontSize: '0.8rem',
            fontWeight: '600',
            letterSpacing: '0.5px',
            padding: '4px 10px',
            borderRadius: '4px',
            background: 'rgba(255, 255, 255, 0.05)',
            border: '1px solid var(--border-light)',
            color: 'var(--text-secondary)'
          }}>
            <span className={`badge-dot ${status === 'connecting' ? 'pulse' : ''}`} style={{
              width: '8px',
              height: '8px',
              borderRadius: '50%',
              backgroundColor: status === 'connected' ? 'var(--success)' : status === 'connecting' ? 'var(--warning)' : 'var(--danger)',
              boxShadow: `0 0 8px ${status === 'connected' ? 'var(--success)' : status === 'connecting' ? 'var(--warning)' : 'var(--danger)'}`
            }} />
            <span>Room:</span>
            <span style={{ fontFamily: 'monospace', color: 'var(--text-primary)', fontSize: '0.9rem', textTransform: 'none' }}>{roomId}</span>
          </div>

          <span className={`role-badge ${role}`}>{role}</span>
          
          {role === 'interviewer' && (
            <button className="secondary" onClick={handleCopyLink} title="Copy invite link for candidate">
              Share Link
            </button>
          )}
          
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

          {/* Execution Console Output / Custom Input Tabs */}
          <div className="console-section">
            <div className="console-tab-header">
              <button 
                className={`console-tab ${consoleTab === 'output' ? 'active' : ''}`}
                onClick={() => setConsoleTab('output')}
              >
                Console Output
              </button>
              <button 
                className={`console-tab ${consoleTab === 'stdin' ? 'active' : ''}`}
                onClick={() => setConsoleTab('stdin')}
              >
                Custom Input
              </button>
            </div>
            
            <div className="console-content">
              {consoleTab === 'output' ? (
                <>
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
                </>
              ) : (
                <textarea
                  className="stdin-textarea"
                  placeholder="Provide standard input (stdin) for your program execution here..."
                  value={stdin}
                  onChange={(e) => updateStdin(e.target.value)}
                  style={{
                    width: '100%',
                    height: '100%',
                    background: 'transparent',
                    border: 'none',
                    resize: 'none',
                    outline: 'none',
                    color: 'var(--text-primary)',
                    fontFamily: "'Fira Code', 'Courier New', Courier, monospace",
                    fontSize: '0.85rem',
                    lineHeight: '1.5',
                    padding: '8px 4px'
                  }}
                />
              )}
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
