#include "UI.hpp"
#include <cctype>

namespace aqua {

UI::UI() : shader_(Shader::fromFiles("shaders/ui.vert","shaders/ui.frag")) {
    glGenVertexArrays(1,&vao_); glGenBuffers(1,&vbo_);
    glBindVertexArray(vao_); glBindBuffer(GL_ARRAY_BUFFER,vbo_);
    glBufferData(GL_ARRAY_BUFFER, 1024*sizeof(UIVertex), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(UIVertex),(void*)offsetof(UIVertex,pos));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(UIVertex),(void*)offsetof(UIVertex,color));
    glBindVertexArray(0);
}
UI::~UI(){ if(vbo_)glDeleteBuffers(1,&vbo_); if(vao_)glDeleteVertexArrays(1,&vao_); }
void UI::begin(int w,int h){ width_=std::max(1,w); height_=std::max(1,h); verts_.clear(); }
void UI::quad(float x0,float y0,float x1,float y1,const glm::vec4& c){
    verts_.push_back({{x0,y0},c}); verts_.push_back({{x1,y0},c}); verts_.push_back({{x1,y1},c});
    verts_.push_back({{x0,y0},c}); verts_.push_back({{x1,y1},c}); verts_.push_back({{x0,y1},c});
}
void UI::rect(float x,float y,float w,float h,const glm::vec4& c){ quad(x,y,x+w,y+h,c); }
void UI::triangle(const glm::vec2& a,const glm::vec2& b,const glm::vec2& c,const glm::vec4& color){
    verts_.push_back({a,color}); verts_.push_back({b,color}); verts_.push_back({c,color});
}
void UI::gradientRect(float x,float y,float w,float h,const glm::vec4& top,const glm::vec4& bottom){
    verts_.push_back({{x,y},top}); verts_.push_back({{x+w,y},top}); verts_.push_back({{x+w,y+h},bottom});
    verts_.push_back({{x,y},top}); verts_.push_back({{x+w,y+h},bottom}); verts_.push_back({{x,y+h},bottom});
}
void UI::circle(float cx,float cy,float radius,const glm::vec4& c,int segments){
    segments=std::max(8,segments);
    for(int i=0;i<segments;++i){
        float a0=2.0f*PI*(float)i/(float)segments,a1=2.0f*PI*(float)(i+1)/(float)segments;
        triangle({cx,cy},{cx+std::cos(a0)*radius,cy+std::sin(a0)*radius},{cx+std::cos(a1)*radius,cy+std::sin(a1)*radius},c);
    }
}
void UI::roundedRect(float x,float y,float w,float h,float radius,const glm::vec4& c){
    float r=clampf(radius,0.0f,std::min(w,h)*0.5f);
    if(r<0.5f){ rect(x,y,w,h,c); return; }
    rect(x+r,y,w-2.0f*r,h,c);
    rect(x,y+r,r,h-2.0f*r,c);
    rect(x+w-r,y+r,r,h-2.0f*r,c);
    const glm::vec2 centers[4]={{x+r,y+r},{x+w-r,y+r},{x+w-r,y+h-r},{x+r,y+h-r}};
    const float starts[4]={PI,1.5f*PI,0.0f,0.5f*PI};
    constexpr int seg=6;
    for(int corner=0;corner<4;++corner){
        for(int i=0;i<seg;++i){
            float a0=starts[corner]+0.5f*PI*(float)i/(float)seg;
            float a1=starts[corner]+0.5f*PI*(float)(i+1)/(float)seg;
            triangle(centers[corner],centers[corner]+glm::vec2(std::cos(a0),std::sin(a0))*r,
                     centers[corner]+glm::vec2(std::cos(a1),std::sin(a1))*r,c);
        }
    }
}
void UI::rectOutline(float x,float y,float w,float h,float t,const glm::vec4& c){
    rect(x,y,w,t,c); rect(x,y+h-t,w,t,c); rect(x,y,t,h,c); rect(x+w-t,y,t,h,c);
}

