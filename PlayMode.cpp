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
#include <cmath>

namespace {
    static bool move_towards(Scene::Transform *t, glm::vec3 target, float speed, float dt) {
        if (!t) return true;
        glm::vec3 d = target - t->position;       // local-space move
        float dist2 = glm::dot(d, d);
        float step = speed * dt;
        if (dist2 <= step*step) { t->position = target; return true; }
        t->position += (d / std::sqrt(dist2)) * step;
        return false;
    }
}

static char to_char_gender(Fan::Gender g)
{
	switch (g)
	{
	case Fan::Gender::F:
		return 'F';
	case Fan::Gender::M:
		return 'M';
	default:
		return 'N';
	}
}
static char to_char_pitch(Fan::Pitch p)
{
	switch (p)
	{
	case Fan::Pitch::L:
		return 'L';
	case Fan::Pitch::M:
		return 'M';
	default:
		return 'H';
	}
}
static char to_char_speed(Fan::Speed s)
{
	switch (s)
	{
	case Fan::Speed::L:
		return 'L';
	case Fan::Speed::M:
		return 'M';
	default:
		return 'H';
	}
}
std::string PlayMode::current_quality_from_ui() const
{
	std::string q;
	q.push_back(to_char_gender(ui.gender));
	q.push_back(to_char_pitch(ui.pitch));
	q.push_back(to_char_speed(ui.speed));
	return q; // e.g., "FMM"
}

