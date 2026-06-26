#pragma once

class ShotRecord;

class Resolver {
public:
	enum Modes : size_t {
		RESOLVE_NONE = 0,
		RESOLVE_WALK,
		RESOLVE_STAND,
		RESOLVE_STAND1,
		RESOLVE_STAND2,
		RESOLVE_AIR,
		RESOLVE_BODY,
		RESOLVE_STOPPED_MOVING,
	};

public:
	LagRecord* FindIdealRecord( AimPlayer* data );
	LagRecord* FindLastRecord( AimPlayer* data );

	LagRecord *FindFirstRecord( AimPlayer *data );

	void OnBodyUpdate( Player* player, float value );
	float GetAwayAngle( LagRecord* record );

	// 2018-style helpers: animation/delta desync, pitch and rotation resolving.
	float GetMaxDesync( LagRecord* record );
	float AnimationSide( AimPlayer* data, LagRecord* record );
	void  ResolvePitch( AimPlayer* data, LagRecord* record );
	bool  DetectRotation( AimPlayer* data, LagRecord* record, float& predicted );

	void MatchShot( AimPlayer* data, LagRecord* record );
	void SetMode( LagRecord* record );

	// learning core: candidate angle tables + score-based slot selection.
	// gamesense-style; the resolver converges on what lands per player instead
	// of blindly cycling angles by shot count.
	int   SelectSlot( const float* scores, int n );
	float CandidateStand( float base, float maxdesync, float away, int side, int slot );
	float CandidateStandNS( float away, int slot );
	float CandidateAir( float ref, int slot );

	// hit / miss feedback applied from shots.cpp - rewards / penalizes the
	// exact candidate slot the record was resolved with.
	void  ResolverFeedback( AimPlayer* data, LagRecord* record, bool hit, bool head );

	void ResolveAngles( Player* player, LagRecord* record );
	void ResolveWalk( AimPlayer* data, LagRecord* record );
	void ResolveStand( AimPlayer* data, LagRecord* record );
	void StandNS( AimPlayer* data, LagRecord* record );
	void ResolveAir( AimPlayer* data, LagRecord* record );

	void AirNS( AimPlayer* data, LagRecord* record );
	void ResolvePoses( Player* player, LagRecord* record );

public:
	std::array< vec3_t, 64 > m_impacts;
};

extern Resolver g_resolver;