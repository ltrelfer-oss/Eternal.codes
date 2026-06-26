#include "includes.h"

Resolver g_resolver{};;

LagRecord* Resolver::FindIdealRecord( AimPlayer* data ) {
    LagRecord *first_valid, *current;

	if( data->m_records.empty( ) )
		return nullptr;

    first_valid = nullptr;

    // iterate records.
	for( const auto &it : data->m_records ) {
		if( it->dormant( ) || it->immune( ) || !it->valid( ) )
			continue;

        // get current record.
        current = it.get( );

        // first record that was valid, store it for later.
        if( !first_valid )
            first_valid = current;

        // try to find a record with a shot, lby update, walking or no anti-aim.
		if( it->m_shot || it->m_mode == Modes::RESOLVE_BODY || it->m_mode == Modes::RESOLVE_WALK || it->m_mode == Modes::RESOLVE_NONE )
            return current;
	}

	// none found above, return the first valid record if possible.
	return ( first_valid ) ? first_valid : nullptr;
}

LagRecord* Resolver::FindLastRecord( AimPlayer* data ) {
    LagRecord* current;

	if( data->m_records.empty( ) )
		return nullptr;

	// iterate records in reverse.
	for( auto it = data->m_records.crbegin( ); it != data->m_records.crend( ); ++it ) {
		current = it->get( );

		// if this record is valid.
		// we are done since we iterated in reverse.
		if( current->valid( ) && !current->immune( ) && !current->dormant( ) )
			return current;
	}

	return nullptr;
}

void Resolver::OnBodyUpdate( Player* player, float value ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// set data.
	data->m_old_body = data->m_body;
	data->m_body     = value;
}

float Resolver::GetAwayAngle( LagRecord* record ) {
	float  delta{ std::numeric_limits< float >::max( ) };
	vec3_t pos;
	ang_t  away;

	// other cheats predict you by their own latency.
	// they do this because, then they can put their away angle to exactly
	// where you are on the server at that moment in time.

	// the idea is that you would need to know where they 'saw' you when they created their user-command.
	// lets say you move on your client right now, this would take half of our latency to arrive at the server.
	// the delay between the server and the target client is compensated by themselves already, that is fortunate for us.

	// we have no historical origins.
	// no choice but to use the most recent one.
	//if( g_cl.m_net_pos.empty( ) ) {
		math::VectorAngles( g_cl.m_local->m_vecOrigin( ) - record->m_pred_origin, away );
		return away.y;
	//}

	// half of our rtt.
	// also known as the one-way delay.
	//float owd = ( g_cl.m_latency / 2.f );

	// since our origins are computed here on the client
	// we have to compensate for the delay between our client and the server
	// therefore the OWD should be subtracted from the target time.
	//float target = record->m_pred_time; //- owd;

	// iterate all.
	//for( const auto &net : g_cl.m_net_pos ) {
		// get the delta between this records time context
		// and the target time.
	//	float dt = std::abs( target - net.m_time );

		// the best origin.
	//	if( dt < delta ) {
	//		delta = dt;
	//		pos   = net.m_pos;
	//	}
	//}

	//math::VectorAngles( pos - record->m_pred_origin, away );
	//return away.y;
}

float Resolver::GetMaxDesync( LagRecord* record ) {
	// the animation state clamps the lower body to ~58-60 deg from the eye
	// yaw while standing; that window shrinks as the player runs.
	float speed = record->m_anim_velocity.length_2d( );

	// running scale ( 0 standing .. 1 at/above run speed ~260 ).
	float run = speed / 260.f;
	math::clamp( run, 0.f, 1.f );

	// 60 deg standing down to ~30 deg while moving.
	return 60.f - ( run * 30.f );
}

float Resolver::AnimationSide( AimPlayer* data, LagRecord* record ) {
	// delta-based signal: networked eye yaw vs the lower body yaw target.
	float lby_delta = math::NormalizedAngle( record->m_eye_angles.y - record->m_body );

	// animation-based signal: the foot yaw the animstate settled on.
	float anim_delta = 0.f;
	CCSGOPlayerAnimState* state = data->m_player->m_PlayerAnimState( );
	if( state )
		anim_delta = math::NormalizedAngle( record->m_eye_angles.y - state->m_goal_feet_yaw );

	// combine both signals; the sign indicates which way the body is turned.
	float sum = lby_delta + anim_delta;

	if( sum > 5.f )
		return 1.f;   // body turned to the right of the eye yaw.

	if( sum < -5.f )
		return -1.f;  // body turned to the left of the eye yaw.

	return 0.f;
}

