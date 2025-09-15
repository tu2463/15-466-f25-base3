#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "VoiceUI.hpp"

#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <cmath>
#include <unordered_map>

namespace
{
	struct RectNDC
	{
		float x0, y0, x1, y1;
	};

	// Listen button rect in DrawLines' NDC space: x ∈ [-aspect, +aspect], y ∈ [-1, +1]
	// Placed on the left / lower side; sized using the same text height H used below.
	static RectNDC listen_rect_ndc(float aspect)
	{
		constexpr float H = 0.09f; // text height for the left panel
		RectNDC r;
		r.x0 = -aspect + 0.8f * H;
		r.x1 = -aspect + 5.0f * H; // a wide-ish button box
		r.y0 = -1.0f + 0.6f * H;
		r.y1 = -1.0f + 1.8f * H;
		return r;
	}

	// Convert pixel → NDC used by DrawLines overlay (so clicks match what we draw)
	static glm::vec2 mouse_px_to_ndc(glm::vec2 mouse_px, glm::uvec2 wnd)
	{
		float aspect = float(wnd.x) / float(wnd.y);
		float x_ndc = -aspect + 2.0f * aspect * (mouse_px.x / float(wnd.x));
		float y_ndc = +1.0f - 2.0f * (mouse_px.y / float(wnd.y));
		return glm::vec2(x_ndc, y_ndc);
	}
}

GLuint parrot_meshes_for_lit_color_texture_program = 0;
Load<MeshBuffer> parrot_meshes(LoadTagDefault, []() -> MeshBuffer const *
							   {
	MeshBuffer const *ret = new MeshBuffer(data_path("parrot.pnct"));
	parrot_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret; });

