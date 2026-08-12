#include "App.hpp"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace aqua {

namespace {
struct HudRect { float x,y,w,h; };
struct HudLayout {
    std::array<HudRect,3> fan;
    HudRect allFans;
    std::array<HudRect,3> speed;
    HudRect pause,reset,day,night,fullscreen;
};

HudLayout makeHudLayout(){
    HudLayout h{};
    for(int i=0;i<3;++i) h.fan[i]={32.0f,274.0f+i*64.0f,266.0f,54.0f};
    h.allFans={32.0f,466.0f,266.0f,42.0f};
    for(int i=0;i<3;++i) h.speed[i]={32.0f+i*92.0f,596.0f,82.0f,44.0f};
    h.pause={32.0f,650.0f,129.0f,44.0f};
    h.reset={169.0f,650.0f,129.0f,44.0f};
    h.day={32.0f,704.0f,62.0f,44.0f};
    h.night={99.0f,704.0f,62.0f,44.0f};
    h.fullscreen={169.0f,704.0f,129.0f,44.0f};
    return h;
}
} // namespace

App::App(int w,int h):width_(w),height_(h){
    initWindow();
    scene_=std::make_unique<SceneRenderer>(width_,height_);
    setEnvironmentMode(false);
    ui_=std::make_unique<UI>();
    lastTime_=glfwGetTime();
}

App::~App(){
    ui_.reset(); scene_.reset();
    if(window_) glfwDestroyWindow(window_);
    glfwTerminate();
}

void App::initWindow(){
    if(!glfwInit()) throw std::runtime_error("GLFW initialization failed");
#ifdef __EMSCRIPTEN__
    glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES,4);
#endif
    glfwWindowHint(GLFW_RESIZABLE,GL_TRUE);
    window_=glfwCreateWindow(width_,height_,"AquaNexus - Photoreal 3-Pond Aquaculture Digital Twin",nullptr,nullptr);
    if(!window_){ glfwTerminate(); throw std::runtime_error("Could not create OpenGL window"); }
    glfwMakeContextCurrent(window_); glfwSwapInterval(1);
#ifndef __EMSCRIPTEN__
    glewExperimental=GL_TRUE;
    GLenum e=glewInit();
    if(e!=GLEW_OK) throw std::runtime_error(std::string("GLEW initialization failed: ")+(const char*)glewGetErrorString(e));
    glGetError();
    glEnable(GL_MULTISAMPLE);
#endif
    glfwSetWindowUserPointer(window_,this);
    glfwSetScrollCallback(window_,scrollCallback);
    glfwSetFramebufferSizeCallback(window_,framebufferCallback);
    glfwGetWindowPos(window_,&windowedX_,&windowedY_);
    glfwGetWindowSize(window_,&windowedW_,&windowedH_);
    glfwGetFramebufferSize(window_,&width_,&height_);
    glViewport(0,0,width_,height_);
}

void App::scrollCallback(GLFWwindow* w,double,double y){ if(auto* a=(App*)glfwGetWindowUserPointer(w)) a->onScroll(y); }
void App::framebufferCallback(GLFWwindow* w,int wi,int he){
    if(auto* a=(App*)glfwGetWindowUserPointer(w)){
        a->width_=std::max(1,wi); a->height_=std::max(1,he);
        if(a->scene_) a->scene_->resize(a->width_,a->height_);
    }
}
void App::onScroll(double y){ camera_.distance=clampf(camera_.distance-(float)y*2.6f,17.0f,82.0f); }

void App::setEnvironmentMode(bool night){
    nightMode_=night;
    sim_.setNightMode(night);
    if(scene_) scene_->setNightMode(night);
}

void App::toggleFullscreen(){
#ifndef __EMSCRIPTEN__
    if(!fullscreen_){
        glfwGetWindowPos(window_,&windowedX_,&windowedY_);
        glfwGetWindowSize(window_,&windowedW_,&windowedH_);
        GLFWmonitor* monitor=glfwGetPrimaryMonitor();
        const GLFWvidmode* mode=monitor?glfwGetVideoMode(monitor):nullptr;
        if(monitor&&mode){
            glfwSetWindowMonitor(window_,monitor,0,0,mode->width,mode->height,mode->refreshRate);
            fullscreen_=true;
        }
    } else {
        glfwSetWindowMonitor(window_,nullptr,windowedX_,windowedY_,windowedW_,windowedH_,0);
        fullscreen_=false;
    }
#endif
}

