#include "Mesh.hpp"
#include <cstddef>

namespace aqua {

Mesh::Mesh(const std::vector<Vertex>& v, const std::vector<uint32_t>& i) { upload(v, i); }
Mesh::~Mesh() { destroy(); }
Mesh::Mesh(Mesh&& o) noexcept : vao_(o.vao_), vbo_(o.vbo_), ebo_(o.ebo_), indexCount_(o.indexCount_) {
    o.vao_=o.vbo_=o.ebo_=0; o.indexCount_=0;
}
Mesh& Mesh::operator=(Mesh&& o) noexcept {
    if (this != &o) {
        destroy(); vao_=o.vao_; vbo_=o.vbo_; ebo_=o.ebo_; indexCount_=o.indexCount_;
        o.vao_=o.vbo_=o.ebo_=0; o.indexCount_=0;
    }
    return *this;
}
void Mesh::destroy() {
    if (ebo_) glDeleteBuffers(1,&ebo_);
    if (vbo_) glDeleteBuffers(1,&vbo_);
    if (vao_) glDeleteVertexArrays(1,&vao_);
    vao_=vbo_=ebo_=0; indexCount_=0;
}
void Mesh::upload(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    destroy();
    glGenVertexArrays(1,&vao_); glGenBuffers(1,&vbo_); glGenBuffers(1,&ebo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER,vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size()*sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size()*sizeof(uint32_t)), indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,position));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,normal));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,uv));
    glBindVertexArray(0); indexCount_=static_cast<GLsizei>(indices.size());
}
void Mesh::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES,indexCount_,GL_UNSIGNED_INT,nullptr);
    glBindVertexArray(0);
}

Mesh Mesh::cube() {
    std::vector<Vertex> v;
    std::vector<uint32_t> i;
    const glm::vec3 p[8] = {
        {-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f},
        {-0.5f,-0.5f,0.5f},{0.5f,-0.5f,0.5f},{0.5f,0.5f,0.5f},{-0.5f,0.5f,0.5f}
    };
    struct F { int a,b,c,d; glm::vec3 n; };
    const F f[6] = {
        {4,5,6,7,{0,0,1}}, {1,0,3,2,{0,0,-1}}, {0,4,7,3,{-1,0,0}},
        {5,1,2,6,{1,0,0}}, {3,7,6,2,{0,1,0}}, {0,1,5,4,{0,-1,0}}
    };
    for (const auto& q : f) {
        uint32_t b = (uint32_t)v.size();
        v.push_back({p[q.a],q.n,{0,0}}); v.push_back({p[q.b],q.n,{1,0}});
        v.push_back({p[q.c],q.n,{1,1}}); v.push_back({p[q.d],q.n,{0,1}});
        i.insert(i.end(),{b,b+1,b+2,b,b+2,b+3});
    }
    return Mesh(v,i);
}

Mesh Mesh::plane(int d) {
    d = std::max(1,d);
    std::vector<Vertex> v; std::vector<uint32_t> i;
    for(int z=0; z<=d; ++z) for(int x=0; x<=d; ++x) {
        float fx=(float)x/d, fz=(float)z/d;
        v.push_back({{fx-0.5f,0.0f,fz-0.5f},{0,1,0},{fx,fz}});
    }
    int row=d+1;
    for(int z=0; z<d; ++z) for(int x=0; x<d; ++x) {
        uint32_t a=z*row+x,b=a+1,c=a+row,d0=c+1;
        i.insert(i.end(),{a,c,b,b,c,d0});
    }
    return Mesh(v,i);
}

Mesh Mesh::uvSphere(int slices, int stacks) {
    slices=std::max(8,slices); stacks=std::max(4,stacks);
    std::vector<Vertex> v; std::vector<uint32_t> i;
    for(int y=0;y<=stacks;++y){
        float fy=(float)y/stacks; float phi=fy*PI;
        for(int x=0;x<=slices;++x){
            float fx=(float)x/slices; float th=fx*2.0f*PI;
            glm::vec3 n(std::sin(phi)*std::cos(th), std::cos(phi), std::sin(phi)*std::sin(th));
            v.push_back({n*0.5f,n,{fx,fy}});
        }
    }
    int row=slices+1;
    for(int y=0;y<stacks;++y) for(int x=0;x<slices;++x){
        uint32_t a=y*row+x,b=a+1,c=a+row,d=c+1;
        i.insert(i.end(),{a,c,b,b,c,d});
    }
    return Mesh(v,i);
}

