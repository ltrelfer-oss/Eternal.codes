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
	if( g_cl.m_net_pos.empty( ) ) {
		math::VectorAngles( g_cl.m_local->m_vecOrigin( ) - record->m_pred_origin, away );
		return away.y;
	}

	// half of our rtt.
	// also known as the one-way delay.
	float owd = ( g_cl.m_latency / 2.f );

	// since our origins are computed here on the client
	// we have to compensate for the delay between our client and the server
	// therefore the OWD should be subtracted from the target time.
	float target = record->m_pred_time - owd;

	// iterate all.
	for( const auto &net : g_cl.m_net_pos ) {
		// get the delta between this records time context
		// and the target time.
		float dt = std::abs( target - net.m_time );

		// the best origin.
		if( dt < delta ) {
			delta = dt;
			pos   = net.m_pos;
		}
	}

	math::VectorAngles( pos - record->m_pred_origin, away );
	return away.y;
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

	// if we are in nospread mode, force all players pitches to down.
	// TODO; we should check thei actual pitch and up too, since those are the other 2 possible angles.
	// this should be somehow combined into some iteration that matches with the air angle iteration.
	if( g_menu.main.config.mode.get( ) == 1 )
		record->m_eye_angles.x = 90.f;

	// we arrived here we can do the acutal resolve.
	if( record->m_mode == Modes::RESOLVE_WALK ) 
		ResolveWalk( data, record );

	else if( record->m_mode == Modes::RESOLVE_STAND )
		ResolveStand( data, record );

	else if( record->m_mode == Modes::RESOLVE_AIR )
		ResolveAir( data, record );

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

	// copy the last record that this player was walking
	// we need it later on because it gives us crucial data.
	std::memcpy( &data->m_walk_record, record, sizeof( LagRecord ) );
}

void Resolver::ResolveAnimations( AimPlayer* data, LagRecord* record ) {
	// need at least 2 records to compare animation layer deltas.
	if( data->m_records.size( ) < 2 )
		return;

	LagRecord* previous = data->m_records[ 1 ].get( );
	if( !previous || previous->dormant( ) )
		return;

	// reset per-tick animation resolve state.
	data->m_anim_resolved = false;

	// --- layer 3 analysis: body lean / idle turn adjustment ---
	// this layer drives body rotation while standing.
	// activity 979 = ACT_CSGO_IDLE_TURN_BALANCEADJUST (body actively rotating)
	// activity 980 = ACT_CSGO_IDLE_ADJUST_STOPPEDMOVING (just stopped)
	C_AnimationLayer* layer3      = &record->m_layers[ 3 ];
	C_AnimationLayer* prev_layer3 = &previous->m_layers[ 3 ];

	int act = data->m_player->GetSequenceActivity( layer3->m_sequence );

	if( act == 979 && layer3->m_weight > 0.f ) {
		// body is actively adjusting while standing.
		// the cycle progression direction indicates which way the body is turning.
		if( layer3->m_cycle != prev_layer3->m_cycle ) {
			float cycle_delta = layer3->m_cycle - prev_layer3->m_cycle;

			// handle cycle wrap-around (0.0 to 1.0).
			if( cycle_delta > 0.5f )
				cycle_delta -= 1.f;
			else if( cycle_delta < -0.5f )
				cycle_delta += 1.f;

			if( std::abs( cycle_delta ) > 0.01f ) {
				data->m_desync_side   = ( cycle_delta > 0.f ) ? 1 : -1;
				data->m_anim_resolved = true;
			}
		}

		// weight just appeared (was near-zero, now significant) -> desync started.
		if( prev_layer3->m_weight < 0.01f && layer3->m_weight > 0.1f )
			data->m_anim_resolved = true;
	}

	// --- LBY update detection via layer weight snap ---
	// when the server forces an LBY update, layer 3 weight typically
	// drops sharply as the body snaps to the new target angle.
	if( prev_layer3->m_weight > 0.5f && layer3->m_weight < 0.1f ) {
		data->m_last_lby_time = record->m_anim_time;
		data->m_resolved_yaw  = record->m_body;
		data->m_anim_resolved = true;
	}

	// --- body yaw delta detection ---
	// significant LBY change between consecutive records is a strong signal
	// that the server forced a body update.
	float body_delta = math::NormalizedAngle( record->m_body - previous->m_body );
	if( std::abs( body_delta ) > 35.f ) {
		data->m_last_lby_time = record->m_anim_time;
		data->m_resolved_yaw  = record->m_body;
		data->m_anim_resolved = true;
	}

	// --- layer 12 cross-reference: whole body rotation ---
	// cross-check layer 12 weight delta with our layer 3 finding.
	// if they disagree, reduce confidence in the detected side.
	C_AnimationLayer* layer12      = &record->m_layers[ 12 ];
	C_AnimationLayer* prev_layer12 = &previous->m_layers[ 12 ];

	if( data->m_desync_side != 0 && layer12->m_weight > 0.f && prev_layer12->m_weight > 0.f ) {
		float w_delta = layer12->m_weight - prev_layer12->m_weight;
		if( std::abs( w_delta ) > 0.01f ) {
			int cross_side = ( w_delta > 0.f ) ? 1 : -1;
			if( cross_side != data->m_desync_side )
				data->m_desync_side = 0;
		}
	}
}

