#include <GLFW/glfw3.h>

#include "arcball.h"
#include "camera3d.h"
#include "cube.h"
#include "error.h"
#include "node.h"
#include "phongmaterial.h"
#include "pipeline.h"
#include "renderer.h"
#include "sampler.h"
#include "scene.h"
#include "shader.h"
#include "sphere.h"
#include "spotlight.h"
#include "state.h"
#include "texture.h"
#include "textureset.h"
#include "transform.h"
#include "wgpucontext.h"

#include <cassert>
#include <cstdio>

static float viewer_pos[3] = {2.2f, 3.4f, 4.2f};

static WgpuContextPtr ctx;
static RendererPtr renderer;
static ScenePtr scene;
static Camera3DPtr camera;
static ArcballPtr arcball;

static const float kCutHeight = 1.65f;
static const float kFogDensity = 0.1f;

struct ShaderSetup {
  ShaderPtr shader;
  PipelinePtr pipeline;
};

static ShaderSetup MakeShaderSetup(SpotLightPtr spot, bool clip,
                                   WGPUCullMode cull) {
  ShaderPtr shader = Shader::Make(
      ctx->GetDevice(), "../shaders/ilum_frag/lit_proj2.wgsl", spot, "world");

  VertexBufferSpec coordBuf, normalBuf, tangentBuf, texcoordBuf;
  coordBuf.arrayStride = 3 * 4;
  coordBuf.attributes.push_back(
      {std::string("coord"), std::nullopt, WGPUVertexFormat_Float32x3, 0});
  normalBuf.arrayStride = 3 * 4;
  normalBuf.attributes.push_back(
      {std::string("normal"), std::nullopt, WGPUVertexFormat_Float32x3, 0});
  tangentBuf.arrayStride = 3 * 4;
  tangentBuf.attributes.push_back(
      {std::string("tangent"), std::nullopt, WGPUVertexFormat_Float32x3, 0});
  texcoordBuf.arrayStride = 2 * 4;
  texcoordBuf.attributes.push_back(
      {std::string("texcoord"), std::nullopt, WGPUVertexFormat_Float32x2, 0});
  shader->SetVertexBuffers({coordBuf, normalBuf, tangentBuf, texcoordBuf});

  shader->SetValue("second_light_enabled", 1.0f);
  shader->SetValue("second_light_pos", Vec4{-2.8f, 3.2f, -1.8f, 1.0f});
  shader->SetValue("second_light_color", Vec3{0.80f, 0.70f, 0.95f});

  shader->SetValue("clip_plane_enabled", clip ? 1.0f : 0.0f);
  shader->SetValue("clip_plane", Vec4{0.0f, -1.0f, 0.0f, kCutHeight});

  shader->SetValue("fog_enabled", 1.0f);
  shader->SetValue("fog_density", kFogDensity);
  shader->SetValue("fog_color", Vec3{0.30f, 0.34f, 0.42f});

  PipelineDesc pdesc;
  WGPUPrimitiveState primitive = {};
  primitive.cullMode = cull;
  pdesc.primitive = primitive;
  PipelinePtr pipeline = Pipeline::Make(shader, ctx->GetSurfaceFormat(), pdesc);

  return {shader, pipeline};
}

