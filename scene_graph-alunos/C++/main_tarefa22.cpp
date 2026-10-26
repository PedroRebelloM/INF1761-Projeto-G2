#include <GLFW/glfw3.h>

#include "wgpucontext.h"
#include "arcball.h"
#include "scene.h"
#include "state.h"
#include "camera3d.h"
#include "phongmaterial.h"
#include "spotlight.h"
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

// Posicao inicial da camera
static float viewer_pos[3] = {1.8f, 3.2f, 4.0f};

static WgpuContextPtr ctx;
static RendererPtr renderer;
static ScenePtr scene;
static Camera3DPtr camera;
static ArcballPtr arcball;

static void initialize (void)
{
  camera = Camera3D::Make(viewer_pos[0], viewer_pos[1], viewer_pos[2]);
  camera->SetCenter(0.0f, 1.2f, 0.0f);
  arcball = camera->CreateArcball();

  // SpotLight configurada no espaco de mundo (world)
  // Posicao: acima e a frente da mesa
  // Direcao: apontando para o centro da mesa
  // Abertura: theta = 26 graus (cone visivel sobre a mesa e os objetos)
  // Expoente de concentracao: sexp = 18.0 (decaimento suave e foco nitido)
  SpotLightPtr spot = SpotLight::Make(
    0.2f, 4.2f, 1.5f,        // spos
    -0.1f, -2.8f, -1.5f,     // sdir (sera normalizada internamente)
    26.0f,                   // theta (cutoff em graus)
    18.0f,                   // sexp (expoente)
    "world"
  );
  spot->SetAmbient(0.12f, 0.12f, 0.14f);
  spot->SetDiffuse(1.0f, 1.0f, 1.0f);
  spot->SetSpecular(1.0f, 1.0f, 1.0f);

  // Instancias unicas de geometria basica (tamanho unitario)
  CubePtr cube = Cube::Make(ctx->GetDevice());
  SpherePtr sphere = Sphere::Make(ctx->GetDevice(), 64, 64);

  // Materiais Phong diferenciados conforme a Tarefa 2.1:
  // 1. Tampo da mesa: marrom, fosco
  PhongMaterialPtr matTampo = PhongMaterial::Make(0.48f, 0.28f, 0.14f);
  matTampo->SetShininess(8.0f);

  // 2. Quatro pes da mesa: rosa, roxo, azul e ciano (todos foscos)
  PhongMaterialPtr matPeRosa  = PhongMaterial::Make(0.95f, 0.40f, 0.70f);
  matPeRosa->SetShininess(8.0f);
  PhongMaterialPtr matPeRoxo  = PhongMaterial::Make(0.60f, 0.15f, 0.85f);
  matPeRoxo->SetShininess(8.0f);
  PhongMaterialPtr matPeAzul  = PhongMaterial::Make(0.15f, 0.35f, 0.90f);
  matPeAzul->SetShininess(8.0f);
  PhongMaterialPtr matPeCiano = PhongMaterial::Make(0.00f, 0.80f, 0.85f);
  matPeCiano->SetShininess(8.0f);

  // 3. Caixa: dourada, brilho intermediario
  PhongMaterialPtr matCaixa = PhongMaterial::Make(0.85f, 0.70f, 0.18f);
  matCaixa->SetSpecular(0.9f, 0.85f, 0.5f);
  matCaixa->SetShininess(32.0f);

  // 4. Esfera verde (sobre a caixa): brilho especular intenso
  PhongMaterialPtr matEsferaVerde = PhongMaterial::Make(0.15f, 0.82f, 0.25f);
  matEsferaVerde->SetSpecular(1.0f, 1.0f, 1.0f);
  matEsferaVerde->SetShininess(85.0f);

  // 5. Esfera vermelha (no tampo): brilho especular intenso
  PhongMaterialPtr matEsferaVermelha = PhongMaterial::Make(0.90f, 0.15f, 0.15f);
  matEsferaVermelha->SetSpecular(1.0f, 1.0f, 1.0f);
  matEsferaVermelha->SetShininess(90.0f);

  // Shader com calculo de SpotLight por fragmento
  ShaderPtr shader = Shader::Make(ctx->GetDevice(), "../shaders/ilum_frag/lit_spot.wgsl", spot, "world");
  VertexBufferSpec coordBuf, normalBuf;
  coordBuf.arrayStride = 3 * 4;
  coordBuf.attributes.push_back({std::string("coord"), std::nullopt, WGPUVertexFormat_Float32x3, 0});
  normalBuf.arrayStride = 3 * 4;
  normalBuf.attributes.push_back({std::string("normal"), std::nullopt, WGPUVertexFormat_Float32x3, 0});
  shader->SetVertexBuffers({coordBuf, normalBuf});

  PipelineDesc pdesc;
  WGPUPrimitiveState primitive = {};
  primitive.cullMode = WGPUCullMode_Back;
  pdesc.primitive = primitive;
  PipelinePtr pipeline = Pipeline::Make(shader, ctx->GetSurfaceFormat(), pdesc);

  // Registro dos materiais no shader
  shader->AddMaterial(matTampo.get());
  shader->AddMaterial(matPeRosa.get());
  shader->AddMaterial(matPeRoxo.get());
  shader->AddMaterial(matPeAzul.get());
  shader->AddMaterial(matPeCiano.get());
  shader->AddMaterial(matCaixa.get());
  shader->AddMaterial(matEsferaVerde.get());
  shader->AddMaterial(matEsferaVermelha.get());

  // === MONTAGEM DO GRAFO DE CENA ===

  // 1. Tampo da mesa (cubo achatado)
  // Tampo: largura 3.6, espessura 0.15, profundidade 2.4. Posicionado em y = 1.2
  TransformPtr trfTampo = Transform::Make();
  trfTampo->Translate(0.0f, 1.2f, 0.0f);
  trfTampo->Scale(3.6f, 0.15f, 2.4f);
  NodePtr nodeTampo = Node::Make(trfTampo, {matTampo}, {cube});

  // 2. Pes da mesa (cubos alongados de altura 1.2, base em y = 0)
  float legOffsetX = 1.6f;
  float legOffsetZ = 1.0f;
  float legWidth = 0.18f;
  float legHeight = 1.2f;

  TransformPtr trfPe1 = Transform::Make(); // Frente-Esquerda (Rosa)
  trfPe1->Translate(-legOffsetX, 0.0f, legOffsetZ);
  trfPe1->Scale(legWidth, legHeight, legWidth);
  NodePtr nodePe1 = Node::Make(trfPe1, {matPeRosa}, {cube});

  TransformPtr trfPe2 = Transform::Make(); // Frente-Direita (Roxo)
  trfPe2->Translate(legOffsetX, 0.0f, legOffsetZ);
  trfPe2->Scale(legWidth, legHeight, legWidth);
  NodePtr nodePe2 = Node::Make(trfPe2, {matPeRoxo}, {cube});

  TransformPtr trfPe3 = Transform::Make(); // Fundo-Esquerda (Azul)
  trfPe3->Translate(-legOffsetX, 0.0f, -legOffsetZ);
  trfPe3->Scale(legWidth, legHeight, legWidth);
  NodePtr nodePe3 = Node::Make(trfPe3, {matPeAzul}, {cube});

  TransformPtr trfPe4 = Transform::Make(); // Fundo-Direita (Ciano)
  trfPe4->Translate(legOffsetX, 0.0f, -legOffsetZ);
  trfPe4->Scale(legWidth, legHeight, legWidth);
  NodePtr nodePe4 = Node::Make(trfPe4, {matPeCiano}, {cube});

  // 3. Caixa dourada e Esfera Verde (compartilham no pai para nao deformar a esfera)
  // No pai: translada para o tampo (y = 1.35) e rotaciona
  TransformPtr trfCaixaPai = Transform::Make();
  trfCaixaPai->Translate(-0.55f, 1.35f, -0.15f);
  trfCaixaPai->Rotate(25.0f, 0.0f, 1.0f, 0.0f);

  // Filho 1: Caixa com escala nao-uniforme
  TransformPtr trfCaixa = Transform::Make();
  trfCaixa->Scale(0.75f, 0.50f, 0.75f);
  NodePtr nodeCaixa = Node::Make(trfCaixa, {matCaixa}, {cube});

  // Filho 2: Esfera verde pousada sobre a caixa (escala uniforme)
  // Altura da caixa = 0.50. Raio da esfera = 0.22. Posicao y = 0.50 + 0.22 = 0.72
  TransformPtr trfEsferaVerde = Transform::Make();
  trfEsferaVerde->Translate(0.0f, 0.72f, 0.0f);
  trfEsferaVerde->Scale(0.22f, 0.22f, 0.22f);
  NodePtr nodeEsferaVerde = Node::Make(trfEsferaVerde, {matEsferaVerde}, {sphere});

  NodePtr nodeCaixaAgrupada = Node::Make(trfCaixaPai, {}, {}, {nodeCaixa, nodeEsferaVerde});

  // 4. Esfera vermelha apoiada diretamente no tampo
  // Raio da esfera = 0.30. Posicao y = 1.35 + 0.30 = 1.65
  TransformPtr trfEsferaVermelha = Transform::Make();
  trfEsferaVermelha->Translate(0.75f, 1.65f, 0.25f);
  trfEsferaVermelha->Scale(0.30f, 0.30f, 0.30f);
  NodePtr nodeEsferaVermelha = Node::Make(trfEsferaVermelha, {matEsferaVermelha}, {sphere});

  // Raiz da cena
  NodePtr root = Node::Make(pipeline, {
    nodeTampo,
    nodePe1,
    nodePe2,
    nodePe3,
    nodePe4,
    nodeCaixaAgrupada,
    nodeEsferaVermelha
  });

  scene = Scene::Make(root);
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
  if (key == GLFW_KEY_Q && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
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

  GLFWwindow* win = glfwCreateWindow(800, 600, "INF1761 - Tarefa 2.2: Luz de Spot (Cena 3D)", nullptr, nullptr);
  assert(win);
  glfwSetFramebufferSizeCallback(win, resize);
  glfwSetKeyCallback(win, keyboard);
  glfwSetMouseButtonCallback(win, mousebutton);

  int fbw, fbh;
  glfwGetFramebufferSize(win, &fbw, &fbh);
  ctx = WgpuContext::Make(win, fbw, fbh);

  // Fundo sutilmente escurecido para valorizar a iluminacao do cone de spot
  renderer = Renderer::Make(ctx->GetDevice(), true, {0.05f, 0.05f, 0.07f, 1.0f});

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
