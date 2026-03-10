#pragma once

// ============================================================================
// Adaptive FakeYaw System
//
// Replaces the "default" fakeyaw preset with a modular, context-aware system
// that selects and blends movement presets based on:
//   - Recent damage taken / successful evasions
//   - Threat level (visible enemies, closest distance)
//   - Height/stance state (standing, crouched, airborne, elevated)
//   - Rolling combat history
//
// Key classes:
//   MovementPresetConfig     - Tuning parameters for one preset
//   MovementHistoryTracker   - Rolling buffer of combat events
//   DistortionGenerator      - Time-based bounded yaw distortion
//   ThreatAssessment         - Derives threat signals from game world
//   AdaptiveMovementController - Scores presets, switches with hysteresis,
//                                blends output offsets
// ============================================================================

// ----------------------------------------------------------------------------
// MovementPresetConfig
// All distances/offsets are in degrees unless otherwise noted.
// ----------------------------------------------------------------------------
struct MovementPresetConfig {
    const char* name;

    // Base yaw offset added to m_direction when this preset is active.
    float baseYawOffset;

    // Maximum distortion amplitude (degrees).
    float distortionAmplitude;

    // Distortion oscillation speed (cycles per second).
    float distortionFrequency;

    // How aggressively (0-1) this preset reacts to incoming state changes.
    // Higher = more eager to switch away from a failing preset.
    float switchAggressiveness;

    // Low-pass smoothing factor for the blended offset (0-1).
    // Lower values = smoother transitions.
    float smoothingFactor;

    // Amplitude scalar applied based on stance/height state.
    float stanceMultiplier;

    // Amplitude scalar applied based on current threat level.
    float threatMultiplier;

    // ---- Switch-jitter layer (applied on top of distortion) ----
    bool  switchJitterEnabled;   // if true, alternating offset is added
    float switchJitterRange;     // max swing in degrees
    float switchJitterSpeed;     // alternation frequency (Hz)
    float switchJitterOffset;    // static bias added to the jitter
};

// ----------------------------------------------------------------------------
// HeightState
// Derived from FL_ONGROUND, IN_DUCK and velocity each tick.
// ----------------------------------------------------------------------------
enum class HeightState : int {
    Standing = 0,   // on ground, not crouching
    Crouched,       // on ground, crouching
    Airborne,       // in the air (jumping / falling)
    Elevated,       // airborne but with upward velocity (initial jump)
};

// ----------------------------------------------------------------------------
// PresetIndex
// One entry per built-in preset; COUNT is a sentinel for array sizing.
// ----------------------------------------------------------------------------
enum class PresetIndex : int {
    Stable          = 0,
    DefensiveWide   = 1,
    DefensiveTight  = 2,
    Distorted       = 3,
    Reactive        = 4,
    LowProfile      = 5,
    HighPressure    = 6,
    COUNT
};

constexpr int ADAPTIVE_PRESET_COUNT = static_cast< int >( PresetIndex::COUNT );

// ----------------------------------------------------------------------------
// MovementHistoryEntry
// One sample pushed every update tick.
// ----------------------------------------------------------------------------
struct MovementHistoryEntry {
    float       timestamp;       // g_csgo.m_globals->m_curtime
    float       damageTaken;     // damage we received this frame (0 = none)
    int         evasionCount;    // 1 if an enemy near us failed to hit this frame
    HeightState heightState;
    int         activePreset;    // which preset was active
};

// ----------------------------------------------------------------------------
// MovementHistoryTracker
// Keeps a fixed-size rolling deque of recent movement/combat history.
// ----------------------------------------------------------------------------
class MovementHistoryTracker {
public:
    static constexpr int   MAX_ENTRIES     = 128;
    static constexpr float DEFAULT_WINDOW  = 5.0f; // seconds

    MovementHistoryTracker( );

    // Push a new frame of data.
    void Push( float timestamp, float damageTaken, int evasion,
               HeightState hs, int preset );

    // Aggregate helpers – all look back 'window' seconds from 'now'.
    float GetRecentDamage( float now, float window = DEFAULT_WINDOW ) const;
    float GetRecentEvasions( float now, float window = DEFAULT_WINDOW ) const;
    int   GetRecentHitCount( float now, float window = DEFAULT_WINDOW ) const;

    // Average damage received while a specific preset was active.
    float GetPresetDamageAvg( float now, float window, int presetIndex ) const;

    std::deque< MovementHistoryEntry > m_entries;
};

