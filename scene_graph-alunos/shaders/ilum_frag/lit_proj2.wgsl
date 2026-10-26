struct Matrix {
  vertex: mat4x4<f32>,
  normal: mat4x4<f32>,
}
@group(1) @binding(0) var<storage, read> matrix: array<Matrix>;

struct Global {
  projection: mat4x4<f32>,
  camera_position: vec4<f32>,
  light_position: vec4<f32>,
  second_light_pos: vec4<f32>,
  clip_plane: vec4<f32>,

  light_ambient: vec3<f32>,
  light_cutoff: f32,

  light_diffuse: vec3<f32>,
  light_exponent: f32,

  light_specular: vec3<f32>,
  second_light_enabled: f32,

  light_direction: vec3<f32>,
  clip_plane_enabled: f32,

  second_light_color: vec3<f32>,
  fog_enabled: f32,

  fog_color: vec3<f32>,
  fog_density: f32,
}
@group(0) @binding(0) var<uniform> global: Global;

struct Material {
  base_color: vec3<f32>,
  opacity: f32,

  specular_color: vec3<f32>,
  shininess: f32,
}
@group(2) @binding(0) var<uniform> material: Material;

@group(3) @binding(0) var decal_texture: texture_2d<f32>;
@group(3) @binding(1) var decal_sampler: sampler;

struct VertexOutput {
  @builtin(position) clip_position: vec4<f32>,
  @location(0) normal: vec3<f32>,
  @location(1) light: vec3<f32>,
  @location(2) view: vec3<f32>,
  @location(3) texcoord: vec2<f32>,
  @location(4) world_pos: vec3<f32>,
  @location(5) light2: vec3<f32>,
}

@vertex
fn vs_main (
  @builtin(instance_index) instance_index: u32,
  @location(0) coord: vec3<f32>,
  @location(1) normal: vec3<f32>,
  @location(2) tangent: vec3<f32>,
  @location(3) texcoord: vec2<f32>
) -> VertexOutput {
  let m = matrix[instance_index];
  var out: VertexOutput;
  let p = (m.vertex * vec4<f32>(coord, 1.0)).xyz;
  out.world_pos = p;
  out.normal = (m.normal * vec4<f32>(normal, 0.0)).xyz;
  out.light = normalize(global.light_position.xyz - p);
  out.light2 = normalize(global.second_light_pos.xyz - p);
  out.view = normalize(global.camera_position.xyz - p);
  out.texcoord = texcoord;
  out.clip_position = global.projection * (m.vertex * vec4<f32>(coord, 1.0));
  return out;
}

@fragment
fn fs_main (in: VertexOutput, @builtin(front_facing) front_facing: bool) -> @location(0) vec4<f32> {
  if (global.clip_plane_enabled > 0.5) {
    let dist = dot(global.clip_plane, vec4<f32>(in.world_pos, 1.0));
    if (dist < 0.0) {
      discard;
    }
  }

  var normal = normalize(in.normal);
  if (!front_facing) {
    normal = -normal;
  }

  let light = normalize(in.light);
  let view = normalize(in.view);

  let tex_sample = textureSample(decal_texture, decal_sampler, in.texcoord);
  var base_col = material.base_color * tex_sample.rgb;

  if (!front_facing) {
    base_col = vec3<f32>(0.85, 0.25, 0.20);
  }

  var color = base_col * global.light_ambient.rgb;

  let sdir = normalize(global.light_direction.xyz);
  let c = dot(-light, sdir);
  if (c >= global.light_cutoff) {
    let kspot = pow(max(c, 0.0), global.light_exponent);
    let ndotl = dot(normal, light);
    if (ndotl > 0.0) {
      let diffuse = base_col * global.light_diffuse.rgb * ndotl;
      let refl = normalize(reflect(-light, normal));
      let specular = material.specular_color * global.light_specular.rgb * pow(max(0.0, dot(refl, view)), material.shininess);
      color += kspot * (diffuse + specular);
    }
  }

  if (global.second_light_enabled > 0.5) {
    let light2 = normalize(in.light2);
    let ndotl2 = dot(normal, light2);
    if (ndotl2 > 0.0) {
      let diffuse2 = base_col * global.second_light_color.rgb * ndotl2;
      let refl2 = normalize(reflect(-light2, normal));
      let specular2 = material.specular_color * global.second_light_color.rgb * pow(max(0.0, dot(refl2, view)), material.shininess);
      color += 0.65 * (diffuse2 + specular2);
    }
  }

  if (global.fog_enabled > 0.5) {
    let dist_cam = distance(global.camera_position.xyz, in.world_pos);
    let fog_factor = clamp((dist_cam - 3.5) * global.fog_density, 0.0, 0.60);
    color = mix(color, global.fog_color.rgb, fog_factor);
  }

  return vec4<f32>(color, material.opacity);
}
