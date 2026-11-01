/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 OBS Beauty Camera contributors */

#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>
#include <shellapi.h>

#include <obs-module.h>
#include <obs.h>
#include <obs-source.h>
#include <util/platform.h>
#include <graphics/graphics.h>
#include <graphics/effect.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>

#define MODULE_NAME "obs-beauty-camera"
#define SOURCE_ID "beauty_camera_source"
#define FILTER_ID "beauty_camera_filter"
#define BEAUTY_VERSION "2.1.0"

#define PRESET_COUNT 9
#define BEAUTY_MAX_FACES 3
#define ANALYSIS_INTERVAL_NS 250000000ULL

struct beauty_params {
	bool enabled;
	int preset;
	int compare_mode;
	float master;
	float smooth;
	float whitening;
	float rosy;
	float clarity;
	float skin_protect;
	float smooth_radius;
	float mask_softness;
	float brightness;
	float contrast;
	float saturation;
	float temperature;
	bool face_enabled;
	bool face_auto;
	float face_x;
	float face_y;
	float face_radius;
	float face_slim;
	float eye_enlarge;
	float chin_lift;
	float spot;
	float eye_bright;
	float teeth;
	float dark_circle;
	float contour;
	float side_shadow;
	int makeup_style;
	float lip;
	float blush;
	float eyeshadow;
	int look_mode;
	float look_amount;
	float body;
	float bg_blur;
	float glow;
	float grain;
	float vignette;
};

struct beauty_values {
	float smooth;
	float whiten;
	float rosy;
	float clarity;
	float saturation;
	float temperature;
	float contrast;
	float brightness;
	float slim;
	float eye;
	float chin;
	float spot;
	float eye_bright;
	float teeth;
	float dark_circle;
	float contour;
	float side_shadow;
	float lip;
	float blush;
	float eyeshadow;
	float body;
	float bg_blur;
	float glow;
	float grain;
	float vignette;
	int makeup_style;
	int look_mode;
	float look_amount;
};

struct beauty_source {
	obs_source_t *source;
	obs_source_t *camera;
	obs_source_t *filter;
};

struct beauty_uniforms {
	gs_eparam_t *texel;
	gs_eparam_t *aspect;
	gs_eparam_t *pixel_scale;
	gs_eparam_t *compare_mode;
	gs_eparam_t *smooth_amount;
	gs_eparam_t *whiten_amount;
	gs_eparam_t *rosy_amount;
	gs_eparam_t *clarity_amount;
	gs_eparam_t *saturation;
	gs_eparam_t *temperature;
	gs_eparam_t *contrast;
	gs_eparam_t *brightness;
	gs_eparam_t *slim_amount;
	gs_eparam_t *eye_amount;
	gs_eparam_t *chin_amount;
	gs_eparam_t *skin_protect;
	gs_eparam_t *mask_softness;
	gs_eparam_t *smooth_radius;
	gs_eparam_t *face_enabled;
	gs_eparam_t *face_center;
	gs_eparam_t *face_radius;
	gs_eparam_t *time_sec;
	gs_eparam_t *spot_amount;
	gs_eparam_t *eye_bright_amount;
	gs_eparam_t *teeth_amount;
	gs_eparam_t *dark_circle_amount;
	gs_eparam_t *contour_amount;
	gs_eparam_t *side_shadow_amount;
	gs_eparam_t *makeup_style;
	gs_eparam_t *lip_amount;
	gs_eparam_t *blush_amount;
	gs_eparam_t *eyeshadow_amount;
	gs_eparam_t *look_mode;
	gs_eparam_t *look_amount;
	gs_eparam_t *body_amount;
	gs_eparam_t *bg_blur_amount;
	gs_eparam_t *glow_amount;
	gs_eparam_t *grain_amount;
	gs_eparam_t *vignette_amount;
	gs_eparam_t *face_count;
	gs_eparam_t *face0_on;
	gs_eparam_t *face0_center;
	gs_eparam_t *face0_radius;
	gs_eparam_t *face1_on;
	gs_eparam_t *face1_center;
	gs_eparam_t *face1_radius;
	gs_eparam_t *face2_on;
	gs_eparam_t *face2_center;
	gs_eparam_t *face2_radius;
};

struct tracked_face {
	float cx;
	float cy;
	float r;
	float score;
	int misses;
};

struct beauty_tracker {
	gs_texrender_t *texrender;
	gs_stagesurf_t *stage;
	gs_effect_t *detect_effect;
	uint8_t *pixels;
	uint8_t *mask;
	uint8_t *visited;
	int32_t *stack;
	int32_t *row_counts;
	uint32_t tex_w;
	uint32_t tex_h;
	uint64_t last_analysis_ns;
	int face_count;
	struct tracked_face faces[BEAUTY_MAX_FACES];
};

struct beauty_filter {
	obs_source_t *source;
	gs_effect_t *effect;
	struct beauty_uniforms u;
	struct beauty_params p;
	struct beauty_tracker tracker;
	float time_sec;
	bool rendered_once;
};

static void tracker_free(struct beauty_tracker *t);

#define BEAUTY_DOUBLE_SETTINGS(X)  \
	X("master", 100.0)         \
	X("smooth", 32.0)          \
	X("whitening", 24.0)       \
	X("rosy", 9.0)             \
	X("clarity", 20.0)         \
	X("skin_protect", 60.0)    \
	X("smooth_radius", 3.5)    \
	X("mask_softness", 55.0)   \
	X("brightness", 2.0)       \
	X("contrast", 5.0)         \
	X("saturation", 3.0)       \
	X("temperature", -1.0)     \
	X("face_x", 50.0)          \
	X("face_y", 42.0)          \
	X("face_radius", 45.0)     \
	X("face_slim", 4.0)        \
	X("eye_enlarge", 4.0)      \
	X("chin_lift", 0.0)        \
	X("spot", 30.0)            \
	X("eye_bright", 16.0)      \
	X("teeth", 10.0)           \
	X("dark_circle", 15.0)     \
	X("contour", 5.0)          \
	X("side_shadow", 0.0)      \
	X("lip", 8.0)              \
	X("blush", 5.0)            \
	X("eyeshadow", 3.0)        \
	X("look_amount", 45.0)     \
	X("body", 0.0)             \
	X("bg_blur", 0.0)          \
	X("glow", 6.0)             \
	X("grain", 0.0)            \
	X("vignette", 0.0)

static void apply_camera_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "video_device_id", "");
	obs_data_set_default_string(settings, "resolution", "");
	obs_data_set_default_int(settings, "res_type", 0);
	obs_data_set_default_int(settings, "frame_interval", -1);
	obs_data_set_default_int(settings, "video_format", 0);
	obs_data_set_default_int(settings, "buffering", 0);
	obs_data_set_default_string(settings, "color_space", "default");
	obs_data_set_default_string(settings, "color_range", "default");
	obs_data_set_default_bool(settings, "flip_vertically", false);
}

static void apply_beauty_defaults(obs_data_t *settings)
{
#define SET_DEFAULT_DOUBLE(key, value) \
	obs_data_set_default_double(settings, key, value);
	BEAUTY_DOUBLE_SETTINGS(SET_DEFAULT_DOUBLE)
#undef SET_DEFAULT_DOUBLE

	obs_data_set_default_int(settings, "preset", 9);
	obs_data_set_default_int(settings, "compare_mode", 0);
	obs_data_set_default_int(settings, "makeup_style", 1);
	obs_data_set_default_int(settings, "look_mode", 1);
	obs_data_set_default_bool(settings, "enabled", true);
	obs_data_set_default_bool(settings, "face_enabled", true);
	obs_data_set_default_bool(settings, "face_auto", true);
}

static void beauty_source_defaults(obs_data_t *settings)
{
	apply_camera_defaults(settings);
	apply_beauty_defaults(settings);
}

static void beauty_filter_defaults(obs_data_t *settings)
{
	apply_beauty_defaults(settings);
}

