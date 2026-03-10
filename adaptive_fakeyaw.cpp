#include "includes.h"

// Global instance.
AdaptiveMovementController g_adaptive_fakeyaw{ };

// ============================================================================
// Preset definitions
// Tune these values to change each preset's behaviour.
// ============================================================================
MovementPresetConfig
AdaptiveMovementController::s_presets[ ADAPTIVE_PRESET_COUNT ] = {
    // --- Stable -----------------------------------------------------------
    // Calm, low-noise movement. Preferred at low threat with no recent hits.
    {
        /* name                */ XOR("Stable"),
        /* baseYawOffset       */ 180.f,
        /* distortionAmplitude */ 5.f,
        /* distortionFrequency */ 0.5f,
        /* switchAggressiveness*/ 0.2f,
        /* smoothingFactor     */ 0.08f,
        /* stanceMultiplier    */ 1.0f,
        /* threatMultiplier    */ 0.5f,
        /* switchJitterEnabled */ false,
        /* switchJitterRange   */ 45.f,
        /* switchJitterSpeed   */ 2.0f,
        /* switchJitterOffset  */ 0.f,
    },

    // --- DefensiveWide ---------------------------------------------------
    // Wider offset distribution. Good at moderate threat / medium engagement.
    {
        /* name                */ XOR("DefensiveWide"),
        /* baseYawOffset       */ 180.f,
        /* distortionAmplitude */ 18.f,
        /* distortionFrequency */ 1.0f,
        /* switchAggressiveness*/ 0.5f,
        /* smoothingFactor     */ 0.18f,
        /* stanceMultiplier    */ 1.2f,
        /* threatMultiplier    */ 0.8f,
        /* switchJitterEnabled */ false,
        /* switchJitterRange   */ 45.f,
        /* switchJitterSpeed   */ 2.0f,
        /* switchJitterOffset  */ 0.f,
    },

    // --- DefensiveTight --------------------------------------------------
    // Tighter but quicker pattern. Good under sustained fire from one direction.
    {
        /* name                */ XOR("DefensiveTight"),
        /* baseYawOffset       */ 180.f,
        /* distortionAmplitude */ 8.f,
        /* distortionFrequency */ 0.8f,
        /* switchAggressiveness*/ 0.6f,
        /* smoothingFactor     */ 0.14f,
        /* stanceMultiplier    */ 0.8f,
        /* threatMultiplier    */ 1.2f,
        /* switchJitterEnabled */ false,
        /* switchJitterRange   */ 45.f,
        /* switchJitterSpeed   */ 2.0f,
        /* switchJitterOffset  */ 0.f,
    },

    // --- Distorted -------------------------------------------------------
    // Moderate oscillation with burst irregularity.
    // Useful when enemies seem to have a consistent tracking read on us.
    {
        /* name                */ XOR("Distorted"),
        /* baseYawOffset       */ 180.f,
        /* distortionAmplitude */ 28.f,
        /* distortionFrequency */ 1.6f,
        /* switchAggressiveness*/ 0.4f,
        /* smoothingFactor     */ 0.22f,
        /* stanceMultiplier    */ 1.0f,
        /* threatMultiplier    */ 0.7f,
        /* switchJitterEnabled */ false,
        /* switchJitterRange   */ 45.f,
        /* switchJitterSpeed   */ 2.0f,
        /* switchJitterOffset  */ 0.f,
    },

    // --- Reactive --------------------------------------------------------
    // Fast response to recent damage patterns. Shorter hold time implicitly
    // allowed because aggressiveness is high.
    {
        /* name                */ XOR("Reactive"),
        /* baseYawOffset       */ 180.f,
        /* distortionAmplitude */ 22.f,
        /* distortionFrequency */ 2.1f,
        /* switchAggressiveness*/ 0.8f,
        /* smoothingFactor     */ 0.30f,
        /* stanceMultiplier    */ 1.0f,
        /* threatMultiplier    */ 0.9f,
        /* switchJitterEnabled */ false,
        /* switchJitterRange   */ 45.f,
        /* switchJitterSpeed   */ 2.0f,
        /* switchJitterOffset  */ 0.f,
    },

    // --- LowProfile ------------------------------------------------------
    // Reduced offsets / amplitude. Biased toward crouched / near-cover play.
    {
        /* name                */ XOR("LowProfile"),
        /* baseYawOffset       */ 180.f,
        /* distortionAmplitude */ 6.f,
        /* distortionFrequency */ 0.4f,
        /* switchAggressiveness*/ 0.3f,
        /* smoothingFactor     */ 0.09f,
        /* stanceMultiplier    */ 0.55f,
        /* threatMultiplier    */ 0.6f,
        /* switchJitterEnabled */ false,
        /* switchJitterRange   */ 45.f,
        /* switchJitterSpeed   */ 2.0f,
        /* switchJitterOffset  */ 0.f,
    },

    // --- HighPressure ----------------------------------------------------
    // Maximum unpredictability under extreme threat.
    // Strong threat weighting, strict anti-pattern-repetition bias.
    {
        /* name                */ XOR("HighPressure"),
        /* baseYawOffset       */ 180.f,
        /* distortionAmplitude */ 38.f,
        /* distortionFrequency */ 2.6f,
        /* switchAggressiveness*/ 0.9f,
        /* smoothingFactor     */ 0.36f,
        /* stanceMultiplier    */ 1.3f,
        /* threatMultiplier    */ 1.5f,
        /* switchJitterEnabled */ false,
        /* switchJitterRange   */ 45.f,
        /* switchJitterSpeed   */ 2.0f,
        /* switchJitterOffset  */ 0.f,
    },
};

