#include <GLFW/glfw3.h>

#include "wgpucontext.h"
#include "arcball.h"
#include "scene.h"
#include "state.h"
#include "camera3d.h"
#include "phongmaterial.h"
#include "spotlight.h"
#include "texture.h"
#include "sampler.h"
#include "textureset.h"
#include "transform.h"
#include "node.h"
#include "cube.h"
#include "sphere.h"
#include "shader.h"
#include "pipeline.h"
#include "renderer.h"
#include "error.h"

#include <cassert>
#include <cstdio>
#include <cmath>

// Posicao inicial da camera
static float viewer_pos[3] = {2.2f, 3.4f, 4.2f};

static GLFWwindow* g_window = nullptr;
static WgpuContextPtr ctx;
static RendererPtr renderer;
static ScenePtr scene;
static Camera3DPtr camera;
static ArcballPtr arcball;
static ShaderPtr shader;

// Estados das extensoes interativas (iniciam LIGADAS para feedback visual imediato)
static bool g_second_light = true;
static bool g_clip_plane = true;
static float g_cut_height = 1.70f;
static bool g_fog = true;
static float g_fog_density = 0.07f;

static void update_title ()
{
  if (!g_window) return;
  char title[256];
  snprintf(title, sizeof(title),
           "INF1761 Proj2 | Corte (C): %s y=%.2f | Fog (F): %s | 2a Luz (L): %s",
           g_clip_plane ? "ON" : "OFF", g_cut_height,
           g_fog ? "ON" : "OFF",
           g_second_light ? "ON" : "OFF");
  glfwSetWindowTitle(g_window, title);
}

static void update_shader_uniforms ()
{
  // 1. Segunda fonte de luz (Fill light adicional do lado oposto)
  shader->SetValue("second_light_enabled", g_second_light ? 1.0f : 0.0f);
  shader->SetValue("second_light_pos", Vec4{-2.8f, 3.2f, -1.8f, 1.0f});
  shader->SetValue("second_light_color", Vec3{0.80f, 0.70f, 0.95f});

  // 2. Plano de corte horizontal (fatia a cena em y = g_cut_height):
  // descarta tudo acima de g_cut_height (0, -1, 0, cut_height)
  shader->SetValue("clip_plane_enabled", g_clip_plane ? 1.0f : 0.0f);
  shader->SetValue("clip_plane", Vec4{0.0f, -1.0f, 0.0f, g_cut_height});

  // 3. Fog (neblina atmosférica bem visível)
  shader->SetValue("fog_enabled", g_fog ? 1.0f : 0.0f);
  shader->SetValue("fog_density", g_fog_density);
  shader->SetValue("fog_color", Vec3{0.30f, 0.34f, 0.42f});

  update_title();
}

static void print_controls ()
{
  printf("\n==============================================================\n");
  printf("  INF1761 - Computacao Grafica - Projeto 2: Cena 3D Completa  \n");
  printf("  Aluno: Pedro Rebello de Morais                             \n");
  printf("==============================================================\n");
  printf("  Status Inicial dos Efeitos (todos LIGADOS para demonstracao):\n");
  printf("  [L] 2a Fonte de Luz    : %s\n", g_second_light ? "LIGADA" : "DESLIGADA");
  printf("  [C] Plano de Corte     : %s (altura y = %.2f)\n", g_clip_plane ? "LIGADO (topo fatiado)" : "DESLIGADO", g_cut_height);
  printf("  [F] Neblina / Fog      : %s (densidade = %.2f)\n", g_fog ? "LIGADA" : "DESLIGADA", g_fog_density);
  printf("\n  Controles no Teclado (clique na JANELA GRAFICA para focar):\n");
  printf("  - Mouse (arrastar)    : Orbitar a camera com o Arcball\n");
  printf("  - Tecla [C]           : Ligar / Desligar o Plano de Corte\n");
  printf("  - Teclas [UP] / [DOWN]: Subir / Descer a altura do corte (ou [W]/[S])\n");
  printf("  - Tecla [F]           : Ligar / Desligar a Neblina (Fog)\n");
  printf("  - Teclas [[ ] e []]   : Diminuir / Aumentar densidade do Fog\n");
  printf("  - Tecla [L]           : Ligar / Desligar a 2a Fonte de Luz\n");
  printf("  - Tecla [Q] / [ESC]   : Sair da aplicacao\n");
  printf("==============================================================\n\n");
}