static void beauty_params_load(struct beauty_params *p, obs_data_t *settings)
{
	p->enabled = obs_data_get_bool(settings, "enabled");
	p->preset = (int)obs_data_get_int(settings, "preset");
	p->compare_mode = (int)obs_data_get_int(settings, "compare_mode");
	p->master = (float)obs_data_get_double(settings, "master");
	p->smooth = (float)obs_data_get_double(settings, "smooth");
	p->whitening = (float)obs_data_get_double(settings, "whitening");
	p->rosy = (float)obs_data_get_double(settings, "rosy");
	p->clarity = (float)obs_data_get_double(settings, "clarity");
	p->skin_protect = (float)obs_data_get_double(settings, "skin_protect");
	p->smooth_radius = (float)obs_data_get_double(settings, "smooth_radius");
	p->mask_softness = (float)obs_data_get_double(settings, "mask_softness");
	p->brightness = (float)obs_data_get_double(settings, "brightness");
	p->contrast = (float)obs_data_get_double(settings, "contrast");
	p->saturation = (float)obs_data_get_double(settings, "saturation");
	p->temperature = (float)obs_data_get_double(settings, "temperature");
	p->face_enabled = obs_data_get_bool(settings, "face_enabled");
	p->face_auto = obs_data_get_bool(settings, "face_auto");
	p->face_x = (float)obs_data_get_double(settings, "face_x");
	p->face_y = (float)obs_data_get_double(settings, "face_y");
	p->face_radius = (float)obs_data_get_double(settings, "face_radius");
	p->face_slim = (float)obs_data_get_double(settings, "face_slim");
	p->eye_enlarge = (float)obs_data_get_double(settings, "eye_enlarge");
	p->chin_lift = (float)obs_data_get_double(settings, "chin_lift");
	p->spot = (float)obs_data_get_double(settings, "spot");
	p->eye_bright = (float)obs_data_get_double(settings, "eye_bright");
	p->teeth = (float)obs_data_get_double(settings, "teeth");
	p->dark_circle = (float)obs_data_get_double(settings, "dark_circle");
	p->contour = (float)obs_data_get_double(settings, "contour");
	p->side_shadow = (float)obs_data_get_double(settings, "side_shadow");
	p->makeup_style = (int)obs_data_get_int(settings, "makeup_style");
	p->lip = (float)obs_data_get_double(settings, "lip");
	p->blush = (float)obs_data_get_double(settings, "blush");
	p->eyeshadow = (float)obs_data_get_double(settings, "eyeshadow");
	p->look_mode = (int)obs_data_get_int(settings, "look_mode");
	p->look_amount = (float)obs_data_get_double(settings, "look_amount");
	p->body = (float)obs_data_get_double(settings, "body");
	p->bg_blur = (float)obs_data_get_double(settings, "bg_blur");
	p->glow = (float)obs_data_get_double(settings, "glow");
	p->grain = (float)obs_data_get_double(settings, "grain");
	p->vignette = (float)obs_data_get_double(settings, "vignette");
}

static void beauty_values_from_params(const struct beauty_params *p,
				      struct beauty_values *v)
{
	static const struct beauty_values presets[PRESET_COUNT] = {
		/* 自然 */
		{0.30f, 0.15f, 0.12f, 0.08f, 0.05f, 0.02f, 0.03f, 0.00f,
		 0.08f, 0.06f, 0.04f, 0.25f, 0.15f, 0.10f, 0.12f, 0.10f,
		 0.05f, 0.18f, 0.10f, 0.00f, 0.00f, 0.00f, 0.08f, 0.00f,
		 0.00f, 1, 0, 0.00f},
		/* 清透 */
		{0.38f, 0.38f, 0.08f, 0.16f, 0.12f, -0.03f, 0.08f, 0.02f,
		 0.05f, 0.05f, 0.00f, 0.35f, 0.25f, 0.25f, 0.22f, 0.06f,
		 0.00f, 0.12f, 0.06f, 0.05f, 0.00f, 0.00f, 0.15f, 0.00f,
		 0.00f, 1, 1, 1.00f},
		/* 奶油肌 */
		{0.62f, 0.30f, 0.25f, 0.03f, -0.06f, 0.07f, -0.04f, 0.03f,
		 0.08f, 0.10f, 0.04f, 0.40f, 0.15f, 0.10f, 0.18f, 0.05f,
		 0.00f, 0.22f, 0.22f, 0.05f, 0.00f, 0.00f, 0.25f, 0.00f,
		 0.00f, 2, 2, 0.80f},
		/* 立体 */
		{0.22f, 0.08f, 0.10f, 0.28f, 0.10f, 0.02f, 0.16f, 0.00f,
		 0.20f, 0.05f, 0.08f, 0.20f, 0.10f, 0.10f, 0.10f, 0.45f,
		 0.30f, 0.10f, 0.05f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
		 0.00f, 2, 0, 0.00f},
		/* 直播 */
		{0.45f, 0.26f, 0.18f, 0.12f, 0.16f, 0.03f, 0.05f, 0.02f,
		 0.14f, 0.12f, 0.07f, 0.45f, 0.30f, 0.30f, 0.28f, 0.18f,
		 0.10f, 0.25f, 0.15f, 0.10f, 0.00f, 0.12f, 0.18f, 0.00f,
		 0.00f, 2, 1, 0.50f},
		/* 甜美 */
		{0.40f, 0.22f, 0.35f, 0.05f, 0.10f, 0.05f, -0.02f, 0.03f,
		 0.10f, 0.15f, 0.05f, 0.30f, 0.20f, 0.20f, 0.20f, 0.08f,
		 0.00f, 0.45f, 0.45f, 0.15f, 0.00f, 0.00f, 0.20f, 0.00f,
		 0.00f, 3, 2, 0.60f},
		/* 冷白 */
		{0.35f, 0.45f, 0.05f, 0.12f, -0.12f, -0.16f, 0.05f, 0.02f,
		 0.10f, 0.08f, 0.00f, 0.35f, 0.30f, 0.30f, 0.25f, 0.15f,
		 0.05f, 0.18f, 0.05f, 0.00f, 0.00f, 0.00f, 0.12f, 0.00f,
		 0.00f, 1, 4, 1.00f},
		/* 港风 */
		{0.30f, 0.12f, 0.22f, 0.22f, 0.22f, -0.02f, 0.25f, -0.02f,
		 0.12f, 0.06f, 0.03f, 0.20f, 0.08f, 0.10f, 0.15f, 0.40f,
		 0.35f, 0.55f, 0.12f, 0.25f, 0.00f, 0.00f, 0.00f, 0.25f,
		 0.20f, 3, 3, 0.90f},
		/* 精致：默认，轻磨皮 + 轻美白 + 清晰度，保留皮肤纹理 */
		{0.32f, 0.24f, 0.09f, 0.20f, 0.03f, -0.01f, 0.05f, 0.02f,
		 0.04f, 0.04f, 0.00f, 0.30f, 0.16f, 0.10f, 0.15f, 0.05f,
		 0.00f, 0.08f, 0.05f, 0.03f, 0.00f, 0.00f, 0.06f, 0.00f,
		 0.00f, 1, 1, 0.45f},
	};

	memset(v, 0, sizeof(*v));

	if (p->preset >= 1 && p->preset <= PRESET_COUNT) {
		*v = presets[p->preset - 1];
	} else {
		v->smooth = p->smooth / 100.0f;
		v->whiten = p->whitening / 100.0f;
		v->rosy = p->rosy / 100.0f;
		v->clarity = p->clarity / 100.0f;
		v->saturation = p->saturation / 100.0f;
		v->temperature = p->temperature / 100.0f;
		v->contrast = p->contrast / 100.0f;
		v->brightness = p->brightness / 100.0f;
		v->slim = p->face_slim / 100.0f;
		v->eye = p->eye_enlarge / 100.0f;
		v->chin = p->chin_lift / 100.0f;
		v->spot = p->spot / 100.0f;
		v->eye_bright = p->eye_bright / 100.0f;
		v->teeth = p->teeth / 100.0f;
		v->dark_circle = p->dark_circle / 100.0f;
		v->contour = p->contour / 100.0f;
		v->side_shadow = p->side_shadow / 100.0f;
		v->lip = p->lip / 100.0f;
		v->blush = p->blush / 100.0f;
		v->eyeshadow = p->eyeshadow / 100.0f;
		v->body = p->body / 100.0f;
		v->bg_blur = p->bg_blur / 100.0f;
		v->glow = p->glow / 100.0f;
		v->grain = p->grain / 100.0f;
		v->vignette = p->vignette / 100.0f;
		v->look_amount = p->look_amount / 100.0f;
		v->makeup_style = p->makeup_style;
		v->look_mode = p->look_mode;
	}