// ============================================================================
// MovementHistoryTracker
// ============================================================================

MovementHistoryTracker::MovementHistoryTracker( ) { }

void MovementHistoryTracker::Push( float timestamp, float damageTaken,
                                   int evasion, HeightState hs, int preset ) {
    MovementHistoryEntry e;
    e.timestamp    = timestamp;
    e.damageTaken  = damageTaken;
    e.evasionCount = evasion;
    e.heightState  = hs;
    e.activePreset = preset;
    m_entries.push_front( e );

    while( static_cast< int >( m_entries.size( ) ) > MAX_ENTRIES )
        m_entries.pop_back( );
}

float MovementHistoryTracker::GetRecentDamage( float now, float window ) const {
    float total = 0.f;
    for( const auto& e : m_entries ) {
        if( ( now - e.timestamp ) > window ) break;
        total += e.damageTaken;
    }
    return total;
}

float MovementHistoryTracker::GetRecentEvasions( float now, float window ) const {
    float total = 0.f;
    for( const auto& e : m_entries ) {
        if( ( now - e.timestamp ) > window ) break;
        total += static_cast< float >( e.evasionCount );
    }
    return total;
}

int MovementHistoryTracker::GetRecentHitCount( float now, float window ) const {
    int count = 0;
    for( const auto& e : m_entries ) {
        if( ( now - e.timestamp ) > window ) break;
        if( e.damageTaken > 0.f ) ++count;
    }
    return count;
}

float MovementHistoryTracker::GetPresetDamageAvg( float now, float window,
                                                   int presetIndex ) const {
    float total  = 0.f;
    int   frames = 0;
    for( const auto& e : m_entries ) {
        if( ( now - e.timestamp ) > window ) break;
        if( e.activePreset != presetIndex ) continue;
        total += e.damageTaken;
        ++frames;
    }
    return ( frames > 0 ) ? ( total / static_cast< float >( frames ) ) : 0.f;
}

// ============================================================================
// DistortionGenerator
// ============================================================================

// Pseudo-noise constants – chosen to produce low-correlation output.
// TIME_QUANTUM: bucket width in seconds (~6-7 unique values/s).
// SEED_SCALE:   spreads different seeds far apart in the hash space.
// SIN/COS_FREQ / SIN/COS_PHASE: prime-ish multipliers for sine/cosine inputs.
static constexpr float PN_TIME_QUANTUM = 0.15f;
static constexpr float PN_SEED_SCALE   = 137.f;
static constexpr float PN_SIN_FREQ     = 127.1f;
static constexpr float PN_SIN_PHASE    = 311.7f;
static constexpr float PN_COS_FREQ     = 269.5f;
static constexpr float PN_COS_PHASE    = 183.3f;