Mesh Mesh::cylinder(int segments, bool capped) {
    segments=std::max(8,segments);
    std::vector<Vertex> v; std::vector<uint32_t> i;
    for(int s=0;s<=segments;++s){
        float u=(float)s/segments; float a=u*2.0f*PI; float c=std::cos(a), sn=std::sin(a);
        glm::vec3 n(c,sn,0);
        v.push_back({{c,sn,-0.5f},n,{u,0}});
        v.push_back({{c,sn, 0.5f},n,{u,1}});
    }
    for(int s=0;s<segments;++s){
        uint32_t a=s*2,b=a+1,c=a+2,d=a+3;
        i.insert(i.end(),{a,c,b,b,c,d});
    }
    if(capped){
        uint32_t base=(uint32_t)v.size();
        v.push_back({{0,0,-0.5f},{0,0,-1},{0.5f,0.5f}});
        v.push_back({{0,0, 0.5f},{0,0, 1},{0.5f,0.5f}});
        for(int s=0;s<=segments;++s){
            float u=(float)s/segments; float a=u*2.0f*PI; float c=std::cos(a),sn=std::sin(a);
            v.push_back({{c,sn,-0.5f},{0,0,-1},{c*0.5f+0.5f,sn*0.5f+0.5f}});
            v.push_back({{c,sn, 0.5f},{0,0, 1},{c*0.5f+0.5f,sn*0.5f+0.5f}});
        }
        for(int s=0;s<segments;++s){
            uint32_t r0=base+2+s*2, r1=r0+2;
            i.insert(i.end(),{base,r1,r0});
            i.insert(i.end(),{base+1,r0+1,r1+1});
        }
    }
    return Mesh(v,i);
}

Mesh Mesh::cone(int segments) {
    segments=std::max(8,segments);
    std::vector<Vertex> v; std::vector<uint32_t> i;
    for(int s=0;s<=segments;++s){
        float u=(float)s/segments; float a=u*2.0f*PI; float c=std::cos(a),sn=std::sin(a);
        glm::vec3 n=glm::normalize(glm::vec3(c,sn,1.0f));
        v.push_back({{c,sn,-0.5f},n,{u,0}});
        v.push_back({{0,0,0.5f},n,{u,1}});
    }
    for(int s=0;s<segments;++s){ uint32_t a=s*2; i.insert(i.end(),{a,a+2,a+1}); }
    uint32_t center=(uint32_t)v.size(); v.push_back({{0,0,-0.5f},{0,0,-1},{0.5f,0.5f}});
    for(int s=0;s<segments;++s){ uint32_t a=s*2,b=(s+1)*2; i.insert(i.end(),{center,b,a}); }
    return Mesh(v,i);
}

Mesh Mesh::trapezoidPrism() {
    // Local X is length, Y is height, Z is bank thickness.
    // The top is narrower than the bottom to create a natural sloped pond embankment.
    const float zb=0.50f, zt=0.30f;
    const glm::vec3 p[8]={
        {-0.5f,-0.5f,-zb},{0.5f,-0.5f,-zb},{0.5f,-0.5f,zb},{-0.5f,-0.5f,zb},
        {-0.5f, 0.5f,-zt},{0.5f, 0.5f,-zt},{0.5f, 0.5f,zt},{-0.5f, 0.5f,zt}
    };
    std::vector<Vertex> v; std::vector<uint32_t> idx;
    auto face=[&](int a,int b,int c,int d){
        glm::vec3 n=glm::normalize(glm::cross(p[b]-p[a],p[c]-p[a]));
        uint32_t base=(uint32_t)v.size();
        v.push_back({p[a],n,{0,0}}); v.push_back({p[b],n,{1,0}}); v.push_back({p[c],n,{1,1}}); v.push_back({p[d],n,{0,1}});
        idx.insert(idx.end(),{base,base+1,base+2,base,base+2,base+3});
    };
    face(0,1,5,4); // -Z slope
    face(2,3,7,6); // +Z slope
    face(3,0,4,7); // -X end
    face(1,2,6,5); // +X end
    face(4,5,6,7); // top
    face(3,2,1,0); // bottom
    return Mesh(v,idx);
}

Mesh Mesh::leafBlade(int segments) {
    segments=std::max(4,segments);
    std::vector<Vertex> v; std::vector<uint32_t> idx;
    auto addSide=[&](bool back){
        uint32_t base=(uint32_t)v.size();
        for(int i=0;i<=segments;++i){
            float t=(float)i/segments;
            float z=t-0.08f;
            float width=(0.10f+0.90f*std::sin(PI*t))*0.5f;
            float y=-0.18f*t*t-0.015f*std::sin(t*PI*2.0f);
            glm::vec3 n=back?glm::vec3(0,-1,0):glm::vec3(0,1,0);
            v.push_back({{-width,y,z},n,{0,t}});
            v.push_back({{ width,y,z},n,{1,t}});
        }
        for(int i=0;i<segments;++i){
            uint32_t a=base+i*2,b=a+1,c=a+2,d=a+3;
            if(!back) idx.insert(idx.end(),{a,c,b,b,c,d});
            else idx.insert(idx.end(),{a,b,c,b,d,c});
        }
    };
    addSide(false); addSide(true);
    return Mesh(v,idx);
}

