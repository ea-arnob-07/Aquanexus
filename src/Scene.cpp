#include "Scene.hpp"

namespace aqua {

static glm::mat4 trs(const glm::vec3& p,const glm::vec3& s){
    return glm::translate(glm::mat4(1.0f),p)*glm::scale(glm::mat4(1.0f),s);
}

SceneRenderer::SceneRenderer(int w,int h)
    : width_(std::max(1,w)),height_(std::max(1,h)),
      cube_(Mesh::cube()),plane_(Mesh::plane(1)),waterPlane_(Mesh::roundedWaterSurface(128,40,0.490f)),
      sphere_(Mesh::uvSphere(30,18)),cylinder_(Mesh::cylinder(32,true)),cone_(Mesh::cone(28)),
      trapezoid_(Mesh::trapezoidPrism()),leaf_(Mesh::leafBlade(14)),disc_(Mesh::disc(40)),screenQuad_(Mesh::plane(1)),
      pondBank_(Mesh::roundedPondBank(112)),rock_(Mesh::irregularRock(20,12)),
      grassPatch_(Mesh::grassPatch(2200,1337u)),ricePatch_(Mesh::grassPatch(1800,7331u)),
      pbr_(Shader::fromFiles("shaders/pbr.vert","shaders/pbr.frag")),
      shadow_(Shader::fromFiles("shaders/shadow.vert","shaders/shadow.frag")),
      sky_(Shader::fromFiles("shaders/sky.vert","shaders/sky.frag")),
      water_(Shader::fromFiles("shaders/water.vert","shaders/water.frag")),
      glass_(Shader::fromFiles("shaders/glass.vert","shaders/glass.frag")),
      bright_(Shader::fromFiles("shaders/bright.vert","shaders/bright.frag")),
      blur_(Shader::fromFiles("shaders/blur.vert","shaders/blur.frag")),
      post_(Shader::fromFiles("shaders/post.vert","shaders/post.frag")) {
    initWorld();
    initShadowFBO();
    recreateSceneFBOs();
}

SceneRenderer::~SceneRenderer(){
    destroyFBOs();
    if(shadowTex_) glDeleteTextures(1,&shadowTex_);
    if(shadowFBO_) glDeleteFramebuffers(1,&shadowFBO_);
}

void SceneRenderer::resize(int w,int h){
    w=std::max(1,w); h=std::max(1,h);
    width_=w; height_=h;
    if(w!=fboW_||h!=fboH_) recreateSceneFBOs();
}

void SceneRenderer::setNightMode(bool enabled){
    nightMode_=enabled;
    if(nightMode_){
        // Cool moonlight keeps the scene readable while remaining much dimmer than daylight.
        sunDir_=glm::normalize(glm::vec3(-0.18f,-0.83f,0.53f));
        sunColor_=glm::vec3(0.72f,0.92f,1.42f);
    } else {
        sunDir_=glm::vec3(-0.40f,-0.87f,-0.28f);
        sunColor_=glm::vec3(5.1f,4.55f,3.70f);
    }
}

Mesh SceneRenderer::makeTube(const PipePath& path,float radius,int rings,int sides){
    rings=std::max(8,rings); sides=std::max(8,sides);
    std::vector<Vertex> v; std::vector<uint32_t> idx;
    v.reserve((rings+1)*(sides+1));
    glm::vec3 prevX(1,0,0);
    for(int r=0;r<=rings;++r){
        float t=(float)r/rings;
        glm::vec3 c=path.point(t), z=path.tangent(t);
        glm::vec3 helper=std::fabs(z.y)>0.92f?glm::vec3(1,0,0):glm::vec3(0,1,0);
        glm::vec3 x=glm::normalize(glm::cross(helper,z));
        if(r>0 && glm::dot(x,prevX)<0.0f) x=-x;
        glm::vec3 y=glm::normalize(glm::cross(z,x)); prevX=x;
        for(int s=0;s<=sides;++s){
            float u=(float)s/sides; float a=u*2.0f*PI;
            glm::vec3 n=std::cos(a)*x+std::sin(a)*y;
            v.push_back({c+n*radius,n,{u,t}});
        }
    }
    int row=sides+1;
    for(int r=0;r<rings;++r) for(int s=0;s<sides;++s){
        uint32_t a=r*row+s,b=a+1,c=a+row,d=c+1;
        idx.insert(idx.end(),{a,c,b,b,c,d});
    }
    return Mesh(v,idx);
}

glm::vec2 SceneRenderer::pondWaterHalfSize(int index) const {
    static const glm::vec2 halfSizes[3] = {
        // Slightly larger exclusion footprint keeps grass/trees from spilling into the pond banks.
        {14.0f*0.98f*0.5f, 10.0f*0.98f*0.5f},
        {14.8f*0.98f*0.5f, 10.4f*0.98f*0.5f},
        {14.2f*0.98f*0.5f, 10.0f*0.98f*0.5f}
    };
    return halfSizes[std::clamp(index,0,2)];
}

bool SceneRenderer::pointInsidePondFootprint(const glm::vec3& p,float margin) const {
    static const glm::vec3 centers[3] = {
        {-24.0f,0.0f,4.5f}, {0.0f,0.0f,0.6f}, {25.0f,0.0f,-3.8f}
    };
    for(int i=0;i<3;++i){
        glm::vec2 half=pondWaterHalfSize(i);
        if(std::fabs(p.x-centers[i].x) <= half.x + margin && std::fabs(p.z-centers[i].z) <= half.y + margin) return true;
    }
    return false;
}

bool SceneRenderer::rectIntersectsPond(const glm::vec3& p,float halfX,float halfZ,float margin) const {
    static const glm::vec3 centers[3] = {
        {-24.0f,0.0f,4.5f}, {0.0f,0.0f,0.6f}, {25.0f,0.0f,-3.8f}
    };
    for(int i=0;i<3;++i){
        glm::vec2 pondHalf=pondWaterHalfSize(i);
        if(std::fabs(p.x-centers[i].x) <= (halfX + pondHalf.x + margin) &&
           std::fabs(p.z-centers[i].z) <= (halfZ + pondHalf.y + margin)) return true;
    }
    return false;
}

void SceneRenderer::initWorld(){
    // The world arrangement intentionally matches the supplied photographic reference:
    // Pond 1 = near/bottom-right, Pond 2 = left-middle, Pond 3 = far/top-right.
    // New concept: the ponds are farther apart in a village-scale chain.
    // Two visible flat pipes connect Pond 1 -> Pond 2 and Pond 2 -> Pond 3.
    // A third return line runs low and behind the scene so circulation can still loop internally.
    pipes_[0].control={
        // Ground-hugging pipe: the center line stays just above terrain and both ends penetrate pond water.
        glm::vec3(-18.55f,0.38f,4.55f), glm::vec3(-12.35f,0.52f,4.10f),
        glm::vec3( -8.15f,0.50f,1.95f), glm::vec3( -6.35f,0.34f,1.15f)
    };
    pipes_[1].control={
        glm::vec3(  6.45f,0.34f,0.75f), glm::vec3( 11.65f,0.50f,0.05f),
        glm::vec3( 17.20f,0.50f,-2.70f), glm::vec3( 19.20f,0.34f,-3.55f)
    };
    pipes_[2].control={
        // Opposite-side Pond 3 connection, also kept close to the ground.
        glm::vec3( 31.10f,0.34f,-8.20f), glm::vec3( 17.60f,0.50f,-15.40f),
        glm::vec3( -6.60f,0.50f,-12.10f), glm::vec3(-18.65f,0.34f, 0.35f)
    };
    for(int i=0;i<3;++i){
        pipeOuter_[i]=makeTube(pipes_[i],0.46f,60,28);
        pipeInner_[i]=makeTube(pipes_[i],0.365f,60,24);
    }

    // Perimeter-only vegetation: keep the three pond zones completely clear of large trees.
    trees_={
        {-31,0,-20},{-23,0,-23},{-13,0,-24},{-2,0,-24},{10,0,-24},{21,0,-23},{31,0,-19},
        {-32,0,19},{-23,0,23},{-11,0,24},{2,0,24},{14,0,24},{25,0,22},{33,0,16}
    };
    bushes_={
        {-30,0,-16},{-18,0,-21},{-6,0,-22},{8,0,-22},{22,0,-19},{30,0,-14},
        {-30,0,16},{-18,0,20},{-6,0,21},{8,0,21},{22,0,18},{30,0,13}
    };
    // Huts are intentionally pushed to the outside boundary, away from Pond 1 and Pond 3.
    huts_={{-29,0,-19},{-11,0,21},{14,0,21},{30,0,14}};

    fencePosts_.clear();
}

void SceneRenderer::initShadowFBO(){
    glGenFramebuffers(1,&shadowFBO_); glGenTextures(1,&shadowTex_);
    glBindTexture(GL_TEXTURE_2D,shadowTex_);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,shadowSize_,shadowSize_,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
#ifndef __EMSCRIPTEN__
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
    float border[4]={1,1,1,1}; glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
#else
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
#endif
    glBindFramebuffer(GL_FRAMEBUFFER,shadowFBO_);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,shadowTex_,0);
#ifdef __EMSCRIPTEN__
    { GLenum none=GL_NONE; glDrawBuffers(1,&none); glReadBuffer(GL_NONE); }
#else
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
#endif
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Shadow framebuffer is incomplete");
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void SceneRenderer::destroyFBOs(){
    if(sceneDepthRBO_) glDeleteRenderbuffers(1,&sceneDepthRBO_); sceneDepthRBO_=0;
    if(sceneColor_) glDeleteTextures(1,&sceneColor_); sceneColor_=0;
    if(sceneFBO_) glDeleteFramebuffers(1,&sceneFBO_); sceneFBO_=0;
    if(opaqueDepth_) glDeleteTextures(1,&opaqueDepth_); opaqueDepth_=0;
    if(opaqueColor_) glDeleteTextures(1,&opaqueColor_); opaqueColor_=0;
    if(opaqueFBO_) glDeleteFramebuffers(1,&opaqueFBO_); opaqueFBO_=0;
    for(int i=0;i<2;++i){
        if(bloomTex_[i]) glDeleteTextures(1,&bloomTex_[i]); bloomTex_[i]=0;
        if(bloomFBO_[i]) glDeleteFramebuffers(1,&bloomFBO_[i]); bloomFBO_[i]=0;
    }
}

void SceneRenderer::recreateSceneFBOs(){
    destroyFBOs(); fboW_=width_; fboH_=height_;
#ifdef __EMSCRIPTEN__
    GLint colorInternal=GL_RGBA8; GLenum colorType=GL_UNSIGNED_BYTE;
#else
    GLint colorInternal=GL_RGBA16F; GLenum colorType=GL_FLOAT;
#endif

    glGenFramebuffers(1,&opaqueFBO_); glBindFramebuffer(GL_FRAMEBUFFER,opaqueFBO_);
    glGenTextures(1,&opaqueColor_); glBindTexture(GL_TEXTURE_2D,opaqueColor_);
    glTexImage2D(GL_TEXTURE_2D,0,colorInternal,fboW_,fboH_,0,GL_RGBA,colorType,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,opaqueColor_,0);

    glGenTextures(1,&opaqueDepth_); glBindTexture(GL_TEXTURE_2D,opaqueDepth_);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,fboW_,fboH_,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,opaqueDepth_,0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Opaque framebuffer is incomplete");

    glGenFramebuffers(1,&sceneFBO_); glBindFramebuffer(GL_FRAMEBUFFER,sceneFBO_);
    glGenTextures(1,&sceneColor_); glBindTexture(GL_TEXTURE_2D,sceneColor_);
    glTexImage2D(GL_TEXTURE_2D,0,colorInternal,fboW_,fboH_,0,GL_RGBA,colorType,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,sceneColor_,0);
    glGenRenderbuffers(1,&sceneDepthRBO_); glBindRenderbuffer(GL_RENDERBUFFER,sceneDepthRBO_);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,fboW_,fboH_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,sceneDepthRBO_);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Scene framebuffer is incomplete");

