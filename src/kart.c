// kart.c -- upstream's kart.cpp, which is the game's feel: the same arithmetic
// in the same order. Oddities kept on purpose are marked.

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sbk.h"

extern float sbk_pi4;

#define kart_radius 9.5f
#define kart_mass 30.0f              // and so, for two alike, what is left of their sum and difference
#define kart_accel 200.0f
#define kart_brake_rec_accel (-80.0f)
#define kart_drag 0.005f
#define kart_friction 0.85f
#define kart_turn_speed 0.5f
#define return_to_track_time 2.0f
#define kart_dt 0.01f

float kart_radius_f = kart_radius;

int item_sprite_x[3] = { 2, 3, 4 };    // items.hpp: all at y 15, a tile each

void kart_reset(Kart *k) {
	memset((char *) k, 0, sizeof(Kart));
	sprite_init(&k->sprite);
	k->sprite.origin_x = 16;
	k->sprite.origin_y = 26;
	k->sprite.size_w = 4;
	k->sprite.size_h = 4;
	k->sprite.rotation_frames = 16;
	k->current_lap = -1;              // -1 because we start behind the finish line
	k->current_item = ITEM_NONE;
}

int kart_finished(Kart *k) { return k->current_lap >= 3; }

int kart_lap_time(Kart *k, int lap) {
	if (lap < 0 || lap >= 3) return 0;
	if (lap == k->current_lap) return (int) (sbk_now - k->lap_start_time[lap]);
	if (lap < 2) return (int) (k->lap_start_time[lap + 1] - k->lap_start_time[lap]);
	return (int) (k->finish_time - k->lap_start_time[2]);
}

int kart_race_time(Kart *k) {
	int time, lap;
	time = 0;
	for (lap = 0; lap < 3; lap++) time += kart_lap_time(k, lap);
	return time;
}

void kart_collect_item(Kart *k) {
	if (k->current_item != ITEM_NONE) return;
	k->current_item = (int) (sbk_random() % 3);
}

void kart_disable(Kart *k) {
	if (k->disable_time) return;
	k->vel.x = 0.0f; k->vel.y = 0.0f; k->vel.z = 0.0f;
	k->disable_time = 50;
}

// Vec2::angle
static float vec2_angle(float x, float y, float ox, float oy) {
	return atan2f(x * oy - y * ox, x * ox + y * oy);
}

static void kart_use_item(Kart *k) {
	TrackObject *ob;
	int item, type;
	float d;
	Vec3 pos;
	Sprite3D *s;
	item = k->current_item;
	s = &k->sprite;
	if (item == ITEM_BOOST) {
		k->boost_time = 50;
		k->current_item = ITEM_NONE;
		return;
	}
	type = OBJ_DROPPED;
	if (item == ITEM_DROP) {
		// kart_radius + item_sprite.w * 4.0f
		d = 13.5f;
		pos.x = s->world_pos.x - s->look_dir.x * d;
		pos.y = s->world_pos.y - s->look_dir.y * d;
		pos.z = s->world_pos.z - s->look_dir.z * d;
	} else {
		d = 15.5f;
		pos.x = s->world_pos.x + s->look_dir.x * d;
		pos.y = s->world_pos.y + s->look_dir.y * d;
		pos.z = s->world_pos.z + s->look_dir.z * d;
		type = OBJ_PROJECTILE;
	}
	ob = track_add_object(k->race_state->track);
	sprite_init(&ob->sprite);
	ob->type = type;
	ob->respawn_timer = 0;
	ob->sprite.world_pos = pos;
	ob->sprite.origin_x = 4;
	ob->sprite.origin_y = 8;
	ob->sprite.sheet_x = item_sprite_x[item];
	ob->sprite.sheet_y = 15;
	ob->sprite.size_w = 1;
	ob->sprite.size_h = 1;
	ob->sprite.sheet = k->race_state->track->tiles;
	ob->vel.x = s->look_dir.x * 200.0f;
	ob->vel.y = s->look_dir.y * 200.0f;
	ob->vel.z = s->look_dir.z * 200.0f;
	k->current_item = ITEM_NONE;
}