// pick the highest-scoring candidate slot. a tiny index penalty breaks ties
// toward lower ( more likely ) slots, which also gives a deterministic
// cold-start ordering before any feedback has been learned.
int Resolver::SelectSlot( const float* scores, int n ) {
	int   best = 0;
	float best_val = scores[ 0 ];

	for( int i = 1; i < n; ++i ) {
		float val = scores[ i ] - ( float )i * 0.01f;
		if( val > best_val ) {
			best_val = val;
			best     = i;
		}
	}

	return best;
}

// matchmaking stand candidate table ( lby-centered ), ordered by likelihood.
float Resolver::CandidateStand( float base, float maxdesync, float away, int side, int slot ) {
	switch( slot ) {
	case 0:  return base + side * maxdesync;          // seeded side, max desync.
	case 1:  return base - side * maxdesync;          // opposite side, max desync.
	case 2:  return base + side * maxdesync * 0.5f;   // seeded side, half desync.
	case 3:  return base - side * maxdesync * 0.5f;   // opposite side, half desync.
	case 4:  return base;                             // zero desync ( facing lby ).
	case 5:  return base + 180.f;                     // flipped lby.
	case 6:  return away + 180.f;                     // away-based backwards.
	case 7:  return away;                             // straight at us.
	default: return base;
	}
}

// nospread stand candidate table ( away-centered true bruteforce ).
float Resolver::CandidateStandNS( float away, int slot ) {
	switch( slot ) {
	case 0:  return away + 180.f;
	case 1:  return away + 90.f;
	case 2:  return away - 90.f;
	case 3:  return away + 45.f;
	case 4:  return away - 45.f;
	case 5:  return away + 135.f;
	case 6:  return away - 135.f;
	case 7:  return away;
	default: return away + 180.f;
	}
}

// air candidate table ( reference = velocity yaw ), ordered by likelihood.
float Resolver::CandidateAir( float ref, int slot ) {
	switch( slot ) {
	case 0:  return ref + 180.f;
	case 1:  return ref - 90.f;
	case 2:  return ref + 90.f;
	case 3:  return ref - 135.f;
	case 4:  return ref + 135.f;
	case 5:  return ref - 150.f;
	case 6:  return ref + 150.f;
	case 7:  return ref - 45.f;
	case 8:  return ref + 45.f;
	default: return ref + 180.f;
	}
}

void Resolver::ResolverFeedback( AimPlayer* data, LagRecord* record, bool hit, bool head ) {
	// pick the score table that matches the mode the record was resolved with.
	float* scores = nullptr;
	int    n      = 0;

	switch( record->m_mode ) {
	case Modes::RESOLVE_STAND:
	case Modes::RESOLVE_STAND1:
	case Modes::RESOLVE_STAND2:
		scores = data->m_stand_score;
		n      = AimPlayer::STAND_SLOTS;
		break;

	case Modes::RESOLVE_AIR:
		scores = data->m_air_score;
		n      = AimPlayer::AIR_SLOTS;
		break;

	default:
		// walk / body / stopped-moving reveal the real angle, nothing to learn.
		break;
	}

	int idx = record->m_resolve_index;

	if( scores && idx >= 0 && idx < n ) {
		if( hit ) {
			// reward harder for a headshot ( exact angle ) than a body hit.
			scores[ idx ] += head ? 3.f : 1.5f;
			data->m_missed_shots = 0;
		}
		else {
			// wrong angle, push this slot down so the next-best is explored.
			scores[ idx ] -= 1.f;
			++data->m_missed_shots;
		}

		math::clamp( scores[ idx ], -10.f, 10.f );
	}

	// learn the pitch slot too ( zero / down / up ).
	int pidx = record->m_pitch_idx;
	if( pidx >= 0 && pidx < AimPlayer::PITCH_SLOTS ) {
		data->m_pitch_score[ pidx ] += hit ? ( head ? 3.f : 1.f ) : -1.f;
		math::clamp( data->m_pitch_score[ pidx ], -10.f, 10.f );
	}
}