    int bw=std::max(1,fboW_/2), bh=std::max(1,fboH_/2);
    glGenFramebuffers(2,bloomFBO_); glGenTextures(2,bloomTex_);
    for(int i=0;i<2;++i){
        glBindFramebuffer(GL_FRAMEBUFFER,bloomFBO_[i]);
        glBindTexture(GL_TEXTURE_2D,bloomTex_[i]);
        glTexImage2D(GL_TEXTURE_2D,0,colorInternal,bw,bh,0,GL_RGBA,colorType,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,bloomTex_[i],0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Bloom framebuffer is incomplete");
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}

glm::mat4 SceneRenderer::lightSpaceMatrix() const {
    glm::vec3 center(0,0,-1), pos=center-sunDir_*68.0f;
    glm::mat4 view=glm::lookAt(pos,center,glm::vec3(0,1,0));
    glm::mat4 proj=glm::ortho(-42.0f,42.0f,-39.0f,39.0f,1.0f,125.0f);
    return proj*view;
}

glm::mat4 SceneRenderer::modelBox(const glm::vec3& p,const glm::vec3& s) const { return trs(p,s); }
glm::mat4 SceneRenderer::modelCylinderZ(const glm::vec3& p,const glm::vec3& dir,float radius,float length) const {
    return glm::translate(glm::mat4(1),p)*alignZTo(dir)*glm::scale(glm::mat4(1),glm::vec3(radius,radius,length));
}
glm::mat4 SceneRenderer::modelCylinderY(const glm::vec3& p,float radius,float height) const {
    // Mesh::cylinder is aligned to local Z; rotate its Z axis to world Y.
    return glm::translate(glm::mat4(1),p)*alignZTo(glm::vec3(0,1,0))*glm::scale(glm::mat4(1),glm::vec3(radius,radius,height));
}

void SceneRenderer::beginPBR(const RenderGlobals& g,float time){
    pbr_.use();
    pbr_.set("uView",g.view); pbr_.set("uProj",g.proj); pbr_.set("uLightSpace",g.lightSpace);
    pbr_.set("uSunDir",sunDir_); pbr_.set("uSunColor",sunColor_); pbr_.set("uCameraPos",g.cameraPos); pbr_.set("uTime",time);
    pbr_.set("uNightBlend",nightMode_?1.0f:0.0f);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D,shadowTex_); pbr_.set("uShadowMap",4);
}

void SceneRenderer::drawObject(const Mesh& mesh,const glm::mat4& model,const Material& mat,bool shadowPass){
    if(shadowPass){
        shadow_.set("uModel",model); shadow_.set("uMaterialMode",mat.mode); mesh.draw();
    } else {
        pbr_.set("uModel",model); pbr_.set("uBaseColor",mat.color); pbr_.set("uRoughness",mat.roughness); pbr_.set("uMetallic",mat.metallic);
        pbr_.set("uMaterialMode",mat.mode); pbr_.set("uReceiveShadow",mat.receiveShadow?1:0); mesh.draw();
    }
}

void SceneRenderer::drawPondStructure(const PondState& p,int index,bool shadowPass){
    Material bank{{0.38f,0.23f,0.09f},0.98f,0.0f,2,true};
    Material pondBed{{0.060f,0.082f,0.110f},0.84f,0.0f,1,true};
    Material deepCore{{0.030f,0.050f,0.080f},0.82f,0.0f,1,true};
    Material crest{{0.32f,0.19f,0.082f},0.95f,0.0f,2,true};
    Material fieldStone{{0.30f,0.275f,0.215f},0.96f,0.0f,7,true};
    Material mossStone{{0.105f,0.205f,0.060f},0.95f,0.0f,3,true};

    // Earthen, dug-in village pond basin.
    glm::mat4 bankModel=glm::translate(glm::mat4(1.0f),p.center)
        *glm::scale(glm::mat4(1.0f),glm::vec3(p.size.x,1.0f,p.size.y));
    drawObject(pondBank_,bankModel,bank,shadowPass);

    // Slight soil crest at the top rim instead of a stone fence.
    drawObject(cube_,modelBox(p.center+glm::vec3(0,0.84f, p.size.y*0.47f),{p.size.x*1.02f,0.10f,0.20f}),crest,shadowPass);
    drawObject(cube_,modelBox(p.center+glm::vec3(0,0.84f,-p.size.y*0.47f),{p.size.x*1.02f,0.10f,0.20f}),crest,shadowPass);
    drawObject(cube_,modelBox(p.center+glm::vec3( p.size.x*0.47f,0.84f,0),{0.20f,0.10f,p.size.y*1.02f}),crest,shadowPass);
    drawObject(cube_,modelBox(p.center+glm::vec3(-p.size.x*0.47f,0.84f,0),{0.20f,0.10f,p.size.y*1.02f}),crest,shadowPass);

    // Replace the flat brown sheet look with a subtler submerged bluish pond bed.
    drawObject(cube_,modelBox(p.center+glm::vec3(0,p.baseWaterY-0.14f,0),{p.size.x*0.80f,0.12f,p.size.y*0.80f}),pondBed,shadowPass);
    drawObject(cube_,modelBox(p.center+glm::vec3(0,p.baseWaterY-0.62f,0),{p.size.x*0.46f,0.16f,p.size.y*0.46f}),deepCore,shadowPass);

    // Irregular field stones and compacted clods break the perfectly clean CG rim silhouette.
    // They are deliberately placed on the dry outside face, never on the water surface.
    for(int k=0;k<20;++k){
        uint32_t seed=3100u+(uint32_t)index*113u+(uint32_t)k*17u;
        float u=hash01(seed)*2.0f-1.0f;
        float jitter=hash01(seed+1u)*0.16f;
        glm::vec3 q=p.center;
        switch(k&3){
            case 0: q+=glm::vec3(u*p.size.x*0.48f,0.34f+jitter, p.size.y*0.535f); break;
            case 1: q+=glm::vec3(u*p.size.x*0.48f,0.34f+jitter,-p.size.y*0.535f); break;
            case 2: q+=glm::vec3( p.size.x*0.535f,0.34f+jitter,u*p.size.y*0.48f); break;
            default:q+=glm::vec3(-p.size.x*0.535f,0.34f+jitter,u*p.size.y*0.48f); break;
        }
        float s=0.22f+0.28f*hash01(seed+2u);
        glm::mat4 m=glm::translate(glm::mat4(1),q)
            *glm::rotate(glm::mat4(1),hash01(seed+3u)*PI,glm::vec3(0,1,0))
            *glm::scale(glm::mat4(1),glm::vec3(s,0.55f*s,0.78f*s));
        drawObject(rock_,m,(k%6==0)?mossStone:fieldStone,shadowPass);
    }
}

void SceneRenderer::drawPalms(bool shadowPass){
    Material trunk{{0.30f,0.16f,0.060f},0.92f,0.0f,4,true};
    Material barkDark{{0.15f,0.075f,0.025f},0.96f,0.0f,4,true};
    Material leafA{{0.055f,0.28f,0.050f},0.79f,0.0f,3,true};
    Material leafB{{0.12f,0.39f,0.060f},0.80f,0.0f,3,true};
    Material dryLeaf{{0.34f,0.23f,0.070f},0.96f,0.0f,5,true};

    for(size_t i=0;i<trees_.size();++i){
        glm::vec3 p=trees_[i];
        if(pointInsidePondFootprint(p,3.25f)) continue;
        float s=0.82f+0.34f*hash01((uint32_t)i*17u+4u);
        float h=5.2f*s;
        // More, shorter tapered segments make the curved trunk read as biological rather than stacked tubes.
        const int seg=9;
        for(int k=0;k<seg;++k){
            float t=(k+0.5f)/seg;
            glm::vec3 bend((std::sin((float)i*1.7f)*0.16f)*t*t,0,(std::cos((float)i*1.3f)*0.13f)*t*t);
            float r=(0.235f-0.100f*t)*s;
            drawObject(cylinder_,modelCylinderY(p+glm::vec3(0,h*t,0)+bend,r,h/seg*1.08f),trunk,shadowPass);

            // Dark fibrous scars around each old frond joint.
            float jt=(float)(k+1)/seg;
            glm::vec3 jointBend((std::sin((float)i*1.7f)*0.16f)*jt*jt,0,(std::cos((float)i*1.3f)*0.13f)*jt*jt);
            float jr=(0.242f-0.102f*jt)*s;
            drawObject(cylinder_,modelCylinderY(p+glm::vec3(0,h*jt,0)+jointBend,jr,0.040f*s),barkDark,shadowPass);
        }
        // Buttress roots anchor the trunk into the soil.
        for(int k=0;k<4;++k){
            float a=(float)k*PI*0.5f+(float)i*0.31f;
            glm::vec3 start=p+glm::vec3(0,0.18f*s,0);
            glm::vec3 end=p+glm::vec3(std::cos(a)*0.62f*s,0.02f,std::sin(a)*0.62f*s);
            drawObject(cylinder_,modelCylinderZ((start+end)*0.5f,end-start,0.075f*s,glm::length(end-start)),trunk,shadowPass);
        }

        float topT=1.0f;
        glm::vec3 topBend((std::sin((float)i*1.7f)*0.16f)*topT,0,(std::cos((float)i*1.3f)*0.13f)*topT);
        glm::vec3 crown=p+glm::vec3(0,h+0.05f,0)+topBend;
        for(int k=0;k<15;++k){
            float a=(float)k/15.0f*2.0f*PI+(float)i*0.37f;
            float tilt=glm::radians(26.0f+24.0f*std::sin(k*1.6f+(float)i));
            glm::mat4 m=glm::translate(glm::mat4(1),crown)
                *glm::rotate(glm::mat4(1),a,glm::vec3(0,1,0))
                *glm::rotate(glm::mat4(1),tilt,glm::vec3(1,0,0))
                *glm::scale(glm::mat4(1),glm::vec3((0.63f+0.12f*hash01((uint32_t)i*41u+(uint32_t)k))*s,0.72f*s,(2.8f+0.55f*hash01((uint32_t)i*53u+(uint32_t)k))*s));
            drawObject(leaf_,m,(k&1)?leafA:leafB,shadowPass);
        }
        // A couple of older fronds hang beneath the fresh crown.
        for(int k=0;k<2;++k){
            float a=(float)k*2.7f+(float)i*0.91f;
            glm::mat4 m=glm::translate(glm::mat4(1),crown+glm::vec3(0,-0.12f*s,0))
                *glm::rotate(glm::mat4(1),a,glm::vec3(0,1,0))
                *glm::rotate(glm::mat4(1),glm::radians(68.0f),glm::vec3(1,0,0))
                *glm::scale(glm::mat4(1),glm::vec3(0.42f*s,0.45f*s,2.15f*s));
            drawObject(leaf_,m,dryLeaf,shadowPass);
        }
        // coconuts
        if((i%3)==0){
            Material coco{{0.23f,0.17f,0.055f},0.84f,0.0f,4,true};
            for(int c=0;c<3;++c){
                float a=c*2.1f+i;
                drawObject(sphere_,trs(crown+glm::vec3(std::cos(a)*0.24f,-0.18f,std::sin(a)*0.24f),glm::vec3(0.20f*s)),coco,shadowPass);
            }
        }
    }
}

void SceneRenderer::drawHuts(bool shadowPass){
    Material mudWall{{0.50f,0.33f,0.16f},0.96f,0.0f,12,true};
    Material limePatch{{0.61f,0.50f,0.31f},0.98f,0.0f,12,true};
    Material bamboo{{0.34f,0.20f,0.075f},0.91f,0.0f,4,true};
    Material thatch{{0.42f,0.25f,0.080f},0.98f,0.0f,5,true};
    Material dryThatch{{0.57f,0.39f,0.13f},0.99f,0.0f,5,true};
    Material dark{{0.025f,0.022f,0.018f},0.80f,0.0f,0,true};
    Material foundation{{0.30f,0.285f,0.245f},0.97f,0.0f,13,true};
    Material clayPot{{0.38f,0.14f,0.055f},0.92f,0.0f,7,true};
    Material windowGlow{{1.00f,0.34f,0.055f},0.18f,0.0f,17,false};
    Material lanternFrame{{0.12f,0.085f,0.035f},0.38f,0.55f,6,true};

    for(size_t i=0;i<huts_.size();++i){
        glm::vec3 p=huts_[i]; float w=3.5f+(i%2)*0.55f,d=2.85f,h=2.15f;
        if(rectIntersectsPond(p,(w+0.75f)*0.5f,d*0.5f,3.25f)) continue;
        // Raised rubble foundation protects the earthen wall from monsoon splash.
        drawObject(cube_,modelBox(p+glm::vec3(0,0.10f,0),{w+0.34f,0.20f,d+0.32f}),foundation,shadowPass);
        drawObject(cube_,modelBox(p+glm::vec3(0,h*0.47f,0),{w,h,d}),mudWall,shadowPass);
        // Uneven repaired plaster patches keep the wall from reading as one pristine box.
        drawObject(rock_,trs(p+glm::vec3(-w*0.29f,0.82f,d*0.508f),{0.72f,0.60f,0.055f}),limePatch,shadowPass);
        drawObject(rock_,trs(p+glm::vec3( w*0.34f,0.48f,d*0.509f),{0.50f,0.34f,0.052f}),limePatch,shadowPass);

        // Structural bamboo posts and woven facade rails.
        for(float x:{-w*0.47f,w*0.47f}) for(float z:{-d*0.47f,d*0.47f})
            drawObject(cylinder_,modelCylinderY(p+glm::vec3(x,1.12f,z),0.055f,2.34f),bamboo,shadowPass);
        // bamboo wall ribs
        for(int k=-3;k<=3;++k){
            float x=k*w/7.0f;
            drawObject(cylinder_,modelCylinderY(p+glm::vec3(x,1.05f,d*0.505f),0.025f,1.85f),bamboo,shadowPass);
        }
        for(float y:{0.46f,1.18f,1.86f})
            drawObject(cylinder_,modelCylinderZ(p+glm::vec3(0,y,d*0.512f),glm::vec3(1,0,0),0.032f,w),bamboo,shadowPass);

        // broad thatch roof with thick eaves
        glm::mat4 r1=glm::translate(glm::mat4(1),p+glm::vec3(0,2.46f,-0.77f))*glm::rotate(glm::mat4(1),glm::radians(28.0f),glm::vec3(1,0,0))*glm::scale(glm::mat4(1),glm::vec3(w+0.75f,0.22f,d*0.72f));
        glm::mat4 r2=glm::translate(glm::mat4(1),p+glm::vec3(0,2.46f, 0.77f))*glm::rotate(glm::mat4(1),glm::radians(-28.0f),glm::vec3(1,0,0))*glm::scale(glm::mat4(1),glm::vec3(w+0.75f,0.22f,d*0.72f));
        drawObject(cube_,r1,thatch,shadowPass); drawObject(cube_,r2,thatch,shadowPass);
        drawObject(cylinder_,modelCylinderZ(p+glm::vec3(0,2.90f,0),glm::vec3(1,0,0),0.075f,w+0.92f),dryThatch,shadowPass);

        // Individual hanging straw ends soften both roof eaves.
        for(int k=-5;k<=5;++k){
            float x=(float)k*(w+0.58f)/11.0f;
            float drop=0.24f+0.10f*hash01(4200u+(uint32_t)i*31u+(uint32_t)(k+5));
            drawObject(cylinder_,modelCylinderY(p+glm::vec3(x,2.12f-drop*0.5f, d*0.635f),0.014f,drop),dryThatch,shadowPass);
            drawObject(cylinder_,modelCylinderY(p+glm::vec3(x,2.12f-drop*0.5f,-d*0.635f),0.014f,drop),dryThatch,shadowPass);
        }

        // Shaded front veranda, steps, door frame and wooden window shutter.
        glm::vec3 front=p+glm::vec3(0,0,d*0.72f);
        drawObject(cube_,modelBox(front+glm::vec3(0,0.18f,0),{w*0.82f,0.15f,0.82f}),bamboo,shadowPass);
        drawObject(cube_,modelBox(front+glm::vec3(0,0.07f,0.55f),{0.92f,0.12f,0.34f}),foundation,shadowPass);
        for(float x:{-w*0.36f,w*0.36f})
            drawObject(cylinder_,modelCylinderY(front+glm::vec3(x,1.17f,0.24f),0.050f,2.20f),bamboo,shadowPass);
        drawObject(cube_,modelBox(p+glm::vec3(0,0.65f,d*0.51f),{0.80f,1.35f,0.10f}),dark,shadowPass);
        drawObject(cube_,modelBox(p+glm::vec3(w*0.28f,1.25f,d*0.51f),{0.58f,0.52f,0.08f}),dark,shadowPass);
        drawObject(cube_,modelBox(p+glm::vec3(w*0.28f,1.25f,d*0.585f),{0.48f,0.39f,0.018f}),windowGlow,shadowPass);
        for(float x:{-0.46f,0.46f})
            drawObject(cylinder_,modelCylinderY(p+glm::vec3(x,0.74f,d*0.57f),0.035f,1.52f),bamboo,shadowPass);
        drawObject(cylinder_,modelCylinderZ(p+glm::vec3(0,1.49f,d*0.57f),glm::vec3(1,0,0),0.038f,0.98f),bamboo,shadowPass);
        drawObject(cube_,modelBox(p+glm::vec3(w*0.28f,1.25f,d*0.57f),{0.64f,0.065f,0.08f}),bamboo,shadowPass);
        drawObject(cube_,modelBox(p+glm::vec3(w*0.28f,1.25f,d*0.575f),{0.055f,0.58f,0.08f}),bamboo,shadowPass);

        glm::vec3 lamp=p+glm::vec3(-0.63f,1.38f,d*0.68f);
        drawObject(cylinder_,modelCylinderY(lamp+glm::vec3(0,0.16f,0),0.055f,0.26f),lanternFrame,shadowPass);
        drawObject(sphere_,trs(lamp,{0.20f,0.26f,0.20f}),windowGlow,shadowPass);
        drawObject(cylinder_,modelCylinderY(lamp-glm::vec3(0,0.15f,0),0.12f,0.055f),lanternFrame,shadowPass);

        // Everyday props: water pot and a small stack of split firewood.
        glm::vec3 pot=p+glm::vec3(-w*0.34f,0.43f,d*0.80f);
        drawObject(sphere_,trs(pot,{0.48f,0.58f,0.48f}),clayPot,shadowPass);
        drawObject(cylinder_,modelCylinderY(pot+glm::vec3(0,0.29f,0),0.15f,0.13f),clayPot,shadowPass);
        for(int k=0;k<5;++k){
            glm::vec3 logP=p+glm::vec3(w*0.34f+(k%2)*0.09f,0.16f+(k/2)*0.10f,d*0.81f);
            drawObject(cylinder_,modelCylinderZ(logP,glm::vec3(1,0,0.18f*(k&1?1.0f:-1.0f)),0.055f,0.72f),bamboo,shadowPass);
        }
    }
}

void SceneRenderer::drawCrops(bool shadowPass){
    // v14: all flat brown crop-bed / wooden-sheet-like ground geometry was removed.
    (void)shadowPass;
}

void SceneRenderer::drawDenseVegetation(bool shadowPass){
    Material meadow{{0.095f,0.265f,0.043f},0.90f,0.0f,9,true};
    Material meadowDry{{0.18f,0.31f,0.055f},0.91f,0.0f,9,true};
    struct Patch{glm::vec3 p,s;float r;int m;};
    // Only distant border vegetation remains. No grass/reeds/rice are allowed inside or beside pond footprints.
    const Patch patches[]={
        {{-24,0,-23},{12,0.88f,4.5f},0.02f,0},{{-8,0,-24},{12,0.92f,4.0f},0.0f,1},
        {{10,0,-24},{13,0.90f,4.0f},0.01f,0},{{27,0,-21},{8,0.94f,5.0f},-0.03f,1},
        {{-24,0,23},{12,0.90f,4.2f},0.01f,1},{{-8,0,24},{12,0.88f,3.8f},0.0f,0},
        {{10,0,24},{13,0.92f,4.0f},0.0f,1},{{27,0,21},{8,0.90f,4.8f},0.02f,0}
    };
    for(const auto& q:patches){
        if(rectIntersectsPond(q.p,q.s.x*0.50f,q.s.z*0.50f,4.0f)) continue;
        glm::mat4 m=glm::translate(glm::mat4(1),q.p+glm::vec3(0,-0.16f,0))
            *glm::rotate(glm::mat4(1),q.r,glm::vec3(0,1,0))*glm::scale(glm::mat4(1),q.s);
        drawObject(grassPatch_,m,q.m==0?meadow:meadowDry,shadowPass);
    }
}

void SceneRenderer::drawVillageLife(bool shadowPass){
    // Anatomical silhouettes and small asymmetries make rural life readable at both near and far cameras.
    Material skin{{0.46f,0.27f,0.16f},0.86f,0.0f,14,true};
    Material clothA{{0.15f,0.30f,0.58f},0.82f,0.0f,16,true};
    Material clothB{{0.72f,0.68f,0.48f},0.86f,0.0f,16,true};
    Material cowBody{{0.33f,0.19f,0.09f},0.90f,0.0f,15,true};
    Material cowWhite{{0.72f,0.69f,0.60f},0.90f,0.0f,15,true};
    Material goatBody{{0.64f,0.57f,0.43f},0.92f,0.0f,15,true};
    Material dark{{0.055f,0.045f,0.035f},0.88f,0.0f,15,true};
    Material hair{{0.025f,0.020f,0.016f},0.92f,0.0f,15,true};
    Material horn{{0.66f,0.58f,0.40f},0.88f,0.0f,7,true};

    auto person=[&](glm::vec3 p,const Material& cloth,float yaw){
        glm::mat4 R=glm::translate(glm::mat4(1),p)*glm::rotate(glm::mat4(1),yaw,glm::vec3(0,1,0));
        auto world=[&](glm::vec3 q){ return glm::vec3(R*glm::vec4(q,1.0f)); };
        drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,1.02f,0})*glm::scale(glm::mat4(1),{0.48f,0.88f,0.34f}),cloth,shadowPass);
        drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,0.68f,0})*glm::scale(glm::mat4(1),{0.50f,0.32f,0.36f}),cloth,shadowPass);
        drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,1.63f,0.015f})*glm::scale(glm::mat4(1),{0.38f,0.44f,0.38f}),skin,shadowPass);
        drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,1.78f,-0.025f})*glm::scale(glm::mat4(1),{0.39f,0.20f,0.39f}),hair,shadowPass);
        for(float side:{-1.0f,1.0f}){
            glm::vec3 shoulder=world({side*0.23f,1.28f,0});
            glm::vec3 hand=world({side*0.31f,0.82f,0.06f*side});
            drawObject(cylinder_,modelCylinderZ((shoulder+hand)*0.5f,hand-shoulder,0.052f,glm::length(hand-shoulder)),skin,shadowPass);
            drawObject(sphere_,trs(hand,glm::vec3(0.13f)),skin,shadowPass);
            drawObject(cylinder_,modelCylinderY(world({side*0.11f,0.34f,0}),0.050f,0.68f),dark,shadowPass);
            drawObject(sphere_,trs(world({side*0.11f,0.045f,0.075f}),{0.16f,0.10f,0.28f}),dark,shadowPass);
        }
    };
    auto cow=[&](glm::vec3 p,float yaw){
        glm::mat4 R=glm::translate(glm::mat4(1),p)*glm::rotate(glm::mat4(1),yaw,glm::vec3(0,1,0));
        auto world=[&](glm::vec3 q){ return glm::vec3(R*glm::vec4(q,1.0f)); };
        drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,0.82f,0})*glm::scale(glm::mat4(1),{0.86f,0.94f,1.78f}),cowBody,shadowPass);
        drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,1.02f,0.73f})*glm::rotate(glm::mat4(1),glm::radians(-18.0f),glm::vec3(1,0,0))*glm::scale(glm::mat4(1),{0.56f,0.78f,0.68f}),cowWhite,shadowPass);
        drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,1.10f,1.17f})*glm::scale(glm::mat4(1),{0.62f,0.55f,0.76f}),cowWhite,shadowPass);
        drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,0.98f,1.50f})*glm::scale(glm::mat4(1),{0.52f,0.30f,0.42f}),cowBody,shadowPass);
        for(float side:{-1.0f,1.0f}){
            drawObject(sphere_,R*glm::translate(glm::mat4(1),{side*0.285f,1.16f,1.40f})*glm::scale(glm::mat4(1),{0.070f,0.075f,0.060f}),hair,shadowPass);
            drawObject(sphere_,R*glm::translate(glm::mat4(1),{side*0.14f,1.02f,1.69f})*glm::scale(glm::mat4(1),{0.052f,0.040f,0.035f}),hair,shadowPass);
        }
        // Natural hide patches sit just above the body surface.
        drawObject(sphere_,R*glm::translate(glm::mat4(1),{0.425f,0.90f,-0.18f})*glm::scale(glm::mat4(1),{0.035f,0.42f,0.58f}),cowWhite,shadowPass);
        drawObject(sphere_,R*glm::translate(glm::mat4(1),{-0.425f,0.72f,0.34f})*glm::scale(glm::mat4(1),{0.035f,0.31f,0.46f}),cowWhite,shadowPass);
        for(float x:{-0.30f,0.30f}) for(float z:{-0.58f,0.58f}){
            glm::vec3 leg=world({x,0.36f,z});
            drawObject(cylinder_,modelCylinderY(leg,0.072f,0.72f),cowBody,shadowPass);
            drawObject(sphere_,trs(world({x,0.055f,z+0.025f}),{0.18f,0.12f,0.22f}),dark,shadowPass);
        }
        for(float side:{-1.0f,1.0f}){
            drawObject(sphere_,R*glm::translate(glm::mat4(1),{side*0.36f,1.18f,1.14f})*glm::rotate(glm::mat4(1),side*0.32f,glm::vec3(0,0,1))*glm::scale(glm::mat4(1),{0.28f,0.10f,0.18f}),cowBody,shadowPass);
            glm::vec3 hornDir=glm::normalize(glm::vec3(side*0.55f,0.48f,0.22f));
            drawObject(cone_,R*glm::translate(glm::mat4(1),{side*0.22f,1.34f,1.18f})*alignZTo(hornDir)*glm::scale(glm::mat4(1),{0.075f,0.075f,0.36f}),horn,shadowPass);
        }
        glm::vec3 tail0=world({0,0.93f,-0.82f}),tail1=world({0,0.47f,-1.23f});
        drawObject(cylinder_,modelCylinderZ((tail0+tail1)*0.5f,tail1-tail0,0.035f,glm::length(tail1-tail0)),cowBody,shadowPass);
        drawObject(sphere_,trs(tail1,{0.18f,0.22f,0.18f}),dark,shadowPass);
    };
    auto goat=[&](glm::vec3 p,float yaw){
        glm::mat4 R=glm::translate(glm::mat4(1),p)*glm::rotate(glm::mat4(1),yaw,glm::vec3(0,1,0));
        auto world=[&](glm::vec3 q){ return glm::vec3(R*glm::vec4(q,1.0f)); };
        drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,0.60f,0})*glm::scale(glm::mat4(1),{0.54f,0.60f,1.06f}),goatBody,shadowPass);
        drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,0.77f,0.60f})*glm::rotate(glm::mat4(1),glm::radians(-23.0f),glm::vec3(1,0,0))*glm::scale(glm::mat4(1),{0.36f,0.57f,0.46f}),goatBody,shadowPass);
        drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,0.85f,0.84f})*glm::scale(glm::mat4(1),{0.42f,0.38f,0.50f}),goatBody,shadowPass);
        for(float side:{-1.0f,1.0f})
            drawObject(sphere_,R*glm::translate(glm::mat4(1),{side*0.19f,0.91f,1.02f})*glm::scale(glm::mat4(1),{0.052f,0.057f,0.045f}),hair,shadowPass);
        for(float x:{-0.19f,0.19f}) for(float z:{-0.32f,0.32f}){
            drawObject(cylinder_,modelCylinderY(world({x,0.25f,z}),0.043f,0.50f),goatBody,shadowPass);
            drawObject(sphere_,trs(world({x,0.035f,z+0.02f}),{0.11f,0.075f,0.14f}),dark,shadowPass);
        }
        for(float side:{-1.0f,1.0f}){
            drawObject(sphere_,R*glm::translate(glm::mat4(1),{side*0.24f,0.91f,0.79f})*glm::scale(glm::mat4(1),{0.18f,0.07f,0.15f}),goatBody,shadowPass);
            glm::vec3 hornDir=glm::normalize(glm::vec3(side*0.22f,0.70f,-0.34f));
            drawObject(cone_,R*glm::translate(glm::mat4(1),{side*0.13f,1.04f,0.74f})*alignZTo(hornDir)*glm::scale(glm::mat4(1),{0.045f,0.045f,0.27f}),horn,shadowPass);
        }
        drawObject(cone_,R*glm::translate(glm::mat4(1),{0,0.64f,1.06f})*glm::rotate(glm::mat4(1),glm::radians(180.0f),glm::vec3(1,0,0))*glm::scale(glm::mat4(1),{0.10f,0.10f,0.26f}),dark,shadowPass);
        glm::vec3 tail0=world({0,0.68f,-0.48f}),tail1=world({0,0.82f,-0.76f});
        drawObject(cylinder_,modelCylinderZ((tail0+tail1)*0.5f,tail1-tail0,0.035f,glm::length(tail1-tail0)),goatBody,shadowPass);
    };

    person({-27.0f,0,-17.2f},clothA,0.35f);
    person({ 28.5f,0, 12.0f},clothB,-0.60f);
    cow({-25.0f,0,-18.3f},0.40f);
    cow({ 27.0f,0, 15.7f},-0.55f);
    goat({-28.0f,0,-16.1f},0.18f);
    goat({ 29.4f,0, 15.1f},-0.38f);
    goat({ 26.0f,0, 13.4f},0.62f);
}