float DistortionGenerator::PseudoNoise( float time, float seed ) const {
    // Quantise time into PN_TIME_QUANTUM buckets and mix with seed to get
    // a stable fractional value that changes at most ~6-7 times per second.
    float t  = std::floor( time / PN_TIME_QUANTUM ) + seed * PN_SEED_SCALE;
    float v  = std::sin( t * PN_SIN_FREQ + PN_SIN_PHASE );
    // Combine with a cosine at a different frequency for more texture.
    return v * std::cos( t * PN_COS_FREQ + PN_COS_PHASE );
}

float DistortionGenerator::Generate( float time, float amplitude,
                                      float frequency, HeightState hs,
                                      float threatLevel ) const {
    // --- Stance scalar ---------------------------------------------------
    float stanceScale = 1.0f;
    switch( hs ) {
    case HeightState::Crouched:  stanceScale = 0.55f;  break;
    case HeightState::Airborne:  stanceScale = 1.2f;   break;
    case HeightState::Elevated:  stanceScale = 1.35f;  break;
    default:                     stanceScale = 1.0f;   break;
    }

    // Airborne/elevated use a gentler oscillation to avoid snapping.
    float freqScale = 1.0f;
    if( hs == HeightState::Airborne || hs == HeightState::Elevated )
        freqScale = 0.5f;

    // --- Threat scalar: clamped so amplitude never exceeds 1.5x -----------
    float tl = std::clamp( threatLevel, 0.f, 1.f );
    float threatScale = 1.0f + tl * 0.5f;

    // --- Effective amplitude (hard-capped at 60 degrees) -----------------
    float effAmplitude = std::clamp( amplitude * stanceScale * threatScale,
                                     0.f, 60.f );

    // --- Low-frequency sine wave -----------------------------------------
    float sine = std::sin( time * frequency * freqScale * math::pi_2 );

    // --- Pseudo-random micro-adjustment (seed = 0) -----------------------
    float noise = PseudoNoise( time, 0.f ) * 0.4f; // 40% noise blend

    // --- Combine, clamp, return ------------------------------------------
    float raw = ( sine * 0.6f + noise ) * effAmplitude;
    math::clamp( raw, -effAmplitude, effAmplitude );
    return raw;
}

// ============================================================================
// ThreatAssessment
// ============================================================================

ThreatAssessment::ThreatAssessment( )
    : threatLevel{ 0.f }, visibleThreatCount{ 0 },
      closestThreatDistance{ 9999.f }, timeSinceLastDamage{ 9999.f } { }

void ThreatAssessment::Update( float now, float lastDamageTime ) {
    timeSinceLastDamage = now - lastDamageTime;

    visibleThreatCount    = 0;
    closestThreatDistance = 9999.f;

    if( !g_cl.m_local )
        return;

    vec3_t localOrigin = g_cl.m_local->m_vecOrigin( );

    for( int i = 1; i <= g_csgo.m_globals->m_max_clients; ++i ) {
        Player* player = g_csgo.m_entlist->GetClientEntity< Player* >( i );

        if( !player )
            continue;

        if( player->m_bIsLocalPlayer( ) )
            continue;

        if( !player->alive( ) )
            continue;

        // Only count enemies.
        if( player->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) )
            continue;

        if( player->dormant( ) )
            continue;

        ++visibleThreatCount;

        float dist = ( player->m_vecOrigin( ) - localOrigin ).length( );
        if( dist < closestThreatDistance )
            closestThreatDistance = dist;
    }

    // ---- Compute threat level [0,1] ------------------------------------

    // Proximity score: 0 at >=1500 units, 1 at 0 units.
    float proxScore = 0.f;
    if( closestThreatDistance < 1500.f )
        proxScore = 1.f - ( closestThreatDistance / 1500.f );

    // Count score: 0 threat at 0 enemies, saturates at 3.
    float countScore = std::clamp(
        static_cast< float >( visibleThreatCount ) / 3.f, 0.f, 1.f );

    // Recency score: threat decays to 0 after 6 seconds without damage.
    float recencyScore = std::clamp(
        1.f - ( timeSinceLastDamage / 6.f ), 0.f, 1.f );

    // Weighted sum.
    float raw = proxScore * 0.45f + countScore * 0.30f + recencyScore * 0.25f;
    threatLevel = std::clamp( raw, 0.f, 1.f );
}