Load<Scene> parrot_scene(LoadTagDefault, []() -> Scene const *
						 { return new Scene(data_path("parrot.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name)
											{
												Mesh const &mesh = parrot_meshes->lookup(mesh_name);

												scene.drawables.emplace_back(transform);
												Scene::Drawable &drawable = scene.drawables.back();

												drawable.pipeline = lit_color_texture_program_pipeline;

												drawable.pipeline.vao = parrot_meshes_for_lit_color_texture_program;
												drawable.pipeline.type = mesh.type;
												drawable.pipeline.start = mesh.start;
												drawable.pipeline.count = mesh.count; }); });

// Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
// 	return new Sound::Sample(data_path("dusty-floor.opus"));
// });

Load<Sound::Sample> honk_sample(LoadTagDefault, []() -> Sound::Sample const *
								{ return new Sound::Sample(data_path("honk.wav")); });

PlayMode::PlayMode() : scene(*parrot_scene)
{
	for (auto &transform : scene.transforms)
	{
		if (transform.name == "Parrot")
			Parrot = &transform;
		else if (transform.name == "FMM")
			FMM = &transform;
	}
	if (Parrot == nullptr)
		throw std::runtime_error("Parrot not found.");
	if (FMM == nullptr)
		throw std::runtime_error("FMM not found.");

	// get pointer to camera for convenience:
	if (scene.cameras.size() != 1)
		throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	fan_FMM.name = "FMM";
	fan_FMM.gender = Fan::Gender::F;
	fan_FMM.pitch = Fan::Pitch::M;
	fan_FMM.speed = Fan::Speed::M;
	fan_FMM.voice = "Aria"; // so file_key() == "FMM_1"
	fan_FMM.transform = FMM;

	// start music loop playing:
	//  (note: position will be over-ridden in update())
	//  leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);
	//  bg_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);
}

PlayMode::~PlayMode()
{
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size)
{

	if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
	{
		glm::vec2 mouse_px(evt.button.x, evt.button.y);
		printf("mouse down @ %f,%f\n", mouse_px.x, mouse_px.y);

		// Only handle left button:
		if (evt.button.button == SDL_BUTTON_LEFT)
		{

			// Convert to overlay NDC and test against our button rect:
			float aspect = float(window_size.x) / float(window_size.y);
			glm::vec2 ndc = mouse_px_to_ndc(mouse_px, window_size);
			RectNDC R = listen_rect_ndc(aspect);

			bool inside =
				(ndc.x >= R.x0 && ndc.x <= R.x1 &&
				 ndc.y >= R.y0 && ndc.y <= R.y1);

			printf("Listen btn hit-test: ndc=(%f,%f) rect=[%f,%f]-[%f,%f] inside=%d\n",
				   ndc.x, ndc.y, R.x0, R.y0, R.x1, R.y1, int(inside));

			if (inside)
			{
				// Play current fan's voice at their world position:
				std::string key = fan_FMM.file_key(); // e.g., "FMM_1" or "FMM_Aria"
				printf("Listen clicked -> playing key='%s'\n", key.c_str());

				auto *samp = get_sample_for(key); // loads "<key>.wav" (or .opus if you change get_sample_for)
				glm::vec3 pos = fan_world_position(fan_FMM);
				Sound::play_3D(*samp, 1.0f, pos, 3.0f); // keep your 3D spatial
				return true;
			}
		}
	}

	return false;
}

void PlayMode::update(float elapsed)
{

	// //slowly rotates through [0,1):
	// wobble += elapsed / 10.0f;
	// wobble -= std::floor(wobble);

	// hip->rotation = hip_base_rotation * glm::angleAxis(
	// 	glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 1.0f, 0.0f)
	// );
	// upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
	// 	glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 0.0f, 1.0f)
	// );
	// lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
	// 	glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 0.0f, 1.0f)
	// );

	// //move sound to follow leg tip position:
	// leg_tip_loop->set_position(get_leg_tip_position(), 1.0f / 60.0f);

	// //move camera:
	// {

	// 	//combine inputs into a move:
	// 	constexpr float PlayerSpeed = 30.0f;
	// 	glm::vec2 move = glm::vec2(0.0f);
	// 	if (left.pressed && !right.pressed) move.x =-1.0f;
	// 	if (!left.pressed && right.pressed) move.x = 1.0f;
	// 	if (down.pressed && !up.pressed) move.y =-1.0f;
	// 	if (!down.pressed && up.pressed) move.y = 1.0f;

	// 	//make it so that moving diagonally doesn't go faster:
	// 	if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

	// 	glm::mat4x3 frame = camera->transform->make_parent_from_local();
	// 	glm::vec3 frame_right = frame[0];
	// 	//glm::vec3 up = frame[1];
	// 	glm::vec3 frame_forward = -frame[2];

	// 	camera->transform->position += move.x * frame_right + move.y * frame_forward;
	// }

	// { //update listener to camera position:
	// 	glm::mat4x3 frame = camera->transform->make_parent_from_local();
	// 	glm::vec3 frame_right = frame[0];
	// 	glm::vec3 frame_at = frame[3];
	// 	Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	// }

	// reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size)
{
	// update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	// set up light type and position for lit_color_texture_program:
	//  TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); // 1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); // this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);
	glDisable(GL_DEPTH_TEST);
	float aspect = float(drawable_size.x) / float(drawable_size.y);
	DrawLines lines(glm::mat4(
		1.0f / aspect, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f));

	{ // use DrawLines to overlay some text:
		auto box = [](bool on)
		{ return on ? "[x]" : "[ ]"; };
		auto radio = [](bool on)
		{ return on ? "(x)" : "( )"; };

		constexpr float H = 0.07f;		// text height
		float x0 = +aspect - 15.0f * H; // right side panel start
		float y = +1.0f - 1.2f * H;		// top row

		glm::vec3 X(H, 0.0f, 0.0f), Y(0.0f, H, 0.0f);
		auto draw = [&](std::string const &t)
		{
			// shadow
			lines.draw_text(t, glm::vec3(x0, y, 0.0f), X, Y, glm::u8vec4(0x00, 0x00, 0x00, 0xff));
			// main
			float ofs = 2.0f / drawable_size.y;
			lines.draw_text(t, glm::vec3(x0 + ofs, y + ofs, 0.0f), X, Y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
			y -= 1.2f * H;
		};

		// Line 1: Gender (square check boxes)
		draw(
			"Gender:  F " + std::string(box(ui.gender == Fan::Gender::F)) +
			"   M " + std::string(box(ui.gender == Fan::Gender::M)));

		// Line 2: Pitch (circle radio)
		draw(
			"Pitch:   Low " + std::string(radio(ui.pitch == Fan::Pitch::L)) +
			"  Mid " + std::string(radio(ui.pitch == Fan::Pitch::M)) +
			"  High " + std::string(radio(ui.pitch == Fan::Pitch::H)));

		// Line 3: Speed (circle radio)
		draw(
			"Speed:   Low " + std::string(radio(ui.speed == Fan::Speed::L)) +
			"  Mid " + std::string(radio(ui.speed == Fan::Speed::M)) +
			"  High " + std::string(radio(ui.speed == Fan::Speed::H)));

		// 	DrawLines lines(glm::mat4(
		// 		1.0f / aspect, 0.0f, 0.0f, 0.0f,
		// 		0.0f, 1.0f, 0.0f, 0.0f,
		// 		0.0f, 0.0f, 1.0f, 0.0f,
		// 		0.0f, 0.0f, 0.0f, 1.0f
		// 	));

		// 	constexpr float H = 0.09f;
		// 	lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
		// 		glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
		// 		glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
		// 		glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		// 	float ofs = 2.0f / drawable_size.y;
		// 	lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
		// 		glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
		// 		glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
		// 		glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}

	// ---- Left / lower "Listen" button ----
	{
		constexpr float H = 0.09f;

		// Button rect in NDC:
		RectNDC R = listen_rect_ndc(aspect);

		glm::vec3 X(H, 0.0f, 0.0f), Y(0.0f, H, 0.0f);

		// Button label near the rect's left/top with a bit of padding:
		float pad = 0.15f * H;
		glm::vec3 label_pos(R.x0 + pad, R.y0 + 0.45f * (R.y1 - R.y0), 0.0f);

		// Shadow
		lines.draw_text("[ Listen ]", label_pos, X, Y, glm::u8vec4(0x00, 0x00, 0x00, 0xff));
		// Main
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("[ Listen ]",
						label_pos + glm::vec3(ofs, ofs, 0.0f),
						X, Y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));

		glm::u8vec4 border(0xff, 0xff, 0xff, 0x66);
		lines.draw(glm::vec3(R.x0, R.y0, 0.0f), glm::vec3(R.x1, R.y0, 0.0f), border);
		lines.draw(glm::vec3(R.x1, R.y0, 0.0f), glm::vec3(R.x1, R.y1, 0.0f), border);
		lines.draw(glm::vec3(R.x1, R.y1, 0.0f), glm::vec3(R.x0, R.y1, 0.0f), border);
		lines.draw(glm::vec3(R.x0, R.y1, 0.0f), glm::vec3(R.x0, R.y0, 0.0f), border);
	}

	GL_ERRORS();
}

// glm::vec3 PlayMode::get_leg_tip_position() {
// 	//the vertex position here was read from the model in blender:
// 	return lower_leg->make_world_from_local() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
// }

glm::vec3 PlayMode::fan_world_position(Fan const &fan) const
{
	// world position of the transform's origin:
	glm::mat4x3 xf = fan.transform->make_world_from_local();
	return xf[3];
}

bool PlayMode::click_hits_fan(glm::vec2 mouse_px, glm::uvec2 wnd, Fan const &fan) const
{
	// Build a camera ray from the pixel
	float aspect = float(wnd.x) / float(wnd.y);
	float tan_half = std::tan(0.5f * camera->fovy);

	float nx = (mouse_px.x / float(wnd.x)) * 2.0f - 1.0f;
	float ny = 1.0f - (mouse_px.y / float(wnd.y)) * 2.0f;

	glm::vec3 dir_cam = glm::normalize(glm::vec3(nx * aspect * tan_half, ny * tan_half, -1.0f));

	// *** Use WORLD transform, not parent: ***
	glm::mat4x3 cframe = camera->transform->make_world_from_local();
	glm::vec3 right = cframe[0];
	glm::vec3 up = cframe[1];
	glm::vec3 forward = -cframe[2]; // camera looks along -Z in camera-local
	glm::vec3 origin = cframe[3];

	glm::vec3 dir = glm::normalize(dir_cam.x * right + dir_cam.y * up + dir_cam.z * forward);

	glm::vec3 fan_pos = fan_world_position(fan);
	glm::vec3 to = fan_pos - origin;

	// front/behind test:
	float t = glm::dot(to, dir);
	printf("click t=%f\n", t);
	if (t <= 0.0f)
		return false; // behind (or exactly at) camera along this ray

	// closest approach to the sphere:
	glm::vec3 closest = origin + t * dir;
	float d = glm::length(closest - fan_pos);
	printf("click d=%f (radius=%f)\n", d, fan.click_radius);
	return d <= fan.click_radius;
}

Sound::Sample const *PlayMode::get_sample_for(std::string const &key)
{
	// loads "<key>.wav" from data path; cache it
	auto it = sample_cache.find(key);
	if (it != sample_cache.end())
		return it->second.get();

	std::string filename = key + ".wav"; // convert your mp3 to wav or use .opus
	auto samp = std::make_unique<Sound::Sample>(data_path(filename));
	Sound::Sample *ptr = samp.get();
	sample_cache.emplace(key, std::move(samp));
	return ptr;
}