void SceneRenderer::drawVillage(bool shadowPass){
    Material ground{{0.20f,0.34f,0.085f},0.96f,0.0f,11,true};
    Material waterChannel{{0.055f,0.22f,0.30f},0.58f,0.0f,0,true};

    drawObject(plane_,trs({0,-0.24f,0},{70,1,62}),ground,shadowPass);

    // Keep only the shallow rural drainage channel. All brown road/wood-sheet/plank geometry is removed.
    drawObject(cube_,modelBox({-11.5f,-0.30f,16.8f},{4.0f,0.05f,21.0f}),waterChannel,shadowPass);

    drawPalms(shadowPass);
    drawHuts(shadowPass);
    drawCrops(shadowPass);
    drawDenseVegetation(shadowPass);
    drawVillageLife(shadowPass);

    Material bushA{{0.07f,0.31f,0.045f},0.86f,0.0f,3,true};
    Material bushB{{0.16f,0.43f,0.065f},0.84f,0.0f,3,true};
    for(size_t i=0;i<bushes_.size();++i){
        glm::vec3 p=bushes_[i]; float s=0.76f+0.27f*hash01((uint32_t)i+200u);
        if(pointInsidePondFootprint(p,3.25f)) continue;
        drawObject(cylinder_,modelCylinderY(p+glm::vec3(0,0.44f,0),0.075f*s,0.88f*s),Material{{0.20f,0.105f,0.035f},0.94f,0.0f,4,true},shadowPass);
        drawObject(rock_,trs(p+glm::vec3(0,0.42f,0),{1.55f*s,1.05f*s,1.30f*s}),bushA,shadowPass);
        drawObject(rock_,trs(p+glm::vec3(0.62f*s,0.34f,0.20f*s),{1.05f*s,0.82f*s,0.94f*s}),bushB,shadowPass);
        drawObject(rock_,trs(p+glm::vec3(-0.48f*s,0.30f,-0.18f*s),{0.90f*s,0.72f*s,0.82f*s}),bushB,shadowPass);
    }

    // Scenic perimeter only: everything below sits beyond the original pond layout.
    // It fills the previously empty frame without moving or recoloring any existing object.
    // Distant scenery is omitted from the shadow pass to keep the original shadow budget intact.
    if(!shadowPass){
#ifndef __EMSCRIPTEN__
        Material farGround{{0.20f,0.34f,0.085f},0.97f,0.0f,11,false};
        Material hillNear{{0.14f,0.29f,0.095f},0.98f,0.0f,3,false};
        Material hillFar{{0.095f,0.20f,0.105f},0.99f,0.0f,3,false};
        Material river{{0.075f,0.30f,0.39f},0.28f,0.02f,0,false};
        Material riverBank{{0.27f,0.18f,0.075f},0.98f,0.0f,2,false};
        Material fieldA{{0.10f,0.29f,0.045f},0.91f,0.0f,9,false};
        Material fieldB{{0.19f,0.34f,0.060f},0.92f,0.0f,9,false};

        // A lower continuation plane closes every gap beyond the original 70 x 62 m ground.
        drawObject(plane_,trs({0,-0.34f,0},{220,1,220}),farGround,false);

        // Layered, irregular hills form a complete natural horizon in every camera direction.
        for(int i=0;i<20;++i){
            float a=(float)i/20.0f*2.0f*PI+0.11f;
            float r=78.0f+hash01(1400u+(uint32_t)i)*13.0f;
            float sx=24.0f+hash01(1500u+(uint32_t)i)*16.0f;
            float sy=18.0f+hash01(1600u+(uint32_t)i)*13.0f;
            float sz=18.0f+hash01(1700u+(uint32_t)i)*11.0f;
            glm::vec3 p(std::cos(a)*r,5.0f+sy*0.055f,std::sin(a)*r);
            drawObject(rock_,trs(p,{sx,sy,sz}),(i%3)==0?hillFar:hillNear,false);
        }

        // A distant river and earthen banks provide a believable home for fishing boats.
        drawObject(plane_,trs({0,-0.265f,-60.0f},{122,1,17}),river,false);
        drawObject(trapezoid_,trs({0,-0.05f,-51.2f},{124,0.78f,2.5f}),riverBank,false);
        drawObject(trapezoid_,trs({0,-0.05f,-68.8f},{124,0.78f,2.5f}),riverBank,false);

        // Repeated high-detail grass geometry reads as rice/meadow fields in the outer frame.
        struct FarField { glm::vec3 p,s; float yaw; int tone; };
        const FarField farFields[]={
            {{-52,-0.30f,-25},{18,0.80f,12}, 0.10f,0},{{-52,-0.30f,  2},{18,0.86f,12},-0.08f,1},
            {{-50,-0.30f, 31},{20,0.84f,13}, 0.06f,0},{{ 52,-0.30f,-28},{18,0.82f,12},-0.10f,1},
            {{ 52,-0.30f,  2},{18,0.88f,12}, 0.07f,0},{{ 51,-0.30f, 32},{20,0.82f,13},-0.05f,1},
            {{-24,-0.30f, 51},{18,0.84f,12}, 0.04f,1},{{  1,-0.30f, 53},{19,0.86f,12},-0.03f,0},
            {{ 27,-0.30f, 50},{18,0.82f,12}, 0.05f,1}
        };
        for(const auto& f:farFields){
            glm::mat4 m=glm::translate(glm::mat4(1),f.p)
                *glm::rotate(glm::mat4(1),f.yaw,glm::vec3(0,1,0))*glm::scale(glm::mat4(1),f.s);
            drawObject(grassPatch_,m,f.tone==0?fieldA:fieldB,false);
        }

        // Dense broadleaf tree belt.  A deliberate opening is left over the river.
        Material trunk{{0.25f,0.145f,0.052f},0.94f,0.0f,4,false};
        Material canopyA{{0.045f,0.24f,0.040f},0.88f,0.0f,3,false};
        Material canopyB{{0.095f,0.34f,0.055f},0.87f,0.0f,3,false};
        Material canopySun{{0.17f,0.40f,0.070f},0.86f,0.0f,3,false};
        for(int i=0;i<46;++i){
            float a=(float)i/46.0f*2.0f*PI+0.045f*std::sin((float)i*2.3f);
            float r=48.0f+hash01(1800u+(uint32_t)i)*12.0f;
            glm::vec3 p(std::cos(a)*r,-0.24f,std::sin(a)*r);
            if(p.z<-43.0f && std::fabs(p.x)<43.0f) continue;
            float s=0.82f+hash01(1900u+(uint32_t)i)*0.56f;
            float h=(5.2f+hash01(2000u+(uint32_t)i)*2.8f)*s;
            drawObject(cylinder_,modelCylinderY(p+glm::vec3(0,h*0.50f,0),0.18f*s,h),trunk,false);
            glm::vec3 c=p+glm::vec3(0,h+0.20f,0);
            // Forking limbs remain partly visible beneath the crown and remove the lollipop-tree look.
            for(int b=0;b<3;++b){
                float ba=(float)b*2.08f+hash01(2050u+(uint32_t)i)*1.7f;
                glm::vec3 start=p+glm::vec3(0,h*(0.70f+0.07f*b),0);
                glm::vec3 end=start+glm::vec3(std::cos(ba)*1.55f*s,0.85f*s,std::sin(ba)*1.55f*s);
                drawObject(cylinder_,modelCylinderZ((start+end)*0.5f,end-start,(0.105f-0.018f*b)*s,glm::length(end-start)),trunk,false);
            }
            const Material& crown=(i%5==0)?canopySun:((i&1)?canopyA:canopyB);
            drawObject(rock_,trs(c,{3.8f*s,3.9f*s,3.5f*s}),crown,false);
            drawObject(rock_,trs(c+glm::vec3( 1.35f*s,-0.20f,0.30f*s),{2.8f*s,3.0f*s,2.7f*s}),canopyB,false);
            drawObject(rock_,trs(c+glm::vec3(-1.18f*s,-0.35f,-0.40f*s),{2.6f*s,2.8f*s,2.6f*s}),canopyA,false);
        }

        // Small rural figures remain outside the original pond area.
        Material skinFar{{0.46f,0.27f,0.16f},0.88f,0.0f,14,false};
        Material clothBlue{{0.10f,0.27f,0.52f},0.84f,0.0f,16,false};
        Material clothRed{{0.58f,0.12f,0.075f},0.86f,0.0f,16,false};
        Material clothOchre{{0.62f,0.47f,0.13f},0.88f,0.0f,16,false};
        Material animalDark{{0.080f,0.055f,0.035f},0.91f,0.0f,15,false};
        Material cowBrown{{0.36f,0.20f,0.085f},0.92f,0.0f,15,false};
        Material cowCream{{0.72f,0.67f,0.54f},0.92f,0.0f,15,false};

        auto farPerson=[&](glm::vec3 p,const Material& cloth,float s){
            drawObject(sphere_,trs(p+glm::vec3(0,1.02f*s,0),glm::vec3(0.46f*s,0.86f*s,0.33f*s)),cloth,false);
            drawObject(sphere_,trs(p+glm::vec3(0,1.55f*s,0),glm::vec3(0.34f*s,0.38f*s,0.34f*s)),skinFar,false);
            drawObject(cylinder_,modelCylinderY(p+glm::vec3(-0.11f*s,0.28f*s,0),0.045f*s,0.58f*s),animalDark,false);
            drawObject(cylinder_,modelCylinderY(p+glm::vec3( 0.11f*s,0.28f*s,0),0.045f*s,0.58f*s),animalDark,false);
            for(float side:{-1.0f,1.0f}){
                glm::vec3 shoulder=p+glm::vec3(side*0.23f*s,1.22f*s,0);
                glm::vec3 hand=p+glm::vec3(side*0.30f*s,0.80f*s,0.04f*s);
                drawObject(cylinder_,modelCylinderZ((shoulder+hand)*0.5f,hand-shoulder,0.043f*s,glm::length(hand-shoulder)),skinFar,false);
            }
        };
        auto farCow=[&](glm::vec3 p,float yaw,float s){
            glm::mat4 R=glm::translate(glm::mat4(1),p)*glm::rotate(glm::mat4(1),yaw,glm::vec3(0,1,0));
            auto world=[&](glm::vec3 q){ return glm::vec3(R*glm::vec4(q,1.0f)); };
            drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,0.78f*s,0})*glm::scale(glm::mat4(1),{0.84f*s,0.92f*s,1.80f*s}),cowBrown,false);
            drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,0.94f*s,0.78f*s})*glm::scale(glm::mat4(1),{0.54f*s,0.69f*s,0.64f*s}),cowCream,false);
            drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,1.02f*s,1.16f*s})*glm::scale(glm::mat4(1),{0.62f*s,0.54f*s,0.72f*s}),cowCream,false);
            drawObject(sphere_,R*glm::translate(glm::mat4(1),{0,0.91f*s,1.45f*s})*glm::scale(glm::mat4(1),{0.50f*s,0.28f*s,0.38f*s}),cowBrown,false);
            for(float x:{-0.30f,0.30f}) for(float z:{-0.58f,0.58f})
                drawObject(cylinder_,modelCylinderY(world({x*s,0.34f*s,z*s}),0.07f*s,0.68f*s),animalDark,false);
            glm::vec3 tail0=world({0,0.88f*s,-0.82f*s}),tail1=world({0,0.46f*s,-1.16f*s});
            drawObject(cylinder_,modelCylinderZ((tail0+tail1)*0.5f,tail1-tail0,0.032f*s,glm::length(tail1-tail0)),cowBrown,false);
        };

        farPerson({-43.0f,-0.24f,-30.5f},clothBlue,1.05f);
        farPerson({-39.5f,-0.24f, 29.0f},clothRed,0.98f);
        farPerson({ 42.5f,-0.24f,-27.0f},clothOchre,1.02f);
        farPerson({ 45.0f,-0.24f, 24.0f},clothBlue,0.94f);
        farPerson({ -7.5f,-0.24f,-49.0f},clothRed,0.90f);
        farCow({-45.0f,-0.24f, 16.0f}, 0.42f,1.00f);
        farCow({ 44.0f,-0.24f, 11.0f},-0.64f,0.94f);
        farCow({ 34.0f,-0.24f,-36.0f}, 0.18f,0.92f);

        // Fishing boats and a slightly larger village transport boat on the far river.
        Material hull{{0.25f,0.095f,0.035f},0.90f,0.0f,4,false};
        Material hullEdge{{0.48f,0.28f,0.085f},0.88f,0.0f,4,false};
        Material cabin{{0.64f,0.58f,0.42f},0.91f,0.0f,5,false};
        Material sailA{{0.72f,0.69f,0.54f},0.92f,0.0f,5,false};
        Material sailB{{0.56f,0.18f,0.095f},0.90f,0.0f,5,false};
        Material rope{{0.105f,0.070f,0.030f},0.98f,0.0f,4,false};
        auto boat=[&](glm::vec3 p,float yaw,float s,const Material& sail){
            glm::mat4 R=glm::translate(glm::mat4(1),p)*glm::rotate(glm::mat4(1),yaw,glm::vec3(0,1,0));
            auto world=[&](glm::vec3 q){ return glm::vec3(R*glm::vec4(q,1.0f)); };
            drawObject(trapezoid_,R*glm::translate(glm::mat4(1),{0,0.02f,0})*glm::scale(glm::mat4(1),{5.8f*s,0.72f*s,1.55f*s}),hull,false);
            drawObject(cube_,R*glm::translate(glm::mat4(1),{0,0.34f,0})*glm::scale(glm::mat4(1),{5.2f*s,0.10f*s,1.28f*s}),hullEdge,false);
            drawObject(cube_,R*glm::translate(glm::mat4(1),{-1.35f*s,0.78f*s,0})*glm::scale(glm::mat4(1),{1.55f*s,0.88f*s,1.02f*s}),cabin,false);
            glm::vec3 mastPos=glm::vec3(R*glm::vec4(0.45f*s,1.62f*s,0,1));
            drawObject(cylinder_,modelCylinderY(mastPos,0.045f*s,3.10f*s),trunk,false);
            glm::mat4 sailM=R*glm::translate(glm::mat4(1),{0.45f*s,0.62f*s,0.04f*s})
                *glm::rotate(glm::mat4(1),glm::radians(-90.0f),glm::vec3(1,0,0))
                *glm::scale(glm::mat4(1),{0.92f*s,1.0f,2.45f*s});
            drawObject(leaf_,sailM,sail,false);

            // Gunwale ribs, rigging and oar make each silhouette read as a working boat.
            for(int k=-2;k<=2;++k){
                float x=(float)k*0.88f*s;
                for(float z:{-0.59f*s,0.59f*s})
                    drawObject(cylinder_,modelCylinderY(world({x,0.58f*s,z}),0.024f*s,0.52f*s),hullEdge,false);
            }
            glm::vec3 mastTop=world({0.45f*s,3.16f*s,0});
            for(glm::vec3 localEnd:{glm::vec3(-2.45f*s,0.47f*s,0),glm::vec3(2.45f*s,0.47f*s,0)}){
                glm::vec3 end=world(localEnd);
                drawObject(cylinder_,modelCylinderZ((mastTop+end)*0.5f,end-mastTop,0.014f*s,glm::length(end-mastTop)),rope,false);
            }
            glm::vec3 oar0=world({0.1f*s,0.66f*s,-0.22f*s}),oar1=world({1.7f*s,0.05f*s,-1.30f*s});
            drawObject(cylinder_,modelCylinderZ((oar0+oar1)*0.5f,oar1-oar0,0.033f*s,glm::length(oar1-oar0)),trunk,false);
            drawObject(leaf_,R*glm::translate(glm::mat4(1),{1.72f*s,0.04f*s,-1.31f*s})*glm::rotate(glm::mat4(1),glm::radians(63.0f),glm::vec3(0,1,0))*glm::scale(glm::mat4(1),{0.22f*s,0.24f*s,0.55f*s}),trunk,false);

            // Seated fisherman.
            drawObject(sphere_,R*glm::translate(glm::mat4(1),{1.15f*s,0.92f*s,0})*glm::scale(glm::mat4(1),{0.30f*s,0.36f*s,0.30f*s}),skinFar,false);
            drawObject(sphere_,R*glm::translate(glm::mat4(1),{1.15f*s,0.62f*s,0})*glm::scale(glm::mat4(1),{0.42f*s,0.54f*s,0.32f*s}),clothBlue,false);
        };
        boat({-29.0f,-0.08f,-59.2f}, 0.10f,0.82f,sailA);
        boat({  4.0f,-0.08f,-61.0f},-0.08f,1.18f,sailB);
        boat({ 31.0f,-0.08f,-58.5f}, 0.16f,0.90f,sailA);

        // Two loose bird flocks add life to the upper, formerly empty part of the frame.
        Material birdBody{{0.055f,0.060f,0.055f},0.78f,0.0f,0,false};
        Material birdWing{{0.11f,0.12f,0.10f},0.82f,0.0f,0,false};
        Material birdBeak{{0.64f,0.40f,0.08f},0.82f,0.0f,7,false};
        for(int i=0;i<18;++i){
            int flock=i<10?0:1;
            int j=flock==0?i:i-10;
            float side=(j&1)?1.0f:-1.0f;
            glm::vec3 p=flock==0
                ?glm::vec3(-26.0f+j*3.8f,11.5f+j*0.42f, -34.0f-std::fabs(j-4.5f)*1.45f)
                :glm::vec3( 22.0f+j*3.5f,14.0f-j*0.35f,  29.0f+std::fabs(j-3.5f)*1.30f);
            float yaw=flock==0?0.18f:-2.55f;
            glm::mat4 B=glm::translate(glm::mat4(1),p)*glm::rotate(glm::mat4(1),yaw,glm::vec3(0,1,0));
            drawObject(sphere_,B*glm::scale(glm::mat4(1),{0.22f,0.14f,0.62f}),birdBody,false);
            drawObject(sphere_,B*glm::translate(glm::mat4(1),{-0.32f,0.08f,0})
                *glm::rotate(glm::mat4(1), 0.24f*side,glm::vec3(0,0,1))*glm::scale(glm::mat4(1),{0.58f,0.055f,0.22f}),birdWing,false);
            drawObject(sphere_,B*glm::translate(glm::mat4(1),{ 0.32f,0.08f,0})
                *glm::rotate(glm::mat4(1),-0.24f*side,glm::vec3(0,0,1))*glm::scale(glm::mat4(1),{0.58f,0.055f,0.22f}),birdWing,false);
            drawObject(cone_,B*glm::translate(glm::mat4(1),{0,0,0.38f})*glm::scale(glm::mat4(1),{0.045f,0.045f,0.18f}),birdBeak,false);
            drawObject(cube_,B*glm::translate(glm::mat4(1),{-0.09f,0,-0.36f})*glm::rotate(glm::mat4(1),glm::radians(-9.0f),glm::vec3(0,1,0))*glm::scale(glm::mat4(1),{0.055f,0.035f,0.28f}),birdWing,false);
            drawObject(cube_,B*glm::translate(glm::mat4(1),{ 0.09f,0,-0.36f})*glm::rotate(glm::mat4(1),glm::radians( 9.0f),glm::vec3(0,1,0))*glm::scale(glm::mat4(1),{0.055f,0.035f,0.28f}),birdWing,false);
        }