// ============================================================================
// AdaptiveMovementController
// ============================================================================

AdaptiveMovementController::AdaptiveMovementController( )
    : currentPreset{ static_cast< int >( PresetIndex::Stable ) },
      targetMovementOffset{ 180.f },
      blendedMovementOffset{ 180.f },
      distortionOffset{ 0.f },
      switchJitterOut{ 0.f },
      confidenceScore{ 0.f },
      lastSwitchReason{ "init" },
      currentHeightState{ HeightState::Standing },
      currentThreatLevel{ 0.f },
      recentHits{ 0 },
      recentEvasions{ 0 },
      currentDistortion{ 0.f },
      m_activePreset{ static_cast< int >( PresetIndex::Stable ) },
      m_targetPreset{ static_cast< int >( PresetIndex::Stable ) },
      m_blendAlpha{ 1.f },
      m_holdTimer{ 0.f },
      m_switchCooldown{ 0.f },
      m_lastSwitchTime{ -999.f },
      m_lastDamageTime{ -999.f },
      m_lastEvasionTime{ -999.f },
      m_smoothedOffset{ 180.f },
      m_prevTime{ 0.f },
      m_debugThrottle{ 0 } { }

void AdaptiveMovementController::OnDamageTaken( float damage ) {
    if( !g_cl.m_local || !g_cl.m_local->alive( ) )
        return;

    m_lastDamageTime = g_csgo.m_globals->m_curtime;
    m_history.Push(
        m_lastDamageTime,
        damage,
        0,
        DeriveHeightState( ),
        m_activePreset
    );
}

void AdaptiveMovementController::OnEvasion( ) {
    m_lastEvasionTime = g_csgo.m_globals->m_curtime;
    m_history.Push(
        m_lastEvasionTime,
        0.f,
        1,
        DeriveHeightState( ),
        m_activePreset
    );
}

void AdaptiveMovementController::Reset( ) {
    m_history.m_entries.clear( );
    m_lastDamageTime  = -999.f;
    m_lastEvasionTime = -999.f;
    m_lastSwitchTime  = -999.f;
    m_holdTimer       = 0.f;
    m_switchCooldown  = 0.f;
    m_blendAlpha      = 1.f;
    m_activePreset    = static_cast< int >( PresetIndex::Stable );
    m_targetPreset    = static_cast< int >( PresetIndex::Stable );
    m_smoothedOffset  = 180.f;
    lastSwitchReason  = XOR( "round start" );
}

void AdaptiveMovementController::SyncFromMenu( ) {
    for( int i = 0; i < ADAPTIVE_PRESET_COUNT; ++i ) {
        MovementPresetConfig& p = s_presets[ i ];
        p.baseYawOffset       = g_menu.main.antiaim.adaptive_yaw_off[ i ].get( );
        p.distortionAmplitude = g_menu.main.antiaim.adaptive_dist_amp[ i ].get( );
        p.distortionFrequency = g_menu.main.antiaim.adaptive_dist_freq[ i ].get( );
        p.smoothingFactor     = g_menu.main.antiaim.adaptive_smooth[ i ].get( );

        p.switchJitterEnabled = g_menu.main.antiaim.adaptive_sw_jitter[ i ].get( );
        p.switchJitterRange   = g_menu.main.antiaim.adaptive_sw_range[ i ].get( );
        p.switchJitterSpeed   = g_menu.main.antiaim.adaptive_sw_speed[ i ].get( );
        p.switchJitterOffset  = g_menu.main.antiaim.adaptive_sw_offset[ i ].get( );
    }
}

HeightState AdaptiveMovementController::DeriveHeightState( ) const {
    if( !g_cl.m_local )
        return HeightState::Standing;

    bool onGround  = ( g_cl.m_flags & FL_ONGROUND ) != 0;
    bool crouching = ( g_cl.m_buttons & IN_DUCK ) != 0;

    if( !onGround ) {
        float vz = g_cl.m_local->m_vecVelocity( ).z;
        return ( vz > 10.f ) ? HeightState::Elevated : HeightState::Airborne;
    }

    return crouching ? HeightState::Crouched : HeightState::Standing;
}