	float m = fminf(fmaxf(p->master / 100.0f, 0.0f), 1.0f);
	v->smooth *= m;
	v->whiten *= m;
	v->rosy *= m;
	v->clarity *= m;
	v->saturation *= m;
	v->temperature *= m;
	v->contrast *= m;
	v->brightness *= m;
	v->slim *= m;
	v->eye *= m;
	v->chin *= m;
	v->spot *= m;
	v->eye_bright *= m;
	v->teeth *= m;
	v->dark_circle *= m;
	v->contour *= m;
	v->side_shadow *= m;
	v->lip *= m;
	v->blush *= m;
	v->eyeshadow *= m;
	v->body *= m;
	v->bg_blur *= m;
	v->glow *= m;
	v->grain *= m;
	v->vignette *= m;
	v->look_amount *= m;
}

/* ------------------------------------------------------------------------- */
/* camera child source                                                        */

static obs_data_t *build_dshow_settings(obs_data_t *src, bool probe)
{
	obs_data_t *ds = obs_data_create();

	const char *device = src ? obs_data_get_string(src, "video_device_id") : "";
	if (device && *device)
		obs_data_set_string(ds, "video_device_id", device);

	const char *resolution = src ? obs_data_get_string(src, "resolution") : "";
	if (resolution && *resolution)
		obs_data_set_string(ds, "resolution", resolution);

	obs_data_set_int(ds, "res_type",
			 src ? obs_data_get_int(src, "res_type") : 0);
	obs_data_set_int(ds, "frame_interval",
			 src ? obs_data_get_int(src, "frame_interval") : -1);
	obs_data_set_int(ds, "video_format",
			 src ? obs_data_get_int(src, "video_format") : 0);
	obs_data_set_int(ds, "buffering",
			 src ? obs_data_get_int(src, "buffering") : 0);
	obs_data_set_string(
		ds, "color_space",
		src ? obs_data_get_string(src, "color_space") : "default");
	obs_data_set_string(
		ds, "color_range",
		src ? obs_data_get_string(src, "color_range") : "default");
	obs_data_set_bool(ds, "flip_vertically",
			  src ? obs_data_get_bool(src, "flip_vertically")
			      : false);
	obs_data_set_bool(ds, "autorotation", true);
	obs_data_set_bool(ds, "hw_decode", false);
	obs_data_set_bool(ds, "deactivate_when_not_showing", false);
	obs_data_set_bool(ds, "active", !probe);

	return ds;
}

static obs_properties_t *probe_dshow_properties(obs_data_t *settings)
{
	obs_data_t *ds = build_dshow_settings(settings, true);
	obs_source_t *tmp =
		obs_source_create_private("dshow_input", "beauty-camera-probe", ds);
	obs_data_release(ds);

	if (!tmp) {
		blog(LOG_WARNING,
		     "[beauty-camera] OBS video capture source (dshow_input) "
		     "is not available");
		return NULL;
	}

	obs_properties_t *props = obs_source_properties(tmp);
	obs_source_release(tmp);
	return props;
}

/* ------------------------------------------------------------------------- */
/* properties helpers                                                         */

static void copy_list_items(obs_property_t *dst, obs_property_t *src)
{
	if (!dst || !src)
		return;

	enum obs_combo_format format = obs_property_list_format(src);
	size_t count = obs_property_list_item_count(src);

	obs_property_list_clear(dst);

	for (size_t i = 0; i < count; i++) {
		const char *name = obs_property_list_item_name(src, i);

		switch (format) {
		case OBS_COMBO_FORMAT_INT:
			obs_property_list_add_int(
				dst, name, obs_property_list_item_int(src, i));
			break;
		case OBS_COMBO_FORMAT_FLOAT:
			obs_property_list_add_float(
				dst, name,
				obs_property_list_item_float(src, i));
			break;
		case OBS_COMBO_FORMAT_STRING:
		default:
			obs_property_list_add_string(
				dst, name,
				obs_property_list_item_string(src, i));
			break;
		}

		if (obs_property_list_item_disabled(src, i))
			obs_property_list_item_disable(dst, i, true);
	}
}

static void clone_list_property(obs_properties_t *dst, obs_properties_t *src,
				const char *key, const char *label)
{
	obs_property_t *src_prop = obs_properties_get(src, key);
	if (!src_prop)
		return;

	obs_property_t *dst_prop = obs_properties_add_list(
		dst, key, label, obs_property_list_type(src_prop),
		obs_property_list_format(src_prop));

	copy_list_items(dst_prop, src_prop);
}

static bool camera_device_changed(obs_properties_t *props, obs_property_t *p,
				  obs_data_t *settings)
{
	UNUSED_PARAMETER(p);

	obs_properties_t *src = probe_dshow_properties(settings);
	if (!src)
		return true;

	copy_list_items(obs_properties_get(props, "resolution"),
			obs_properties_get(src, "resolution"));
	copy_list_items(obs_properties_get(props, "frame_interval"),
			obs_properties_get(src, "frame_interval"));
	copy_list_items(obs_properties_get(props, "video_format"),
			obs_properties_get(src, "video_format"));

	obs_properties_destroy(src);
	return true;
}

static bool res_type_changed(obs_properties_t *props, obs_property_t *p,
			     obs_data_t *settings)
{
	UNUSED_PARAMETER(p);

	obs_property_t *resolution = obs_properties_get(props, "resolution");
	if (resolution)
		obs_property_set_enabled(
			resolution, obs_data_get_int(settings, "res_type") != 0);

	return true;
}

static const char *const preset_bound_keys[] = {
	"smooth",      "whitening", "rosy",       "clarity",
	"brightness",  "contrast",  "saturation", "temperature",
	"face_slim",   "eye_enlarge", "chin_lift", "spot",
	"eye_bright",  "teeth",     "dark_circle", "contour",
	"side_shadow", "makeup_style", "lip",     "blush",
	"eyeshadow",   "look_mode", "look_amount", "body",
	"bg_blur",     "glow",      "grain",      "vignette",
};

static void apply_preset_ui_state(obs_properties_t *props, obs_data_t *settings)
{
	bool custom = obs_data_get_int(settings, "preset") == 0;

	for (size_t i = 0;
	     i < sizeof(preset_bound_keys) / sizeof(preset_bound_keys[0]); i++) {
		obs_property_t *prop =
			obs_properties_get(props, preset_bound_keys[i]);
		if (prop)
			obs_property_set_enabled(prop, custom);
	}
}

static bool preset_changed(obs_properties_t *props, obs_property_t *p,
			   obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	apply_preset_ui_state(props, settings);
	return true;
}

static void apply_face_ui_state(obs_properties_t *props, obs_data_t *settings)
{
	bool manual = obs_data_get_bool(settings, "face_enabled") &&
		      !obs_data_get_bool(settings, "face_auto");
	const char *const keys[] = {"face_x", "face_y", "face_radius"};

	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
		obs_property_t *prop = obs_properties_get(props, keys[i]);
		if (prop)
			obs_property_set_enabled(prop, manual);
	}
}

static bool face_mode_changed(obs_properties_t *props, obs_property_t *p,
			      obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	apply_face_ui_state(props, settings);
	return true;
}

static void reset_beauty_settings(obs_source_t *source)
{
	if (!source)
		return;

	obs_data_t *settings = obs_source_get_settings(source);
	if (!settings)
		return;

#define SET_VALUE_DOUBLE(key, value) obs_data_set_double(settings, key, value);
	BEAUTY_DOUBLE_SETTINGS(SET_VALUE_DOUBLE)
#undef SET_VALUE_DOUBLE

	obs_data_set_int(settings, "preset", 9);
	obs_data_set_int(settings, "compare_mode", 0);
	obs_data_set_int(settings, "makeup_style", 1);
	obs_data_set_int(settings, "look_mode", 1);
	obs_data_set_bool(settings, "enabled", true);
	obs_data_set_bool(settings, "face_enabled", true);
	obs_data_set_bool(settings, "face_auto", true);

	obs_source_update(source, NULL);
	obs_data_release(settings);
}

static bool reset_source_clicked(obs_properties_t *props, obs_property_t *p,
				 void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);

	struct beauty_source *ctx = data;
	reset_beauty_settings(ctx->source);
	return true;
}

static bool reset_filter_clicked(obs_properties_t *props, obs_property_t *p,
				 void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);

	struct beauty_filter *filter = data;
	reset_beauty_settings(filter->source);
	return true;
}

/* ------------------------------------------------------------------------- */
/* properties construction                                                    */

static void add_slider(obs_properties_t *props, const char *key,
		       const char *label, double min, double max, double step)
{
	obs_properties_add_float_slider(props, key, label, min, max, step);
}