void Resolver::ResolvePitch( AimPlayer* data, LagRecord* record ) {
	// the 3 pitches that fake-pitch anti-aims pin to: zero / fake-down / fake-up.
	static const float pitches[ 3 ] = { 0.f, 89.f, -89.f };

	bool nospread = g_menu.main.config.mode.get( ) == 1;

	// a faked pitch sits pinned at an extreme value.
	bool faked = std::abs( record->m_eye_angles.x ) > 85.f;

	// in matchmaking with a believable pitch, trust the networked value.
	if( !nospread && !faked ) {
		// mark as not bruteforced so feedback won't touch a pitch slot.
		record->m_pitch_idx = -1;
		return;
	}

	// learn which of the 3 pitches actually lands instead of cycling.
	int slot = SelectSlot( data->m_pitch_score, AimPlayer::PITCH_SLOTS );
	record->m_pitch_idx    = slot;
	record->m_eye_angles.x = pitches[ slot ];
}

bool Resolver::DetectRotation( AimPlayer* data, LagRecord* record, float& predicted ) {
	// raw networked yaw ( resolve hasn't overwritten it yet ).
	float raw = record->m_eye_angles.y;

	// per-tick change since the previous resolved record.
	float delta = math::NormalizedAngle( raw - data->m_last_eye_yaw );

	bool result = false;

	// spinning: a large, consistent same-direction change two ticks in a row.
	if( std::abs( delta ) > 22.f && std::abs( data->m_yaw_rate ) > 22.f
		&& ( delta * data->m_yaw_rate ) > 0.f ) {
		// predict where the spin continues to next tick.
		predicted = math::NormalizedAngle( raw + delta );
		result = true;
	}

	// remember the rate for the next tick.
	data->m_yaw_rate = delta;
	return result;
}

void Resolver::MatchShot( AimPlayer* data, LagRecord* record ) {
	// do not attempt to do this in nospread mode.
	if( g_menu.main.config.mode.get( ) == 1 )
		return;

	float shoot_time = -1.f;

	Weapon* weapon = data->m_player->GetActiveWeapon( );
	if( weapon ) {
		// with logging this time was always one tick behind.
		// so add one tick to the last shoot time.
		shoot_time = weapon->m_fLastShotTime( ) + g_csgo.m_globals->m_interval;
	}

	// this record has a shot on it.
	if( game::TIME_TO_TICKS( shoot_time ) == game::TIME_TO_TICKS( record->m_sim_time ) ) {
		if( record->m_lag <= 2 )
			record->m_shot = true;
		
		// more then 1 choke, cant hit pitch, apply prev pitch.
		else if( data->m_records.size( ) >= 2 ) {
			LagRecord* previous = data->m_records[ 1 ].get( );

			if( previous && !previous->dormant( ) )
				record->m_eye_angles.x = previous->m_eye_angles.x;
		}
	}
}

void Resolver::SetMode( LagRecord* record ) {
	// the resolver has 3 modes to chose from.
	// these modes will vary more under the hood depending on what data we have about the player
	// and what kind of hack vs. hack we are playing (mm/nospread).

	float speed = record->m_anim_velocity.length( );

	// if on ground, moving, and not fakewalking.
	if( ( record->m_flags & FL_ONGROUND ) && speed > 0.1f && !record->m_fake_walk )
		record->m_mode = Modes::RESOLVE_WALK;

	// if on ground, not moving or fakewalking.
	if( ( record->m_flags & FL_ONGROUND ) && ( speed <= 0.1f || record->m_fake_walk ) )
		record->m_mode = Modes::RESOLVE_STAND;

	// if not on ground.
	else if( !( record->m_flags & FL_ONGROUND ) )
		record->m_mode = Modes::RESOLVE_AIR;
}