// ----------------------------------------------------------------------------
// Preset scoring
// Returns a per-preset score array – higher = better suited to conditions.
// ----------------------------------------------------------------------------
std::array< float, ADAPTIVE_PRESET_COUNT >
AdaptiveMovementController::ScorePresets(
    const ThreatAssessment& threat,
    HeightState             hs,
    float                   recentDmg,
    float                   recentEvas ) const {

    std::array< float, ADAPTIVE_PRESET_COUNT > scores{ };
    scores.fill( 0.f );

    float tl       = threat.threatLevel;          // [0,1]
    float dist     = threat.closestThreatDistance;
    float dmgNorm  = std::clamp( recentDmg  / 100.f, 0.f, 1.f );
    float evasNorm = std::clamp( recentEvas / 5.f,   0.f, 1.f );

    // ---- Stable [PresetIndex::Stable] -----------------------------------
    // Good when: low threat, no recent damage, few or no threats visible.
    scores[ 0 ] = ( 1.f - tl ) * 0.6f
                + ( 1.f - dmgNorm ) * 0.3f
                + ( threat.visibleThreatCount == 0 ? 0.1f : 0.f );

    // ---- DefensiveWide [PresetIndex::DefensiveWide] ----------------------
    // Good when: moderate threat, a few hits, threats not right on top of us.
    scores[ 1 ] = ( tl > 0.3f ? tl * 0.4f : 0.f )
                + ( dist > 300.f && dist < 1200.f ? 0.3f : 0.f )
                + dmgNorm * 0.15f
                + ( hs == HeightState::Standing ? 0.15f : 0.f );

    // ---- DefensiveTight [PresetIndex::DefensiveTight] --------------------
    // Good when: high threat, multiple recent hits, tight engagement range.
    scores[ 2 ] = tl * 0.35f
                + dmgNorm * 0.35f
                + ( dist < 400.f ? 0.2f : 0.f )
                + ( hs == HeightState::Standing ? 0.1f : 0.f );

    // ---- Distorted [PresetIndex::Distorted] ------------------------------
    // Good when: evasions failing (enemy hits consistently), need noise.
    scores[ 3 ] = dmgNorm * 0.40f
                + ( 1.f - evasNorm ) * 0.25f
                + ( tl > 0.5f ? tl * 0.2f : 0.f )
                + ( hs != HeightState::Crouched ? 0.15f : 0.f );

    // ---- Reactive [PresetIndex::Reactive] --------------------------------
    // Good when: rapid damage events, need fast pattern changes.
    {
        int   hitCount  = m_history.GetRecentHitCount(
            g_csgo.m_globals->m_curtime, 3.f );
        float hitScore  = std::clamp(
            static_cast< float >( hitCount ) / 5.f, 0.f, 1.f );
        scores[ 4 ] = hitScore * 0.50f
                    + tl * 0.25f
                    + dmgNorm * 0.25f;
    }

    // ---- LowProfile [PresetIndex::LowProfile] ----------------------------
    // Good when: crouching, low-threat / near cover, want minimal footprint.
    scores[ 5 ] = ( hs == HeightState::Crouched ? 0.50f : 0.f )
                + ( 1.f - tl ) * 0.25f
                + ( 1.f - dmgNorm ) * 0.15f
                + evasNorm * 0.10f;

    // ---- HighPressure [PresetIndex::HighPressure] ------------------------
    // Good when: very high threat, many threats, repeated damage taken.
    scores[ 6 ] = ( tl > 0.65f ? tl * 0.50f : 0.f )
                + dmgNorm * 0.30f
                + ( threat.visibleThreatCount >= 2 ? 0.20f : 0.f );

    // History-based penalty: preset associated with recent damage is
    // disfavoured proportionally.
    float now = g_csgo.m_globals->m_curtime;
    for( int i = 0; i < ADAPTIVE_PRESET_COUNT; ++i ) {
        float avgDmg  = m_history.GetPresetDamageAvg( now, 5.f, i );
        float penalty = std::clamp( avgDmg / 20.f, 0.f, 0.4f );
        scores[ i ]   = std::max( 0.f, scores[ i ] - penalty );
    }

    return scores;
}

