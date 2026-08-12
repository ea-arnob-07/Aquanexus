#pragma once
#include "Common.hpp"
#include "Shader.hpp"

namespace aqua {

struct UIVertex { glm::vec2 pos; glm::vec4 color; };

class UI {
public:
    UI();
    ~UI();
    UI(const UI&) = delete;
    UI& operator=(const UI&) = delete;

    void begin(int width, int height);
    void rect(float x, float y, float w, float h, const glm::vec4& c);
    void gradientRect(float x,float y,float w,float h,const glm::vec4& top,const glm::vec4& bottom);
    void roundedRect(float x,float y,float w,float h,float radius,const glm::vec4& c);
    void circle(float cx,float cy,float radius,const glm::vec4& c,int segments=20);
    void rectOutline(float x, float y, float w, float h, float thickness, const glm::vec4& c);
    void text(float x, float y, float scale, const std::string& s, const glm::vec4& c, bool shadow=true);
    float textWidth(float scale, const std::string& s) const;
    void flush();

private:
    GLuint vao_=0,vbo_=0;
    Shader shader_;
    std::vector<UIVertex> verts_;
    int width_=1,height_=1;
    static std::array<uint8_t,7> glyph(char c);
    void quad(float x0,float y0,float x1,float y1,const glm::vec4& c);
    void triangle(const glm::vec2& a,const glm::vec2& b,const glm::vec2& c,const glm::vec4& color);
};

inline bool pointInRect(double mx,double my,float x,float y,float w,float h){
    return mx>=x && mx<=x+w && my>=y && my<=y+h;
}

} // namespace aqua