void Resolver::ResolveAngles( Player* player, LagRecord* record ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// mark this record if it contains a shot.
	MatchShot( data, record );

	// next up mark this record with a resolver mode that will be used.
	SetMode( record );

	// raw networked yaw before any resolver overwrites it ( rotation delta ).
	float raw_yaw = record->m_eye_angles.y;

	// resolve the pitch ( zero / fake-down / fake-up ) using miss feedback.
	// this replaces the old 'force pitch to 90' nospread behaviour and also
	// brute-forces faked pitches in matchmaking.
	ResolvePitch( data, record );

	// detect spinning / jitter anti-aim and predict its continuation.
	float predicted = 0.f;
	bool rotating = DetectRotation( data, record, predicted );

	// we arrived here we can do the acutal resolve.
	if( record->m_mode == Modes::RESOLVE_WALK ) 
		ResolveWalk( data, record );

	else if( record->m_mode == Modes::RESOLVE_STAND )
		ResolveStand( data, record );

	else if( record->m_mode == Modes::RESOLVE_AIR )
		ResolveAir( data, record );

	// a rotating player networks a moving real yaw; the brute resolvers
	// fight against that, so override them with the predicted continuation.
	if( rotating )
		record->m_eye_angles.y = predicted;

	// store the raw yaw for the next tick's rotation delta.
	data->m_last_eye_yaw = raw_yaw;

	// normalize the eye angles, doesn't really matter but its clean.
	math::NormalizeAngle( record->m_eye_angles.y );
}

void Resolver::ResolveWalk( AimPlayer* data, LagRecord* record ) {
	// apply lby to eyeangles.
	record->m_eye_angles.y = record->m_body;

	// delay body update.
	data->m_body_update = record->m_anim_time + 0.22f;

	// reset stand and body index.
	data->m_stand_index  = 0;
	data->m_stand_index2 = 0;
	data->m_body_index   = 0;

	// a walking player reveals real angles, so reset brute state.
	data->m_pitch_index  = 0;
	data->m_missed_shots = 0;

	// store the away angle for server-feedback learning.
	record->m_away = GetAwayAngle( record );

	// remember just the fields we need from the last walking record. a full
	// memcpy would alias this record's bone buffer pointer and double-free it
	// when either record is destroyed.
	data->m_walk_record.m_sim_time      = record->m_sim_time;
	data->m_walk_record.m_anim_time     = record->m_anim_time;
	data->m_walk_record.m_body          = record->m_body;
	data->m_walk_record.m_origin        = record->m_origin;
	data->m_walk_record.m_anim_velocity = record->m_anim_velocity;
}

void Resolver::ResolveStand( AimPlayer* data, LagRecord* record ) {
	// for no-spread call a seperate resolver.
	if( g_menu.main.config.mode.get( ) == 1 ) {
		StandNS( data, record );
		return;
	}

	// get predicted away angle for the player and remember it for feedback.
	float away = GetAwayAngle( record );
	record->m_away = away;

	// max desync window for this record ( animation based ).
	float maxdesync = GetMaxDesync( record );

	// pointer for easy access.
	LagRecord* move = &data->m_walk_record;

	// we have a valid moving record.
	if( move->m_sim_time > 0.f ) {
		vec3_t delta = move->m_origin - record->m_origin;

		// check if moving record is close.
		if( delta.length( ) <= 128.f ) {
			// indicate that we are using the moving lby.
			data->m_moved = true;
		}
	}

	// a valid moving context was found
	if( data->m_moved ) {
		float delta = record->m_anim_time - move->m_anim_time;

		// it has not been time for this first update yet.
		if( delta < 0.22f ) {
			// set angles to current LBY.
			record->m_eye_angles.y = move->m_body;

			// set resolve mode.
			record->m_mode = Modes::RESOLVE_STOPPED_MOVING;

			// exit out of the resolver, thats it.
			return;
		}

		// LBY SHOULD HAVE UPDATED HERE.
		else if( record->m_anim_time >= data->m_body_update ) {
			// only shoot the LBY flick 3 times.
			// if we happen to miss then we most likely mispredicted.
			if( data->m_body_index <= 3 ) {
				// set angles to current LBY.
				record->m_eye_angles.y = record->m_body;

				// predict next body update.
				data->m_body_update = record->m_anim_time + 1.1f;

				// set the resolve mode.
				record->m_mode = Modes::RESOLVE_BODY;

				return;
			}

			// set to stand1 -> known last move.
			record->m_mode = Modes::RESOLVE_STAND1;
		}
	}

	// no moving context -> pure standing desync.
	if( record->m_mode != Modes::RESOLVE_STAND1 )
		record->m_mode = Modes::RESOLVE_STAND2;

	// base yaw we desync around: last-known LBY when we moved, else networked LBY.
	float base = ( record->m_mode == Modes::RESOLVE_STAND1 ) ? move->m_body : record->m_body;

	// server-based resolving: if a head hit taught us an offset and we have
	// not started missing, trust the learned angle first. the offset is stored
	// relative to the networked lby so it tracks the player.
	if( data->m_has_stand && data->m_missed_shots < 1 ) {
		record->m_eye_angles.y = record->m_body + data->m_prefer_stand;
		return;
	}

	// animation + delta based side seed.
	float seed = AnimationSide( data, record );
	int   side = ( seed >= 0.f ) ? 1 : -1;
	data->m_side = side;

	// learning resolver: pick the candidate slot that has landed the most for
	// this player instead of cycling blindly. mm uses the first 6 candidates.
	int slot = SelectSlot( data->m_stand_score, 6 );

	// remember which slot we resolved with so hit / miss feedback rewards or
	// penalizes the right candidate.
	record->m_resolve_index = slot;

	record->m_eye_angles.y = CandidateStand( base, maxdesync, away, side, slot );
}