bool App::keyPressed(int key) const { return glfwGetKey(window_,key)==GLFW_PRESS && !prevKeys_[key]; }
std::string App::fnum(float v,int precision){ std::ostringstream ss; ss<<std::fixed<<std::setprecision(precision)<<v; return ss.str(); }

void App::processInput(float dt){
    if(keyPressed(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window_,GL_TRUE);
    if(keyPressed(GLFW_KEY_1)) sim_.toggleFan(0);
    if(keyPressed(GLFW_KEY_2)) sim_.toggleFan(1);
    if(keyPressed(GLFW_KEY_3)) sim_.toggleFan(2);
    if(keyPressed(GLFW_KEY_F)){
        bool all=sim_.fans()[0].on&&sim_.fans()[1].on&&sim_.fans()[2].on;
        sim_.setAllFans(!all);
    }
    if(keyPressed(GLFW_KEY_R)){ sim_.reset(); camera_.reset(); simSpeed_=1.0f; paused_=false; }
    if(keyPressed(GLFW_KEY_V)) camera_.reset();
    if(keyPressed(GLFW_KEY_H)) showUI_=!showUI_;
    if(keyPressed(GLFW_KEY_B)) sensorHudExpanded_=!sensorHudExpanded_;
    if(keyPressed(GLFW_KEY_D)) setEnvironmentMode(false);
    if(keyPressed(GLFW_KEY_N)) setEnvironmentMode(true);
    if(keyPressed(GLFW_KEY_F11)) toggleFullscreen();
    if(keyPressed(GLFW_KEY_C)) camera_.setCinematic(!camera_.cinematic);
    if(keyPressed(GLFW_KEY_SPACE)) paused_=!paused_;
    if(keyPressed(GLFW_KEY_X)) simSpeed_ = simSpeed_<2.0f ? 5.0f : (simSpeed_<10.0f ? 20.0f : 1.0f);

    camera_.update(window_,dt);

    double mx,my; glfwGetCursorPos(window_,&mx,&my);
    int windowW=1,windowH=1; glfwGetWindowSize(window_,&windowW,&windowH);
    mx*=double(width_)/double(std::max(1,windowW));
    my*=double(height_)/double(std::max(1,windowH));
    bool ml=glfwGetMouseButton(window_,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
    bool click=ml&&!prevMouseLeft_;
    if(click && showUI_){
        const HudLayout hud=makeHudLayout();
        for(int i=0;i<3;++i){
            const auto& r=hud.fan[i];
            if(pointInRect(mx,my,r.x,r.y,r.w,r.h)) sim_.toggleFan(i);
        }
        if(pointInRect(mx,my,hud.allFans.x,hud.allFans.y,hud.allFans.w,hud.allFans.h)){
            bool all=sim_.fans()[0].on&&sim_.fans()[1].on&&sim_.fans()[2].on;
            sim_.setAllFans(!all);
        }
        const float speeds[3]={1.0f,5.0f,20.0f};
        for(int i=0;i<3;++i){
            const auto& r=hud.speed[i];
            if(pointInRect(mx,my,r.x,r.y,r.w,r.h)) simSpeed_=speeds[i];
        }
        if(pointInRect(mx,my,hud.pause.x,hud.pause.y,hud.pause.w,hud.pause.h)) paused_=!paused_;
        if(pointInRect(mx,my,hud.reset.x,hud.reset.y,hud.reset.w,hud.reset.h)){
            sim_.reset(); camera_.reset(); simSpeed_=1.0f; paused_=false;
        }
        if(pointInRect(mx,my,hud.day.x,hud.day.y,hud.day.w,hud.day.h)) setEnvironmentMode(false);
        if(pointInRect(mx,my,hud.night.x,hud.night.y,hud.night.w,hud.night.h)) setEnvironmentMode(true);
        if(pointInRect(mx,my,hud.fullscreen.x,hud.fullscreen.y,hud.fullscreen.w,hud.fullscreen.h)) toggleFullscreen();
    }
    prevMouseLeft_=ml;
    for(int k=0;k<=GLFW_KEY_LAST;++k) prevKeys_[k]=(unsigned char)(glfwGetKey(window_,k)==GLFW_PRESS);
}

void App::drawUI(){
    ui_->begin(width_,height_);
    if(!showUI_){
        ui_->flush(); return;
    }

    scene_->appendWorldLabels(*ui_,sim_,camera_,width_,height_);

    const glm::vec4 panel{0.010f,0.025f,0.045f,0.56f};
    const glm::vec4 panelEdge{0.045f,0.225f,0.285f,0.68f};
    const glm::vec4 shadow{0.0f,0.0f,0.015f,0.20f};
    const glm::vec4 text{0.91f,0.97f,1.0f,1.0f};
    const glm::vec4 muted{0.57f,0.72f,0.82f,1.0f};
    const glm::vec4 green{0.25f,1.0f,0.55f,1.0f};
    const glm::vec4 cyan{0.18f,0.78f,1.0f,1.0f};
    const glm::vec4 amber{1.0f,0.72f,0.22f,1.0f};
    const glm::vec4 red{1.0f,0.30f,0.34f,1.0f};
    const HudLayout hud=makeHudLayout();

    auto card=[&](float x,float y,float w,float h){
        ui_->roundedRect(x+4,y+6,w,h,12,shadow);
        ui_->roundedRect(x,y,w,h,12,panelEdge);
        ui_->roundedRect(x+1,y+1,w-2,h-2,11,panel);
    };
    auto button=[&](const HudRect& r,const glm::vec4& bg,const glm::vec4& edge){
        ui_->roundedRect(r.x,r.y,r.w,r.h,8,edge);
        ui_->roundedRect(r.x+1,r.y+1,r.w-2,r.h-2,7,bg);
    };
    auto centered=[&](const HudRect& r,float y,float scale,const std::string& label,const glm::vec4& color){
        ui_->text(r.x+(r.w-ui_->textWidth(scale,label))*0.5f,y,scale,label,color);
    };

    // System status card.
    card(20,82,290,120);
    ui_->roundedRect(34,97,34,3,1.5f,cyan);
    ui_->text(78,91,1.25f,"SYSTEM STATUS",text);
    bool allOn=sim_.fans()[0].on&&sim_.fans()[1].on&&sim_.fans()[2].on;
    float circ=sim_.circulation01();
    struct StatusRow { const char* label; const char* value; glm::vec4 color; };
    const StatusRow statusRows[3]={
        {"ALL SYSTEMS",allOn?"ONLINE":"PARTIAL",allOn?green:amber},
        {"CIRCULATION",circ>0.10f?"ACTIVE":"IDLE",circ>0.10f?green:amber},
        {"SCENE LIGHT",nightMode_?"NIGHT":"DAY",nightMode_?cyan:amber}
    };
    for(int i=0;i<3;++i){
        float y=125.0f+i*23.0f;
        ui_->text(36,y,0.94f,statusRows[i].label,muted);
        ui_->circle(229,y+4.5f,3.6f,statusRows[i].color,14);
        float vw=ui_->textWidth(0.92f,statusRows[i].value);
        ui_->text(294-vw,y,0.92f,statusRows[i].value,statusRows[i].color);
    }

    // Every fan row clearly states what clicking it will do.
    card(20,216,290,306);
    ui_->roundedRect(34,231,34,3,1.5f,cyan);
    ui_->text(78,225,1.25f,"CIRCULATION FANS",text);
    ui_->text(34,250,0.78f,"CLICK A FAN TO START OR STOP ITS PUMP",muted);
    for(int i=0;i<3;++i){
        const auto& f=sim_.fans()[i]; const HudRect& r=hud.fan[i];
        glm::vec4 bg=f.on?glm::vec4(0.018f,0.17f,0.13f,0.62f):glm::vec4(0.17f,0.040f,0.060f,0.62f);
        glm::vec4 edge=f.on?glm::vec4(0.13f,0.62f,0.44f,0.74f):glm::vec4(0.65f,0.15f,0.22f,0.74f);
        button(r,bg,edge);
        ui_->circle(r.x+13,r.y+14,4.0f,f.on?green:red,16);
        ui_->text(r.x+24,r.y+8,0.92f,"FAN "+std::to_string(i+1)+"  POND "+std::to_string(f.fromPond+1)+" -> POND "+std::to_string(f.toPond+1),text);
        ui_->text(r.x+12,r.y+31,0.78f,f.on?"RUNNING - CLICK TO STOP":"STOPPED - CLICK TO START",f.on?green:red);
        std::string flow=fnum(52.0f*f.speed,0)+" L/S";
        ui_->text(r.x+r.w-ui_->textWidth(0.78f,flow)-12,r.y+31,0.78f,flow,cyan);
        HudRect badge{r.x+r.w-43,r.y+7,34,16};
        ui_->roundedRect(badge.x,badge.y,badge.w,badge.h,6,{0.035f,0.105f,0.145f,0.64f});
        centered(badge,badge.y+5,0.58f,"KEY "+std::to_string(i+1),muted);
    }
    button(hud.allFans,allOn?glm::vec4(0.20f,0.035f,0.055f,0.66f):glm::vec4(0.025f,0.21f,0.14f,0.66f),allOn?glm::vec4(0.75f,0.16f,0.23f,0.78f):glm::vec4(0.12f,0.68f,0.45f,0.78f));
    ui_->text(hud.allFans.x+13,hud.allFans.y+14,0.92f,allOn?"STOP ALL CIRCULATION FANS":"START ALL CIRCULATION FANS",allOn?red:green);
    ui_->text(hud.allFans.x+hud.allFans.w-ui_->textWidth(0.72f,"F KEY")-12,hud.allFans.y+15,0.72f,"F KEY",muted);

    // Simulation and view actions.
    card(20,536,290,226);
    ui_->roundedRect(34,551,34,3,1.5f,cyan);
    ui_->text(78,545,1.25f,"SIMULATION AND VIEW",text);
    ui_->text(34,570,0.78f,"BUTTON LABELS SHOW THEIR EXACT ACTION",muted);
    const float speeds[3]={1.0f,5.0f,20.0f};
    const char* speedNames[3]={"NORMAL","FAST","DEMO"};
    for(int i=0;i<3;++i){
        bool active=std::fabs(simSpeed_-speeds[i])<0.1f;
        button(hud.speed[i],active?glm::vec4(0.025f,0.20f,0.17f,0.66f):glm::vec4(0.025f,0.065f,0.095f,0.62f),active?glm::vec4(0.12f,0.72f,0.53f,0.78f):panelEdge);
        centered(hud.speed[i],hud.speed[i].y+7,0.84f,speedNames[i],active?green:text);
        centered(hud.speed[i],hud.speed[i].y+26,0.70f,std::to_string((int)speeds[i])+"X SPEED",muted);
    }
    button(hud.pause,paused_?glm::vec4(0.20f,0.13f,0.025f,0.66f):glm::vec4(0.025f,0.065f,0.095f,0.62f),paused_?glm::vec4(0.72f,0.46f,0.10f,0.78f):panelEdge);
    centered(hud.pause,hud.pause.y+7,0.84f,paused_?"RESUME SIM":"PAUSE SIM",paused_?amber:text);
    centered(hud.pause,hud.pause.y+26,0.68f,"SPACE KEY",muted);
    button(hud.reset,{0.025f,0.065f,0.095f,0.62f},panelEdge);
    centered(hud.reset,hud.reset.y+7,0.84f,"RESET ALL",text);
    centered(hud.reset,hud.reset.y+26,0.68f,"R KEY",muted);
    button(hud.day,nightMode_?glm::vec4(0.025f,0.065f,0.095f,0.58f):glm::vec4(0.20f,0.15f,0.035f,0.68f),nightMode_?panelEdge:glm::vec4(0.80f,0.57f,0.13f,0.80f));
    centered(hud.day,hud.day.y+7,0.82f,"DAY",nightMode_?muted:amber);
    centered(hud.day,hud.day.y+26,0.64f,"D KEY",muted);
    button(hud.night,nightMode_?glm::vec4(0.025f,0.13f,0.20f,0.68f):glm::vec4(0.025f,0.065f,0.095f,0.58f),nightMode_?glm::vec4(0.13f,0.55f,0.82f,0.80f):panelEdge);
    centered(hud.night,hud.night.y+7,0.76f,"NIGHT",nightMode_?cyan:muted);
    centered(hud.night,hud.night.y+26,0.64f,"N KEY",muted);
    button(hud.fullscreen,{0.025f,0.080f,0.115f,0.62f},panelEdge);
    centered(hud.fullscreen,hud.fullscreen.y+7,0.76f,fullscreen_?"EXIT FULLSCREEN":"ENTER FULLSCREEN",cyan);
    centered(hud.fullscreen,hud.fullscreen.y+26,0.68f,"F11 KEY",muted);

    // Live telemetry card on the right. B opens a larger, easier-to-read detail view.
    if(width_>760){
        const auto& s=sim_.sensors();
        struct Row{std::string n,v,detail;float score;};
        std::vector<Row> rows={
            {"TEMPERATURE",fnum(s.temperatureC,1)+" C","LIVE POND WATER TEMPERATURE",1.0f-saturate(std::fabs(s.temperatureC-28.5f)/6.0f)},
            {"AMMONIA NH3",fnum(s.ammoniaMgL,2)+" MG/L","UNIONISED AMMONIA CONCENTRATION",1.0f-saturate(s.ammoniaMgL/1.45f)},
            {"PRESSURE",fnum(s.pressureKPa,1)+" KPA","HYDROSTATIC PIPE PRESSURE",1.0f},
            {"PH LEVEL",fnum(s.pH,2),"WATER ACIDITY AND ALKALINITY",1.0f-saturate(std::fabs(s.pH-7.4f)/1.55f)},
            {"DISSOLVED O2",fnum(s.dissolvedOxygenMgL,2)+" MG/L","OXYGEN AVAILABLE TO THE FISH",saturate((s.dissolvedOxygenMgL-3.0f)/4.2f)},
            {"WATER LEVEL",fnum(s.waterLevelM,2)+" M","CURRENT POND WATER DEPTH",1.0f-saturate(std::fabs(s.waterLevelM-1.18f)/0.35f)}
        };
        glm::vec4 qc=s.quality01>0.65f?green:(s.quality01>0.38f?amber:red);
        std::string qv=fnum(s.quality01*100,1)+"%  "+(s.quality01>0.65f?"GOOD":(s.quality01>0.38f?"WATCH":"POOR"));
        std::string liveState=sim_.circulation01()>0.10f?"CIRCULATION STABLE":"CIRCULATION STOPPED";

        if(!sensorHudExpanded_){
            float rw=324.0f,rx=(float)width_-rw-20.0f,ry=82.0f,rh=430.0f;
            card(rx,ry,rw,rh);
            ui_->roundedRect(rx+14,ry+15,34,3,1.5f,cyan);
            ui_->text(rx+58,ry+7,1.45f,"POND 2 TELEMETRY",text);
            ui_->text(rx+14,ry+35,0.92f,"LIVE SENSOR NETWORK - 6 CHANNELS",muted);
            float yy=ry+67;
            for(size_t i=0;i<rows.size();++i){
                float score=rows[i].score; glm::vec4 status=score>0.66f?green:(score>0.38f?amber:red);
                HudRect row{rx+14,yy+(float)i*45.0f,rw-28,37};
                ui_->roundedRect(row.x,row.y,row.w,row.h,7,{0.018f,0.060f,0.078f,0.58f});
                ui_->circle(row.x+12,row.y+18.5f,4.2f,status,14);
                ui_->text(row.x+23,row.y+9,1.12f,rows[i].n,muted);
                float vw=ui_->textWidth(1.12f,rows[i].v);
                ui_->text(row.x+row.w-vw-11,row.y+9,1.12f,rows[i].v,text);
            }
            float qy=ry+347;
            ui_->text(rx+14,qy,1.05f,"WATER QUALITY INDEX",muted);
            ui_->roundedRect(rx+14,qy+21,rw-28,12,6,{0.015f,0.050f,0.065f,0.62f});
            ui_->roundedRect(rx+14,qy+21,(rw-28)*s.quality01,12,6,qc);
            ui_->text(rx+14,qy+44,1.20f,qv,qc);
            float lw=ui_->textWidth(0.82f,liveState);
            ui_->text(rx+rw-lw-14,qy+45,0.82f,liveState,sim_.circulation01()>0.10f?cyan:amber);
            std::string expandHint="B - EXPAND SENSOR DETAILS";
            float hw=ui_->textWidth(0.78f,expandHint);
            ui_->text(rx+rw-hw-14,ry+rh-18,0.78f,expandHint,cyan);
        } else {
            float rw=std::min(600.0f,(float)width_-40.0f);
            float rh=std::min(820.0f,(float)height_-80.0f);
            float rx=(float)width_-rw-30.0f;
            float ry=((float)height_-rh)*0.5f;
            card(rx,ry,rw,rh);
            ui_->roundedRect(rx+22,ry+22,52,4,2.0f,cyan);
            ui_->text(rx+90,ry+12,1.82f,"POND 2 SENSOR DETAILS",text);
            ui_->text(rx+22,ry+46,1.04f,"LIVE SENSOR NETWORK - 6 CHANNELS",muted);
            std::string closeHint="PRESS B TO RETURN";
            float chw=ui_->textWidth(1.02f,closeHint);
            ui_->text(rx+rw-chw-22,ry+46,1.02f,closeHint,cyan);

            float yy=ry+82.0f;
            float rowStep=clampf((rh-232.0f)/6.0f,68.0f,88.0f);
            float rowHeight=rowStep-8.0f;
            for(size_t i=0;i<rows.size();++i){
                float score=rows[i].score; glm::vec4 statusColor=score>0.66f?green:(score>0.38f?amber:red);
                std::string statusText=score>0.66f?"NORMAL":(score>0.38f?"WATCH":"ALERT");
                HudRect row{rx+22,yy+(float)i*rowStep,rw-44,rowHeight};
                ui_->roundedRect(row.x,row.y,row.w,row.h,9,{0.018f,0.060f,0.078f,0.62f});
                ui_->circle(row.x+17,row.y+19,5.0f,statusColor,16);
                ui_->text(row.x+32,row.y+10,1.42f,rows[i].n,text);
                float vw=ui_->textWidth(1.52f,rows[i].v);
                ui_->text(row.x+row.w-vw-18,row.y+9,1.52f,rows[i].v,statusColor);
                ui_->text(row.x+32,row.y+38,0.98f,rows[i].detail,muted);
                float sw=ui_->textWidth(0.98f,statusText);
                ui_->text(row.x+row.w-sw-18,row.y+38,0.98f,statusText,statusColor);
            }

            float qy=yy+6.0f*rowStep+6.0f;
            ui_->text(rx+22,qy,1.30f,"OVERALL WATER QUALITY INDEX",text);
            ui_->roundedRect(rx+22,qy+30,rw-44,18,9,{0.015f,0.050f,0.065f,0.68f});
            ui_->roundedRect(rx+22,qy+30,(rw-44)*s.quality01,18,9,qc);
            ui_->text(rx+22,qy+61,1.55f,qv,qc);
            float lw=ui_->textWidth(1.04f,liveState);
            ui_->text(rx+rw-lw-22,qy+65,1.04f,liveState,sim_.circulation01()>0.10f?cyan:amber);
            ui_->text(rx+22,qy+94,0.92f,"VALUES UPDATE LIVE WITH DAY NIGHT AND CIRCULATION CONDITIONS",muted);
        }
    }

    ui_->flush();
}

void App::frame(){
    if(glfwWindowShouldClose(window_)){
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        return;
    }
    double now=glfwGetTime(); float dt=(float)(now-lastTime_); lastTime_=now;
    if(dt<=0||dt>0.25f) dt=1.0f/60.0f;
    fpsSmooth_=lerpf(fpsSmooth_,1.0f/dt,1.0f-std::exp(-dt*3.0f));
    processInput(dt);
    if(!paused_) sim_.update(dt*simSpeed_);
    scene_->render(sim_,camera_);
    drawUI();
    glfwSwapBuffers(window_); glfwPollEvents();
}

void App::run(){
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg([](void* p){ static_cast<App*>(p)->frame(); },this,0,1);
#else
    while(!glfwWindowShouldClose(window_)) frame();
#endif
}

} // namespace aqua