static void initialize (void)
{
  camera = Camera3D::Make(viewer_pos[0], viewer_pos[1], viewer_pos[2]);
  camera->SetCenter(0.0f, 1.2f, 0.0f);
  arcball = camera->CreateArcball();

  // 1. SpotLight principal (espaco de mundo)
  SpotLightPtr spot = SpotLight::Make(
    0.3f, 4.4f, 1.8f,        // spos (acima e a frente da mesa)
    -0.15f, -2.8f, -1.5f,    // sdir (apontando para o tampo)
    28.0f,                   // theta (abertura do cone)
    16.0f,                   // sexp (expoente de concentracao)
    "world"
  );
  spot->SetAmbient(0.12f, 0.12f, 0.14f);
  spot->SetDiffuse(1.0f, 1.0f, 1.0f);
  spot->SetSpecular(1.0f, 1.0f, 1.0f);

  // Instancias unitarias unicas de geometria basica
  CubePtr cube = Cube::Make(ctx->GetDevice());
  SpherePtr sphere = Sphere::Make(ctx->GetDevice(), 64, 64);

  // Shader com calculo de iluminacao por fragmento e extensoes
  shader = Shader::Make(ctx->GetDevice(), "../shaders/ilum_frag/lit_proj2.wgsl", spot, "world");

  VertexBufferSpec coordBuf, normalBuf, tangentBuf, texcoordBuf;
  coordBuf.arrayStride = 3 * 4;
  coordBuf.attributes.push_back({std::string("coord"), std::nullopt, WGPUVertexFormat_Float32x3, 0});

  normalBuf.arrayStride = 3 * 4;
  normalBuf.attributes.push_back({std::string("normal"), std::nullopt, WGPUVertexFormat_Float32x3, 0});

  tangentBuf.arrayStride = 3 * 4;
  tangentBuf.attributes.push_back({std::string("tangent"), std::nullopt, WGPUVertexFormat_Float32x3, 0});

  texcoordBuf.arrayStride = 2 * 4;
  texcoordBuf.attributes.push_back({std::string("texcoord"), std::nullopt, WGPUVertexFormat_Float32x2, 0});

  shader->SetVertexBuffers({coordBuf, normalBuf, tangentBuf, texcoordBuf});

  PipelineDesc pdesc;
  WGPUPrimitiveState primitive = {};
  primitive.cullMode = WGPUCullMode_None; // permite visualizar o interior dos objetos cortados pelo plano
  pdesc.primitive = primitive;
  PipelinePtr pipeline = Pipeline::Make(shader, ctx->GetSurfaceFormat(), pdesc);

  // 2. Texturas Decal e Samplers
  SamplerPtr defaultSampler = Sampler::Make(ctx->GetDevice(), "decal_sampler");

  // Textura branca 1x1 para objetos solidos (pes da mesa)
  TexturePtr whiteTex = Texture::Make(ctx->GetDevice(), "decal_texture", Vec4{1.0f, 1.0f, 1.0f, 1.0f});
  TextureSetPtr texSetSolid = TextureSet::Make({whiteTex.get(), defaultSampler.get()});
  shader->AddTextureSet(texSetSolid.get());

  // Textura de madeira (Decal no tampo da mesa): wood.jpg
  TexturePtr woodTex = Texture::Make(ctx->GetDevice(), "decal_texture", "../images/wood.jpg");
  TextureSetPtr texSetMadeira = TextureSet::Make({woodTex.get(), defaultSampler.get()});
  shader->AddTextureSet(texSetMadeira.get());

  // Textura 1 (Decal na caixa dourada): paper.jpg
  TexturePtr paperTex = Texture::Make(ctx->GetDevice(), "decal_texture", "../images/paper.jpg");
  TextureSetPtr texSetCaixa = TextureSet::Make({paperTex.get(), defaultSampler.get()});
  shader->AddTextureSet(texSetCaixa.get());

  // Textura 2 (Decal na Esfera sobre a caixa): snow_02_disp_4k.png
  TexturePtr snowTex = Texture::Make(ctx->GetDevice(), "decal_texture", "../images/snow_02_disp_4k.png");
  TextureSetPtr texSetSnow = TextureSet::Make({snowTex.get(), defaultSampler.get()});
  shader->AddTextureSet(texSetSnow.get());

  // Textura 3 (Decal na Esfera apoiada no tampo): rocky_terrain_03_disp_4k.png
  TexturePtr rockyTex = Texture::Make(ctx->GetDevice(), "decal_texture", "../images/rocky_terrain_03_disp_4k.png");
  TextureSetPtr texSetRocky = TextureSet::Make({rockyTex.get(), defaultSampler.get()});
  shader->AddTextureSet(texSetRocky.get());

  // 3. Materiais Phong dos objetos
  PhongMaterialPtr matTampo = PhongMaterial::Make(1.0f, 1.0f, 1.0f); // cor vem da textura de madeira
  matTampo->SetShininess(8.0f);

  PhongMaterialPtr matPeRosa  = PhongMaterial::Make(0.95f, 0.40f, 0.70f);
  matPeRosa->SetShininess(8.0f);
  PhongMaterialPtr matPeRoxo  = PhongMaterial::Make(0.60f, 0.15f, 0.85f);
  matPeRoxo->SetShininess(8.0f);
  PhongMaterialPtr matPeAzul  = PhongMaterial::Make(0.15f, 0.35f, 0.90f);
  matPeAzul->SetShininess(8.0f);
  PhongMaterialPtr matPeCiano = PhongMaterial::Make(0.00f, 0.80f, 0.85f);
  matPeCiano->SetShininess(8.0f);

  PhongMaterialPtr matCaixa = PhongMaterial::Make(0.95f, 0.82f, 0.35f);
  matCaixa->SetSpecular(0.9f, 0.85f, 0.5f);
  matCaixa->SetShininess(32.0f);

  // Esfera sobre a caixa (neve)
  PhongMaterialPtr matEsferaNeve = PhongMaterial::Make(1.0f, 1.0f, 1.0f);
  matEsferaNeve->SetSpecular(0.8f, 0.85f, 0.95f);
  matEsferaNeve->SetShininess(60.0f);

  // Esfera no tampo (rocha)
  PhongMaterialPtr matEsferaRocha = PhongMaterial::Make(1.0f, 1.0f, 1.0f);
  matEsferaRocha->SetSpecular(0.7f, 0.7f, 0.7f);
  matEsferaRocha->SetShininess(40.0f);

  shader->AddMaterial(matTampo.get());
  shader->AddMaterial(matPeRosa.get());
  shader->AddMaterial(matPeRoxo.get());
  shader->AddMaterial(matPeAzul.get());
  shader->AddMaterial(matPeCiano.get());
  shader->AddMaterial(matCaixa.get());
  shader->AddMaterial(matEsferaNeve.get());
  shader->AddMaterial(matEsferaRocha.get());

  // 4. Montagem do Grafo de Cena

  // Tampo da mesa (cubo achatado)
  TransformPtr trfTampo = Transform::Make();
  trfTampo->Translate(0.0f, 1.2f, 0.0f);
  trfTampo->Scale(3.6f, 0.15f, 2.4f);
  NodePtr nodeTampo = Node::Make(trfTampo, {matTampo, texSetMadeira}, {cube});

  // Pes da mesa (cubos alongados de altura 1.2)
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

  // Caixa com textura decal (paper.jpg) e Esfera com textura de Neve
  TransformPtr trfCaixaPai = Transform::Make();
  trfCaixaPai->Translate(-0.55f, 1.35f, -0.15f);
  trfCaixaPai->Rotate(25.0f, 0.0f, 1.0f, 0.0f);

  TransformPtr trfCaixa = Transform::Make();
  trfCaixa->Scale(0.75f, 0.50f, 0.75f);
  NodePtr nodeCaixa = Node::Make(trfCaixa, {matCaixa, texSetCaixa}, {cube});

  // Esfera sobre a caixa (escala uniforme) com textura de Neve
  TransformPtr trfEsferaNeve = Transform::Make();
  trfEsferaNeve->Translate(0.0f, 0.72f, 0.0f);
  trfEsferaNeve->Scale(0.22f, 0.22f, 0.22f);
  NodePtr nodeEsferaNeve = Node::Make(trfEsferaNeve, {matEsferaNeve, texSetSnow}, {sphere});

  NodePtr nodeCaixaAgrupada = Node::Make(trfCaixaPai, {}, {}, {nodeCaixa, nodeEsferaNeve});

  // Esfera apoiada no tampo com textura de Terreno Rochoso
  TransformPtr trfEsferaRocha = Transform::Make();
  trfEsferaRocha->Translate(0.75f, 1.65f, 0.25f);
  trfEsferaRocha->Rotate(45.0f, 0.0f, 1.0f, 0.0f);
  trfEsferaRocha->Scale(0.30f, 0.30f, 0.30f);
  NodePtr nodeEsferaRocha = Node::Make(trfEsferaRocha, {matEsferaRocha, texSetRocky}, {sphere});

  // Raiz da cena
  NodePtr root = Node::Make(pipeline, {
    nodeTampo,
    nodePe1,
    nodePe2,
    nodePe3,
    nodePe4,
    nodeCaixaAgrupada,
    nodeEsferaRocha
  });

  scene = Scene::Make(root);

  // Inicializa uniforms das extensoes
  update_shader_uniforms();
  print_controls();
}

