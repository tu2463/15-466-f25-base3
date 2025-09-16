#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct Fan
{
    enum class Gender
    {
        F,
        M,
    };
    enum class Pitch
    {
        L,
        M,
        H
    };
    enum class Speed
    {
        L,
        M,
        H
    };

    std::string name; // e.g., "FMM"
    Gender gender = Gender::F;
    Pitch pitch = Pitch::M;
    Speed speed = Speed::M;
    std::string voice = ""; // e.g., "Aria"
    Scene::Transform *transform = nullptr;

    // Credit: helpers built with ChatGPT assistance.
    float click_radius = 0.6f;
    static char to_char(Gender g) { return (g == Gender::F ? 'F' : 'M'); }
    static char to_char(Pitch p) { return (p == Pitch::L ? 'L' : (p == Pitch::M ? 'M' : 'H')); }
    static char to_char(Speed s) { return (s == Speed::L ? 'L' : (s == Speed::M ? 'M' : 'H')); }

    std::string quality() const
    {
        return std::string() + to_char(gender) + to_char(pitch) + to_char(speed); // "FMM"
    }
    std::string file_key() const
    {
        return quality() + "_" + voice; // "FMM_Aria"
    }
};