std::array<uint8_t,7> UI::glyph(char ch){
    char c=(char)std::toupper((unsigned char)ch);
    switch(c){
        case 'A': return {14,17,17,31,17,17,17};
        case 'B': return {30,17,17,30,17,17,30};
        case 'C': return {14,17,16,16,16,17,14};
        case 'D': return {30,17,17,17,17,17,30};
        case 'E': return {31,16,16,30,16,16,31};
        case 'F': return {31,16,16,30,16,16,16};
        case 'G': return {14,17,16,23,17,17,15};
        case 'H': return {17,17,17,31,17,17,17};
        case 'I': return {31,4,4,4,4,4,31};
        case 'J': return {7,2,2,2,18,18,12};
        case 'K': return {17,18,20,24,20,18,17};
        case 'L': return {16,16,16,16,16,16,31};
        case 'M': return {17,27,21,21,17,17,17};
        case 'N': return {17,25,21,19,17,17,17};
        case 'O': return {14,17,17,17,17,17,14};
        case 'P': return {30,17,17,30,16,16,16};
        case 'Q': return {14,17,17,17,21,18,13};
        case 'R': return {30,17,17,30,20,18,17};
        case 'S': return {15,16,16,14,1,1,30};
        case 'T': return {31,4,4,4,4,4,4};
        case 'U': return {17,17,17,17,17,17,14};
        case 'V': return {17,17,17,17,17,10,4};
        case 'W': return {17,17,17,21,21,21,10};
        case 'X': return {17,17,10,4,10,17,17};
        case 'Y': return {17,17,10,4,4,4,4};
        case 'Z': return {31,1,2,4,8,16,31};
        case '0': return {14,17,19,21,25,17,14};
        case '1': return {4,12,4,4,4,4,14};
        case '2': return {14,17,1,2,4,8,31};
        case '3': return {30,1,1,14,1,1,30};
        case '4': return {2,6,10,18,31,2,2};
        case '5': return {31,16,16,30,1,1,30};
        case '6': return {14,16,16,30,17,17,14};
        case '7': return {31,1,2,4,8,8,8};
        case '8': return {14,17,17,14,17,17,14};
        case '9': return {14,17,17,15,1,1,14};
        case '.': return {0,0,0,0,0,12,12};
        case ':': return {0,12,12,0,12,12,0};
        case '-': return {0,0,0,31,0,0,0};
        case '+': return {0,4,4,31,4,4,0};
        case '/': return {1,2,2,4,8,8,16};
        case '%': return {17,2,4,8,17,0,0};
        case '(': return {2,4,8,8,8,4,2};
        case ')': return {8,4,2,2,2,4,8};
        case '[': return {14,8,8,8,8,8,14};
        case ']': return {14,2,2,2,2,2,14};
        case '<': return {1,2,4,8,4,2,1};
        case '>': return {16,8,4,2,4,8,16};
        case '=': return {0,0,31,0,31,0,0};
        case '_': return {0,0,0,0,0,0,31};
        case '!': return {4,4,4,4,4,0,4};
        case '?': return {14,17,1,2,4,0,4};
        case '#': return {10,31,10,10,31,10,0};
        case ' ': default: return {0,0,0,0,0,0,0};
    }
}

float UI::textWidth(float scale,const std::string& s) const { return s.empty()?0.0f:(float)s.size()*6.0f*scale-scale; }

void UI::text(float x,float y,float scale,const std::string& s,const glm::vec4& c,bool shadow){
    auto draw=[&](float ox,float oy,const glm::vec4& col){
        float px=x+ox;
        for(char ch:s){
            auto g=glyph(ch);
            for(int row=0;row<7;++row) for(int colx=0;colx<5;++colx){
                if(g[row] & (1u<<(4-colx))) quad(px+colx*scale,y+oy+row*scale,px+(colx+1)*scale,y+oy+(row+1)*scale,col);
            }
            px+=6.0f*scale;
        }
    };
    if(shadow) draw(scale*0.75f,scale*0.75f,glm::vec4(0,0,0,c.a*0.58f));
    draw(0,0,c);
}

void UI::flush(){
    if(verts_.empty()) return;
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    shader_.use(); shader_.set("uScreen",glm::vec2((float)width_,(float)height_));
    glBindVertexArray(vao_); glBindBuffer(GL_ARRAY_BUFFER,vbo_);
    glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(verts_.size()*sizeof(UIVertex)),verts_.data(),GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES,0,(GLsizei)verts_.size());
    glBindVertexArray(0); glDisable(GL_BLEND); glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST);
}

} // namespace aqua
