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
		RESOLVE_ANIM,
	};

	enum Confidence : size_t {
		CONFIDENCE_LOW = 0,
		CONFIDENCE_MEDIUM,
		CONFIDENCE_HIGH,
	};

public:
	LagRecord* FindIdealRecord( AimPlayer* data );
	LagRecord* FindLastRecord( AimPlayer* data );

	LagRecord *FindFirstRecord( AimPlayer *data );

	void OnBodyUpdate( Player* player, float value );
	float GetAwayAngle( LagRecord* record );

	void MatchShot( AimPlayer* data, LagRecord* record );
	void SetMode( LagRecord* record );

	void ResolveAngles( Player* player, LagRecord* record );
	void ResolveWalk( AimPlayer* data, LagRecord* record );
	void ResolveStand( AimPlayer* data, LagRecord* record );
	void StandNS( AimPlayer* data, LagRecord* record );
	void ResolveAir( AimPlayer* data, LagRecord* record );

	void AirNS( AimPlayer* data, LagRecord* record );
	void ResolveAnimations( AimPlayer* data, LagRecord* record );
	void ResolvePoses( Player* player, LagRecord* record );

	float DetectDesyncFromLayers( AimPlayer* data, LagRecord* record );
	size_t ComputeConfidence( AimPlayer* data, LagRecord* record );

public:
	std::array< vec3_t, 64 > m_impacts;
};

extern Resolver g_resolver;