static void kart_auto_drive(Kart *k) {
	Sprite3D *s;
	TrackInfo *info;
	TimeTrialSave *tt;
	GhostEntry *e, *n;
	int index, seg, next, last, off_track;
	float lx, lz, px, py, rvx, rvy, rpx, rpy, tcx, tcy, dist, recenter, ang, scale, d, nx, ny, turn;

	s = &k->sprite;
	// replay ghost
	if (KART_IS_GHOST(k) && k->ghost_timer++ % 10 == 0) {
		index = k->ghost_timer / 10;
		if (index < k->ghost_len) {
			tt = k->time_trial_data;
			e = tt->ghost_data + index + k->ghost_offset;
			last = k->ghost_len - 1 < index + 1 ? k->ghost_len - 1 : index + 1;
			n = tt->ghost_data + last + k->ghost_offset;

			s->world_pos.x = (float) e->pos_x / 8.0f;
			s->world_pos.z = (float) e->pos_z / 8.0f;

			k->vel.x = (float) (n->pos_x - e->pos_x) / 8.0f * 10.0f;
			k->vel.z = (float) (n->pos_z - e->pos_z) / 8.0f * 10.0f;

			s->look_dir.x = cosf((float) e->look_ang / 10430.0f);
			s->look_dir.z = sinf((float) e->look_ang / 10430.0f);

			k->acc.x = 0.0f; k->acc.y = 0.0f; k->acc.z = 0.0f;
			return;
		}
	}

	// maybe use item if we have one
	if (k->current_item != ITEM_NONE && (sbk_random() & 0x3FF) < 1)
		kart_use_item(k);

	// CPU control
	lx = s->look_dir.x;
	lz = s->look_dir.z;
	k->acc.x = lx * kart_accel;
	k->acc.y = 0.0f;
	k->acc.z = lz * kart_accel;

	// try to face the right way
	px = s->world_pos.x;
	py = s->world_pos.z;
	seg = k->current_route_segment;
	info = k->race_state->track->info;

	rvx = info->segs[seg].rx;
	rvy = info->segs[seg].ry;
	rpx = info->segs[seg].ax + rvx * k->current_route_frac;
	rpy = info->segs[seg].ay + rvy * k->current_route_frac;

	// try to stay on track
	tcx = rpx - px;
	tcy = rpy - py;
	dist = sqrtf(tcx * tcx + tcy * tcy);

	recenter = 0.0f;

	// high friction - almost definitely off the track
	off_track = track_friction(k->race_state->track, px, py) > 1.0f;

	// ~third of the track width for rainbow
	if (dist > 40.0f || off_track) {
		tcx = tcx / dist;
		tcy = tcy / dist;

		ang = vec2_angle(tcx, tcy, lx, lz);
		scale = off_track ? 1.0f : 0.5f;

		d = ang < 0.0f ? -ang : ang;
		if (d < sbk_pi4)
			recenter = 0.0f;
		else
			recenter = ang < 0.0f ? -kart_turn_speed * scale : kart_turn_speed * scale;
	}

	// turn into corners
	next = seg + 1;
	if (next + 1 == info->route_len) next = 0;   // wrap (the first and last points are the same)

	ang = vec2_angle(info->segs[next].nx, info->segs[next].ny, lx, lz);

	// std::min(kart_turn_speed, std::max(-kart_turn_speed, ang * 0.8f))
	turn = ang * 0.8f;
	turn = -kart_turn_speed < turn ? turn : -kart_turn_speed;
	turn = turn < kart_turn_speed ? turn : kart_turn_speed;
	k->turn_speed = turn;

	// whatever is telling us to turn the hardest, unless we're off the track
	d = recenter < 0.0f ? -recenter : recenter;
	ang = turn < 0.0f ? -turn : turn;
	if (off_track || d > ang)
		k->turn_speed = recenter;
}

