// sprite3d.c -- upstream's sprite3d.cpp and camera.hpp

#include <math.h>
#include "sbk.h"

extern float sbk_pi;

float vec3_len(Vec3 *v) { return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z); }

void vec3_normalize(Vec3 *v) {
	float d;
	d = vec3_len(v);
	v->x = v->x / d;
	v->y = v->y / d;
	v->z = v->z / d;
}

void cam_init(Camera *c) {
	c->pos.x = 0.0f; c->pos.y = 0.0f; c->pos.z = 0.0f;
	c->look_at = c->pos;
	c->forward = c->pos;
	c->right = c->pos;
	c->up = c->pos;
	c->focal = 320.0f;
	c->near_z = 1.0f;
	c->far_z = 500.0f;
	c->vx = 0; c->vy = 0; c->vw = 0; c->vh = 0;
}

void cam_update(Camera *c) {
	c->forward.x = c->look_at.x - c->pos.x;
	c->forward.y = c->look_at.y - c->pos.y;
	c->forward.z = c->look_at.z - c->pos.z;
	vec3_normalize(&c->forward);

	c->right.x = -c->forward.z;
	c->right.y = 0.0f;
	c->right.z = c->forward.x;
	vec3_normalize(&c->right);

	// right.cross(forward)
	c->up.x = c->right.y * c->forward.z - c->right.z * c->forward.y;
	c->up.y = c->right.z * c->forward.x - c->right.x * c->forward.z;
	c->up.z = c->right.x * c->forward.y - c->right.y * c->forward.x;
}

void sprite_init(Sprite3D *s) {
	s->world_pos.x = 0.0f; s->world_pos.y = 0.0f; s->world_pos.z = 0.0f;
	s->look_dir = s->world_pos;
	s->scale = 1.0f;
	s->alpha = 1.0f;
	s->origin_x = 0; s->origin_y = 0;
	s->sheet_x = 0; s->sheet_y = 0;
	s->size_w = 1; s->size_h = 1;
	s->rotation_frames = 0;
	s->sheet = 0;
	s->screen_x = 0; s->screen_y = 0;
	s->screen_scale = 1.0f;
	s->z = 0.0f;
}

// Camera::world_to_screen, into the sprite's own
void sprite_update(Sprite3D *s, Camera *c) {
	float dx, dy, dz, x, y, scale;
	dx = s->world_pos.x - c->pos.x;
	dy = s->world_pos.y - c->pos.y;
	dz = s->world_pos.z - c->pos.z;
	x = dx * c->right.x + dy * c->right.y + dz * c->right.z;
	y = -(dx * c->up.x + dy * c->up.y + dz * c->up.z);
	s->z = dx * c->forward.x + dy * c->forward.y + dz * c->forward.z;
	scale = c->focal / s->z;
	s->screen_scale = scale;
	// int32_t, then an int16_t each
	s->screen_x = (((int) (x * scale) + c->vx + c->vw / 2) << 16) >> 16;
	s->screen_y = (((int) (y * scale) + c->vy + c->vh / 2) << 16) >> 16;
}

void sprite_render(Sprite3D *s, Camera *c) {
	int frame, transform, total_frames, rot, a;
	float cx, cz, sx, sz, d, ang, fade_start, pi2;

	frame = 0;
	transform = 0;
	if (s->rotation_frames) {
		total_frames = s->rotation_frames + s->rotation_frames - 2;
		cx = s->world_pos.x - c->pos.x;
		cz = s->world_pos.z - c->pos.z;
		sx = s->look_dir.x;
		sz = s->look_dir.z;

		// normalize both, with y already nothing
		d = sqrtf(cx * cx + 0.0f + cz * cz);
		cx = cx / d;
		cz = cz / d;
		d = sqrtf(sx * sx + 0.0f + sz * sz);
		sx = sx / d;
		sz = sz / d;

		ang = atan2f(cx * sz - cz * sx, cx * sx + 0.0f + cz * sz);
		pi2 = sbk_pi * 2.0f;
		ang = pi2 - ang;

		rot = (int) roundf((ang / pi2) * (float) total_frames);
		frame = (rot + 15) % total_frames;
		if (frame >= s->rotation_frames) {
			frame = (s->rotation_frames - 2) - (frame - s->rotation_frames);
			transform = XFORM_HORIZONTAL;
		}
	}

	fade_start = c->far_z * 0.6f;
	if (s->z > fade_start) {
		a = (int) ((1.0f - (s->z - fade_start) / (c->far_z - fade_start)) * 255.0f * s->alpha);
		if (a < 0) a = 0;
		scr_alpha = a & 255;
	} else {
		scr_alpha = ((int) (s->alpha * 255.0f)) & 255;
	}

	scr_sprite_scaled(s->sheet, s->sheet_x + frame * s->size_w, s->sheet_y, s->size_w, s->size_h,
	                  s->screen_x, s->screen_y, s->origin_x, s->origin_y, s->screen_scale * s->scale, transform);

	scr_alpha = 255;
}