namespace
{
	// Keep your existing convert function:
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

// Load<Sound::Sample> honk_sample(LoadTagDefault, []() -> Sound::Sample const *
// 								{ return new Sound::Sample(data_path("honk.wav")); });

PlayMode::PlayMode() : scene(*parrot_scene)
{
	for (auto &transform : scene.transforms)
	{
		if (transform.name == "Parrot")
			Parrot = &transform;
		else if (transform.name == "FMM")
			FMM = &transform;
		else if (transform.name == "MML")
			MML = &transform;
	}
	if (Parrot == nullptr)
		throw std::runtime_error("Parrot not found.");
	if (FMM == nullptr)
		throw std::runtime_error("FMM not found.");
	if (MML == nullptr)
		throw std::runtime_error("MML not found.");

	// get pointer to camera for convenience:
	if (scene.cameras.size() != 1)
		throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	fan_FMM.name = "FMM";
	fan_FMM.gender = Fan::Gender::F;
	fan_FMM.pitch = Fan::Pitch::M;
	fan_FMM.speed = Fan::Speed::M;
	fan_FMM.voice = "Aria";
	fan_FMM.transform = FMM;

	// base / wait / gone positions (local space)
	// fan_base_pos = FMM->position;
	if (MML)
		MML->position = fan_wait_pos; // park next fan in the queue

	// next fan (MML)
	if (MML)
	{
		fan_MML.name = "MML";
		fan_MML.gender = Fan::Gender::M;
		fan_MML.pitch = Fan::Pitch::M;
		fan_MML.speed = Fan::Speed::L;
		fan_MML.voice = "Andrew";
		fan_MML.transform = MML;
	}

	current_fan = &fan_FMM;
	next_fan    = (MML ? &fan_MML : nullptr);

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
		// printf("mouse down @ %f,%f \n", mouse_px.x, mouse_px.y);

		if (evt.button.button == SDL_BUTTON_LEFT)
		{
			float aspect = float(window_size.x) / float(window_size.y);
			glm::vec2 ndc = mouse_px_to_ndc(mouse_px, window_size);
			// printf(" -> ndc %f,%f\n", ndc.x, ndc.y);
			auto lay = VoiceUI::make_layout(aspect);

			// radios reflect current state before we process clicks:
			VoiceUI::sync_from_state(ui, lay);

			// gender
			{
				char newv = 0;
				if (lay.gender.click(ndc, &newv))
				{
					if (newv == 'F')
					{
						ui.gender = Fan::Gender::F;
					}
					else if (newv == 'M')
					{
						ui.gender = Fan::Gender::M;
					}
					else
					{
						ui.gender = Fan::Gender::N;
					}
					return true;
				}
			}
			// pitch
			{
				char newv = 0;
				if (lay.pitch.click(ndc, &newv))
				{
					if (newv == 'L')
					{
						ui.pitch = Fan::Pitch::L;
					}
					if (newv == 'M')
					{
						ui.pitch = Fan::Pitch::M;
					}
					if (newv == 'H')
					{
						ui.pitch = Fan::Pitch::H;
					}
					return true;
				}
			}
			// speed
			{
				char newv = 0;
				if (lay.speed.click(ndc, &newv))
				{
					if (newv == 'L')
					{
						ui.speed = Fan::Speed::L;
					}
					if (newv == 'M')
					{
						ui.speed = Fan::Speed::M;
					}
					if (newv == 'H')
					{
						ui.speed = Fan::Speed::H;
					}
					return true;
				}
			}

			// speak
			if (lay.speak.hit(ndc))
			{
				std::string q = current_quality_from_ui();		// e.g., "FMM"
				std::string key = q + "_" + current_fan->voice; // e.g., "FMM_Aria"
				printf("Speak clicked -> quality='%s' voice='%s' key='%s'\n",
					   q.c_str(), current_fan->voice.c_str(), key.c_str());

				// play imitation from the Parrot:
				auto *samp = get_sample_for(key);
				glm::mat4x3 pxf = Parrot->make_world_from_local();
				glm::vec3 parrot_pos = pxf[3];
				Sound::play_3D(*samp, 1.0f, parrot_pos, 3.0f);

				// ---- check each quality; set per-line match states (no scoring) ----
				char g_ui = to_char_gender(ui.gender);
				char p_ui = to_char_pitch(ui.pitch);
				char s_ui = to_char_speed(ui.speed);
				char g_fan = to_char_gender(current_fan->gender);
				char p_fan = to_char_pitch(current_fan->pitch);
				char s_fan = to_char_speed(current_fan->speed);

				printf("Check: UI=%c%c%c  Fan=%c%c%c\n", g_ui, p_ui, s_ui, g_fan, p_fan, s_fan);

				bool g_ok = (ui.gender == current_fan->gender);
				bool p_ok = (ui.pitch == current_fan->pitch);
				bool s_ok = (ui.speed == current_fan->speed);

				match_gender = g_ok ? Match::Hit : Match::Miss;
				match_pitch = p_ok ? Match::Hit : Match::Miss;
				match_speed = s_ok ? Match::Hit : Match::Miss;

				// Only proceed to next fan if ALL match:
				if (g_ok && p_ok && s_ok && next_fan && swap_phase == SwapPhase::Idle)
				{
					swap_phase = SwapPhase::Wait;
					swap_timer = 1.0f; // 1s, as requested
					printf("All matched. Swap: waiting %.2fs before moving fans\n", swap_timer);
				}
				else
				{
					if (!(g_ok && p_ok && s_ok))
					{
						printf("Not all matched. Staying with current fan.\n");
					}
				}
				return true;
			}

			// listen
			if (lay.listen.hit(ndc))
			{
				std::string key = current_fan->file_key(); // "FMM_Aria"
				printf("Listen clicked -> playing key='%s'\n", key.c_str());
				auto *samp = get_sample_for(key);
				glm::vec3 pos = fan_world_position(*current_fan);
				Sound::play_3D(*samp, 1.0f, pos, 3.0f);
				return true;
			}
		}
	}

	return false;
}