Mesh Mesh::disc(int segments) {
    segments=std::max(8,segments);
    std::vector<Vertex> v; std::vector<uint32_t> idx;
    v.push_back({{0,0,0},{0,1,0},{0.5f,0.5f}});
    for(int i=0;i<=segments;++i){
        float a=(float)i/segments*2.0f*PI;
        float x=std::cos(a)*0.5f,z=std::sin(a)*0.5f;
        v.push_back({{x,0,z},{0,1,0},{x+0.5f,z+0.5f}});
    }
    for(int i=0;i<segments;++i) idx.insert(idx.end(),{0u,(uint32_t)i+1,(uint32_t)i+2});
    return Mesh(v,idx);
}


Mesh Mesh::roundedPondBank(int perimeterSegments) {
    // A shared high-detail earthen basin. X/Z are normalized by pond size at draw time.
    // Five contour rings form eroded outer slope -> crest -> wet inner slope.
    perimeterSegments = std::max(48, perimeterSegments);
    struct Profile { float radius; float y; };
    const Profile profile[] = {
        {0.610f,-0.40f}, {0.578f,0.24f}, {0.544f,0.82f},
        {0.503f,0.80f}, {0.448f,-0.94f}
    };
    constexpr int rings = int(sizeof(profile)/sizeof(profile[0]));
    std::vector<Vertex> v;
    std::vector<uint32_t> idx;
    v.reserve((rings-1)*perimeterSegments*4);
    idx.reserve((rings-1)*perimeterSegments*6);

    auto squircle=[&](float a,float r,int k)->glm::vec3{
        float c=std::cos(a), sn=std::sin(a);
        constexpr float exponent=0.42f; // superellipse: rounded rectangle, not a perfect circle
        float x=(c<0?-1.0f:1.0f)*std::pow(std::fabs(c),exponent);
        float z=(sn<0?-1.0f:1.0f)*std::pow(std::fabs(sn),exponent);
        float erosion=1.0f + 0.010f*std::sin(a*7.0f+0.9f*k) + 0.006f*std::sin(a*17.0f+1.7f*k);
        return {x*r*erosion, profile[k].y + 0.010f*std::sin(a*11.0f+k), z*r*erosion};
    };

    for(int k=0;k<rings-1;++k){
        for(int j=0;j<perimeterSegments;++j){
            float a0=2.0f*PI*j/perimeterSegments;
            float a1=2.0f*PI*(j+1)/perimeterSegments;
            glm::vec3 p00=squircle(a0,profile[k].radius,k);
            glm::vec3 p01=squircle(a1,profile[k].radius,k);
            glm::vec3 p11=squircle(a1,profile[k+1].radius,k+1);
            glm::vec3 p10=squircle(a0,profile[k+1].radius,k+1);
            glm::vec3 n=glm::normalize(glm::cross(p01-p00,p10-p00));
            if(n.y< -0.85f) n=-n;
            uint32_t b=(uint32_t)v.size();
            float u0=(float)j/perimeterSegments,u1=(float)(j+1)/perimeterSegments;
            v.push_back({p00,n,{u0,(float)k/(rings-1)}});
            v.push_back({p01,n,{u1,(float)k/(rings-1)}});
            v.push_back({p11,n,{u1,(float)(k+1)/(rings-1)}});
            v.push_back({p10,n,{u0,(float)(k+1)/(rings-1)}});
            idx.insert(idx.end(),{b,b+1,b+2,b,b+2,b+3});
        }
    }
    return Mesh(v,idx);
}