void kart_update(Kart *k) {
	Sprite3D *s;
	RaceState *rs;
	Track *trk;
	TrackInfo *info;
	Kart *other;
	RectF *rect;
	TrackObject *ob;
	GhostEntry *ge;
	TimeTrialSave *tt;
	int on_track, seg, ghost_finished, was_above, i, n, forwards, crossed;
	float px, py, friction, len, half, dot;
	float bx, by, bz, dragx, dragy, dragz, fx, fy, fz;
	float finx, finy, f0x, f0y, before, after;
	float vx, vy, dist, pen, ax, ay, az, bvx, bvy, bvz, kx, kz, ox, oz, sr, lim;
	float rx, ry, rl, rpx, rpy, retx, retz, retlx, retlz, frac;
	Vec3 rl3;

	s = &k->sprite;
	rs = k->race_state;
	trk = rs->track;
	info = trk->info;

	if (k->disable_time) k->disable_time--;

	// put back on track
	if (k->return_to_track_timer > 0.0f) {
		half = 1.0f;       // return_to_track_time / 2
		if (k->return_to_track_timer > half) {
			s->world_pos.y = s->world_pos.y + k->return_pos_v.y;
		} else {
			k->return_pos_v.y = 0.0f;
			s->world_pos.x = s->world_pos.x + k->return_pos_v.x;
			s->world_pos.y = s->world_pos.y + k->return_pos_v.y;
			s->world_pos.z = s->world_pos.z + k->return_pos_v.z;
			s->look_dir.x = s->look_dir.x + k->return_look_v.x;
			s->look_dir.y = s->look_dir.y + k->return_look_v.y;
			s->look_dir.z = s->look_dir.z + k->return_look_v.z;
			vec3_normalize(&s->look_dir);
		}
		k->return_to_track_timer = k->return_to_track_timer - kart_dt;
		return;
	}

	if (k->is_player) {
		// player input
		if (btn_state & BTN_A) {
			k->acc.x = s->look_dir.x * kart_accel;
			k->acc.y = 0.0f;
			k->acc.z = s->look_dir.z * kart_accel;
		} else if (btn_state & BTN_B) {
			k->acc.x = s->look_dir.x * kart_brake_rec_accel;
			k->acc.y = 0.0f;
			k->acc.z = s->look_dir.z * kart_brake_rec_accel;
		} else {
			k->acc.x = 0.0f; k->acc.y = 0.0f; k->acc.z = 0.0f;
		}

		if (btn_state & BTN_LEFT) k->turn_speed = kart_turn_speed;
		else if (btn_state & BTN_RIGHT) k->turn_speed = -kart_turn_speed;
		else k->turn_speed = -0.0f;    // joystick.x * -kart_turn_speed: there is no joystick here

		// use item
		if (k->current_item != ITEM_NONE && (btn_released & BTN_X))
			kart_use_item(k);
	} else
		kart_auto_drive(k);

	px = s->world_pos.x;
	py = s->world_pos.z;
	friction = track_friction(trk, px, py);
	on_track = friction != 0.0f;

	k->current_route_segment = track_closest_segment(trk, px, py, &k->current_route_frac);
	seg = k->current_route_segment;

	// trigger boosts if friction is negative
	if (friction < 0.0f) {
		friction = friction * -1.0f;
		k->boost_time = 25;
	}

	// under track
	if (s->world_pos.y < -30.0f) {
		// start putting back on the track
		k->vel.x = 0.0f; k->vel.y = 0.0f; k->vel.z = 0.0f;
		k->acc = k->vel;
		k->return_to_track_timer = return_to_track_time;

		// find a position somewhere on the nearest segment
		rx = info->segs[seg].rx;
		ry = info->segs[seg].ry;
		frac = k->current_route_frac;
		frac = frac < 1.0f ? frac : 1.0f;
		frac = 0.0f < frac ? frac : 0.0f;
		k->current_route_frac = frac;
		rpx = info->segs[seg].ax + rx * frac;
		rpy = info->segs[seg].ay + ry * frac;

		retx = rpx;
		retz = rpy;

		// return to pointing the right way
		rl3.x = rx;
		rl3.y = 0.0f;
		rl3.z = ry;
		vec3_normalize(&rl3);
		retlx = rl3.x;
		retlz = rl3.z;

		// pre-calc how much we need to add at each step
		half = 1.0f;
		k->return_pos_v.x = (retx - s->world_pos.x) / half * kart_dt;
		k->return_pos_v.y = (16.0f - s->world_pos.y) / half * kart_dt;
		k->return_pos_v.z = (retz - s->world_pos.z) / half * kart_dt;
		k->return_look_v.x = (retlx - s->look_dir.x) / half * kart_dt;
		k->return_look_v.y = (rl3.y - s->look_dir.y) / half * kart_dt;
		k->return_look_v.z = (retlz - s->look_dir.z) / half * kart_dt;
		return;
	} else if (s->world_pos.y != 0.0f)
		on_track = 0;

	len = vec3_len(&k->vel);
	if (!on_track) {                 // uh oh, we're flying
		k->acc.x = 0.0f; k->acc.y = -300.0f; k->acc.z = 0.0f;
	} else
		rot_y(&s->look_dir, k->turn_speed * len * kart_dt);

	// boost
	bx = 0.0f; by = 0.0f; bz = 0.0f;
	if (k->boost_time) {
		bx = s->look_dir.x * kart_accel * 4.0f;
		by = 0.0f;
		bz = s->look_dir.z * kart_accel * 4.0f;
		k->boost_time--;
	}

	// update velocity
	dragx = k->vel.x * -kart_drag * len;
	dragy = k->vel.y * -kart_drag * len;
	dragz = k->vel.z * -kart_drag * len;
	fx = k->vel.x * -kart_friction * friction;
	fy = k->vel.y * -kart_friction * friction;
	fz = k->vel.z * -kart_friction * friction;

	// ignore accel if the race hasn't started or replaying a ghost (and not falling)
	ghost_finished = !KART_IS_GHOST(k) || k->ghost_timer / 10 >= k->time_trial_data->ghost_data_used;
	if (!rs->countdown && (k->acc.y != 0.0f || (ghost_finished && k->disable_time == 0))) {
		k->vel.x = k->vel.x + (k->acc.x + dragx + fx + bx) * kart_dt;
		k->vel.y = k->vel.y + (k->acc.y + dragy + fy + by) * kart_dt;
		k->vel.z = k->vel.z + (k->acc.z + dragz + fz + bz) * kart_dt;
	}

	was_above = s->world_pos.y >= 0.0f;

	// apply velocity
	s->world_pos.x = s->world_pos.x + k->vel.x * kart_dt;
	s->world_pos.y = s->world_pos.y + k->vel.y * kart_dt;
	s->world_pos.z = s->world_pos.z + k->vel.z * kart_dt;

	// check if we crossed the finish line
	if (seg == 0 || seg == info->route_len - 2) {
		f0x = (float) info->fin[0];
		f0y = (float) info->fin[1];
		finx = (float) (info->fin[2] - info->fin[0]);
		finy = (float) (info->fin[3] - info->fin[1]);

		before = finx * (py - f0y) - finy * (px - f0x);
		// vel.y: upstream's own, where z is meant. Kept: it is what the laps count by.
		after = finx * ((py + k->vel.y * kart_dt) - f0y) - finy * ((px + k->vel.x * kart_dt) - f0x);
		crossed = (before < 0.0f) != (after < 0.0f);

		if (crossed && !kart_finished(k)) {
			forwards = after < 0.0f;
			k->current_lap += forwards ? 1 : -1;      // uh, negative laps just so you can't cheat
			if (kart_finished(k)) {
				k->is_player = 0;                 // take over after race is done
				k->finish_time = sbk_now;
			} else if (forwards && k->current_lap >= 0 && !k->lap_start_time[k->current_lap])
				k->lap_start_time[k->current_lap] = sbk_now;
		}
	}

	// fell through the track
	if (s->world_pos.y < 0.0f && friction > 0.0f && was_above)
		s->world_pos.y = 0.0f;

	if (!KART_IS_GHOST(k)) {
		// collisions - check every kart before this one
		for (other = rs->karts; other != k; other++) {
			if (KART_IS_GHOST(other)) continue;
			dist = s->world_pos.y - other->sprite.world_pos.y;
			if (dist > 1.0f || dist < -1.0f) continue;

			vx = s->world_pos.x - other->sprite.world_pos.x;
			vy = s->world_pos.z - other->sprite.world_pos.z;
			dist = vx * vx + vy * vy;
			if (dist >= 361.0f) continue;         // (kart_radius + kart_radius) squared

			dist = sqrtf(dist);
			vx = vx / dist;
			vy = vy / dist;
			pen = 19.0f - dist;

			// move apart
			s->world_pos.x = s->world_pos.x + vx * pen * 0.5f;
			s->world_pos.y = s->world_pos.y + 0.0f * pen * 0.5f;
			s->world_pos.z = s->world_pos.z + vy * pen * 0.5f;
			other->sprite.world_pos.x = other->sprite.world_pos.x - vx * pen * 0.5f;
			other->sprite.world_pos.y = other->sprite.world_pos.y - 0.0f * pen * 0.5f;
			other->sprite.world_pos.z = other->sprite.world_pos.z - vy * pen * 0.5f;

			// boing
			ax = (k->vel.x * 0.0f + (other->vel.x * kart_mass * 2.0f)) / 60.0f;
			ay = (k->vel.y * 0.0f + (other->vel.y * kart_mass * 2.0f)) / 60.0f;
			az = (k->vel.z * 0.0f + (other->vel.z * kart_mass * 2.0f)) / 60.0f;
			bvx = (other->vel.x * 0.0f + (k->vel.x * kart_mass * 2.0f)) / 60.0f;
			bvy = (other->vel.y * 0.0f + (k->vel.y * kart_mass * 2.0f)) / 60.0f;
			bvz = (other->vel.z * 0.0f + (k->vel.z * kart_mass * 2.0f)) / 60.0f;
			other->vel.x = bvx; other->vel.y = bvy; other->vel.z = bvz;
			k->vel.x = ax; k->vel.y = ay; k->vel.z = az;
		}

		// collide with track obstacles
		n = info->num_rects;
		rect = info->rects;
		for (i = 0; i < n; i++, rect++) {
			if (rect->w <= 0 || rect->h <= 0) continue;
			kx = s->world_pos.x;
			kz = s->world_pos.z;
			ox = kx;
			oz = kz;
			if (kx < rect->fx) ox = rect->fx;
			else if (kx >= rect->fx2) ox = rect->fx2;
			if (kz < rect->fy) oz = rect->fy;
			else if (kz >= rect->fy2) oz = rect->fy2;

			vx = kx - ox;
			vy = kz - oz;
			dist = vx * vx + vy * vy;
			if (dist > 90.25f) continue;             // kart_radius squared

			dist = sqrtf(dist);
			vx = vx / dist;
			vy = vy / dist;
			pen = kart_radius - dist;

			rl = dist / kart_radius;
			k->vel.x = k->vel.x * rl;
			k->vel.y = k->vel.y * rl;
			k->vel.z = k->vel.z * rl;
			s->world_pos.x = s->world_pos.x + vx * pen;
			s->world_pos.y = s->world_pos.y + 0.0f * pen;
			s->world_pos.z = s->world_pos.z + vy * pen;
		}

		// track sprites: TrackObject::collide
		n = trk->num_objects;
		ob = trk->objects;
		for (i = 0; i < n; i++, ob++) {
			if (ob->type == OBJ_REMOVED || ob->respawn_timer != 0) continue;
			sr = (float) ob->sprite.size_w * 4.0f * ob->sprite.scale;
			lim = (kart_radius + sr) * (kart_radius + sr);
			vx = s->world_pos.x - ob->sprite.world_pos.x;
			// a square past the limit stays past it with another square added
			if (vx * vx >= lim) continue;
			vy = s->world_pos.z - ob->sprite.world_pos.z;
			dist = vx * vx + vy * vy;
			if (dist >= lim) continue;

			if (ob->type == OBJ_ITEM) {
				kart_collect_item(k);
				ob->respawn_timer = 10000;
				continue;
			}

			dist = sqrtf(dist);
			vx = vx / dist;
			vy = vy / dist;
			pen = kart_radius + sr - dist;

			s->world_pos.x = s->world_pos.x + vx * pen;
			s->world_pos.y = s->world_pos.y + 0.0f * pen;
			s->world_pos.z = s->world_pos.z + vy * pen;

			if (ob->type == OBJ_DROPPED || ob->type == OBJ_PROJECTILE) {
				// assume hitting this is bad (okay unless we allow dropped boosts)
				kart_disable(k);
				ob->type = OBJ_REMOVED;
			}
		}
	}

	// record
	tt = k->time_trial_data;
	if (tt && k->is_player) {
		i = k->ghost_timer++ % 10 == 0;
		if (i && !kart_finished(k)) {
			ge = tt->ghost_data + tt->ghost_data_used;
			tt->ghost_data_used = tt->ghost_data_used + 1;
			ge->pos_x = (int) floorf(s->world_pos.x * 8.0f);
			ge->pos_z = (int) floorf(s->world_pos.z * 8.0f);
			ge->look_ang = (int) floorf(atan2f(s->look_dir.z, s->look_dir.x) * 10430.0f);
		}
	}
}