static obs_property_t *add_toggle_row(obs_properties_t *props,
				      const char *key, const char *label,
				      const char *tooltip)
{
	obs_property_t *p = obs_properties_add_bool(props, key, label);
	if (tooltip && *tooltip)
		obs_property_set_long_description(p, tooltip);
	return p;
}

static wchar_t *default_browser_path(void)
{
	wchar_t progid[128];
	DWORD size = sizeof(progid);
	if (RegGetValueW(HKEY_CURRENT_USER,
			 L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice",
			 L"ProgId", RRF_RT_REG_SZ, NULL, progid,
			 &size) != ERROR_SUCCESS)
		return NULL;

	wchar_t key[512];
	swprintf(key, 512, L"%s\\shell\\open\\command", progid);

	wchar_t cmd[2048];
	size = sizeof(cmd);
	if (RegGetValueW(HKEY_CLASSES_ROOT, key, NULL, RRF_RT_REG_SZ, NULL,
			 cmd, &size) != ERROR_SUCCESS)
		return NULL;

	wchar_t *start = cmd;
	while (*start == L' ' || *start == L'\t')
		start++;

	if (*start == L'"') {
		start++;
		wchar_t *end = wcschr(start, L'"');
		if (!end)
			return NULL;
		*end = 0;
	} else {
		wchar_t *end = wcschr(start, L' ');
		if (end)
			*end = 0;
	}

	if (GetFileAttributesW(start) == INVALID_FILE_ATTRIBUTES)
		return NULL;

	size_t len = wcslen(start) + 1;
	wchar_t *exe = bmalloc(len * sizeof(wchar_t));
	if (exe)
		memcpy(exe, start, len * sizeof(wchar_t));
	return exe;
}

static void resolve_manual_path(const wchar_t *path, wchar_t *out,
				size_t count)
{
	if (path[0] == L'\\' || (path[0] && path[1] == L':')) {
		wcsncpy(out, path, count - 1);
		out[count - 1] = 0;
		return;
	}

	wchar_t base[MAX_PATH];
	DWORD len = GetModuleFileNameW(NULL, base, MAX_PATH);
	if (len > 0 && len < MAX_PATH) {
		wchar_t *slash = wcsrchr(base, L'\\');
		if (slash)
			*slash = 0;
	} else {
		base[0] = 0;
	}

	wchar_t combined[4096];
	swprintf(combined, 4096, L"%s\\%s", base, path);
	DWORD n = GetFullPathNameW(combined, (DWORD)count, out, NULL);
	if (n == 0 || n >= count) {
		wcsncpy(out, combined, count - 1);
		out[count - 1] = 0;
	}
}

static void open_manual_file(const wchar_t *path)
{
	wchar_t *browser = default_browser_path();
	if (browser) {
		wchar_t params[4096];
		swprintf(params, 4096, L"\"%s\"", path);
		HINSTANCE result = ShellExecuteW(NULL, L"open", browser, params,
						 NULL, SW_SHOWNORMAL);
		bfree(browser);
		if ((INT_PTR)result > 32)
			return;
	}

	ShellExecuteW(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL);
}

static bool help_clicked(obs_properties_t *props, obs_property_t *prop,
			 void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(prop);
	UNUSED_PARAMETER(data);

	char *path = obs_module_file("manual.html");
	if (!path) {
		blog(LOG_WARNING, "[beauty-camera] manual.html not found");
		return false;
	}

	wchar_t *wpath = NULL;
	if (os_utf8_to_wcs_ptr(path, 0, &wpath) > 0 && wpath) {
		wchar_t manual[4096];
		resolve_manual_path(wpath, manual, 4096);
		open_manual_file(manual);
		bfree(wpath);
	}
	bfree(path);
	return true;
}