void PlayMode::update(float elapsed)
{
	// --- swap state machine ---
    if (swap_phase == SwapPhase::Wait) {
        swap_timer -= elapsed;
        if (swap_timer <= 0.0f) {
            swap_phase = SwapPhase::Moving;
            printf("Swap: start moving %s -> gone and %s -> base\n",
                current_fan ? current_fan->name.c_str() : "(none)",
                next_fan    ? next_fan->name.c_str()    : "(none)");
        }
    } else if (swap_phase == SwapPhase::Moving) {
        bool a_done = true, b_done = true;

        // move previous/current to gone:
        if (current_fan && current_fan->transform) {
            a_done = move_towards(current_fan->transform, fan_gone_pos, move_speed, elapsed);
        }
        // move next to base:
        if (next_fan && next_fan->transform) {
            b_done = move_towards(next_fan->transform, fan_base_pos, move_speed, elapsed);
        }

        if (a_done && b_done) {
            // switch current/next
            current_fan = next_fan;

			// reset line colors
			match_gender = Match::Unknown;
			match_pitch  = Match::Unknown;
			match_speed  = Match::Unknown;

			next_fan = nullptr;

            // after movement, autoplay the new fan's voice from their world position:
            if (current_fan) {
                std::string key = current_fan->file_key(); // e.g., "MML_Andrew"
                printf("Swap: movement done. Autoplay '%s'\n", key.c_str());
                auto *samp = get_sample_for(key);
                glm::vec3 pos = fan_world_position(*current_fan);
                Sound::play_3D(*samp, 1.0f, pos, 3.0f);
            }

            swap_phase = SwapPhase::Idle; // ready for whatever's next
        }
    }

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
	
	auto lay = VoiceUI::make_layout(aspect);

	// ---- top-right labels for Gender / Pitch / Speed ----
	auto color_for = [&](Match m) -> glm::u8vec4
	{
		switch (m)
		{
		case Match::Hit:
			return glm::u8vec4(0x66, 0xff, 0x66, 0xff); // green
		case Match::Miss:
			return glm::u8vec4(0xff, 0x66, 0x66, 0xff); // red
		default:
			return glm::u8vec4(0xff, 0xff, 0xff, 0xff); // white
		}
	};

	{
		const float H = VoiceUI::UI_H();
		const float x0 = VoiceUI::get_x0(aspect); // keep your symbol name
		const float yG = VoiceUI::row_y(0);
		const float yP = VoiceUI::row_y(1);
		const float yS = VoiceUI::row_y(2);

		glm::vec3 X(H, 0, 0), Y(0, H, 0);
		glm::u8vec4 shadow(0, 0, 0, 0xff);

		auto draw_label_col = [&](std::string const &s, float x, float y, glm::u8vec4 col)
		{
			float ofs = 2.0f / drawable_size.y;
			lines.draw_text(s, glm::vec3(x, y, 0.0f), X, Y, shadow);
			lines.draw_text(s, glm::vec3(x + ofs, y + ofs, 0.0f), X, Y, col);
		};

		// colors per line based on last Speak result:
		auto cG = color_for(match_gender);
		auto cP = color_for(match_pitch);
		auto cS = color_for(match_speed);

		// Row headers:
		draw_label_col("Gender:", x0, yG, cG);
		draw_label_col("Pitch:", x0, yP, cP);
		draw_label_col("Speed:", x0, yS, cS);

		// Option labels beside each radio circle (use same row color):
		draw_label_col("F:", lay.gender.items[0].center.x - 2.0f * H, yG, cG);
		draw_label_col("M:", lay.gender.items[1].center.x - 2.0f * H, yG, cG);

		draw_label_col("L:", lay.pitch.items[0].center.x - 2.0f * H, yP, cP);
		draw_label_col("M:", lay.pitch.items[1].center.x - 2.0f * H, yP, cP);
		draw_label_col("H:", lay.pitch.items[2].center.x - 2.0f * H, yP, cP);

		draw_label_col("L:", lay.speed.items[0].center.x - 2.0f * H, yS, cS);
		draw_label_col("M:", lay.speed.items[1].center.x - 2.0f * H, yS, cS);
		draw_label_col("H:", lay.speed.items[2].center.x - 2.0f * H, yS, cS);
	}

	{
		// reflect current UI state into radio groups:
		lay.gender.set_selected((ui.gender == Fan::Gender::F) ? 'F' : (ui.gender == Fan::Gender::M) ? 'M'
																									: '\0');
		lay.pitch.set_selected((ui.pitch == Fan::Pitch::L) ? 'L' : (ui.pitch == Fan::Pitch::M) ? 'M'
																							   : 'H');
		lay.speed.set_selected((ui.speed == Fan::Speed::L) ? 'L' : (ui.speed == Fan::Speed::M) ? 'M'
																							   : 'H');

		lay.gender.draw(lines, drawable_size);
		lay.pitch.draw(lines, drawable_size);
		lay.speed.draw(lines, drawable_size);

		// draw Speak button:
		lay.speak.draw(lines, drawable_size);

		// left/lower Listen button:
		lay.listen.draw(lines, drawable_size);
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