// ----------------------------------------------------------------------------
// Update – main per-tick logic
// ----------------------------------------------------------------------------
void AdaptiveMovementController::Update( float direction ) {
    if( !g_cl.m_local || !g_cl.m_local->alive( ) )
        return;

    // ---- Sync preset parameters from menu sliders ----------------------
    SyncFromMenu( );

    float now = g_csgo.m_globals->m_curtime;
    float dt  = now - m_prevTime;
    m_prevTime = now;

    // Clamp dt to avoid huge jumps after first call or pauses.
    if( dt <= 0.f || dt > 0.5f )
        dt = g_csgo.m_globals->m_interval;

    // ---- Check for forced preset override from menu --------------------
    int overrideIdx = static_cast< int >(
        g_menu.main.antiaim.adaptive_override.get( ) ) - 1; // 0 = auto

    // ---- Update subsystems ---------------------------------------------
    HeightState hs = DeriveHeightState( );
    m_threat.Update( now, m_lastDamageTime );

    // ---- Aggregate history signals -------------------------------------
    float rDmg  = m_history.GetRecentDamage(  now, 4.f );
    float rEvas = m_history.GetRecentEvasions( now, 4.f );
    int   rHits = m_history.GetRecentHitCount( now, 4.f );

    // Push a neutral frame (damage/evasion events push their own entries).
    m_history.Push( now, 0.f, 0, hs, m_activePreset );

    if( overrideIdx >= 0 && overrideIdx < ADAPTIVE_PRESET_COUNT ) {
        // ---- Forced preset mode ----------------------------------------
        if( m_activePreset != overrideIdx ) {
            m_activePreset   = overrideIdx;
            m_targetPreset   = overrideIdx;
            m_blendAlpha     = 0.f;
            m_holdTimer      = 0.f;
            m_lastSwitchTime = now;
            lastSwitchReason = tfm::format(
                XOR( "forced->%s" ), s_presets[ overrideIdx ].name );
        }
    } else {
        // ---- Auto mode: score presets ----------------------------------
        auto scores = ScorePresets( m_threat, hs, rDmg, rEvas );

        // Find the highest-scoring candidate.
        int   bestIdx   = 0;
        float bestScore = scores[ 0 ];
        for( int i = 1; i < ADAPTIVE_PRESET_COUNT; ++i ) {
            if( scores[ i ] > bestScore ) {
                bestScore = scores[ i ];
                bestIdx   = i;
            }
        }

        float activeScore = scores[ m_activePreset ];

        // ---- Hysteresis-gated switching --------------------------------
        m_holdTimer += dt;

        bool canSwitch =
            m_holdTimer  >= MIN_HOLD_TIME &&
            ( now - m_lastSwitchTime ) >= POST_SWITCH_COOLDOWN;

        bool shouldSwitch =
            canSwitch &&
            bestIdx != m_activePreset &&
            ( bestScore - activeScore ) >= SWITCH_THRESHOLD;

        if( shouldSwitch ) {
            if( m_targetPreset == bestIdx ) {
                m_activePreset   = bestIdx;
                m_targetPreset   = bestIdx;
                m_holdTimer      = 0.f;
                m_lastSwitchTime = now;
                m_blendAlpha     = 0.f;

                const MovementPresetConfig& pc = s_presets[ bestIdx ];
                lastSwitchReason = tfm::format(
                    XOR( "->%s (score %.2f thr %.2f dmg %.0f)" ),
                    pc.name, bestScore, m_threat.threatLevel, rDmg );
            } else {
                m_targetPreset = bestIdx;
            }
        } else if( !canSwitch ) {
            m_targetPreset = m_activePreset;
        }

        // Confidence score uses auto-mode scoring.
        float secondBest = 0.f;
        for( int i = 0; i < ADAPTIVE_PRESET_COUNT; ++i ) {
            if( i == bestIdx ) continue;
            if( scores[ i ] > secondBest ) secondBest = scores[ i ];
        }
        confidenceScore = std::clamp( ( bestScore - secondBest ) / 0.5f, 0.f, 1.f );
    }

    // ---- Compute effective amplitude -----------------------------------
    const MovementPresetConfig& active = s_presets[ m_activePreset ];

    float stanceAmplMult = 1.0f;
    switch( hs ) {
    case HeightState::Crouched:  stanceAmplMult = active.stanceMultiplier * 0.6f;  break;
    case HeightState::Airborne:  stanceAmplMult = active.stanceMultiplier * 1.2f;  break;
    case HeightState::Elevated:  stanceAmplMult = active.stanceMultiplier * 1.35f; break;
    default:                     stanceAmplMult = active.stanceMultiplier;         break;
    }

    float effAmplitude = active.distortionAmplitude
                       * stanceAmplMult
                       * ( 1.f + m_threat.threatLevel * active.threatMultiplier );

    // Hard-cap to ±60 degrees to prevent unnatural snapping.
    effAmplitude = std::clamp( effAmplitude, 0.f, 60.f );

    // ---- Distortion offset ---------------------------------------------
    distortionOffset = m_distortion.Generate(
        now,
        effAmplitude,
        active.distortionFrequency,
        hs,
        m_threat.threatLevel );

    // ---- Switch-jitter layer -------------------------------------------
    switchJitterOut = 0.f;
    if( active.switchJitterEnabled ) {
        // Alternating sign at switchJitterSpeed Hz, with range and offset.
        float halfRange = active.switchJitterRange / 2.f;
        float phase     = std::sin( now * active.switchJitterSpeed
                                    * math::pi_2 );
        // Hard left/right switch modulated by sine amplitude (0..1).
        float sign = ( phase >= 0.f ) ? 1.f : -1.f;
        float magnitude = std::abs( phase );
        switchJitterOut = sign * halfRange * magnitude + active.switchJitterOffset;

        // Cap to ±180.
        switchJitterOut = std::clamp( switchJitterOut, -180.f, 180.f );
    }

    // ---- Target and blended offset -------------------------------------
    targetMovementOffset = active.baseYawOffset;

    // Advance blend factor: 150 ms to reach new preset.
    m_blendAlpha += dt * ( 1.f / 0.15f );
    math::clamp( m_blendAlpha, 0.f, 1.f );

    float blendedBase = ( m_blendAlpha >= 1.f )
        ? targetMovementOffset
        : ( m_smoothedOffset * ( 1.f - m_blendAlpha )
            + targetMovementOffset * m_blendAlpha );

    // Low-pass smooth (tick-rate normalised).
    float alpha = std::clamp(
        active.smoothingFactor * ( dt / g_csgo.m_globals->m_interval ),
        0.01f, 1.f );
    m_smoothedOffset += ( blendedBase - m_smoothedOffset ) * alpha;

    blendedMovementOffset = m_smoothedOffset;

    // ---- Expose public debug fields ------------------------------------
    currentPreset      = m_activePreset;
    currentHeightState = hs;
    currentThreatLevel = m_threat.threatLevel;
    recentHits         = rHits;
    recentEvasions     = static_cast< int >( rEvas );
    currentDistortion  = distortionOffset;

    // ---- Periodic debug overlay ----------------------------------------
    ++m_debugThrottle;
    if( m_debugThrottle >= DEBUG_PRINT_INTERVAL ) {
        m_debugThrottle = 0;

        std::string dbg = tfm::format(
            XOR( "[afky] preset=%s thr=%.2f hs=%d hits=%d evas=%d "
                 "dist=%.1f swj=%.1f conf=%.2f %s\n" ),
            s_presets[ m_activePreset ].name,
            m_threat.threatLevel,
            static_cast< int >( hs ),
            rHits,
            static_cast< int >( rEvas ),
            distortionOffset,
            switchJitterOut,
            confidenceScore,
            lastSwitchReason );

        // Green on no recent hits; red if we recently took damage.
        Color dbgColor = ( rHits > 0 ) ? colors::red : colors::transparent_green;
        g_notify.add( dbg, dbgColor, 4.f, false );
    }
}
