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
	fan_FMM.voice = "Aria";
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
		printf("mouse down @ %f,%f \n", mouse_px.x, mouse_px.y);

		if (evt.button.button == SDL_BUTTON_LEFT)
		{
			float aspect = float(window_size.x) / float(window_size.y);
			glm::vec2 ndc = mouse_px_to_ndc(mouse_px, window_size);
			printf(" -> ndc %f,%f\n", ndc.x, ndc.y);
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
						printf("UI: gender -> F\n");
					}
					else if (newv == 'M')
					{
						ui.gender = Fan::Gender::M;
						printf("UI: gender -> M\n");
					}
					else
					{
						ui.gender = Fan::Gender::N;
						printf("UI: gender -> (none)\n");
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
						printf("UI: pitch -> L\n");
					}
					if (newv == 'M')
					{
						ui.pitch = Fan::Pitch::M;
						printf("UI: pitch -> M\n");
					}
					if (newv == 'H')
					{
						ui.pitch = Fan::Pitch::H;
						printf("UI: pitch -> H\n");
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
						printf("UI: speed -> L\n");
					}
					if (newv == 'M')
					{
						ui.speed = Fan::Speed::M;
						printf("UI: speed -> M\n");
					}
					if (newv == 'H')
					{
						ui.speed = Fan::Speed::H;
						printf("UI: speed -> H\n");
					}
					return true;
				}
			}

			// speak
			if (lay.speak.hit(ndc))
			{
				std::string q = current_quality_from_ui(); // e.g., "FMM"
				std::string key = q + "_" + fan_FMM.voice; // e.g., "FMM_Aria"
				printf("Speak clicked -> quality='%s' voice='%s' key='%s'\n",
					   q.c_str(), fan_FMM.voice.c_str(), key.c_str());
				auto *samp = get_sample_for(key);
				glm::mat4x3 pxf = Parrot->make_world_from_local();
				glm::vec3 parrot_pos = pxf[3];
				Sound::play_3D(*samp, 1.0f, parrot_pos, 3.0f);
				return true;
			}

			// listen
			if (lay.listen.hit(ndc))
			{
				std::string key = fan_FMM.file_key(); // "FMM_Aria"
				printf("Listen clicked -> playing key='%s'\n", key.c_str());
				auto *samp = get_sample_for(key);
				glm::vec3 pos = fan_world_position(fan_FMM);
				Sound::play_3D(*samp, 1.0f, pos, 3.0f);
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
	{
		const float H = VoiceUI::UI_H();
		const float x0 = VoiceUI::get_x0(aspect);
		const float y_gender = VoiceUI::row_y(0);
		const float y_pitch = VoiceUI::row_y(1);
		const float y_speed = VoiceUI::row_y(2);

		glm::vec3 X(H, 0.0f, 0.0f), Y(0.0f, H, 0.0f);
		glm::u8vec4 shadow(0x00, 0x00, 0x00, 0xff), mainc(0xff, 0xff, 0xff, 0xff);
		auto draw_label = [&](std::string const &s, float x, float y)
		{
			float ofs = 2.0f / drawable_size.y;
			lines.draw_text(s, glm::vec3(x, y, 0.0f), X, Y, shadow);
			lines.draw_text(s, glm::vec3(x + ofs, y + ofs, 0.0f), X, Y, mainc);
		};

		// Row headers:
		draw_label("Gender:", x0, y_gender);
		draw_label("Pitch:", x0, y_pitch);
		draw_label("Speed:", x0, y_speed);

		// Option labels placed just to the left of each radio circle:
		// Gender F/M:
		draw_label("F:", lay.gender.items[0].center.x - 2.0f * H, y_gender);
		draw_label("M:", lay.gender.items[1].center.x - 2.0f * H, y_gender);

		// Pitch L/M/H:
		draw_label("L:", lay.pitch.items[0].center.x - 2.0f * H, y_pitch);
		draw_label("M:", lay.pitch.items[1].center.x - 2.0f * H, y_pitch);
		draw_label("H:", lay.pitch.items[2].center.x - 2.0f * H, y_pitch);

		// Speed L/M/H:
		draw_label("L:", lay.speed.items[0].center.x - 2.0f * H, y_speed);
		draw_label("M:", lay.speed.items[1].center.x - 2.0f * H, y_speed);
		draw_label("H:", lay.speed.items[2].center.x - 2.0f * H, y_speed);
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
