#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
#include "Fan.hpp"
#include "VoiceUI.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	// --- helpers for click + audio ---
	VoiceUI::State ui;

	glm::vec3 fan_world_position(Fan const& fan) const;
	Sound::Sample const *get_sample_for(std::string const &key); // loads/caches "<key>.wav"

	std::string current_quality_from_ui() const;

	// --- game state ---

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// --- Scene & meshes ---
	Scene::Transform *Parrot = nullptr;
	Scene::Transform *FMM = nullptr;

	Fan fan_FMM;
	glm::vec3 fan_base_position = glm::vec3(0.0f, -5.0f, 0.0f);

	// --- audio sample cache ---
	std::unordered_map<std::string, std::unique_ptr<Sound::Sample>> sample_cache;


	// //hexapod leg to wobble:
	// Scene::Transform *hip = nullptr;
	// Scene::Transform *upper_leg = nullptr;
	// Scene::Transform *lower_leg = nullptr;
	// glm::quat hip_base_rotation;
	// glm::quat upper_leg_base_rotation;
	// glm::quat lower_leg_base_rotation;
	// float wobble = 0.0f;

	// glm::vec3 get_leg_tip_position();

	std::shared_ptr< Sound::PlayingSample > bg_loop;
	//music coming from the tip of the leg (as a demonstration):
	// std::shared_ptr< Sound::PlayingSample > leg_tip_loop;

	//car honk sound:
	std::shared_ptr< Sound::PlayingSample > honk_oneshot;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