static void add_beauty_properties(obs_properties_t *props,
				  bool (*reset_cb)(obs_properties_t *,
						   obs_property_t *, void *))
{
	obs_properties_t *help = obs_properties_create();
	obs_properties_add_text(help, "help_text",
				obs_module_text("HelpText"), OBS_TEXT_INFO);
	obs_properties_add_button(help, "help_open",
				  obs_module_text("HelpButton"), help_clicked);
	obs_properties_add_group(props, "grp_help", obs_module_text("HelpGroup"),
				 OBS_GROUP_NORMAL, help);

	obs_properties_t *general = obs_properties_create();

	obs_properties_add_text(general, "about",
				obs_module_text("AboutText"), OBS_TEXT_INFO);
	add_toggle_row(general, "enabled",
		       obs_module_text("EnableBeauty"), NULL);
	obs_properties_add_float_slider(general, "master",
					obs_module_text("MasterStrength"), 0.0,
					100.0, 1.0);

	obs_property_t *compare = obs_properties_add_list(
		general, "compare_mode", obs_module_text("CompareMode"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(compare, obs_module_text("CompareOff"), 0);
	obs_property_list_add_int(compare, obs_module_text("CompareOriginal"),
				  1);
	obs_property_list_add_int(compare, obs_module_text("CompareSplit"), 2);

	obs_property_t *preset = obs_properties_add_list(
		general, "preset", obs_module_text("Preset"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(preset, obs_module_text("PresetRefined"), 9);
	obs_property_list_add_int(preset, obs_module_text("PresetCustom"), 0);
	obs_property_list_add_int(preset, obs_module_text("PresetNatural"), 1);
	obs_property_list_add_int(preset, obs_module_text("PresetClear"), 2);
	obs_property_list_add_int(preset, obs_module_text("PresetCream"), 3);
	obs_property_list_add_int(preset, obs_module_text("PresetSculpt"), 4);
	obs_property_list_add_int(preset, obs_module_text("PresetLive"), 5);
	obs_property_list_add_int(preset, obs_module_text("PresetSweet"), 6);
	obs_property_list_add_int(preset, obs_module_text("PresetCool"), 7);
	obs_property_list_add_int(preset, obs_module_text("PresetRetro"), 8);
	obs_property_set_modified_callback(preset, preset_changed);

	obs_properties_add_text(general, "preset_info",
				obs_module_text("PresetInfo"), OBS_TEXT_INFO);

	if (reset_cb)
		obs_properties_add_button(general, "reset_beauty",
					  obs_module_text("ResetBeauty"),
					  reset_cb);

	obs_properties_add_group(props, "grp_general",
				 obs_module_text("BeautyGroup"),
				 OBS_GROUP_NORMAL, general);

	obs_properties_t *skin = obs_properties_create();
	add_slider(skin, "smooth", obs_module_text("Smooth"), 0.0, 100.0, 1.0);
	add_slider(skin, "whitening", obs_module_text("Whitening"), 0.0, 100.0,
		   1.0);
	add_slider(skin, "rosy", obs_module_text("Rosy"), 0.0, 100.0, 1.0);
	add_slider(skin, "clarity", obs_module_text("Clarity"), 0.0, 100.0, 1.0);
	add_slider(skin, "spot", obs_module_text("SpotRemove"), 0.0, 100.0,
		   1.0);
	add_slider(skin, "skin_protect", obs_module_text("SkinProtect"), 0.0,
		   100.0, 1.0);
	add_slider(skin, "smooth_radius", obs_module_text("SmoothRadius"), 0.0,
		   8.0, 0.5);
	add_slider(skin, "mask_softness", obs_module_text("MaskSoftness"), 0.0,
		   100.0, 1.0);

	obs_properties_add_group(props, "grp_skin",
				 obs_module_text("SkinGroup"),
				 OBS_GROUP_NORMAL, skin);

	obs_properties_t *eyes = obs_properties_create();
	add_slider(eyes, "eye_enlarge", obs_module_text("EyeEnlarge"), 0.0,
		   100.0, 1.0);
	add_slider(eyes, "eye_bright", obs_module_text("EyeBright"), 0.0, 100.0,
		   1.0);
	add_slider(eyes, "teeth", obs_module_text("TeethWhite"), 0.0, 100.0,
		   1.0);
	add_slider(eyes, "dark_circle", obs_module_text("DarkCircle"), 0.0,
		   100.0, 1.0);

	obs_properties_add_group(props, "grp_eyes",
				 obs_module_text("EyeGroup"),
				 OBS_GROUP_NORMAL, eyes);

	obs_properties_t *face = obs_properties_create();
	obs_property_t *face_enabled =
		add_toggle_row(face, "face_enabled",
			       obs_module_text("FaceEnabled"),
			       obs_module_text("FaceInfo"));
	obs_property_t *face_auto =
		add_toggle_row(face, "face_auto",
			       obs_module_text("FaceAuto"),
			       obs_module_text("FaceAutoInfo"));
	obs_properties_add_text(face, "tracking_info",
				obs_module_text("FaceAutoNote"), OBS_TEXT_INFO);
	obs_properties_add_float_slider(face, "face_x",
					obs_module_text("FaceX"), 0.0, 100.0,
					1.0);
	obs_properties_add_float_slider(face, "face_y",
					obs_module_text("FaceY"), 0.0, 100.0,
					1.0);
	obs_properties_add_float_slider(face, "face_radius",
					obs_module_text("FaceRadius"), 5.0,
					100.0, 1.0);
	add_slider(face, "face_slim", obs_module_text("FaceSlim"), 0.0, 100.0,
		   1.0);
	add_slider(face, "chin_lift", obs_module_text("ChinLift"), 0.0, 100.0,
		   1.0);
	obs_property_set_modified_callback(face_enabled, face_mode_changed);
	obs_property_set_modified_callback(face_auto, face_mode_changed);

	obs_properties_add_group(props, "grp_face",
				 obs_module_text("FaceGroup"),
				 OBS_GROUP_NORMAL, face);

	obs_properties_t *sculpt = obs_properties_create();
	add_slider(sculpt, "contour", obs_module_text("Contour"), 0.0, 100.0,
		   1.0);
	add_slider(sculpt, "side_shadow", obs_module_text("SideShadow"), 0.0,
		   100.0, 1.0);

	obs_properties_add_group(props, "grp_sculpt",
				 obs_module_text("SculptGroup"),
				 OBS_GROUP_NORMAL, sculpt);

	obs_properties_t *makeup = obs_properties_create();
	obs_property_t *style = obs_properties_add_list(
		makeup, "makeup_style", obs_module_text("MakeupStyle"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(style, obs_module_text("MakeupOff"), 0);
	obs_property_list_add_int(style, obs_module_text("MakeupNatural"), 1);
	obs_property_list_add_int(style, obs_module_text("MakeupPeach"), 2);
	obs_property_list_add_int(style, obs_module_text("MakeupRose"), 3);
	add_slider(makeup, "lip", obs_module_text("LipTint"), 0.0, 100.0, 1.0);
	add_slider(makeup, "blush", obs_module_text("Blush"), 0.0, 100.0, 1.0);
	add_slider(makeup, "eyeshadow", obs_module_text("EyeShadow"), 0.0, 100.0,
		   1.0);

	obs_properties_add_group(props, "grp_makeup",
				 obs_module_text("MakeupGroup"),
				 OBS_GROUP_NORMAL, makeup);

	obs_properties_t *look = obs_properties_create();
	obs_property_t *look_mode = obs_properties_add_list(
		look, "look_mode", obs_module_text("LookMode"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(look_mode, obs_module_text("LookOff"), 0);
	obs_property_list_add_int(look_mode, obs_module_text("LookClear"), 1);
	obs_property_list_add_int(look_mode, obs_module_text("LookCream"), 2);
	obs_property_list_add_int(look_mode, obs_module_text("LookFilm"), 3);
	obs_property_list_add_int(look_mode, obs_module_text("LookCool"), 4);
	obs_property_list_add_int(look_mode, obs_module_text("LookWarm"), 5);
	add_slider(look, "look_amount", obs_module_text("LookAmount"), 0.0,
		   100.0, 1.0);

	obs_properties_add_group(props, "grp_look",
				 obs_module_text("LookGroup"),
				 OBS_GROUP_NORMAL, look);

	obs_properties_t *body = obs_properties_create();
	add_slider(body, "body", obs_module_text("BodySlim"), 0.0, 100.0, 1.0);
	add_slider(body, "bg_blur", obs_module_text("BgBlur"), 0.0, 100.0,
		   1.0);

	obs_properties_add_group(props, "grp_body",
				 obs_module_text("BodyGroup"),
				 OBS_GROUP_NORMAL, body);

	obs_properties_t *color = obs_properties_create();
	add_slider(color, "brightness", obs_module_text("Brightness"), -100.0,
		   100.0, 1.0);
	add_slider(color, "contrast", obs_module_text("Contrast"), -100.0,
		   100.0, 1.0);
	add_slider(color, "saturation", obs_module_text("Saturation"), -100.0,
		   100.0, 1.0);
	add_slider(color, "temperature", obs_module_text("Temperature"),
		   -100.0, 100.0, 1.0);

	obs_properties_add_group(props, "grp_color",
				 obs_module_text("ColorGroup"),
				 OBS_GROUP_NORMAL, color);

	obs_properties_t *fx = obs_properties_create();
	add_slider(fx, "glow", obs_module_text("Glow"), 0.0, 100.0, 1.0);
	add_slider(fx, "grain", obs_module_text("Grain"), 0.0, 100.0, 1.0);
	add_slider(fx, "vignette", obs_module_text("Vignette"), 0.0, 100.0,
		   1.0);

	obs_properties_add_group(props, "grp_fx",
				 obs_module_text("EffectGroup"),
				 OBS_GROUP_NORMAL, fx);
}

static void add_camera_properties(obs_properties_t *props,
				 struct beauty_source *ctx)
{
	obs_properties_t *camera = obs_properties_create();
	obs_data_t *settings = obs_source_get_settings(ctx->source);
	obs_properties_t *src = settings ? probe_dshow_properties(settings) : NULL;

	if (src && obs_properties_get(src, "video_device_id")) {
		clone_list_property(camera, src, "video_device_id",
				    obs_module_text("CameraDevice"));
		clone_list_property(camera, src, "res_type",
				    obs_module_text("ResType"));
		clone_list_property(camera, src, "resolution",
				    obs_module_text("Resolution"));
		clone_list_property(camera, src, "frame_interval",
				    obs_module_text("FrameRate"));
		clone_list_property(camera, src, "video_format",
				    obs_module_text("VideoFormat"));
		clone_list_property(camera, src, "color_space",
				    obs_module_text("ColorSpace"));
		clone_list_property(camera, src, "color_range",
				    obs_module_text("ColorRange"));
		clone_list_property(camera, src, "buffering",
				    obs_module_text("Buffering"));
		add_toggle_row(camera, "flip_vertically",
			       obs_module_text("FlipImage"), NULL);

		obs_property_t *device =
			obs_properties_get(camera, "video_device_id");
		if (device)
			obs_property_set_modified_callback(
				device, camera_device_changed);
		obs_property_t *res_type =
			obs_properties_get(camera, "res_type");
		if (res_type) {
			obs_property_set_modified_callback(res_type,
							   res_type_changed);
			if (settings)
				res_type_changed(camera, res_type, settings);
		}
	} else {
		obs_properties_add_text(
			camera, "dshow_missing",
			obs_module_text("CameraDeviceMissing"), OBS_TEXT_INFO);
	}

	if (src)
		obs_properties_destroy(src);
	if (settings)
		obs_data_release(settings);

	obs_properties_add_group(props, "grp_camera",
				 obs_module_text("CameraGroup"),
				 OBS_GROUP_NORMAL, camera);
}

/* ------------------------------------------------------------------------- */
/* camera source                                                              */

static const char *beauty_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("BeautyCamera");
}

static void *beauty_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct beauty_source *ctx = bzalloc(sizeof(*ctx));
	ctx->source = source;

	obs_data_t *ds = build_dshow_settings(settings, false);
	ctx->camera = obs_source_create_private(
		"dshow_input", obs_source_get_name(source), ds);
	obs_data_release(ds);

	if (ctx->camera) {
		ctx->filter = obs_source_create_private(
			FILTER_ID, "Beauty Camera Filter", settings);
		if (ctx->filter)
			obs_source_filter_add(ctx->camera, ctx->filter);
	} else {
		blog(LOG_WARNING,
		     "[beauty-camera] camera disabled: dshow_input missing");
	}

	return ctx;
}

static void beauty_source_destroy(void *data)
{
	struct beauty_source *ctx = data;

	if (ctx->filter) {
		if (ctx->camera)
			obs_source_filter_remove(ctx->camera, ctx->filter);
		obs_source_release(ctx->filter);
	}
	if (ctx->camera)
		obs_source_release(ctx->camera);

	bfree(ctx);
}

static void beauty_source_update(void *data, obs_data_t *settings)
{
	struct beauty_source *ctx = data;

	if (ctx->camera) {
		obs_data_t *ds = build_dshow_settings(settings, false);
		obs_source_update(ctx->camera, ds);
		obs_data_release(ds);
	}

	if (ctx->filter)
		obs_source_update(ctx->filter, settings);
}

static uint32_t beauty_source_get_width(void *data)
{
	struct beauty_source *ctx = data;
	return ctx->camera ? obs_source_get_width(ctx->camera) : 0;
}

static uint32_t beauty_source_get_height(void *data)
{
	struct beauty_source *ctx = data;
	return ctx->camera ? obs_source_get_height(ctx->camera) : 0;
}

static void beauty_source_video_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);

	struct beauty_source *ctx = data;
	if (ctx->camera)
		obs_source_video_render(ctx->camera);
}

static void beauty_source_enum_active_sources(void *data,
					      obs_source_enum_proc_t callback,
					      void *param)
{
	struct beauty_source *ctx = data;
	if (ctx->camera)
		callback(ctx->source, ctx->camera, param);
}

static obs_properties_t *beauty_source_get_properties(void *data)
{
	struct beauty_source *ctx = data;
	obs_properties_t *props = obs_properties_create();

	add_camera_properties(props, ctx);
	add_beauty_properties(props, reset_source_clicked);

	obs_data_t *settings = obs_source_get_settings(ctx->source);
	if (settings) {
		apply_preset_ui_state(props, settings);
		apply_face_ui_state(props, settings);
		obs_data_release(settings);
	}

	return props;
}

struct obs_source_info beauty_camera_source_info = {
	.id = SOURCE_ID,
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = beauty_source_get_name,
	.create = beauty_source_create,
	.destroy = beauty_source_destroy,
	.update = beauty_source_update,
	.get_defaults = beauty_source_defaults,
	.get_properties = beauty_source_get_properties,
	.get_width = beauty_source_get_width,
	.get_height = beauty_source_get_height,
	.video_render = beauty_source_video_render,
	.enum_active_sources = beauty_source_enum_active_sources,
	.icon_type = OBS_ICON_TYPE_CAMERA,
};

/* ------------------------------------------------------------------------- */
/* beauty filter                                                              */

static const char *beauty_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("BeautyCameraFilter");
}

static void beauty_filter_update(void *data, obs_data_t *settings)
{
	struct beauty_filter *filter = data;
	beauty_params_load(&filter->p, settings);
}

static void *beauty_filter_create(obs_data_t *settings, obs_source_t *source)
{
	struct beauty_filter *filter = bzalloc(sizeof(*filter));
	filter->source = source;

	char *effect_path = obs_module_file("beauty.effect");
	if (!effect_path) {
		blog(LOG_ERROR,
		     "[beauty-camera] beauty.effect not found in module data");
		return filter;
	}

	obs_enter_graphics();
	filter->effect = gs_effect_create_from_file(effect_path, NULL);
	if (filter->effect) {
		struct beauty_uniforms *u = &filter->u;
#define GET_PARAM(field, name) \
	u->field = gs_effect_get_param_by_name(filter->effect, name)
		GET_PARAM(texel, "texel");
		GET_PARAM(aspect, "aspect");
		GET_PARAM(pixel_scale, "pixel_scale");
		GET_PARAM(compare_mode, "compare_mode");
		GET_PARAM(smooth_amount, "smooth_amount");
		GET_PARAM(whiten_amount, "whiten_amount");
		GET_PARAM(rosy_amount, "rosy_amount");
		GET_PARAM(clarity_amount, "clarity_amount");
		GET_PARAM(saturation, "saturation");
		GET_PARAM(temperature, "temperature");
		GET_PARAM(contrast, "contrast");
		GET_PARAM(brightness, "brightness");
		GET_PARAM(slim_amount, "slim_amount");
		GET_PARAM(eye_amount, "eye_amount");
		GET_PARAM(chin_amount, "chin_amount");
		GET_PARAM(skin_protect, "skin_protect");
		GET_PARAM(mask_softness, "mask_softness");
		GET_PARAM(smooth_radius, "smooth_radius");
		GET_PARAM(face_enabled, "face_enabled");
		GET_PARAM(face_center, "face_center");
		GET_PARAM(face_radius, "face_radius");
		GET_PARAM(time_sec, "time_sec");
		GET_PARAM(spot_amount, "spot_amount");
		GET_PARAM(eye_bright_amount, "eye_bright_amount");
		GET_PARAM(teeth_amount, "teeth_amount");
		GET_PARAM(dark_circle_amount, "dark_circle_amount");
		GET_PARAM(contour_amount, "contour_amount");
		GET_PARAM(side_shadow_amount, "side_shadow_amount");
		GET_PARAM(makeup_style, "makeup_style");
		GET_PARAM(lip_amount, "lip_amount");
		GET_PARAM(blush_amount, "blush_amount");
		GET_PARAM(eyeshadow_amount, "eyeshadow_amount");
		GET_PARAM(look_mode, "look_mode");
		GET_PARAM(look_amount, "look_amount");
		GET_PARAM(body_amount, "body_amount");
		GET_PARAM(bg_blur_amount, "bg_blur_amount");
		GET_PARAM(glow_amount, "glow_amount");
		GET_PARAM(grain_amount, "grain_amount");
		GET_PARAM(vignette_amount, "vignette_amount");
		GET_PARAM(face_count, "face_count");
		GET_PARAM(face0_on, "face0_on");
		GET_PARAM(face0_center, "face0_center");
		GET_PARAM(face0_radius, "face0_radius");
		GET_PARAM(face1_on, "face1_on");
		GET_PARAM(face1_center, "face1_center");
		GET_PARAM(face1_radius, "face1_radius");
		GET_PARAM(face2_on, "face2_on");
		GET_PARAM(face2_center, "face2_center");
		GET_PARAM(face2_radius, "face2_radius");
#undef GET_PARAM
	} else {
		blog(LOG_ERROR,
		     "[beauty-camera] failed to compile beauty.effect");
	}
	obs_leave_graphics();

	bfree(effect_path);

	beauty_filter_update(filter, settings);
	return filter;
}

static void beauty_filter_destroy(void *data)
{
	struct beauty_filter *filter = data;

	obs_enter_graphics();
	if (filter->effect)
		gs_effect_destroy(filter->effect);
	tracker_free(&filter->tracker);
	obs_leave_graphics();

	bfree(filter);
}

static inline void set_float(gs_eparam_t *param, float value)
{
	if (param)
		gs_effect_set_float(param, value);
}

static inline void set_vec2(gs_eparam_t *param, float x, float y)
{
	if (param) {
		struct vec2 value;
		vec2_set(&value, x, y);
		gs_effect_set_vec2(param, &value);
	}
}

/* ------------------------------------------------------------------------- */
/* automatic face tracking (GPU downscale -> CPU skin segmentation)           */

struct face_candidate {
	float cx;
	float cy;
	float r;
	int32_t score;
};

static bool skin_pixel(uint8_t r, uint8_t g, uint8_t b)
{
	float rf = r / 255.0f;
	float gf = g / 255.0f;
	float bf = b / 255.0f;
	float cb = -0.168736f * rf - 0.331264f * gf + 0.5f * bf + 0.5f;
	float cr = 0.5f * rf - 0.418688f * gf - 0.081312f * bf + 0.5f;
	float l = 0.299f * rf + 0.587f * gf + 0.114f * bf;

	return cb > 0.300f && cb < 0.480f && cr > 0.515f && cr < 0.690f &&
	       l > 0.18f && l < 0.97f;
}

static void tracker_free(struct beauty_tracker *t)
{
	if (t->texrender)
		gs_texrender_destroy(t->texrender);
	if (t->stage)
		gs_stagesurface_destroy(t->stage);

	bfree(t->pixels);
	bfree(t->mask);
	bfree(t->visited);
	bfree(t->stack);
	bfree(t->row_counts);

	memset(t, 0, sizeof(*t));
}

static void tracker_analyze(struct beauty_filter *f, uint32_t rw, uint32_t rh)
{
	struct beauty_tracker *t = &f->tracker;
	struct face_candidate cands[8];
	int cand_count = 0;
	uint32_t total = rw * rh;

	memset(t->visited, 0, total);

	for (uint32_t i = 0; i < total; i++) {
		const uint8_t *p = t->pixels + (size_t)i * 4;
		t->mask[i] = skin_pixel(p[0], p[1], p[2]) ? 1 : 0;
	}

	for (uint32_t y = 0; y < rh; y++) {
		for (uint32_t x = 0; x < rw; x++) {
			uint32_t seed = y * rw + x;
			if (!t->mask[seed] || t->visited[seed])
				continue;
			if (cand_count >= 8)
				break;

			int32_t sp = 0;
			t->stack[sp++] = (int32_t)seed;
			t->visited[seed] = 1;

			int32_t minx = (int32_t)x, maxx = (int32_t)x;
			int32_t miny = (int32_t)y, maxy = (int32_t)y;
			int32_t area = 0;

			memset(t->row_counts, 0,
			       (size_t)rh * sizeof(int32_t));

			while (sp > 0) {
				int32_t ci = t->stack[--sp];
				int32_t cy = ci / (int32_t)rw;
				int32_t cx = ci - cy * (int32_t)rw;

				area++;
				t->row_counts[cy]++;
				if (cx < minx)
					minx = cx;
				if (cx > maxx)
					maxx = cx;
				if (cy < miny)
					miny = cy;
				if (cy > maxy)
					maxy = cy;

				if (cx > 0) {
					int32_t ni = ci - 1;
					if (!t->visited[ni] && t->mask[ni]) {
						t->visited[ni] = 1;
						t->stack[sp++] = ni;
					}
				}
				if (cx + 1 < (int32_t)rw) {
					int32_t ni = ci + 1;
					if (!t->visited[ni] && t->mask[ni]) {
						t->visited[ni] = 1;
						t->stack[sp++] = ni;
					}
				}
				if (cy > 0) {
					int32_t ni = ci - (int32_t)rw;
					if (!t->visited[ni] && t->mask[ni]) {
						t->visited[ni] = 1;
						t->stack[sp++] = ni;
					}
				}
				if (cy + 1 < (int32_t)rh) {
					int32_t ni = ci + (int32_t)rw;
					if (!t->visited[ni] && t->mask[ni]) {
						t->visited[ni] = 1;
						t->stack[sp++] = ni;
					}
				}
			}

			int32_t bw = maxx - minx + 1;
			int32_t bh = maxy - miny + 1;

			int32_t peak = 0;
			int32_t peak_row = miny;
			int32_t peak_limit = miny + (int32_t)(bh * 0.60f);
			if (peak_limit > maxy)
				peak_limit = maxy;
			for (int32_t yy = miny; yy <= peak_limit; yy++) {
				if (t->row_counts[yy] > peak) {
					peak = t->row_counts[yy];
					peak_row = yy;
				}
			}
			if (area < (int32_t)(total / 500))
				continue;
			if (bh < (int32_t)(rh * 0.12f) ||
			    bh > (int32_t)(rh * 0.95f))
				continue;
			if ((float)bw / (float)bh < 0.35f ||
			    (float)bw / (float)bh > 1.60f)
				continue;
			if (peak < bw * 0.55f)
				continue;

			struct face_candidate *c = &cands[cand_count++];
			c->cx = ((float)minx + (float)maxx + 1.0f) * 0.5f /
				(float)rw;
			c->cy = ((float)peak_row + 0.5f) / (float)rh;
			if (c->cy < 0.05f)
				c->cy = 0.05f;
			c->r = 0.62f * (float)peak / (float)rh;
			if (c->r < 0.10f)
				c->r = 0.10f;
			if (c->r > 0.45f)
				c->r = 0.45f;
			c->score = area;
		}
	}

	int chosen[BEAUTY_MAX_FACES] = {-1, -1, -1};

	for (int k = 0; k < BEAUTY_MAX_FACES; k++) {
		int best = -1;
		for (int i = 0; i < cand_count; i++) {
			bool taken = false;
			for (int j = 0; j < k; j++) {
				if (chosen[j] == i) {
					taken = true;
					break;
				}
			}
			if (taken)
				continue;
			if (best < 0 || cands[i].score > cands[best].score)
				best = i;
		}
		chosen[k] = best;
		if (best < 0)
			break;
	}

	struct tracked_face updated[BEAUTY_MAX_FACES];
	bool used_tracked[BEAUTY_MAX_FACES] = {false};
	int updated_count = 0;

	for (int k = 0; k < BEAUTY_MAX_FACES && chosen[k] >= 0; k++) {
		struct face_candidate *c = &cands[chosen[k]];
		int match = -1;
		float best_d = 0.20f;
		for (int i = 0; i < t->face_count; i++) {
			if (used_tracked[i])
				continue;
			float dx = fabsf(t->faces[i].cx - c->cx);
			float dy = fabsf(t->faces[i].cy - c->cy);
			float d = dx + dy;
			if (d < best_d) {
				best_d = d;
				match = i;
			}
		}

		struct tracked_face *nf = &updated[updated_count++];
		nf->score = (float)c->score;
		nf->misses = 0;
		if (match >= 0) {
			nf->cx = t->faces[match].cx +
				 (c->cx - t->faces[match].cx) * 0.45f;
			nf->cy = t->faces[match].cy +
				 (c->cy - t->faces[match].cy) * 0.45f;
			nf->r = t->faces[match].r +
				(c->r - t->faces[match].r) * 0.35f;
			used_tracked[match] = true;
		} else {
			nf->cx = c->cx;
			nf->cy = c->cy;
			nf->r = c->r;
		}
	}

	for (int i = 0; i < t->face_count && updated_count < BEAUTY_MAX_FACES;
	     i++) {
		if (used_tracked[i])
			continue;
		struct tracked_face nf = t->faces[i];
		nf.misses++;
		if (nf.misses > 4)
			continue;
		updated[updated_count++] = nf;
	}

	for (int i = 0; i < updated_count; i++) {
		for (int j = i + 1; j < updated_count; j++) {
			if (updated[j].score > updated[i].score) {
				struct tracked_face tmp = updated[i];
				updated[i] = updated[j];
				updated[j] = tmp;
			}
		}
	}

	int old_count = t->face_count;
	for (int i = 0; i < updated_count; i++)
		t->faces[i] = updated[i];
	t->face_count = updated_count;

	if (t->face_count != old_count) {
		blog(LOG_INFO,
		     "[beauty-camera] auto face tracking: %d face(s)",
		     t->face_count);
		for (int i = 0; i < t->face_count; i++)
			blog(LOG_INFO,
			     "[beauty-camera]   face%d center=(%.3f, %.3f) radius=%.3f",
			     i, t->faces[i].cx, t->faces[i].cy, t->faces[i].r);
	}
}

static void tracker_update(struct beauty_filter *f, obs_source_t *target,
			   uint32_t width, uint32_t height)
{
	struct beauty_tracker *t = &f->tracker;

	if (!f->p.face_auto || !f->p.face_enabled) {
		t->face_count = 0;
		return;
	}

	uint64_t now = os_gettime_ns();
	if (t->last_analysis_ns &&
	    now - t->last_analysis_ns < ANALYSIS_INTERVAL_NS)
		return;
	t->last_analysis_ns = now;

	uint32_t rw = 160;
	uint32_t rh = (uint32_t)lroundf((float)height * 160.0f / (float)width);
	if (rh < 8)
		rh = 8;
	if (rw > width || rh > height) {
		rw = width;
		rh = height;
	}
	if (rw < 16 || rh < 16) {
		t->face_count = 0;
		return;
	}

	if (t->tex_w != rw || t->tex_h != rh) {
		if (t->texrender) {
			gs_texrender_destroy(t->texrender);
			t->texrender = NULL;
		}
		if (t->stage) {
			gs_stagesurface_destroy(t->stage);
			t->stage = NULL;
		}
		bfree(t->pixels);
		bfree(t->mask);
		bfree(t->visited);
		bfree(t->stack);
		bfree(t->row_counts);

		t->pixels = bmalloc((size_t)rw * rh * 4);
		t->mask = bmalloc((size_t)rw * rh);
		t->visited = bmalloc((size_t)rw * rh);
		t->stack = bmalloc((size_t)rw * rh * sizeof(int32_t));
		t->row_counts = bmalloc((size_t)rh * sizeof(int32_t));
		t->texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
		t->stage = gs_stagesurface_create(rw, rh, GS_RGBA);
		t->tex_w = rw;
		t->tex_h = rh;
	}

	if (!t->texrender || !t->stage || !t->pixels || !t->mask ||
	    !t->visited || !t->stack || !t->row_counts)
		return;

	if (!t->detect_effect) {
		char *path = obs_find_data_file("default.effect");
		if (path) {
			t->detect_effect =
				gs_effect_create_from_file(path, NULL);
			bfree(path);
		}
	}

	gs_texrender_reset(t->texrender);
	gs_viewport_push();
	gs_projection_push();
	if (gs_texrender_begin(t->texrender, rw, rh)) {
		struct vec4 clear_color;
		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_set_viewport(0, 0, (int)rw, (int)rh);
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f,
			 100.0f);

		gs_technique_t *tech = t->detect_effect
						? gs_effect_get_technique(
							  t->detect_effect,
							  "Draw")
						: NULL;
		if (tech) {
			size_t passes = gs_technique_begin(tech);
			for (size_t i = 0; i < passes; i++) {
				gs_technique_begin_pass(tech, i);
				obs_source_video_render(target);
				gs_technique_end_pass(tech);
			}
			gs_technique_end(tech);
		} else {
			obs_source_video_render(target);
		}

		gs_texrender_end(t->texrender);
	}
	gs_projection_pop();
	gs_viewport_pop();

	gs_stage_texture(t->stage, gs_texrender_get_texture(t->texrender));

	uint8_t *data;
	uint32_t linesize;
	if (gs_stagesurface_map(t->stage, &data, &linesize)) {
		for (uint32_t y = 0; y < rh; y++)
			memcpy(t->pixels + (size_t)y * rw * 4,
			       data + (size_t)y * linesize, (size_t)rw * 4);
		gs_stagesurface_unmap(t->stage);
	} else {
		return;
	}

	const char *tname = obs_source_get_name(target);
	int old_face_count = t->face_count;
	tracker_analyze(f, rw, rh);
	if (t->face_count != old_face_count)
		blog(LOG_INFO, "[beauty-camera] tracker source '%s': %d face(s)",
		     tname ? tname : "?", t->face_count);
}

static void beauty_filter_video_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);

	struct beauty_filter *filter = data;
	obs_source_t *parent = obs_filter_get_parent(filter->source);
	obs_source_t *target = obs_filter_get_target(filter->source);

	if (!filter->effect || !filter->p.enabled ||
	    filter->p.compare_mode == 1 || !parent || !target) {
		obs_source_skip_video_filter(filter->source);
		return;
	}

	uint32_t width = obs_source_get_width(target);
	uint32_t height = obs_source_get_height(target);
	if (!width || !height) {
		obs_source_skip_video_filter(filter->source);
		return;
	}

	filter->time_sec =
		fmodf((float)os_gettime_ns() / 1000000000.0f, 3600.0f);
	tracker_update(filter, target, width, height);

	if (!obs_source_process_filter_begin(filter->source, GS_RGBA,
					     OBS_ALLOW_DIRECT_RENDERING))
		return;

	struct beauty_values v;
	beauty_values_from_params(&filter->p, &v);

	set_vec2(filter->u.texel, 1.0f / (float)width, 1.0f / (float)height);
	set_float(filter->u.aspect, (float)width / (float)height);
	set_float(filter->u.pixel_scale,
		  fmaxf((float)height / 1080.0f, 0.5f));
	set_float(filter->u.compare_mode, (float)filter->p.compare_mode);

	set_float(filter->u.smooth_amount, v.smooth);
	set_float(filter->u.whiten_amount, v.whiten);
	set_float(filter->u.rosy_amount, v.rosy);
	set_float(filter->u.clarity_amount, v.clarity);
	set_float(filter->u.saturation, v.saturation);
	set_float(filter->u.temperature, v.temperature);
	set_float(filter->u.contrast, v.contrast);
	set_float(filter->u.brightness, v.brightness);
	set_float(filter->u.slim_amount, v.slim);
	set_float(filter->u.eye_amount, v.eye);
	set_float(filter->u.chin_amount, v.chin);

	set_float(filter->u.skin_protect,
		  filter->p.skin_protect / 100.0f);
	set_float(filter->u.mask_softness,
		  filter->p.mask_softness / 100.0f);
	set_float(filter->u.smooth_radius, filter->p.smooth_radius);

	set_float(filter->u.face_enabled,
		  filter->p.face_enabled ? 1.0f : 0.0f);
	set_vec2(filter->u.face_center, filter->p.face_x / 100.0f,
		 filter->p.face_y / 100.0f);
	set_float(filter->u.face_radius,
		  filter->p.face_radius / 100.0f);

	set_float(filter->u.time_sec, filter->time_sec);
	set_float(filter->u.spot_amount, v.spot);
	set_float(filter->u.eye_bright_amount, v.eye_bright);
	set_float(filter->u.teeth_amount, v.teeth);
	set_float(filter->u.dark_circle_amount, v.dark_circle);
	set_float(filter->u.contour_amount, v.contour);
	set_float(filter->u.side_shadow_amount, v.side_shadow);
	set_float(filter->u.makeup_style, (float)v.makeup_style);
	set_float(filter->u.lip_amount, v.lip);
	set_float(filter->u.blush_amount, v.blush);
	set_float(filter->u.eyeshadow_amount, v.eyeshadow);
	set_float(filter->u.look_mode, (float)v.look_mode);
	set_float(filter->u.look_amount, v.look_amount);
	set_float(filter->u.body_amount, v.body);
	set_float(filter->u.bg_blur_amount, v.bg_blur);
	set_float(filter->u.glow_amount, v.glow);
	set_float(filter->u.grain_amount, v.grain);
	set_float(filter->u.vignette_amount, v.vignette);

	struct beauty_tracker *tr = &filter->tracker;
	int face_count = tr->face_count;
	set_float(filter->u.face_count, (float)face_count);

	set_float(filter->u.face0_on, 0.0f);
	set_vec2(filter->u.face0_center, 0.0f, 0.0f);
	set_float(filter->u.face0_radius, 0.0f);
	set_float(filter->u.face1_on, 0.0f);
	set_vec2(filter->u.face1_center, 0.0f, 0.0f);
	set_float(filter->u.face1_radius, 0.0f);
	set_float(filter->u.face2_on, 0.0f);
	set_vec2(filter->u.face2_center, 0.0f, 0.0f);
	set_float(filter->u.face2_radius, 0.0f);

	if (face_count > 0) {
		set_float(filter->u.face0_on, 1.0f);
		set_vec2(filter->u.face0_center, tr->faces[0].cx,
			 tr->faces[0].cy);
		set_float(filter->u.face0_radius, tr->faces[0].r);
	}
	if (face_count > 1) {
		set_float(filter->u.face1_on, 1.0f);
		set_vec2(filter->u.face1_center, tr->faces[1].cx,
			 tr->faces[1].cy);
		set_float(filter->u.face1_radius, tr->faces[1].r);
	}
	if (face_count > 2) {
		set_float(filter->u.face2_on, 1.0f);
		set_vec2(filter->u.face2_center, tr->faces[2].cx,
			 tr->faces[2].cy);
		set_float(filter->u.face2_radius, tr->faces[2].r);
	}

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	obs_source_process_filter_end(filter->source, filter->effect, 0, 0);
	gs_blend_state_pop();
}

