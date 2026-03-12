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

        // try to find a record with a shot, lby update, walking, animation-based, or no anti-aim.
		if( it->m_shot || it->m_mode == Modes::RESOLVE_BODY || it->m_mode == Modes::RESOLVE_WALK || it->m_mode == Modes::RESOLVE_NONE || it->m_mode == Modes::RESOLVE_ANIM )
            return current;

		// if confidence flag is enabled, prefer higher confidence records.
		if( g_menu.main.aimbot.resolver_confidence.get( ) > 0 ) {
			size_t min_conf = g_menu.main.aimbot.resolver_confidence.get( ) - 1;
			if( it->m_resolver_confidence >= min_conf )
				return current;
		}
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

LagRecord* Resolver::FindFirstRecord( AimPlayer* data ) {
	if( data->m_records.empty( ) )
		return nullptr;

	// iterate records from oldest to newest.
	for( auto it = data->m_records.crbegin( ); it != data->m_records.crend( ); ++it ) {
		LagRecord* current = it->get( );

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
	ang_t  away;

	math::VectorAngles( g_cl.m_local->m_vecOrigin( ) - record->m_pred_origin, away );
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

float Resolver::DetectDesyncFromLayers( AimPlayer* data, LagRecord* record ) {
	// need previous records for comparison.
	if( data->m_records.size( ) < 2 )
		return 0.f;

	LagRecord* previous = data->m_records[ 1 ].get( );
	if( !previous || previous->dormant( ) )
		return 0.f;

	// analyze layer 3 (body adjust/lean layer).
	C_AnimationLayer* curr_layer = &record->m_layers[ 3 ];
	C_AnimationLayer* prev_layer = &previous->m_layers[ 3 ];

	int act = data->m_player->GetSequenceActivity( curr_layer->m_sequence );

	// ACT_CSGO_IDLE_TURN_BALANCEADJUST = 980
	// ACT_CSGO_IDLE_ADJUST_STOPPEDMOVING = 979
	// these activities fire when the body adjusts to match actual yaw.
	if( act == 979 || act == 980 ) {
		float weight_delta = curr_layer->m_weight - prev_layer->m_weight;

		// significant weight change indicates active adjustment.
		if( std::abs( weight_delta ) > 0.0f ) {
			// playback rate direction indicates desync side.
			if( curr_layer->m_playback_rate > 0.f )
				return -60.f;
			else if( curr_layer->m_playback_rate < 0.f )
				return 60.f;
		}

		// cycle progression can also indicate direction.
		if( curr_layer->m_weight > 0.f && curr_layer->m_cycle > prev_layer->m_cycle ) {
			return curr_layer->m_playback_rate > 0.f ? -58.f : 58.f;
		}
	}

	// analyze layer 12 weight delta for lean direction.
	float lean_delta = record->m_layers[ 12 ].m_weight - previous->m_layers[ 12 ].m_weight;
	if( std::abs( lean_delta ) > 0.01f ) {
		return lean_delta > 0.f ? 58.f : -58.f;
	}

	// check if we have a cached desync from previous detections.
	if( std::abs( data->m_last_desync ) > 1.f )
		return data->m_last_desync;

	return 0.f;
}

size_t Resolver::ComputeConfidence( AimPlayer* data, LagRecord* record ) {
	// walking - LBY is reliable.
	if( record->m_mode == Modes::RESOLVE_WALK || record->m_mode == Modes::RESOLVE_NONE )
		return Confidence::CONFIDENCE_HIGH;

	// body update was recently detected.
	if( record->m_mode == Modes::RESOLVE_BODY || record->m_mode == Modes::RESOLVE_STOPPED_MOVING )
		return Confidence::CONFIDENCE_HIGH;

	// animation layer analysis gave us a direction.
	if( record->m_mode == Modes::RESOLVE_ANIM )
		return Confidence::CONFIDENCE_MEDIUM;

	// stand with known move context.
	if( record->m_mode == Modes::RESOLVE_STAND1 )
		return Confidence::CONFIDENCE_MEDIUM;

	// everything else is low confidence bruteforce.
	return Confidence::CONFIDENCE_LOW;
}

void Resolver::ResolveAngles( Player* player, LagRecord* record ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// mark this record if it contains a shot.
	MatchShot( data, record );

	// next up mark this record with a resolver mode that will be used.
	SetMode( record );

	// if we are in nospread mode, force all players pitches to down.
	if( g_menu.main.config.mode.get( ) == 1 )
		record->m_eye_angles.x = 90.f;

	// we arrived here we can do the actual resolve.
	if( record->m_mode == Modes::RESOLVE_WALK ) 
		ResolveWalk( data, record );

	else if( record->m_mode == Modes::RESOLVE_STAND )
		ResolveStand( data, record );

	else if( record->m_mode == Modes::RESOLVE_AIR )
		ResolveAir( data, record );

	// compute and store the confidence level for this record.
	record->m_resolver_confidence = ComputeConfidence( data, record );

	// normalize the eye angles.
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
	data->m_last_desync  = 0.f;

	// copy the last record that this player was walking
	// we need it later on because it gives us crucial data.
	std::memcpy( &data->m_walk_record, record, sizeof( LagRecord ) );
}

void Resolver::ResolveStand( AimPlayer* data, LagRecord* record ) {
	// for no-spread call a separate resolver.
	if( g_menu.main.config.mode.get( ) == 1 ) {
		StandNS( data, record );
		return;
	}

	// get predicted away angle for the player.
	float away = GetAwayAngle( record );

	// pointer for easy access.
	LagRecord* move = &data->m_walk_record;

	// try animation-based desync detection first.
	float desync = DetectDesyncFromLayers( data, record );

	// cache desync for future use.
	if( std::abs( desync ) > 1.f )
		data->m_last_desync = desync;

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
		float time_delta = record->m_anim_time - move->m_anim_time;

		// it has not been time for this first update yet.
		if( time_delta < 0.22f ) {
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

			// body index exceeded: try animation-based resolution.
			if( std::abs( desync ) > 1.f ) {
				record->m_mode = Modes::RESOLVE_ANIM;
				record->m_eye_angles.y = record->m_body + desync;
				return;
			}

			// set to stand1 -> known last move.
			record->m_mode = Modes::RESOLVE_STAND1;

			C_AnimationLayer* curr = &record->m_layers[ 3 ];
			int act = data->m_player->GetSequenceActivity( curr->m_sequence );

			// bruteforce with move context.
			switch( data->m_stand_index % 5 ) {
			case 0:
				record->m_eye_angles.y = move->m_body;
				break;
			case 1:
				record->m_eye_angles.y = move->m_body + 180.f;
				break;
			case 2:
				record->m_eye_angles.y = away + 90.f;
				break;
			case 3:
				record->m_eye_angles.y = away - 90.f;
				break;
			case 4:
				record->m_eye_angles.y = away + 180.f;
				break;
			}

			return;
		}
	}

	// no known move context: try animation-based resolution.
	if( std::abs( desync ) > 1.f ) {
		record->m_mode = Modes::RESOLVE_ANIM;
		record->m_eye_angles.y = record->m_body + desync;
		return;
	}

	// stand2 -> no known last move, pure bruteforce.
	record->m_mode = Modes::RESOLVE_STAND2;

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
		record->m_eye_angles.y = away + 110.f;
		break;

	case 4:
		record->m_eye_angles.y = away - 110.f;
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

	// try animation-based desync detection for nospread too.
	float desync = DetectDesyncFromLayers( data, record );
	if( std::abs( desync ) > 1.f ) {
		record->m_mode = Modes::RESOLVE_ANIM;
		record->m_eye_angles.y = away + desync;
		record->m_body = record->m_eye_angles.y;
		return;
	}

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
	// for no-spread call a separate resolver.
	if( g_menu.main.config.mode.get( ) == 1 ) {
		AirNS( data, record );
		return;
	}

	// we have barely any speed.
	// either we jumped in place or we just left the ground.
	if( record->m_velocity.length_2d( ) < 60.f ) {
		record->m_mode = Modes::RESOLVE_STAND;

		// invoke our stand resolver.
		ResolveStand( data, record );

		// we are done.
		return;
	}

	float velyaw = math::rad_to_deg( std::atan2( record->m_velocity.y, record->m_velocity.x ) );
	float away = GetAwayAngle( record );

	// try animation-based desync detection.
	float desync = DetectDesyncFromLayers( data, record );
	if( std::abs( desync ) > 1.f ) {
		record->m_mode = Modes::RESOLVE_ANIM;
		record->m_eye_angles.y = velyaw + desync;
		data->m_last_desync = desync;
		return;
	}

	// bruteforce using velocity direction and away angle.
	switch( data->m_shots % 5 ) {
	case 0:
		record->m_eye_angles.y = velyaw + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 2:
		record->m_eye_angles.y = velyaw - 90.f;
		break;

	case 3:
		record->m_eye_angles.y = velyaw + 90.f;
		break;

	case 4:
		record->m_eye_angles.y = away;
		break;
	}
}

void Resolver::AirNS( AimPlayer* data, LagRecord* record ) {
	// get away angles.
	float away = GetAwayAngle( record );

	// try animation-based desync detection for nospread too.
	float desync = DetectDesyncFromLayers( data, record );
	if( std::abs( desync ) > 1.f ) {
		record->m_mode = Modes::RESOLVE_ANIM;
		record->m_eye_angles.y = away + desync;
		return;
	}

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

void Resolver::ResolveAnimations( AimPlayer* data, LagRecord* record ) {
	float away = GetAwayAngle( record );
	float desync = DetectDesyncFromLayers( data, record );

	if( std::abs( desync ) > 1.f ) {
		// we detected a desync direction from animation layers.
		record->m_mode = Modes::RESOLVE_ANIM;
		record->m_eye_angles.y = record->m_body + desync;
		data->m_last_desync = desync;
		return;
	}

	// fallback: could not determine from animations.
	record->m_eye_angles.y = away + 180.f;
}

void Resolver::ResolvePoses( Player* player, LagRecord* record ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// only do this bs when in air.
	if( record->m_mode == Modes::RESOLVE_AIR ) {
		// lean_yaw
		player->m_flPoseParameter( )[ 2 ]  = g_csgo.RandomInt( 0, 4 ) * 0.25f;   

		// body_yaw
		player->m_flPoseParameter( )[ 11 ] = g_csgo.RandomInt( 1, 3 ) * 0.25f;
	}

	// for animation-resolved records, set body_yaw based on detected desync.
	if( record->m_mode == Modes::RESOLVE_ANIM ) {
		float desync = data->m_last_desync;
		if( std::abs( desync ) > 1.f ) {
			// map desync angle to pose parameter range [0, 1].
			float pose = 0.5f + ( desync / 120.f ) * 0.5f;
			math::clamp( pose, 0.f, 1.f );
			player->m_flPoseParameter( )[ 11 ] = pose;
		}
	}
}