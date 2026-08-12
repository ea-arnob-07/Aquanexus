#include "App.hpp"

int main(){
    try{
        aqua::App app(1600,960);
        app.run();
    }catch(const std::exception& e){
        std::cerr<<"AquaVillage Cinematic fatal error:\n"<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
