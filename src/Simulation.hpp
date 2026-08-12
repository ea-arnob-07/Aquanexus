#pragma once
#include "Common.hpp"

namespace aqua {

struct PondState {
    std::string name;
    glm::vec3 center;
    glm::vec2 size;
    float baseWaterY = -0.72f;
    float waterLevel = 1.15f;
    float waste = 0.18f;
    int fishCount = 16;
};

struct FanState {
    std::string name;
    int fromPond = 0;
    int toPond = 1;
    bool on = false;
    float speed = 0.0f;
    float angle = 0.0f;
};

struct SensorReadings {
    float temperatureC = 28.0f;
    float ammoniaMgL = 0.25f;
    float pressureKPa = 112.0f;
    float pH = 7.4f;
    float dissolvedOxygenMgL = 6.5f;
    float waterLevelM = 1.15f;
    float quality01 = 0.8f;
};

struct FishAgent {
    int pond = 0;
    float phase = 0.0f;
    float lane = 0.0f;
    float speed = 0.6f;
    float depth = 0.55f;
    float scale = 1.0f;
    float wobble = 0.0f;
    float currentAffinity = 0.5f;
};

class Simulation {
public:
    Simulation();
    void reset();
    void update(float dt);
    void toggleFan(int index);
    void setAllFans(bool on);
    void setNightMode(bool enabled) { nightMode_=enabled; }
    bool nightMode() const { return nightMode_; }

    const std::array<PondState,3>& ponds() const { return ponds_; }
    std::array<PondState,3>& pondsMutable() { return ponds_; }
    const std::array<FanState,3>& fans() const { return fans_; }
    std::array<FanState,3>& fansMutable() { return fans_; }
    const SensorReadings& sensors() const { return sensors_; }
    const std::vector<FishAgent>& fish() const { return fish_; }
    float time() const { return time_; }
    float circulation01() const;
    float pondQuality(int pond) const;
    glm::vec3 fishPosition(const FishAgent& f) const;
    glm::vec3 fishDirection(const FishAgent& f) const;

private:
    std::array<PondState,3> ponds_;
    std::array<FanState,3> fans_;
    SensorReadings sensors_;
    std::vector<FishAgent> fish_;
    float time_ = 0.0f;
    float rngSeed_ = 22.0f;
    bool nightMode_ = false;

    void initializeFish();
    void updateWaterBalance(float dt);
    void updateWaste(float dt);
    void updateSensors(float dt);
};

} // namespace aqua