void Resolver::ResolveStand( AimPlayer* data, LagRecord* record ) {
	// for no-spread call a seperate resolver.
	if( g_menu.main.config.mode.get( ) == 1 ) {
		StandNS( data, record );
		return;
	}

	// run animation layer analysis for desync detection.
	ResolveAnimations( data, record );

	// get predicted away angle for the player.
	float away = GetAwayAngle( record );

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
		float diff = math::NormalizedAngle( record->m_body - move->m_body );
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
			// animation resolver detected a recent LBY update from layer analysis.
			// trust it if it happened within the current update window.
			if( data->m_anim_resolved && data->m_last_lby_time >= record->m_anim_time - 0.22f ) {
				record->m_eye_angles.y = data->m_resolved_yaw;
				data->m_body_update    = record->m_anim_time + 1.1f;
				record->m_mode         = Modes::RESOLVE_BODY;
				return;
			}

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

			// animation resolver has a desync direction, use it.
			if( data->m_anim_resolved && data->m_desync_side != 0 ) {
				// max desync offset clamped by the engine (~58 degrees).
				float desync_offset = 58.f * data->m_desync_side;
				record->m_eye_angles.y = move->m_body + desync_offset;

				// after enough misses, flip to the other side.
				if( data->m_stand_index > 3 )
					record->m_eye_angles.y = move->m_body - desync_offset;

				return;
			}

			// no animation data available, use deterministic brute-force
			// cycle through known desync offsets instead of random jitter.
			switch( data->m_stand_index % 7 ) {
			case 0:
				record->m_eye_angles.y = move->m_body;
				break;
			case 1:
				record->m_eye_angles.y = move->m_body + 58.f;
				break;
			case 2:
				record->m_eye_angles.y = move->m_body - 58.f;
				break;
			case 3:
				record->m_eye_angles.y = away + 180.f;
				break;
			case 4:
				record->m_eye_angles.y = move->m_body + 180.f;
				break;
			case 5:
				record->m_eye_angles.y = move->m_body + 29.f;
				break;
			case 6:
				record->m_eye_angles.y = move->m_body - 29.f;
				break;
			}

			return;
		}
	}

	// stand2 -> no known last move.
	record->m_mode = Modes::RESOLVE_STAND2;

	// animation resolver detected desync side, use it for better angle selection.
	if( data->m_anim_resolved && data->m_desync_side != 0 ) {
		float desync_offset = 58.f * data->m_desync_side;

		switch( data->m_stand_index2 % 4 ) {
		case 0:
			record->m_eye_angles.y = away + 180.f + desync_offset;
			break;
		case 1:
			record->m_eye_angles.y = record->m_body + desync_offset;
			break;
		case 2:
			record->m_eye_angles.y = away + 180.f - desync_offset;
			break;
		case 3:
			record->m_eye_angles.y = record->m_body - desync_offset;
			break;
		}

		return;
	}

	// fallback to original bruteforce when animation data is unavailable.
	switch( data->m_stand_index2 % 6 ) {

	case 0:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = record->m_body;
		break;

	case 2:
		record->m_eye_angles.y = record->m_body + 180.f;
		break;

	case 3:
		record->m_eye_angles.y = record->m_body + 110.f;
		break;

	case 4:
		record->m_eye_angles.y = record->m_body - 110.f;
		break;

	case 5:
		record->m_eye_angles.y = away;
		break;

	default:
		break;
	}
}