static void initialize(void) {
  camera = Camera3D::Make(viewer_pos[0], viewer_pos[1], viewer_pos[2]);
  camera->SetCenter(0.0f, 1.2f, 0.0f);
  arcball = camera->CreateArcball();

  SpotLightPtr spot = SpotLight::Make(0.3f, 4.4f, 1.8f, -0.15f, -2.8f, -1.5f,
                                      28.0f, 16.0f, "world");
  spot->SetAmbient(0.12f, 0.12f, 0.14f);
  spot->SetDiffuse(1.0f, 1.0f, 1.0f);
  spot->SetSpecular(1.0f, 1.0f, 1.0f);

  CubePtr cube = Cube::Make(ctx->GetDevice());
  SpherePtr sphere = Sphere::Make(ctx->GetDevice(), 64, 64);

  ShaderSetup main = MakeShaderSetup(spot, false, WGPUCullMode_Back);
  ShaderSetup cut = MakeShaderSetup(spot, true, WGPUCullMode_None);

  SamplerPtr defaultSampler = Sampler::Make(ctx->GetDevice(), "decal_sampler");

  TexturePtr whiteTex = Texture::Make(ctx->GetDevice(), "decal_texture",
                                      Vec4{1.0f, 1.0f, 1.0f, 1.0f});
  TextureSetPtr texSetSolid =
      TextureSet::Make({whiteTex.get(), defaultSampler.get()});
  main.shader->AddTextureSet(texSetSolid.get());

  TexturePtr woodTex =
      Texture::Make(ctx->GetDevice(), "decal_texture", "../images/wood.jpg");
  TextureSetPtr texSetMadeira =
      TextureSet::Make({woodTex.get(), defaultSampler.get()});
  main.shader->AddTextureSet(texSetMadeira.get());

  TexturePtr snowTex =
      Texture::Make(ctx->GetDevice(), "decal_texture", "../images/snow.jpg");
  TextureSetPtr texSetCaixa =
      TextureSet::Make({snowTex.get(), defaultSampler.get()});
  main.shader->AddTextureSet(texSetCaixa.get());

  TexturePtr barrelTex =
      Texture::Make(ctx->GetDevice(), "decal_texture", "../images/barrel.jpg");
  TextureSetPtr texSetBarrel =
      TextureSet::Make({barrelTex.get(), defaultSampler.get()});
  main.shader->AddTextureSet(texSetBarrel.get());

  TexturePtr mudTex =
      Texture::Make(ctx->GetDevice(), "decal_texture", "../images/mud.jpg");
  TextureSetPtr texSetMud =
      TextureSet::Make({mudTex.get(), defaultSampler.get()});
  cut.shader->AddTextureSet(texSetMud.get());

  PhongMaterialPtr matTampo = PhongMaterial::Make(1.0f, 1.0f, 1.0f);
  matTampo->SetShininess(8.0f);

  PhongMaterialPtr matPeRosa = PhongMaterial::Make(0.95f, 0.40f, 0.70f);
  matPeRosa->SetShininess(8.0f);
  PhongMaterialPtr matPeRoxo = PhongMaterial::Make(0.60f, 0.15f, 0.85f);
  matPeRoxo->SetShininess(8.0f);
  PhongMaterialPtr matPeAzul = PhongMaterial::Make(0.15f, 0.35f, 0.90f);
  matPeAzul->SetShininess(8.0f);
  PhongMaterialPtr matPeCiano = PhongMaterial::Make(0.00f, 0.80f, 0.85f);
  matPeCiano->SetShininess(8.0f);

  PhongMaterialPtr matCaixa = PhongMaterial::Make(1.0f, 1.0f, 1.0f);
  matCaixa->SetSpecular(0.8f, 0.85f, 0.95f);
  matCaixa->SetShininess(48.0f);

  PhongMaterialPtr matEsferaBarrel = PhongMaterial::Make(1.0f, 1.0f, 1.0f);
  matEsferaBarrel->SetSpecular(0.7f, 0.7f, 0.7f);
  matEsferaBarrel->SetShininess(32.0f);

  PhongMaterialPtr matEsferaMud = PhongMaterial::Make(1.0f, 1.0f, 1.0f);
  matEsferaMud->SetSpecular(0.7f, 0.7f, 0.7f);
  matEsferaMud->SetShininess(40.0f);

  main.shader->AddMaterial(matTampo.get());
  main.shader->AddMaterial(matPeRosa.get());
  main.shader->AddMaterial(matPeRoxo.get());
  main.shader->AddMaterial(matPeAzul.get());
  main.shader->AddMaterial(matPeCiano.get());
  main.shader->AddMaterial(matCaixa.get());
  main.shader->AddMaterial(matEsferaBarrel.get());
  cut.shader->AddMaterial(matEsferaMud.get());

  TransformPtr trfTampo = Transform::Make();
  trfTampo->Translate(0.0f, 1.2f, 0.0f);
  trfTampo->Scale(3.6f, 0.15f, 2.4f);
  NodePtr nodeTampo = Node::Make(trfTampo, {matTampo, texSetMadeira}, {cube});

  float legOffsetX = 1.6f;
  float legOffsetZ = 1.0f;
  float legWidth = 0.18f;
  float legHeight = 1.2f;

  TransformPtr trfPe1 = Transform::Make();
  trfPe1->Translate(-legOffsetX, 0.0f, legOffsetZ);
  trfPe1->Scale(legWidth, legHeight, legWidth);
  NodePtr nodePe1 = Node::Make(trfPe1, {matPeRosa, texSetSolid}, {cube});

  TransformPtr trfPe2 = Transform::Make();
  trfPe2->Translate(legOffsetX, 0.0f, legOffsetZ);
  trfPe2->Scale(legWidth, legHeight, legWidth);
  NodePtr nodePe2 = Node::Make(trfPe2, {matPeRoxo, texSetSolid}, {cube});

  TransformPtr trfPe3 = Transform::Make();
  trfPe3->Translate(-legOffsetX, 0.0f, -legOffsetZ);
  trfPe3->Scale(legWidth, legHeight, legWidth);
  NodePtr nodePe3 = Node::Make(trfPe3, {matPeAzul, texSetSolid}, {cube});

  TransformPtr trfPe4 = Transform::Make();
  trfPe4->Translate(legOffsetX, 0.0f, -legOffsetZ);
  trfPe4->Scale(legWidth, legHeight, legWidth);
  NodePtr nodePe4 = Node::Make(trfPe4, {matPeCiano, texSetSolid}, {cube});

  TransformPtr trfCaixaPai = Transform::Make();
  trfCaixaPai->Translate(-0.55f, 1.35f, -0.15f);
  trfCaixaPai->Rotate(25.0f, 0.0f, 1.0f, 0.0f);

  TransformPtr trfCaixa = Transform::Make();
  trfCaixa->Scale(0.75f, 0.50f, 0.75f);
  NodePtr nodeCaixa = Node::Make(trfCaixa, {matCaixa, texSetCaixa}, {cube});

  TransformPtr trfEsferaBarrel = Transform::Make();
  trfEsferaBarrel->Translate(0.0f, 0.72f, 0.0f);
  trfEsferaBarrel->Scale(0.22f, 0.22f, 0.22f);
  NodePtr nodeEsferaBarrel =
      Node::Make(trfEsferaBarrel, {matEsferaBarrel, texSetBarrel}, {sphere});

  NodePtr nodeCaixaAgrupada =
      Node::Make(trfCaixaPai, {}, {}, {nodeCaixa, nodeEsferaBarrel});

  TransformPtr trfEsferaMud = Transform::Make();
  trfEsferaMud->Translate(0.75f, 1.65f, 0.25f);
  trfEsferaMud->Rotate(45.0f, 0.0f, 1.0f, 0.0f);
  trfEsferaMud->Scale(0.30f, 0.30f, 0.30f);
  NodePtr nodeEsferaMud = Node::Make(cut.pipeline, trfEsferaMud,
                                     {matEsferaMud, texSetMud}, {sphere});

  NodePtr root =
      Node::Make(main.pipeline, {nodeTampo, nodePe1, nodePe2, nodePe3, nodePe4,
                                 nodeCaixaAgrupada, nodeEsferaMud});

  scene = Scene::Make(root);
}

