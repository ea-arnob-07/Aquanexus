#include "Simulation.hpp"

namespace aqua {

namespace {
glm::vec2 upstreamDirectionForPond(const std::array<PondState,3>& ponds,
                                   const std::array<FanState,3>& fans,
                                   int pond,float& inflowStrength){
    glm::vec2 upstream(0.0f);
    inflowStrength=0.0f;
    for(const auto& fan:fans){
        if(fan.toPond!=pond) continue;
        glm::vec2 towardPipe(ponds[fan.fromPond].center.x-ponds[pond].center.x,
                             ponds[fan.fromPond].center.z-ponds[pond].center.z);
        if(glm::length2(towardPipe)>1e-6f) upstream+=glm::normalize(towardPipe)*fan.speed;
        inflowStrength+=fan.speed;
    }
    inflowStrength=saturate(inflowStrength);
    if(glm::length2(upstream)<1e-6f) return glm::vec2(1.0f,0.0f);
    return glm::normalize(upstream);
}

float fishCurrentAffinity(const FishAgent& f){
    return clampf(f.currentAffinity,0.0f,1.0f);
}
} // namespace

Simulation::Simulation(){ reset(); }

void Simulation::reset(){
    time_=0.0f;
    // Wider, village-scale spacing so the ponds feel like real rural ponds rather than a compact demo cluster.
    ponds_[0] = {"POND 1", {-24.0f,0.0f, 4.5f}, {14.0f,10.0f}, -1.46f,1.88f,0.14f,20};
    ponds_[1] = {"POND 2", {  0.0f,0.0f, 0.6f}, {14.8f,10.4f}, -1.49f,1.91f,0.21f,22};
    ponds_[2] = {"POND 3", { 25.0f,0.0f,-3.8f}, {14.2f,10.0f}, -1.44f,1.86f,0.17f,19};

    // Start alive, like the target reference. Keys 1/2/3 or UI can stop any fan.
    fans_[0] = {"FAN 1",0,1,true,0.82f,0.0f};
    fans_[1] = {"FAN 2",1,2,true,0.82f,1.4f};
    fans_[2] = {"FAN 3",2,0,true,0.82f,2.7f};
    sensors_ = {};
    initializeFish();
    updateSensors(1.0f);
}

void Simulation::initializeFish(){
    fish_.clear();
    std::mt19937 rng(13371337u);
    std::uniform_real_distribution<float> u(0.0f,1.0f);
    const int visibleTotals[3]={44,40,40};
    for(int p=0;p<3;++p){
        // Visual agents are intentionally separate from biological stock, so the denser
        // schools do not alter ammonia, oxygen, turbidity or the established water color.
        int total=visibleTotals[p];
        int placed=0;
        while(placed<total){
            // Loose schools share a lane, speed and depth but keep enough jitter to
            // split/rejoin naturally rather than moving as one rigid formation.
            int schoolSize=std::min(total-placed,3+(int)(u(rng)*4.0f));
            float schoolPhase=u(rng)*2.0f*PI;
            float schoolLane=u(rng)*2.0f-1.0f;
            float schoolSpeed=0.34f+u(rng)*0.46f;
            float schoolDepth=0.46f+u(rng)*0.86f;
            for(int k=0;k<schoolSize;++k){
                int ordinal=placed+k;
                bool strongCurrent=(p==0)?((ordinal%10)<7):((ordinal%20)<13);
                FishAgent f;
                f.pond=p;
                f.phase=schoolPhase+(u(rng)-0.5f)*0.55f;
                f.lane=clampf(schoolLane+(u(rng)-0.5f)*0.30f,-1.0f,1.0f);
                f.speed=schoolSpeed+(u(rng)-0.5f)*0.08f;
                f.depth=schoolDepth+(u(rng)-0.5f)*0.30f;
                f.scale=0.34f+u(rng)*0.18f;
                f.wobble=u(rng)*6.28f;
                f.currentAffinity=strongCurrent?(0.84f+u(rng)*0.16f):(0.18f+u(rng)*0.38f);
                fish_.push_back(f);
            }
            placed+=schoolSize;
        }
    }
}

void Simulation::toggleFan(int index){ if(index>=0&&index<3) fans_[index].on=!fans_[index].on; }
void Simulation::setAllFans(bool on){ for(auto& f:fans_) f.on=on; }

float Simulation::circulation01() const {
    return saturate((fans_[0].speed+fans_[1].speed+fans_[2].speed)/3.0f);
}

float Simulation::pondQuality(int p) const {
    float localFlow=0.0f;
    for(const auto& f:fans_) if(f.fromPond==p||f.toPond==p) localFlow+=0.5f*f.speed;
    localFlow=saturate(localFlow);
    float w=ponds_[p].waste;
    float fishLoad=ponds_[p].fishCount/24.0f;
    float q=0.92f - 0.47f*w - 0.10f*fishLoad + 0.24f*localFlow;
    if(p==1){
        float doScore=saturate((sensors_.dissolvedOxygenMgL-3.0f)/4.5f);
        float nh3Score=1.0f-saturate(sensors_.ammoniaMgL/1.45f);
        float phScore=1.0f-saturate(std::fabs(sensors_.pH-7.4f)/1.55f);
        q=0.43f*q+0.28f*doScore+0.20f*nh3Score+0.09f*phScore;
    }
    return saturate(q);
}

void Simulation::update(float dt){
    dt=std::min(dt,0.05f);
    time_+=dt;
    for(auto& f:fans_){
        float target=f.on?1.0f:0.0f;
        float response=1.0f-std::exp(-dt*(f.on?2.45f:4.6f));
        f.speed=lerpf(f.speed,target,response);
        f.angle += dt*(17.5f*f.speed);
        if(f.angle>2.0f*PI) f.angle=std::fmod(f.angle,2.0f*PI);
    }
    updateWaterBalance(dt);
    updateWaste(dt);
    updateSensors(dt);
}

void Simulation::updateWaterBalance(float dt){
    std::array<float,3> delta{0,0,0};
    for(const auto& f:fans_){
        float fromHead=ponds_[f.fromPond].waterLevel;
        float toHead=ponds_[f.toPond].waterLevel;
        float headFactor=clampf(1.0f+(fromHead-toHead)*0.38f,0.60f,1.38f);
        float flow=0.0205f*f.speed*headFactor;
        delta[f.fromPond]-=flow*dt;
        delta[f.toPond]+=flow*dt;
    }
    for(int i=0;i<3;++i) ponds_[i].waterLevel=clampf(ponds_[i].waterLevel+delta[i],1.20f,1.98f);
    float avg=(ponds_[0].waterLevel+ponds_[1].waterLevel+ponds_[2].waterLevel)/3.0f;
    for(auto& p:ponds_) p.waterLevel += (avg-p.waterLevel)*(1.0f-std::exp(-dt*0.032f));
}

void Simulation::updateWaste(float dt){
    for(int i=0;i<3;++i){
        float localFlow=0.0f;
        for(const auto& f:fans_) if(f.fromPond==i||f.toPond==i) localFlow+=f.speed*0.5f;
        localFlow=saturate(localFlow);
        float production=(0.00047f+0.000020f*ponds_[i].fishCount)*dt;
        float settling=0.00014f*(1.0f-localFlow)*dt;
        float flushing=0.00024f*localFlow*dt;
        ponds_[i].waste=clampf(ponds_[i].waste+production+settling-flushing,0.045f,0.96f);
    }
}

void Simulation::updateSensors(float dt){
    // Pond 2 digital-twin sensor model. It is intentionally simplified, but each
    // reading is dynamically coupled to circulation, fish load, waste and water level.
    float incoming=0.5f*(fans_[0].speed+fans_[1].speed);
    float totalCirc=circulation01();
    float w=ponds_[1].waste;
    float fishLoad=ponds_[1].fishCount/22.0f;
    float dayWave=std::sin(time_*0.032f);
    float fastNoise=std::sin(time_*1.73f)*0.030f+std::sin(time_*0.61f+1.3f)*0.020f;

    float night=nightMode_?1.0f:0.0f;
    // A tropical fish pond cools gradually after sunset. Photosynthesis stops while
    // fish/plants continue respiring, so dissolved oxygen and pH also ease downward.
    // Cooler, lower-pH water slightly reduces the unionised NH3 reading.
    float targetTemp=28.5f + 0.72f*dayWave - 0.34f*incoming + fastNoise - 1.10f*night;
    float targetNH3=clampf(0.055f+1.28f*w+0.15f*fishLoad-0.62f*totalCirc+0.020f*std::sin(time_*0.76f)-0.018f*night,0.025f,2.40f);
    float targetDO=clampf(6.18f+2.65f*totalCirc-1.62f*w-0.38f*fishLoad+0.20f*std::sin(time_*0.31f+0.5f)-0.72f*night,2.0f,10.5f);
    float targetPH=clampf(7.55f-0.34f*w-0.095f*(targetNH3-0.2f)+0.09f*incoming+0.025f*std::sin(time_*0.20f)-0.16f*night,6.4f,8.5f);
    float targetLevel=ponds_[1].waterLevel;
    float targetPressure=101.325f + 9.80665f*targetLevel;

    float k=1.0f-std::exp(-dt*1.65f);
    sensors_.temperatureC=lerpf(sensors_.temperatureC,targetTemp,k);
    sensors_.ammoniaMgL=lerpf(sensors_.ammoniaMgL,targetNH3,k);
    sensors_.dissolvedOxygenMgL=lerpf(sensors_.dissolvedOxygenMgL,targetDO,k);
    sensors_.pH=lerpf(sensors_.pH,targetPH,k);
    sensors_.waterLevelM=lerpf(sensors_.waterLevelM,targetLevel,k);
    sensors_.pressureKPa=lerpf(sensors_.pressureKPa,targetPressure,k);

    float doScore=saturate((sensors_.dissolvedOxygenMgL-3.0f)/4.2f);
    float nhScore=1.0f-saturate(sensors_.ammoniaMgL/1.45f);
    float phScore=1.0f-saturate(std::fabs(sensors_.pH-7.4f)/1.55f);
    float tempScore=1.0f-saturate(std::fabs(sensors_.temperatureC-28.5f)/6.0f);
    sensors_.quality01=saturate(doScore*0.40f+nhScore*0.32f+phScore*0.16f+tempScore*0.12f);
}

glm::vec3 Simulation::fishPosition(const FishAgent& f) const {
    const PondState& p=ponds_[f.pond];
    float q=(f.pond==1)?sensors_.quality01:pondQuality(f.pond);
    float speedMul=lerpf(0.38f,1.20f,q);
    float t=time_*f.speed*speedMul+f.phase;
    float rx=p.size.x*(0.28f+0.075f*f.lane);
    float rz=p.size.y*(0.29f-0.045f*f.lane);
    float x=std::sin(t)*rx + std::sin(t*2.7f+f.wobble)*0.42f;
    float z=std::cos(t*0.87f)*rz + std::cos(t*1.9f+f.wobble)*0.36f;

    // Positive rheotaxis: incoming water travels away from the pipe, so responsive
    // fish face back toward the inlet and station-hold against that current.  Their
    // small fore/aft and lateral corrections are blended with the normal school path.
    float inflow=0.0f;
    glm::vec2 upstream=upstreamDirectionForPond(ponds_,fans_,f.pond,inflow);
    glm::vec2 across(-upstream.y,upstream.x);
    float response=smoothstep(0.08f,0.82f,inflow*fishCurrentAffinity(f));
    float individuality=0.5f+0.5f*std::sin(f.wobble*1.91f+f.phase*0.43f);
    float holdingDistance=std::min(p.size.x,p.size.y)*(0.115f+0.085f*individuality+0.030f*f.lane);
    glm::vec2 holdCenter=glm::vec2(p.center.x,p.center.z)+upstream*holdingDistance;
    float correction=std::sin(t*0.72f+f.wobble)*(0.34f+0.34f*individuality);
    float lateral=std::sin(t*1.13f+f.phase*0.37f)*(0.66f+0.76f*individuality+0.22f*std::fabs(f.lane));
    glm::vec2 currentPos=holdCenter+upstream*correction+across*lateral;
    glm::vec2 normalPos=glm::vec2(p.center.x+x,p.center.z+z);
    float holdBlend=response*lerpf(0.80f,0.93f,fishCurrentAffinity(f));
    glm::vec2 blended=glm::mix(normalPos,currentPos,holdBlend);

    float depth=f.depth+0.08f*std::sin(t*1.7f+f.wobble);
    float holdingDepth=0.62f+0.16f*f.lane+0.05f*std::sin(t*1.4f+f.wobble);
    depth=lerpf(depth,holdingDepth,response*0.55f);
    // Poor oxygen makes fish slightly nearer the surface, a visible response to sensors.
    if(f.pond==1) depth*=lerpf(0.64f,1.0f,saturate((sensors_.dissolvedOxygenMgL-2.5f)/4.0f));
    float y=p.baseWaterY+p.waterLevel-depth;
    return glm::vec3(blended.x,y,blended.y);
}

glm::vec3 Simulation::fishDirection(const FishAgent& f) const {
    FishAgent f2=f; f2.phase+=0.025f;
    glm::vec3 a=fishPosition(f), b=fishPosition(f2), d=b-a;
    float inflow=0.0f;
    glm::vec2 upstream=upstreamDirectionForPond(ponds_,fans_,f.pond,inflow);
    float response=smoothstep(0.08f,0.82f,inflow*fishCurrentAffinity(f));
    glm::vec3 againstCurrent=glm::normalize(glm::vec3(upstream.x,0.025f*std::sin(time_*0.9f+f.wobble),upstream.y));
    if(glm::length2(d)<1e-7f) return response>0.01f?againstCurrent:glm::vec3(1,0,0);
    d=glm::normalize(d);
    glm::vec3 steered=glm::mix(d,againstCurrent,response*lerpf(0.86f,0.95f,fishCurrentAffinity(f)));
    if(glm::length2(steered)<1e-7f) return againstCurrent;
    return glm::normalize(steered);
}

} // namespace aqua