#endif
    }
}

void SceneRenderer::drawFish(const Simulation& sim,bool shadowPass){
    Material fin{{0.12f,0.20f,0.23f},0.52f,0.02f,0,true};
    Material eye{{0.006f,0.008f,0.009f},0.22f,0.0f,0,false};
    Material eyeShine{{0.85f,0.90f,0.95f},0.08f,0.0f,0,false};
    Material mouth{{0.12f,0.055f,0.045f},0.56f,0.0f,0,false};
    Material gill{{0.055f,0.095f,0.105f},0.48f,0.0f,0,false};
    Material barbel{{0.070f,0.090f,0.085f},0.58f,0.0f,0,false};
    int n=0;
    for(const auto& f:sim.fish()){
        // Underwater fish cast very little useful terrain shadow; half-density shadow
        // proxies preserve performance while every agent remains visible in the beauty pass.
        if(shadowPass && (n&1)){ ++n; continue; }
        glm::vec3 pos=sim.fishPosition(f), dir=sim.fishDirection(f);
        glm::mat4 base=glm::translate(glm::mat4(1),pos)*alignZTo(dir);
        float s=f.scale;

        // Stable per-fish silver/olive/carp color variation prevents cloned-looking fish.
        uint32_t seed=(uint32_t)(f.pond*97+n*13+7);
        float hueJ=hash01(seed)*0.5f;
        float valJ=0.82f+0.30f*hash01(seed+11);
        glm::vec3 body0=glm::mix(glm::vec3(0.15f,0.27f,0.34f),glm::vec3(0.30f,0.34f,0.30f),hueJ)*valJ;
        glm::vec3 belly0=glm::mix(glm::vec3(0.55f,0.60f,0.58f),glm::vec3(0.70f,0.72f,0.62f),hueJ)*valJ;
        Material body{body0,0.28f+0.10f*hash01(seed+2),0.05f,10,true};
        Material belly{belly0,0.42f,0.02f,0,true};

        // A travelling spine wave moves the torso subtly and the tail strongly.
        float wave=sim.time()*7.2f*f.speed+f.phase;
        float swingMid=std::sin(wave)*0.10f;
        float swingTail=std::sin(wave-0.9f)*0.46f;

        // Tapered head, full torso and narrow caudal peduncle build a biological silhouette.
        drawObject(sphere_,base*glm::translate(glm::mat4(1),glm::vec3(0,0.015f*s,0.92f*s))*glm::scale(glm::mat4(1),glm::vec3(0.34f*s,0.30f*s,0.46f*s)),body,shadowPass);
        glm::mat4 torso=base*glm::rotate(glm::mat4(1),swingMid,glm::vec3(0,1,0));
        drawObject(sphere_,torso*glm::scale(glm::mat4(1),glm::vec3(0.52f*s,0.40f*s,1.05f*s)),body,shadowPass);
        if(!shadowPass)
            drawObject(sphere_,torso*glm::translate(glm::mat4(1),glm::vec3(0,-0.13f*s,0.05f*s))*glm::scale(glm::mat4(1),glm::vec3(0.38f*s,0.21f*s,0.86f*s)),belly,false);
        glm::mat4 peduncle=torso*glm::translate(glm::mat4(1),glm::vec3(0,0,-0.62f*s))*glm::rotate(glm::mat4(1),swingMid*0.6f,glm::vec3(0,1,0));
        drawObject(sphere_,peduncle*glm::scale(glm::mat4(1),glm::vec3(0.22f*s,0.17f*s,0.30f*s)),body,shadowPass);

        // Forked caudal fin follows the strongest part of the travelling wave.
        glm::mat4 tail=peduncle*glm::translate(glm::mat4(1),glm::vec3(0,0,-0.22f*s))*glm::rotate(glm::mat4(1),swingTail,glm::vec3(0,1,0));
        drawObject(cube_,tail*glm::translate(glm::mat4(1),glm::vec3(0,0.20f*s,-0.14f*s))*glm::rotate(glm::mat4(1),glm::radians(30.0f),glm::vec3(1,0,0))*glm::scale(glm::mat4(1),glm::vec3(0.060f*s,0.42f*s,0.46f*s)),fin,shadowPass);
        drawObject(cube_,tail*glm::translate(glm::mat4(1),glm::vec3(0,-0.20f*s,-0.14f*s))*glm::rotate(glm::mat4(1),glm::radians(-30.0f),glm::vec3(1,0,0))*glm::scale(glm::mat4(1),glm::vec3(0.060f*s,0.42f*s,0.46f*s)),fin,shadowPass);

        // Dorsal fin contributes to the shadow silhouette; smaller fins stay in the beauty pass.
        drawObject(cube_,torso*glm::translate(glm::mat4(1),glm::vec3(0,0.36f*s,0.05f*s))*glm::rotate(glm::mat4(1),glm::radians(12.0f),glm::vec3(1,0,0))*glm::scale(glm::mat4(1),glm::vec3(0.045f*s,0.20f*s,0.34f*s)),fin,shadowPass);
        if(!shadowPass){
            drawObject(cube_,torso*glm::translate(glm::mat4(1),glm::vec3(0,-0.30f*s,-0.30f*s))*glm::rotate(glm::mat4(1),glm::radians(-14.0f),glm::vec3(1,0,0))*glm::scale(glm::mat4(1),glm::vec3(0.040f*s,0.14f*s,0.20f*s)),fin,false);
            float pecFlap=std::sin(wave*1.6f+1.1f)*8.0f;
            drawObject(cube_,base*glm::translate(glm::mat4(1),glm::vec3(0.40f*s,-0.05f*s,0.42f*s))*glm::rotate(glm::mat4(1),glm::radians(-26.0f+pecFlap),glm::vec3(0,0,1))*glm::scale(glm::mat4(1),glm::vec3(0.30f*s,0.045f*s,0.24f*s)),fin,false);
            drawObject(cube_,base*glm::translate(glm::mat4(1),glm::vec3(-0.40f*s,-0.05f*s,0.42f*s))*glm::rotate(glm::mat4(1),glm::radians(26.0f-pecFlap),glm::vec3(0,0,1))*glm::scale(glm::mat4(1),glm::vec3(0.30f*s,0.045f*s,0.24f*s)),fin,false);

            // Both eyes, wet catch-lights and a small mouth remain visible from either side.
            glm::vec3 eyeOffset(0.22f*s,0.10f*s,0.78f*s);
            drawObject(sphere_,base*glm::translate(glm::mat4(1),eyeOffset)*glm::scale(glm::mat4(1),glm::vec3(0.075f*s)),eye,false);
            drawObject(sphere_,base*glm::translate(glm::mat4(1),glm::vec3(-eyeOffset.x,eyeOffset.y,eyeOffset.z))*glm::scale(glm::mat4(1),glm::vec3(0.075f*s)),eye,false);
            drawObject(sphere_,base*glm::translate(glm::mat4(1),eyeOffset+glm::vec3(0.02f*s,0.02f*s,0.03f*s))*glm::scale(glm::mat4(1),glm::vec3(0.022f*s)),eyeShine,false);
            drawObject(sphere_,base*glm::translate(glm::mat4(1),glm::vec3(-eyeOffset.x-0.02f*s,eyeOffset.y+0.02f*s,eyeOffset.z+0.03f*s))*glm::scale(glm::mat4(1),glm::vec3(0.022f*s)),eyeShine,false);
            drawObject(sphere_,base*glm::translate(glm::mat4(1),glm::vec3(0,-0.075f*s,1.15f*s))*glm::scale(glm::mat4(1),glm::vec3(0.13f*s,0.045f*s,0.055f*s)),mouth,false);

            // Gill covers, nostrils and fine barbels are especially characteristic of pangas/catfish.
            for(float side:{-1.0f,1.0f}){
                drawObject(sphere_,base*glm::translate(glm::mat4(1),glm::vec3(side*0.315f*s,0.015f*s,0.62f*s))*glm::scale(glm::mat4(1),glm::vec3(0.026f*s,0.24f*s,0.29f*s)),gill,false);
                drawObject(sphere_,base*glm::translate(glm::mat4(1),glm::vec3(side*0.10f*s,0.025f*s,1.16f*s))*glm::scale(glm::mat4(1),glm::vec3(0.030f*s)),eye,false);
                glm::vec3 a=glm::vec3(base*glm::vec4(side*0.12f*s,-0.08f*s,1.16f*s,1));
                glm::vec3 b=glm::vec3(base*glm::vec4(side*0.34f*s,-0.12f*s,1.50f*s,1));
                drawObject(cylinder_,modelCylinderZ((a+b)*0.5f,b-a,0.010f*s,glm::length(b-a)),barbel,false);
                glm::vec3 c=glm::vec3(base*glm::vec4(side*0.08f*s,-0.10f*s,1.12f*s,1));
                glm::vec3 d=glm::vec3(base*glm::vec4(side*0.18f*s,-0.18f*s,1.38f*s,1));
                drawObject(cylinder_,modelCylinderZ((c+d)*0.5f,d-c,0.008f*s,glm::length(d-c)),barbel,false);
            }
        }
        n++;
    }
}

