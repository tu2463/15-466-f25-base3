#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
#include "Fan.hpp"
#include "VoiceUI.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode
{
	PlayMode();
	virtual ~PlayMode();

	bool game_success = false;

	// functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	// --- helpers for click + audio --- // Credit: used ChatGPT for setting up a first draft.
	VoiceUI::State ui;

	glm::vec3 fan_world_position(Fan const &fan) const;
	Sound::Sample const *get_sample_for(std::string const &key);
	std::string current_quality_from_ui() const;

	// --- game state ---

	// input tracking:
	struct Button
	{
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	// local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// --- Scene & meshes ---
	Scene::Transform *Parrot = nullptr;
	Scene::Transform *FMM = nullptr;
	Scene::Transform *MLH = nullptr;

	Fan fan_FMM;
	Fan fan_MLH;
	Fan *current_fan = nullptr;
	Fan *next_fan = nullptr;

	glm::vec3 fan_base_pos = glm::vec3(0.0f, -5.0f, 0.0f);
	glm::vec3 fan_wait_pos = glm::vec3(7.0f, -15.0f, 0.0f);
	glm::vec3 fan_gone_pos = glm::vec3(-7.0f, -15.0f, -0.0f);

	// Proceed to next fan. // Credit: used ChatGPT for animation helper functions.
	enum class SwapPhase
	{
		Idle,
		Wait,
		Moving
	};
	SwapPhase swap_phase = SwapPhase::Idle;

	float swap_timer = 0.0f;
	float move_speed = 6.0f; // units / second

	enum class Match
	{
		Unknown,
		Hit,
		Miss
	};
	Match match_gender = Match::Unknown;
	Match match_pitch = Match::Unknown;
	Match match_speed = Match::Unknown;

	// --- audio sample cache ---
	std::unordered_map<std::string, std::unique_ptr<Sound::Sample>> sample_cache;

	std::shared_ptr<Sound::PlayingSample> bg_loop;
	// music coming from the tip of the leg (as a demonstration):
	//  std::shared_ptr< Sound::PlayingSample > leg_tip_loop;

	// camera:
	Scene::Camera *camera = nullptr;
};
