

@group(0) @binding(0) var texture: texture_2d<f32>;
@group(0) @binding(1) var textureTarget: texture_storage_2d<rgba32float, write>;

//const kernel = array(1.0,2.0,1.0,2.0,4.0,2.0,1.0,2.0,1.0);


fn pcg_hash(statein: u32) -> u32
{
    let state: u32 = statein * 747796405u + 2891336453u;
    let word: u32 = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

@compute
@workgroup_size(32, 1, 1)
fn comp_main(@builtin(global_invocation_id) id: vec3<u32>) {

    var kernel: array<f32, 9> = array(1.0,2.0,1.0,2.0,4.0,2.0,1.0,2.0,1.0);
    var texel: vec2i = vec2i(id.xy);
    var color: vec4f = vec4f(0.0, 0.0, 0.0, 0.0);
    var index: u32 = 0;

    for(var i: i32 = -1; i < 2; i++)
    {
        for(var j: i32 = -1; j < 2; j++)
        {
            color += textureLoad(texture, texel + vec2i(i, j), 0);// * kernel[index];
            index++;
        }
    }

    textureStore(textureTarget, id.xy, max((color / 9.0) - vec4(0.02,0.02,0.02,0.0), vec4(0.0, 0.0, 0.0, 1.0)));
    //textureStore(textureTarget, id.xy, max((color / 9.0), vec4(0.0, 0.0, 0.0, 0.0)));
}