void SceneRenderer::drawDebris(const Simulation& sim,bool shadowPass){
    Material mud{{0.19f,0.105f,0.035f},0.93f,0.0f,8,true};
    Material organic{{0.20f,0.25f,0.045f},0.89f,0.0f,8,true};
    Material twig{{0.28f,0.16f,0.052f},0.92f,0.0f,4,true};
    for(int p=0;p<3;++p){
        const auto& pond=sim.ponds()[p];
        float surface=pond.baseWaterY+pond.waterLevel;
        // Keep the waste cluster to one corner, so it reads as local ammonia/dust buildup.
        glm::vec3 corner=pond.center+glm::vec3(-pond.size.x*0.39f,0,pond.size.y*0.37f);
        int count=(int)(8+pond.waste*26.0f);
        for(int i=0;i<count;++i){
            uint32_t seed=(uint32_t)(p*809+i*43+13);
            float h1=hash01(seed), h2=hash01(seed+1), spread=(float)i/std::max(1,count-1);
            glm::vec3 pos=corner+glm::vec3((h1-0.5f)*1.35f*(1.0f-spread*0.25f),surface-0.05f-h2*0.10f,(h2-0.5f)*1.10f);
            float sz=0.05f+0.11f*hash01(seed+3);
            if(i%5==0){
                glm::mat4 m=glm::translate(glm::mat4(1),pos)*glm::rotate(glm::mat4(1),hash01(seed+4)*PI,glm::vec3(0,1,0))*glm::scale(glm::mat4(1),glm::vec3(sz*0.35f,sz*0.18f,sz*2.2f));
                drawObject(cube_,m,twig,shadowPass);
            } else if(i%2==0) drawObject(cube_,trs(pos,{sz*1.5f,sz*0.32f,sz}),organic,shadowPass);
            else drawObject(sphere_,trs(pos,{sz*1.05f,sz*0.44f,sz}),mud,shadowPass);
        }
    }
}