Mesh Mesh::roundedWaterSurface(int perimeterSegments,int radialSegments,float radius) {
    // Match the pond-bank superellipse instead of using a short rectangular plane.
    // The radial topology gives the water vertex shader enough subdivisions for waves,
    // while the rounded perimeter fills both pond ends without leaking through corners.
    perimeterSegments=std::max(48,perimeterSegments);
    radialSegments=std::max(8,radialSegments);
    radius=clampf(radius,0.46f,0.50f);

    std::vector<Vertex> v;
    std::vector<uint32_t> idx;
    v.reserve(1+(size_t)perimeterSegments*(size_t)radialSegments);
    idx.reserve((size_t)perimeterSegments*(3u+(size_t)(radialSegments-1)*6u));
    v.push_back({{0,0,0},{0,1,0},{0.5f,0.5f}});

    constexpr float exponent=0.42f;
    for(int ring=1;ring<=radialSegments;++ring){
        float rr=radius*(float)ring/(float)radialSegments;
        for(int j=0;j<perimeterSegments;++j){
            float a=2.0f*PI*(float)j/(float)perimeterSegments;
            float c=std::cos(a),sn=std::sin(a);
            float x=(c<0?-1.0f:1.0f)*std::pow(std::fabs(c),exponent)*rr;
            float z=(sn<0?-1.0f:1.0f)*std::pow(std::fabs(sn),exponent)*rr;
            glm::vec2 uv(x/radius*0.5f+0.5f,z/radius*0.5f+0.5f);
            v.push_back({{x,0,z},{0,1,0},uv});
        }
    }

    for(int j=0;j<perimeterSegments;++j){
        uint32_t current=1u+(uint32_t)j;
        uint32_t next=1u+(uint32_t)((j+1)%perimeterSegments);
        idx.insert(idx.end(),{0u,next,current});
    }
    for(int ring=1;ring<radialSegments;++ring){
        uint32_t inner=1u+(uint32_t)(ring-1)*perimeterSegments;
        uint32_t outer=inner+(uint32_t)perimeterSegments;
        for(int j=0;j<perimeterSegments;++j){
            uint32_t j1=(uint32_t)((j+1)%perimeterSegments);
            uint32_t a=inner+(uint32_t)j,b=inner+j1,c=outer+(uint32_t)j,d=outer+j1;
            idx.insert(idx.end(),{a,b,c,b,d,c});
        }
    }
    return Mesh(v,idx);
}

Mesh Mesh::irregularRock(int slices, int stacks) {
    slices=std::max(10,slices); stacks=std::max(6,stacks);
    std::vector<Vertex> v; std::vector<uint32_t> idx;
    for(int y=0;y<=stacks;++y){
        float fy=(float)y/stacks, phi=fy*PI;
        for(int x=0;x<=slices;++x){
            float fx=(float)x/slices, th=fx*2.0f*PI;
            glm::vec3 n0(std::sin(phi)*std::cos(th),std::cos(phi),std::sin(phi)*std::sin(th));
            float warp=0.82f + 0.10f*std::sin(th*3.0f+phi*2.0f) + 0.055f*std::sin(th*7.0f-phi*5.0f);
            glm::vec3 p=n0*0.5f*warp;
            p.y*=0.72f;
            glm::vec3 n=glm::normalize(glm::vec3(n0.x,n0.y/0.72f,n0.z));
            v.push_back({p,n,{fx,fy}});
        }
    }
    int row=slices+1;
    for(int y=0;y<stacks;++y) for(int x=0;x<slices;++x){
        uint32_t a=y*row+x,b=a+1,c=a+row,d=c+1;
        idx.insert(idx.end(),{a,c,b,b,c,d});
    }
    return Mesh(v,idx);
}

Mesh Mesh::grassPatch(int bladeCount, uint32_t seed) {
    bladeCount=std::max(64,bladeCount);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(0.0f,1.0f);
    std::vector<Vertex> v; std::vector<uint32_t> idx;
    v.reserve((size_t)bladeCount*8); idx.reserve((size_t)bladeCount*24);
    for(int b=0;b<bladeCount;++b){
        float cx=u(rng)-0.5f, cz=u(rng)-0.5f;
        float h=0.30f+u(rng)*0.58f;
        float w=0.0028f+u(rng)*0.0048f;
        float a=u(rng)*PI;
        for(int cross=0;cross<2;++cross){
            float ang=a+cross*PI*0.5f;
            glm::vec3 side(std::cos(ang)*w,0,std::sin(ang)*w);
            glm::vec3 bend(std::cos(ang+1.2f)*w*2.2f,0,std::sin(ang+1.2f)*w*2.2f);
            glm::vec3 p0(cx,0,cz), p1(cx,h,cz);
            glm::vec3 normal=glm::normalize(glm::vec3(-std::sin(ang),0.18f,std::cos(ang)));
            uint32_t base=(uint32_t)v.size();
            v.push_back({p0-side, normal,{0,0}});
            v.push_back({p0+side, normal,{1,0}});
            v.push_back({p1+side*0.15f+bend, normal,{1,1}});
            v.push_back({p1-side*0.15f+bend, normal,{0,1}});
            idx.insert(idx.end(),{base,base+1,base+2,base,base+2,base+3,
                                  base+2,base+1,base,base+3,base+2,base});
        }
    }
    return Mesh(v,idx);
}

} // namespace aqua