void Resolver::StandNS( AimPlayer* data, LagRecord* record ) {
	// get away angles.
	float away = GetAwayAngle( record );

	switch( data->m_shots % 8 ) {
	case 0:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = away + 90.f;
		break;
	case 2:
		record->m_eye_angles.y = away - 90.f;
		break;

	case 3:
		record->m_eye_angles.y = away + 45.f;
		break;
	case 4:
		record->m_eye_angles.y = away - 45.f;
		break;

	case 5:
		record->m_eye_angles.y = away + 135.f;
		break;
	case 6:
		record->m_eye_angles.y = away - 135.f;
		break;

	case 7:
		record->m_eye_angles.y = away + 0.f;
		break;

	default:
		break;
	}

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

	switch( data->m_shots % 7 ) {
	case 0:
		record->m_eye_angles.y = velyaw + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = velyaw - 90.f;
		break;

	case 2:
		record->m_eye_angles.y = velyaw + 90.f;
		break;

	case 3:
		record->m_eye_angles.y = velyaw + 135.f;
		break;

	case 4:
		record->m_eye_angles.y = velyaw - 135.f;
		break;

	case 5:
		record->m_eye_angles.y = velyaw + 45.f;
		break;

	case 6:
		record->m_eye_angles.y = velyaw - 45.f;
		break;
	}
}

void Resolver::AirNS( AimPlayer* data, LagRecord* record ) {
	// get away angles.
	float away = GetAwayAngle( record );

	switch( data->m_shots % 9 ) {
	case 0:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = away + 150.f;
		break;
	case 2:
		record->m_eye_angles.y = away - 150.f;
		break;

	case 3:
		record->m_eye_angles.y = away + 165.f;
		break;
	case 4:
		record->m_eye_angles.y = away - 165.f;
		break;

	case 5:
		record->m_eye_angles.y = away + 135.f;
		break;
	case 6:
		record->m_eye_angles.y = away - 135.f;
		break;

	case 7:
		record->m_eye_angles.y = away + 90.f;
		break;
	case 8:
		record->m_eye_angles.y = away - 90.f;
		break;

	default:
		break;
	}
}

void Resolver::ResolvePoses( Player* player, LagRecord* record ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// only do this bs when in air.
	if( record->m_mode == Modes::RESOLVE_AIR ) {
		// compute pose parameters from the resolved eye angles
		// instead of randomizing them, so the hitbox matrix
		// matches the angle the resolver picked.
		float eye_yaw   = record->m_eye_angles.y;
		float body_yaw  = record->m_body;

		// body_yaw pose: normalize the delta between eye and body yaw
		// into the 0-1 pose range. engine clamps desync to ~60 degrees,
		// so map [-60, 60] -> [0, 1] with 0.5 as center (no desync).
		float body_delta = math::NormalizedAngle( eye_yaw - body_yaw );
		float body_pose  = ( body_delta + 60.f ) / 120.f;
		math::clamp( body_pose, 0.f, 1.f );
		player->m_flPoseParameter( )[ 11 ] = body_pose;

		// lean_yaw pose: derive from pitch. map [-90, 90] -> [0, 1].
		float pitch = record->m_eye_angles.x;
		math::clamp( pitch, -90.f, 90.f );
		float lean_pose = ( pitch + 90.f ) / 180.f;
		player->m_flPoseParameter( )[ 2 ] = lean_pose;
	}

	// for standing modes, use animation resolve data for body_yaw when available.
	// body_yaw pose: 0.0 = full left, 0.5 = center, 1.0 = full right.
	if( record->m_mode == Modes::RESOLVE_STAND1 || record->m_mode == Modes::RESOLVE_STAND2 ) {
		if( data->m_anim_resolved && data->m_desync_side != 0 )
			player->m_flPoseParameter( )[ 11 ] = ( data->m_desync_side > 0 ) ? 1.f : 0.f;
	}
}