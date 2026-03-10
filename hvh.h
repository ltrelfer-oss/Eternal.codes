#pragma once

class AdaptiveAngle {
public:
	float m_yaw;
	float m_dist;

public:
	// ctor.
	__forceinline AdaptiveAngle( float yaw, float penalty = 0.f ) {
		// set yaw.
		m_yaw = math::NormalizedAngle( yaw );

		// init distance.
		m_dist = 0.f;

		// remove penalty.
		m_dist -= penalty;
	}
};

enum AntiAimMode : size_t {
	STAND = 0,
	WALK,
	AIR,
};

class HVH {
public:
	size_t m_mode;
	int    m_pitch;
	int    m_yaw;
	float  m_jitter_range;
	float  m_rot_range;
	float  m_rot_speed;
	float  m_rand_update;
	int    m_dir;
	float  m_dir_custom;
	size_t m_base_angle;
	float  m_auto_time;

	bool   m_step_switch;
	int    m_random_lag;
	float  m_next_random_update;
	float  m_random_angle;
	float  m_direction;
	float  m_auto;
	float  m_auto_dist;
	float  m_auto_last;
	float  m_view;

	// --- extended customisation state ---
	float  m_orient_offset;       // blended orientation offset
	float  m_horiz_current_yaw;   // current horizontal yaw (smoothed)
	float  m_horiz_velocity;      // current turn velocity (deg/s)
	float  m_vert_current_pitch;  // current vertical pitch (smoothed)

	// randomization state
	float  m_ext_rand_next;       // next randomization update time
	float  m_ext_rand_angle;      // current randomized offset

	// timeline state
	float  m_tl_start_time;       // timeline playback start
	bool   m_tl_forward;          // ping-pong direction

	// input reaction state
	float  m_input_react_time;    // timestamp of last input event
	float  m_input_yaw_contrib;   // accumulated yaw from input reaction

	// limits state
	float  m_prev_yaw;            // previous frame yaw for rate limiting

	// active profile index (runtime)
	int    m_active_profile;

public:
	void IdealPitch( );
	void AntiAimPitch( );
	void AutoDirection( );
	void GetAntiAimDirection( );
    bool DoEdgeAntiAim( Player *player, ang_t &out );
	void DoRealAntiAim( );
	void DoFakeAntiAim( );
	void AntiAim( );
	void SendPacket( );

	// extended customisation helpers
	void ApplyOrientationLayer( );
	void ApplyHorizontalControl( float dt );
	void ApplyVerticalControl( float dt );
	void ApplyRandomization( float curtime );
	float EvaluateTimeline( float curtime );
	void ApplyInputReaction( float dt );
	void ApplyLimitsAndSafety( float dt );
	void ResetExtendedState( );
	int  ResolveActiveProfile( );
};

extern HVH g_hvh;