void SceneRenderer::drawSensors(const Simulation& sim,bool shadowPass){
    const auto& p=sim.ponds()[1]; float surface=p.baseWaterY+p.waterLevel;
    std::array<glm::vec3,6> colors={
        glm::vec3(0.98f,0.30f,0.10f),glm::vec3(0.70f,0.18f,0.72f),glm::vec3(0.98f,0.70f,0.08f),
        glm::vec3(0.12f,0.72f,0.40f),glm::vec3(0.10f,0.56f,0.96f),glm::vec3(0.70f,0.90f,0.98f)
    };
    Material steel{{0.38f,0.43f,0.45f},0.22f,0.82f,6,true};
    Material cable{{0.018f,0.021f,0.022f},0.55f,0.15f,0,true};
    // Make the Pond 2 probes clearly visible inside the water body.
    for(int i=0;i<6;++i){
        float row=(float)(i/3), col=(float)(i%3);
        glm::vec3 pos=p.center+glm::vec3(2.20f+col*1.25f,0,-2.05f+row*1.75f);
        drawObject(cylinder_,modelCylinderY(pos+glm::vec3(0,surface-0.08f,0),0.095f,2.05f),steel,shadowPass);
        Material tip{colors[i],0.22f,0.10f,6,false};
        drawObject(sphere_,trs(pos+glm::vec3(0,surface+0.84f,0),{0.26f,0.26f,0.26f}),tip,shadowPass);
        drawObject(cylinder_,modelCylinderY(pos+glm::vec3(0,surface-0.98f,0),0.14f,0.40f),tip,shadowPass);
        // Cable rising toward the bank/monitor station.
        glm::vec3 end=p.center+glm::vec3(5.8f,2.15f,-0.25f);
        glm::vec3 mid=(pos+glm::vec3(0,surface+0.84f,0)+end)*0.5f+glm::vec3(0,0.52f,0);
        glm::vec3 start=pos+glm::vec3(0,surface+0.84f,0);
        drawObject(cylinder_,modelCylinderZ((start+mid)*0.5f,mid-start,0.020f,glm::length(mid-start)),cable,shadowPass);
        drawObject(cylinder_,modelCylinderZ((mid+end)*0.5f,end-mid,0.020f,glm::length(end-mid)),cable,shadowPass);
    }
}

