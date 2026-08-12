#include "App.hpp"

int main(){
    try{
#ifdef __EMSCRIPTEN__
        auto* app = new aqua::App(1366,768);
#else
        auto* app = new aqua::App(1600,960);
#endif
        app->run();
    }catch(const std::exception& e){
        std::cerr<<"AquaVillage Cinematic fatal error:\n"<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