static void display ()
{
  WGPUSurfaceTexture surfaceTex = ctx->GetCurrentTexture();
  if (surfaceTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
      surfaceTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
    return;
  renderer->Render(surfaceTex.texture, scene, camera);
  ctx->Present();
  wgpuTextureRelease(surfaceTex.texture);
}

static void error (int code, const char* msg)
{
  printf("GLFW error %d: %s\n", code, msg);
  glfwTerminate();
  exit(0);
}

static void keyboard (GLFWwindow* window, int key, int scancode, int action, int mods)
{
  (void) scancode; (void) mods;
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

  if (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
  // Alternar 2a fonte de luz
  else if (key == GLFW_KEY_L) {
    g_second_light = !g_second_light;
    printf(">> [L] 2a Fonte de Luz: %s\n", g_second_light ? "LIGADA (suave de preenchimento)" : "DESLIGADA (apenas Spot)");
    update_shader_uniforms();
  }
  // Alternar Plano de Corte
  else if (key == GLFW_KEY_C) {
    g_clip_plane = !g_clip_plane;
    printf(">> [C] Plano de Corte: %s (altura y = %.2f)\n", g_clip_plane ? "ATIVADO (corta topo)" : "DESATIVADO", g_cut_height);
    update_shader_uniforms();
  }
  // Subir altura do corte (mostra mais da cena)
  else if (key == GLFW_KEY_UP || key == GLFW_KEY_W) {
    g_cut_height += 0.08f;
    if (g_cut_height > 2.6f) g_cut_height = 2.6f;
    printf(">> [UP/W] Altura do Corte: y = %.2f\n", g_cut_height);
    update_shader_uniforms();
  }
  // Descer altura do corte (fatia mais fundo nos objetos)
  else if (key == GLFW_KEY_DOWN || key == GLFW_KEY_S) {
    g_cut_height -= 0.08f;
    if (g_cut_height < 0.8f) g_cut_height = 0.8f;
    printf(">> [DOWN/S] Altura do Corte: y = %.2f\n", g_cut_height);
    update_shader_uniforms();
  }
  // Alternar Fog (neblina)
  else if (key == GLFW_KEY_F) {
    g_fog = !g_fog;
    printf(">> [F] Neblina (Fog): %s (densidade = %.2f)\n", g_fog ? "ATIVADA" : "DESATIVADA", g_fog_density);
    update_shader_uniforms();
  }
  // Ajustar densidade do fog
  else if (key == GLFW_KEY_LEFT_BRACKET) { // [
    g_fog_density = std::max(0.05f, g_fog_density - 0.05f);
    printf(">> [[] Densidade da Neblina: %.2f\n", g_fog_density);
    update_shader_uniforms();
  }
  else if (key == GLFW_KEY_RIGHT_BRACKET) { // ]
    g_fog_density = std::min(0.60f, g_fog_density + 0.05f);
    printf(">> []] Densidade da Neblina: %.2f\n", g_fog_density);
    update_shader_uniforms();
  }
}

static void resize (GLFWwindow* win, int width, int height)
{
  (void) win;
  if (width > 0 && height > 0) ctx->Configure(width, height);
}

static void cursorpos (GLFWwindow* win, double x, double y)
{
  int wn_w, wn_h, fb_w, fb_h;
  glfwGetWindowSize(win, &wn_w, &wn_h);
  glfwGetFramebufferSize(win, &fb_w, &fb_h);
  x = x * fb_w / wn_w;
  y = (wn_h - y) * fb_h / wn_h;
  arcball->AccumulateMouseMotion((float) x, (float) y, (float) fb_w, (float) fb_h);
}

static void cursorinit (GLFWwindow* win, double x, double y)
{
  int wn_w, wn_h, fb_w, fb_h;
  glfwGetWindowSize(win, &wn_w, &wn_h);
  glfwGetFramebufferSize(win, &fb_w, &fb_h);
  x = x * fb_w / wn_w;
  y = (wn_h - y) * fb_h / wn_h;
  arcball->InitMouseMotion((float) x, (float) y);
  glfwSetCursorPosCallback(win, cursorpos);
}

static void mousebutton (GLFWwindow* win, int button, int action, int mods)
{
  (void) button; (void) mods;
  if (action == GLFW_PRESS)
    glfwSetCursorPosCallback(win, cursorinit);
  else
    glfwSetCursorPosCallback(win, nullptr);
}

int main ()
{
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#ifdef __APPLE__
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
#endif

  glfwSetErrorCallback(error);

  g_window = glfwCreateWindow(800, 600, "INF1761 - Proj2", nullptr, nullptr);
  assert(g_window);
  glfwSetFramebufferSizeCallback(g_window, resize);
  glfwSetKeyCallback(g_window, keyboard);
  glfwSetMouseButtonCallback(g_window, mousebutton);

  int fbw, fbh;
  glfwGetFramebufferSize(g_window, &fbw, &fbh);
  ctx = WgpuContext::Make(g_window, fbw, fbh);

  // Fundo com a cor da neblina atmosferica
  renderer = Renderer::Make(ctx->GetDevice(), true, {0.30f, 0.34f, 0.42f, 1.0f});

  initialize();

  while (!glfwWindowShouldClose(g_window)) {
    display();
    glfwPollEvents();
  }

  ctx = nullptr;
  glfwDestroyWindow(g_window);
  glfwTerminate();
  return 0;
}