static void display() {
  WGPUSurfaceTexture surfaceTex = ctx->GetCurrentTexture();
  if (surfaceTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
      surfaceTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
    return;
  renderer->Render(surfaceTex.texture, scene, camera);
  ctx->Present();
  wgpuTextureRelease(surfaceTex.texture);
}

static void error(int code, const char *msg) {
  printf("GLFW error %d: %s\n", code, msg);
  glfwTerminate();
  exit(0);
}

static void resize(GLFWwindow *win, int width, int height) {
  (void)win;
  if (width > 0 && height > 0)
    ctx->Configure(width, height);
}

static void cursorpos(GLFWwindow *win, double x, double y) {
  int wn_w, wn_h, fb_w, fb_h;
  glfwGetWindowSize(win, &wn_w, &wn_h);
  glfwGetFramebufferSize(win, &fb_w, &fb_h);
  x = x * fb_w / wn_w;
  y = (wn_h - y) * fb_h / wn_h;
  arcball->AccumulateMouseMotion((float)x, (float)y, (float)fb_w, (float)fb_h);
}

static void cursorinit(GLFWwindow *win, double x, double y) {
  int wn_w, wn_h, fb_w, fb_h;
  glfwGetWindowSize(win, &wn_w, &wn_h);
  glfwGetFramebufferSize(win, &fb_w, &fb_h);
  x = x * fb_w / wn_w;
  y = (wn_h - y) * fb_h / wn_h;
  arcball->InitMouseMotion((float)x, (float)y);
  glfwSetCursorPosCallback(win, cursorpos);
}

static void mousebutton(GLFWwindow *win, int button, int action, int mods) {
  (void)button;
  (void)mods;
  if (action == GLFW_PRESS)
    glfwSetCursorPosCallback(win, cursorinit);
  else
    glfwSetCursorPosCallback(win, nullptr);
}

int main() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#ifdef __APPLE__
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
#endif

  glfwSetErrorCallback(error);

  GLFWwindow *win =
      glfwCreateWindow(800, 600, "INF1761 - Projeto G2", nullptr, nullptr);
  assert(win);
  glfwSetFramebufferSizeCallback(win, resize);
  glfwSetMouseButtonCallback(win, mousebutton);

  int fbw, fbh;
  glfwGetFramebufferSize(win, &fbw, &fbh);
  ctx = WgpuContext::Make(win, fbw, fbh);

  renderer =
      Renderer::Make(ctx->GetDevice(), true, {0.30f, 0.34f, 0.42f, 1.0f});

  initialize();

  while (!glfwWindowShouldClose(win)) {
    display();
    glfwPollEvents();
  }

  ctx = nullptr;
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