void SceneRenderer::drawMonitoringStation(const Simulation& sim,bool shadowPass){
    (void)sim;
    // Station beside Pond 1, visually echoing the reference touchscreen.
    glm::vec3 base(15.9f,0.0f,7.1f);
    Material steel{{0.32f,0.36f,0.37f},0.24f,0.82f,6,true};
    Material frame{{0.055f,0.065f,0.070f},0.26f,0.72f,6,true};
    Material screen{{0.025f,0.085f,0.12f},0.10f,0.10f,0,false};
    Material cyan{{0.03f,0.62f,0.92f},0.16f,0.15f,17,false};
    Material green{{0.10f,0.80f,0.34f},0.18f,0.08f,17,false};
    Material amber{{0.96f,0.62f,0.08f},0.18f,0.08f,17,false};
    Material concrete{{0.33f,0.34f,0.32f},0.96f,0.0f,13,true};
    Material rubber{{0.018f,0.020f,0.019f},0.72f,0.04f,0,true};

    drawObject(cylinder_,modelCylinderY(base+glm::vec3(0,1.25f,0),0.16f,2.5f),steel,shadowPass);
    drawObject(cube_,modelBox(base+glm::vec3(0,0.58f,0.02f),{0.72f,0.82f,0.46f}),frame,shadowPass);
    // Cabinet ventilation slots and a weather-sealed service door.
    for(int k=0;k<5;++k)
        drawObject(cube_,modelBox(base+glm::vec3(0,0.42f+k*0.09f,0.255f),{0.43f,0.025f,0.018f}),steel,shadowPass);
    glm::mat4 orient=glm::rotate(glm::mat4(1),glm::radians(-20.0f),glm::vec3(1,0,0))*glm::rotate(glm::mat4(1),glm::radians(-12.0f),glm::vec3(0,1,0));
    glm::mat4 panel=glm::translate(glm::mat4(1),base+glm::vec3(0,2.45f,0))*orient;
    drawObject(cube_,panel*glm::scale(glm::mat4(1),glm::vec3(3.55f,2.05f,0.20f)),frame,shadowPass);
    drawObject(cube_,panel*glm::translate(glm::mat4(1),glm::vec3(0,0,0.115f))*glm::scale(glm::mat4(1),glm::vec3(3.20f,1.70f,0.035f)),screen,shadowPass);
    // Screen dashboard bars and telemetry blocks.
    for(int i=0;i<6;++i){
        float x=-1.25f+(i%3)*1.25f, y=0.48f-(i/3)*0.70f;
        Material c=(i%3==0)?cyan:((i%3==1)?green:amber);
        drawObject(cube_,panel*glm::translate(glm::mat4(1),glm::vec3(x,y,0.145f))*glm::scale(glm::mat4(1),glm::vec3(0.86f,0.12f,0.018f)),c,shadowPass);
        drawObject(cube_,panel*glm::translate(glm::mat4(1),glm::vec3(x,y-0.22f,0.145f))*glm::scale(glm::mat4(1),glm::vec3(0.60f+0.18f*(i%2),0.055f,0.018f)),c,shadowPass);
    }
    // Recessed bezel screws, rain hood and diagonal support struts.
    for(float x:{-1.61f,1.61f}) for(float y:{-0.86f,0.86f})
        drawObject(sphere_,panel*glm::translate(glm::mat4(1),glm::vec3(x,y,0.145f))*glm::scale(glm::mat4(1),glm::vec3(0.075f)),steel,shadowPass);
    drawObject(cube_,panel*glm::translate(glm::mat4(1),glm::vec3(0,1.13f,-0.06f))*glm::rotate(glm::mat4(1),glm::radians(-8.0f),glm::vec3(1,0,0))*glm::scale(glm::mat4(1),glm::vec3(3.92f,0.10f,0.78f)),steel,shadowPass);
    for(float x:{-1.28f,1.28f}){
        glm::vec3 a=base+glm::vec3(0,1.20f,0), b=glm::vec3(panel*glm::vec4(x,-0.88f,-0.12f,1));
        drawObject(cylinder_,modelCylinderZ((a+b)*0.5f,b-a,0.035f,glm::length(b-a)),steel,shadowPass);
    }

    // Power conduit, warning beacon and a weathered concrete footing.
    glm::vec3 cableA=base+glm::vec3(0.34f,0.54f,0),cableB=base+glm::vec3(0.55f,0.08f,0.34f);
    drawObject(cylinder_,modelCylinderZ((cableA+cableB)*0.5f,cableB-cableA,0.034f,glm::length(cableB-cableA)),rubber,shadowPass);
    drawObject(cylinder_,modelCylinderY(base+glm::vec3(0,3.58f,0),0.055f,0.20f),steel,shadowPass);
    drawObject(sphere_,trs(base+glm::vec3(0,3.72f,0),{0.18f,0.22f,0.18f}),amber,shadowPass);
    drawObject(cube_,modelBox(base+glm::vec3(0,0.08f,0),{1.36f,0.16f,1.18f}),concrete,shadowPass);
}

void SceneRenderer::drawFans(const Simulation& sim,bool shadowPass){
    Material steel{{0.19f,0.215f,0.22f},0.23f,0.78f,6,true};
    Material blade{{0.105f,0.28f,0.34f},0.19f,0.66f,6,true};
    Material motor{{0.12f,0.135f,0.14f},0.31f,0.68f,6,true};
    Material darkMetal{{0.055f,0.065f,0.07f},0.28f,0.72f,6,true};
    Material intake{{0.16f,0.18f,0.19f},0.34f,0.70f,6,true};
    Material concrete{{0.32f,0.33f,0.31f},0.97f,0.0f,13,true};
    Material rust{{0.39f,0.13f,0.040f},0.90f,0.18f,6,true};
    Material cable{{0.018f,0.020f,0.020f},0.70f,0.03f,0,true};
    Material activeLED{{0.055f,0.92f,0.24f},0.15f,0.0f,17,false};
    Material idleLED{{0.34f,0.025f,0.015f},0.46f,0.0f,0,false};
    for(int i=0;i<3;++i){
        // User-requested layout: the fan/pump assembly sits at the START of each pipe,
        // near the pond edge, instead of floating in the middle of the pipe span.
        float t=0.030f;
        glm::vec3 pipeP=pipes_[i].point(t), d=pipes_[i].tangent(t);
        glm::vec3 side=glm::cross(glm::vec3(0,1,0),d);
        if(glm::length2(side)<0.001f) side=glm::vec3(1,0,0); else side=glm::normalize(side);
        glm::vec3 up(0,1,0);

        // Place the main pump/fan body slightly before the pipe inlet and a little lower,
        // so it visually reads as the pipe entrance machine mounted at the start.
        glm::vec3 anchor=pipeP - d*0.18f - up*0.05f;
        glm::mat4 base=glm::translate(glm::mat4(1),anchor)*alignZTo(d);

        drawObject(sphere_,base*glm::translate(glm::mat4(1),glm::vec3(0,0.31f,0.18f))*glm::scale(glm::mat4(1),glm::vec3(0.12f)),sim.fans()[i].on?activeLED:idleLED,shadowPass);

        // Pump housing / intake body.
        drawObject(cylinder_,base*glm::scale(glm::mat4(1),glm::vec3(0.28f,0.28f,0.98f)),darkMetal,shadowPass);
        drawObject(cylinder_,base*glm::translate(glm::mat4(1),glm::vec3(0,0,0.34f))*glm::scale(glm::mat4(1),glm::vec3(0.38f,0.38f,0.18f)),steel,shadowPass);
        drawObject(cylinder_,base*glm::translate(glm::mat4(1),glm::vec3(0,0,-0.32f))*glm::scale(glm::mat4(1),glm::vec3(0.42f,0.42f,0.20f)),intake,shadowPass);

        // Visible rotor at the intake/start, not mid-pipe.
        glm::mat4 spin=base*glm::translate(glm::mat4(1),glm::vec3(0,0,-0.22f))*glm::rotate(glm::mat4(1),sim.fans()[i].angle,glm::vec3(0,0,1));
        for(int b=0;b<5;++b){
            glm::mat4 bladeM=spin
                *glm::rotate(glm::mat4(1),b*2.0f*PI/5.0f,glm::vec3(0,0,1))
                *glm::rotate(glm::mat4(1),glm::radians(90.0f),glm::vec3(1,0,0))
                *glm::translate(glm::mat4(1),glm::vec3(0,0,0.02f))
                *glm::scale(glm::mat4(1),glm::vec3(0.42f,0.25f,0.54f));
            drawObject(leaf_,bladeM,blade,shadowPass);
        }
        drawObject(cylinder_,base*glm::translate(glm::mat4(1),glm::vec3(0,0,-0.22f))*glm::scale(glm::mat4(1),glm::vec3(0.10f,0.10f,0.08f)),steel,shadowPass);

        // Intake guard / support ring.
        for(int b=0;b<4;++b){
            glm::mat4 brace=base
                *glm::translate(glm::mat4(1),glm::vec3(0,0,-0.24f))
                *glm::rotate(glm::mat4(1),b*PI/4.0f,glm::vec3(0,0,1))
                *glm::scale(glm::mat4(1),glm::vec3(0.74f,0.030f,0.030f));
            drawObject(cube_,brace,steel,shadowPass);
        }

        // External motor mounted to the side of the entrance assembly.
        glm::vec3 mp=anchor+side*0.72f+up*0.24f;
        drawObject(cylinder_,modelCylinderZ(mp,side,0.28f,0.64f),motor,shadowPass);
        drawObject(cylinder_,modelCylinderZ((anchor+mp)*0.5f,mp-anchor,0.055f,glm::length(mp-anchor)),steel,shadowPass);
        drawObject(cylinder_,modelCylinderZ(mp+side*0.32f,side,0.31f,0.055f),steel,shadowPass);

        // Cooling fins, fasteners and slight corrosion sell the scale and age of the motor.
        for(int k=-3;k<=3;++k)
            drawObject(cylinder_,modelCylinderZ(mp+side*(k*0.074f),side,0.305f,0.024f),(k==3)?rust:steel,shadowPass);
        for(int b=0;b<8;++b){
            float a=(float)b*PI*0.25f;
            glm::mat4 boltM=base*glm::translate(glm::mat4(1),glm::vec3(std::cos(a)*0.34f,std::sin(a)*0.34f,0.345f))*glm::scale(glm::mat4(1),glm::vec3(0.065f));
            drawObject(sphere_,boltM,(b%3==0)?rust:steel,shadowPass);
        }

        // Small concrete base under the machine so it looks mounted at the pond side.
        glm::vec3 plinth=anchor-d*0.04f-up*0.28f;
        drawObject(cube_,modelBox(plinth,{1.05f,0.18f,0.82f}),concrete,shadowPass);
        for(float sgn:{-1.0f,1.0f}){
            glm::vec3 foot=mp+side*sgn*0.18f-up*0.30f;
            drawObject(cube_,modelBox(foot,{0.24f,0.12f,0.34f}),steel,shadowPass);
        }
        glm::vec3 cableA=mp+side*0.28f-up*0.05f;
        glm::vec3 cableB=anchor+side*1.10f-up*0.35f;
        drawObject(cylinder_,modelCylinderZ((cableA+cableB)*0.5f,cableB-cableA,0.030f,glm::length(cableB-cableA)),cable,shadowPass);
    }
}

void SceneRenderer::drawOpaqueGeometry(const Simulation& sim,const RenderGlobals& g,bool shadowPass){
    if(shadowPass){ shadow_.use(); shadow_.set("uLightSpace",g.lightSpace); shadow_.set("uTime",sim.time()); }
    else beginPBR(g,sim.time());
    drawVillage(shadowPass);
    for(int i=0;i<3;++i) drawPondStructure(sim.ponds()[i],i,shadowPass);
    drawFish(sim,shadowPass);
    drawDebris(sim,shadowPass);
    drawSensors(sim,shadowPass);
    drawFans(sim,shadowPass);
}

void SceneRenderer::drawSky(const RenderGlobals& g,float time){
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDepthMask(GL_FALSE);
    sky_.use();
    glm::mat4 inv=glm::inverse(g.proj*g.view);
    sky_.set("uInvViewProj",inv); sky_.set("uCameraPos",g.cameraPos); sky_.set("uSunDir",sunDir_); sky_.set("uTime",time);
    sky_.set("uNightBlend",nightMode_?1.0f:0.0f);
    screenQuad_.draw();
    glDepthMask(GL_TRUE); glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST);
}

void SceneRenderer::drawWater(const Simulation& sim,const RenderGlobals& g){
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_TRUE);
    water_.use();
    water_.set("uView",g.view); water_.set("uProj",g.proj); water_.set("uCameraPos",g.cameraPos);
    water_.set("uSunDir",sunDir_); water_.set("uSunColor",sunColor_); water_.set("uTime",sim.time());
    water_.set("uScreenSize",glm::vec2((float)fboW_,(float)fboH_)); water_.set("uNear",0.08f); water_.set("uFar",190.0f);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,opaqueColor_); water_.set("uSceneColor",0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,opaqueDepth_); water_.set("uSceneDepth",1);
    for(int i=0;i<3;++i){
        const auto& p=sim.ponds()[i];
        float localFlow=0.0f; for(const auto& f:sim.fans()) if(f.fromPond==i||f.toPond==i) localFlow+=f.speed*0.5f;
        glm::vec3 outletA=p.center,outletB=p.center; float strengthA=0.0f,strengthB=0.0f; int found=0;
        for(int f=0;f<3;++f){
            const auto& fan=sim.fans()[f];
            if(fan.fromPond==i || fan.toPond==i){
                glm::vec3 ep=(fan.fromPond==i)?pipes_[f].point(0.012f):pipes_[f].point(0.988f);
                if(found==0){outletA=ep;strengthA=fan.speed;} else {outletB=ep;strengthB=fan.speed;} found++;
            }
        }
        float wy=p.baseWaterY+p.waterLevel;
        glm::mat4 m=glm::translate(glm::mat4(1),glm::vec3(p.center.x,wy,p.center.z))*glm::scale(glm::mat4(1),glm::vec3(p.size.x,1.0f,p.size.y));
        water_.set("uModel",m); water_.set("uFlowStrength",clampf(localFlow,0,1)); water_.set("uTurbidity",p.waste); water_.set("uQuality",sim.pondQuality(i));
        water_.set("uOutletA",outletA); water_.set("uOutletB",outletB); water_.set("uOutletStrengthA",strengthA); water_.set("uOutletStrengthB",strengthB);
        waterPlane_.draw();
    }
    glDisable(GL_BLEND);
}

