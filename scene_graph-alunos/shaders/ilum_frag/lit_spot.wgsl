struct Matrix {
  vertex: mat4x4<f32>,
  normal: mat4x4<f32>,
}
@group(1) @binding(0) var<storage, read> matrix: array<Matrix>;

struct Global {
  projection: mat4x4<f32>,   // espaco de iluminacao -> NDC
  light_ambient: vec3<f32>,
  light_diffuse: vec3<f32>,
  light_specular: vec3<f32>,
  light_position: vec4<f32>,
  light_direction: vec3<f32>,
  light_cutoff: f32,         // cos(theta), onde theta e o angulo de abertura
  light_exponent: f32,       // expoente de concentracao sexp
  camera_position: vec4<f32>,
}
@group(0) @binding(0) var<uniform> global: Global;

struct Material {
  base_color: vec3<f32>,
  opacity: f32,

  specular_color: vec3<f32>,
  shininess: f32,
}
@group(2) @binding(0) var<uniform> material: Material;

struct VertexOutput {
  @builtin(position) clip_position: vec4<f32>,
  @location(0) normal: vec3<f32>,
  @location(1) light: vec3<f32>,
  @location(2) view: vec3<f32>,
}

@vertex
fn vs_main (@builtin(instance_index) instance_index: u32, @location(0) coord: vec3<f32>, @location(1) normal: vec3<f32>) -> VertexOutput {
  let m = matrix[instance_index];
  var out: VertexOutput;
  let p = (m.vertex * vec4<f32>(coord, 1.0)).xyz;
  out.normal = (m.normal * vec4<f32>(normal, 0.0)).xyz;
  out.light = normalize(global.light_position.xyz - p);
  out.view = normalize(global.camera_position.xyz - p);
  out.clip_position = global.projection * (m.vertex * vec4<f32>(coord, 1.0));
  return out;
}

@fragment
fn fs_main (in: VertexOutput) -> @location(0) vec4<f32> {
  let normal = normalize(in.normal);
  let light = normalize(in.light);
  let view = normalize(in.view);

  // Termo ambiente da cena
  var color = material.base_color * global.light_ambient.rgb;

  // Calculo de spot por fragmento conforme o enunciado:
  // l aponta do ponto para a fonte. Logo -l aponta da fonte para o ponto iluminado.
  // c = -l . sdir
  let sdir = normalize(global.light_direction.xyz);
  let c = dot(-light, sdir);

  // Se c >= cos(theta) (armazenado em light_cutoff), o ponto esta dentro do cone
  if (c >= global.light_cutoff) {
    let kspot = pow(max(c, 0.0), global.light_exponent);

    let ndotl = dot(normal, light);
    if (ndotl > 0.0) {
      let diffuse = material.base_color * global.light_diffuse.rgb * ndotl;
      let refl = normalize(reflect(-light, normal));
      let specular = material.specular_color * global.light_specular.rgb * pow(max(0.0, dot(refl, view)), material.shininess);
      color += kspot * (diffuse + specular);
    }
  }

  return vec4<f32>(color, material.opacity);
}