static obs_properties_t *beauty_filter_get_properties(void *data)
{
	struct beauty_filter *filter = data;
	obs_properties_t *props = obs_properties_create();

	add_beauty_properties(props, reset_filter_clicked);

	if (filter && filter->source) {
		obs_data_t *settings = obs_source_get_settings(filter->source);
		if (settings) {
			apply_preset_ui_state(props, settings);
			apply_face_ui_state(props, settings);
			obs_data_release(settings);
		}
	}

	return props;
}

struct obs_source_info beauty_camera_filter_info = {
	.id = FILTER_ID,
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = beauty_filter_get_name,
	.create = beauty_filter_create,
	.destroy = beauty_filter_destroy,
	.update = beauty_filter_update,
	.get_defaults = beauty_filter_defaults,
	.get_properties = beauty_filter_get_properties,
	.video_render = beauty_filter_video_render,
	.icon_type = OBS_ICON_TYPE_CAMERA,
};

/* ------------------------------------------------------------------------- */
/* module                                                                     */

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(MODULE_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Beauty Camera 2.1 - fully local GPU beauty camera and filter for OBS";
}

bool obs_module_load(void)
{
	obs_register_source(&beauty_camera_source_info);
	obs_register_source(&beauty_camera_filter_info);

	blog(LOG_INFO, "[beauty-camera] module loaded (version " BEAUTY_VERSION
		       ")");
	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[beauty-camera] module unloaded");
}