// ----------------------------------------------------------------------------
// DistortionGenerator
// Produces a bounded yaw offset using a low-frequency sine wave plus
// time-quantised pseudo-random micro-adjustments.
// ----------------------------------------------------------------------------
class DistortionGenerator {
public:
    // Returns a bounded offset in degrees.
    // amplitude   - max swing in degrees (before stance/threat scaling)
    // frequency   - oscillation speed (Hz)
    // hs          - scales amplitude based on stance
    // threatLevel - 0..1 scales amplitude further
    float Generate( float time, float amplitude, float frequency,
                    HeightState hs, float threatLevel ) const;

private:
    // Deterministic pseudo-noise at a given time slice (period ~0.15 s).
    float PseudoNoise( float time, float seed ) const;
};

// ----------------------------------------------------------------------------
// ThreatAssessment
// Scans visible enemies each tick and computes a normalised threat level.
// ----------------------------------------------------------------------------
class ThreatAssessment {
public:
    float threatLevel;           // [0, 1]
    int   visibleThreatCount;
    float closestThreatDistance; // units (Hammer)
    float timeSinceLastDamage;   // seconds since we last received damage

    ThreatAssessment( );

    // Recompute signals from current game state.
    // lastDamageTime – the curtime of the last damage event.
    void Update( float now, float lastDamageTime );
};

// ----------------------------------------------------------------------------
// AdaptiveMovementController
// Core controller: scores presets every tick, applies hysteresis-gated
// switching, blends offsets smoothly, applies distortion layer.
// ----------------------------------------------------------------------------
class AdaptiveMovementController {
public:
    AdaptiveMovementController( );

    // Call once per tick (inside DoFakeAntiAim case 1).
    // direction  - current m_direction from HVH.
    // Writes all output fields below.
    void Update( float direction );

    // ---- Output signals ------------------------------------------------
    int   currentPreset;         // active PresetIndex
    float targetMovementOffset;  // ideal yaw = direction + this
    float blendedMovementOffset; // smoothed output (apply to view angle)
    float distortionOffset;      // pure distortion contribution (degrees)
    float switchJitterOut;       // switch-jitter contribution (degrees)
    float confidenceScore;       // 0..1 – confidence in current preset

    // ---- Debug info ----------------------------------------------------
    std::string lastSwitchReason;
    HeightState currentHeightState;
    float       currentThreatLevel;
    int         recentHits;
    int         recentEvasions;
    float       currentDistortion;

    // ---- External event hooks ------------------------------------------
    // Called from events::player_hurt when the local player is the victim.
    void OnDamageTaken( float damage );

    // Called when we detect an enemy near us has not hit for a full second.
    void OnEvasion( );

    // Called on round_start to flush history.
    void Reset( );

private:
    // ---- Preset table (defined in .cpp) --------------------------------
    static MovementPresetConfig s_presets[ ADAPTIVE_PRESET_COUNT ];

    // Read menu slider overrides and apply to s_presets.
    void SyncFromMenu( );

    // ---- Helper: derive HeightState from game flags --------------------
    HeightState DeriveHeightState( ) const;

    // ---- Scoring -------------------------------------------------------
    // Returns per-preset score array based on current signals.
    std::array< float, ADAPTIVE_PRESET_COUNT > ScorePresets(
        const ThreatAssessment& threat,
        HeightState             hs,
        float                   recentDamage,
        float                   recentEvasions
    ) const;

    // ---- Subsystems ----------------------------------------------------
    MovementHistoryTracker m_history;
    DistortionGenerator    m_distortion;
    ThreatAssessment       m_threat;

    // ---- State ---------------------------------------------------------
    int   m_activePreset;
    int   m_targetPreset;
    float m_blendAlpha;        // 0..1, lerp from old to new preset offset

    float m_holdTimer;         // seconds current preset has been held
    float m_switchCooldown;    // minimum seconds between switches
    float m_lastSwitchTime;

    float m_lastDamageTime;    // curtime of last damage event
    float m_lastEvasionTime;   // curtime of last evasion event

    float m_smoothedOffset;    // low-pass filtered blended offset
    float m_prevTime;          // last Update() time (for delta)

    int   m_debugThrottle;     // frame counter for periodic debug output

    // Minimum time (seconds) a preset must be held before switching.
    static constexpr float MIN_HOLD_TIME      = 1.5f;
    // A candidate preset must score this much higher than the active one.
    static constexpr float SWITCH_THRESHOLD   = 0.25f;
    // Seconds of cooldown after a switch before the next is allowed.
    static constexpr float POST_SWITCH_COOLDOWN = 0.8f;
    // How many frames between debug overlay updates.
    static constexpr int   DEBUG_PRINT_INTERVAL = 128;
};

extern AdaptiveMovementController g_adaptive_fakeyaw;