void SceneRenderer::drawTransparentPipes(const Simulation& sim,const RenderGlobals& g){
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE); glDisable(GL_CULL_FACE);
    glass_.use(); glass_.set("uView",g.view); glass_.set("uProj",g.proj); glass_.set("uCameraPos",g.cameraPos);
    glass_.set("uSunDir",sunDir_); glass_.set("uScreenSize",glm::vec2((float)fboW_,(float)fboH_));
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,opaqueColor_); glass_.set("uSceneColor",0);
    for(int i=0;i<3;++i){
        float speed=sim.fans()[i].speed;
        // Thick, clearly visible water body filling most of the pipe volume.
        glass_.set("uModel",glm::mat4(1));
        glass_.set("uTint",glm::vec3(0.06f,0.44f,0.98f));
        glass_.set("uAlpha",0.48f+0.24f*speed);
        glass_.set("uPulse",0.68f+0.22f*speed);
        pipeInner_[i].draw();

        if(speed>0.025f){
            // Overlapping bright flow slugs form a continuous moving core instead of dotted beads.
            const int slugs=15;
            const float slugSpan=0.11f;
            for(int s=0;s<slugs;++s){
                float t0=std::fmod((float)s/slugs + sim.time()*(0.11f+0.16f*speed)*speed,1.0f);
                float t1=std::min(t0+slugSpan,0.995f);
                glm::vec3 p0=pipes_[i].point(t0);
                glm::vec3 p1=pipes_[i].point(t1);
                glm::vec3 dir=p1-p0;
                float len=glm::length(dir);
                if(len>0.001f){
                    glm::vec3 mid=(p0+p1)*0.5f;
                    float rad=0.14f+0.03f*speed;
                    glass_.set("uModel",modelCylinderZ(mid,dir,rad,len));
                    glass_.set("uTint",glm::vec3(0.50f,0.86f,1.0f));
                    glass_.set("uAlpha",0.42f+0.30f*speed);
                    glass_.set("uPulse",0.85f);
                    cylinder_.draw();
                }
            }
            // Fine bubbles within the pipe for extra flow visibility.
            const int beads=120;
            for(int b=0;b<beads;++b){
                float t=std::fmod((float)b/beads + sim.time()*(0.18f+0.26f*speed)*speed,1.0f);
                glm::vec3 p=pipes_[i].point(t);
                float pulse=0.5f+0.5f*std::sin(b*1.83f+sim.time()*10.0f*speed);
                float size=0.030f+0.028f*pulse;
                glass_.set("uModel",trs(p,glm::vec3(size)));
                glass_.set("uTint",glm::vec3(0.84f,0.98f,1.0f)); glass_.set("uAlpha",0.18f+0.30f*speed); glass_.set("uPulse",pulse);
                sphere_.draw();
            }

            // Short inlet/outlet jets make both ends look truly submerged in the pond water.
            glm::vec3 in=pipes_[i].point(0.008f), inTan=glm::normalize(pipes_[i].tangent(0.008f));
            glass_.set("uModel",modelCylinderZ(in-inTan*(0.24f+0.08f*speed),inTan,0.12f+0.05f*speed,0.52f+0.16f*speed));
            glass_.set("uTint",glm::vec3(0.58f,0.90f,1.0f));
            glass_.set("uAlpha",0.30f+0.22f*speed);
            glass_.set("uPulse",1.0f);
            cylinder_.draw();
            glm::vec3 out=pipes_[i].point(0.992f), tan=glm::normalize(pipes_[i].tangent(0.992f));
            glass_.set("uModel",modelCylinderZ(out+tan*(0.28f+0.10f*speed),tan,0.11f+0.05f*speed,0.60f+0.18f*speed));
            glass_.set("uTint",glm::vec3(0.58f,0.90f,1.0f));
            glass_.set("uAlpha",0.34f+0.24f*speed);
            glass_.set("uPulse",1.0f);
            cylinder_.draw();
        }
        // Thick clear acrylic/glass shell: refractive center with bright Fresnel rim.
        glass_.set("uModel",glm::mat4(1)); glass_.set("uTint",glm::vec3(0.88f,0.96f,1.0f)); glass_.set("uAlpha",0.28f); glass_.set("uPulse",0.08f);
        pipeOuter_[i].draw();
    }
    glEnable(GL_CULL_FACE); glDepthMask(GL_TRUE); glDisable(GL_BLEND);
}

void SceneRenderer::drawBloom(){
    int bw=std::max(1,fboW_/2), bh=std::max(1,fboH_/2);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_BLEND);

    glBindFramebuffer(GL_FRAMEBUFFER,bloomFBO_[0]); glViewport(0,0,bw,bh); glClear(GL_COLOR_BUFFER_BIT);
    bright_.use(); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,sceneColor_); bright_.set("uScene",0); screenQuad_.draw();

    bool horizontal=true; int last=0;
    blur_.use(); blur_.set("uScreenSize",glm::vec2((float)bw,(float)bh));
    for(int pass=0;pass<8;++pass){
        int dst=horizontal?1:0;
        glBindFramebuffer(GL_FRAMEBUFFER,bloomFBO_[dst]); glViewport(0,0,bw,bh);
        blur_.set("uDirection",horizontal?glm::vec2(1,0):glm::vec2(0,1));
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,bloomTex_[last]); blur_.set("uImage",0);
        screenQuad_.draw();
        last=dst; horizontal=!horizontal;
    }
    // Guarantee final bloom lives in texture 0 for drawPost.
    if(last!=0){
        glBindFramebuffer(GL_READ_FRAMEBUFFER,bloomFBO_[last]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,bloomFBO_[0]);
        glBlitFramebuffer(0,0,bw,bh,0,0,bw,bh,GL_COLOR_BUFFER_BIT,GL_LINEAR);
    }
    glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST);
}

void SceneRenderer::drawPost(float time){
    glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,width_,height_);
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    post_.use(); post_.set("uScreenSize",glm::vec2((float)width_,(float)height_)); post_.set("uTime",time); post_.set("uExposure",0.96f);
    post_.set("uNear",0.08f); post_.set("uFar",190.0f);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,sceneColor_); post_.set("uScene",0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,bloomTex_[0]); post_.set("uBloom",1);
    glActiveTexture(GL_TEXTURE0+2); glBindTexture(GL_TEXTURE_2D,opaqueDepth_); post_.set("uDepth",2);
    screenQuad_.draw();
    glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST);
}

void SceneRenderer::render(const Simulation& sim,const CameraRig& camera){
    resize(width_,height_);
    float aspect=(float)width_/height_;
    RenderGlobals g{camera.view(),camera.projection(aspect),lightSpaceMatrix(),camera.position()};

    glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);

    // 1) High-resolution directional shadow pass.
    glBindFramebuffer(GL_FRAMEBUFFER,shadowFBO_); glViewport(0,0,shadowSize_,shadowSize_); glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT); drawOpaqueGeometry(sim,g,true); glCullFace(GL_BACK);

    // 2) Opaque HDR pass: sky + PBR rural environment + fish/equipment.
    glBindFramebuffer(GL_FRAMEBUFFER,opaqueFBO_); glViewport(0,0,fboW_,fboH_);
    glClearColor(0.35f,0.58f,0.72f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    drawSky(g,sim.time()); drawOpaqueGeometry(sim,g,false);

    // 3) Copy opaque buffers, then refractive water and glass pipes.
    glBindFramebuffer(GL_READ_FRAMEBUFFER,opaqueFBO_); glBindFramebuffer(GL_DRAW_FRAMEBUFFER,sceneFBO_);
    glBlitFramebuffer(0,0,fboW_,fboH_,0,0,fboW_,fboH_,GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT,GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER,sceneFBO_); glViewport(0,0,fboW_,fboH_);
    drawWater(sim,g); drawTransparentPipes(sim,g);

    // 4) Half-resolution bloom + ACES/photographic final pass.
    drawBloom();
    drawPost(sim.time());
}

void SceneRenderer::appendWorldLabels(UI& ui,const Simulation& sim,const CameraRig& camera,int width,int height) const {
    glm::mat4 vp=camera.projection((float)width/height)*camera.view();
    const glm::vec4 white(0.96f,0.985f,1.0f,0.98f), cyan(0.30f,0.86f,1.0f,0.98f);
    const glm::vec4 amber(1.0f,0.76f,0.16f,0.98f), lime(0.38f,1.0f,0.56f,0.98f);
    for(int i=0;i<3;++i){
        const auto& p=sim.ponds()[i]; bool vis=false;
        glm::vec3 s=projectToScreen(p.center+glm::vec3(0,p.baseWaterY+p.waterLevel+1.65f,0),vp,width,height,vis);
        if(vis){
            std::string pondLabel=p.name;
            float tw=ui.textWidth(1.55f,pondLabel);
            ui.rect(s.x-tw*0.5f-8,s.y-5,tw+16,24,{0.008f,0.025f,0.035f,0.72f});
            ui.rectOutline(s.x-tw*0.5f-8,s.y-5,tw+16,24,1.0f,{0.45f,0.70f,0.78f,0.44f});
            ui.text(s.x-tw*0.5f,s.y,1.55f,pondLabel,white);
        }
        // Corner waste/ammonia tag for each pond.
        glm::vec3 debrisAnchor=p.center+glm::vec3(-p.size.x*0.39f,p.baseWaterY+p.waterLevel+0.44f,p.size.y*0.37f);
        s=projectToScreen(debrisAnchor,vp,width,height,vis);
        if(vis){
            std::string dirt="AMMONIA NH3 / DUST";
            float tw=ui.textWidth(1.00f,dirt);
            ui.rect(s.x-tw*0.5f-5,s.y-2,tw+10,16,{0.08f,0.06f,0.02f,0.78f});
            ui.text(s.x-tw*0.5f,s.y,1.00f,dirt,amber);
        }
    }
    for(int i=0;i<3;++i){
        bool vis=false; glm::vec3 p=pipes_[i].point(0.50f); glm::vec3 s=projectToScreen(p+glm::vec3(0,0.84f,0),vp,width,height,vis);
        if(vis){
            std::string label="FAN "+std::to_string(i+1)+(sim.fans()[i].on?" ON":" OFF"); float tw=ui.textWidth(1.30f,label);
            ui.rect(s.x-tw*0.5f-5,s.y-3,tw+10,18,{0.015f,0.03f,0.035f,0.68f});
            ui.text(s.x-tw*0.5f,s.y,1.30f,label,sim.fans()[i].on?glm::vec4(0.30f,1.0f,0.48f,1):glm::vec4(1.0f,0.43f,0.30f,1));
        }
    }
    bool vis=false; const auto& p=sim.ponds()[1];
    glm::vec3 s=projectToScreen(p.center+glm::vec3(4.2f,p.baseWaterY+p.waterLevel+1.85f,-2.0f),vp,width,height,vis);
    if(vis){
        std::string label="POND 2 - 6 LIVE SENSORS"; float tw=ui.textWidth(1.20f,label);
        ui.rect(s.x-tw*0.5f-5,s.y-3,tw+10,17,{0.015f,0.03f,0.04f,0.68f}); ui.text(s.x-tw*0.5f,s.y,1.20f,label,cyan);
    }
    const char* sensorNames[6] = {"TEMP","NH3","PRESS","PH","DO","LEVEL"};
    for(int i=0;i<6;++i){
        float row=(float)(i/3), col=(float)(i%3);
        glm::vec3 sensorPos=p.center+glm::vec3(2.20f+col*1.25f,p.baseWaterY+p.waterLevel+1.10f,-2.05f+row*1.75f);
        s=projectToScreen(sensorPos,vp,width,height,vis);
        if(vis){
            std::string label=sensorNames[i];
            float tw=ui.textWidth(0.92f,label);
            ui.rect(s.x-tw*0.5f-4,s.y-2,tw+8,14,{0.010f,0.040f,0.055f,0.72f});
            ui.text(s.x-tw*0.5f,s.y,0.92f,label,lime);
        }
    }
}

} // namespace aqua
