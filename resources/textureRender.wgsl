
struct VertexInput {
    @location(0) position: vec2f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

@group(0) @binding(0) var texture: texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {

    var out: VertexOutput;
    out.position = vec4f(in.position * 2.0 - 1.0, 0.0, 1.0);
    out.uv = vec2f(in.position * vec2f(textureDimensions(texture)));
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let color = textureLoad(texture, vec2i(in.uv), 0);
    return color;    
}