void Resolver::StandNS( AimPlayer* data, LagRecord* record ) {
	// get away angles and remember them for server-feedback learning.
	float away = GetAwayAngle( record );
	record->m_away = away;

	// learning resolver: nospread uses all 8 candidate slots, picked by score
	// instead of cycling on shot count.
	int slot = SelectSlot( data->m_stand_score, AimPlayer::STAND_SLOTS );
	record->m_resolve_index = slot;

	record->m_eye_angles.y = CandidateStandNS( away, slot );

	// force LBY to not fuck any pose and do a true bruteforce.
	record->m_body = record->m_eye_angles.y;
}

void Resolver::ResolveAir( AimPlayer* data, LagRecord* record ) {
	// for no-spread call a seperate resolver.
	if( g_menu.main.config.mode.get( ) == 1 ) {
		AirNS( data, record );
		return;
	}

	// else run our matchmaking air resolver.

	// we have barely any speed. 
	// either we jumped in place or we just left the ground.
	// or someone is trying to fool our resolver.
	if( record->m_velocity.length_2d( ) < 60.f ) {
		// set this for completion.
		// so the shot parsing wont pick the hits / misses up.
		// and process them wrongly.
		record->m_mode = Modes::RESOLVE_STAND;

		// invoke our stand resolver.
		ResolveStand( data, record );

		// we are done.
		return;
	}

	// try to predict the direction of the player based on his velocity direction.
	// this should be a rough estimation of where he is looking.
	float velyaw = math::rad_to_deg( std::atan2( record->m_velocity.y, record->m_velocity.x ) );

	// store the reference angle for server-feedback learning.
	record->m_away = velyaw;

	// server-based resolving: trust a learned air offset before brute forcing.
	if( data->m_has_air && data->m_missed_shots < 1 ) {
		record->m_eye_angles.y = velyaw + data->m_prefer_air;
		return;
	}

	// learning resolver: pick the best air candidate by score. mm uses the
	// first 3 candidates ( back / left / right ).
	int slot = SelectSlot( data->m_air_score, 3 );
	record->m_resolve_index = slot;

	record->m_eye_angles.y = CandidateAir( velyaw, slot );
}

void Resolver::AirNS( AimPlayer* data, LagRecord* record ) {
	// get away angles and remember them for server-feedback learning.
	float away = GetAwayAngle( record );
	record->m_away = away;

	// learning resolver: nospread air uses all 9 candidate slots picked by
	// score instead of cycling on shot count.
	int slot = SelectSlot( data->m_air_score, AimPlayer::AIR_SLOTS );
	record->m_resolve_index = slot;

	record->m_eye_angles.y = CandidateAir( away, slot );
}

void Resolver::ResolvePoses( Player* player, LagRecord* record ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// only do this bs when in air.
	if( record->m_mode == Modes::RESOLVE_AIR ) {
		// ang = pose min + pose val x ( pose range )

		// lean_yaw
		player->m_flPoseParameter( )[ 2 ]  = g_csgo.RandomInt( 0, 4 ) * 0.25f;   

		// body_yaw
		player->m_flPoseParameter( )[ 11 ] = g_csgo.RandomInt( 1, 3 ) * 0.25f;
	}
}