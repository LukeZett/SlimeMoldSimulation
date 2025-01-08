

struct Agent
{
    pos: vec2f,
    angle: f32,
};

struct Settings
{
    moveSpeed: f32,
	turnSpeed: f32,
	sensorAngleDegrees: f32,
	sensorOffsetDst: f32,
	sensorSize: i32,
};


@group(0) @binding(0) var textureInput: texture_2d<f32>;
@group(0) @binding(1) var textureTarget: texture_storage_2d<rgba32float, write>;
@group(0) @binding(2) var<storage,read_write> agents: array<Agent,3200000>;

@group(1) @binding(0) var<uniform> settings: Settings;

fn pcg_hash(statein: u32) -> u32
{
    let state: u32 = statein * 747796405u + 2891336453u;
    let word: u32 = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

fn scaleToRange01(statein: u32) -> f32
{
    return f32(statein) / 4294967295.0;
}

fn sense(agent: Agent, sensorAngleOffset: f32) -> f32
{
    let sensorAngle: f32 = sensorAngleOffset + agent.angle;
    let sensorDir: vec2f = vec2f(cos(sensorAngle), sin(sensorAngle));
    let sensorPos: vec2f = agent.pos + sensorDir * settings.sensorOffsetDst;

    let sensorCentre = vec2i(sensorPos);
    
    var sum: f32 = 0;
    
    for(var offsetX: i32 = -settings.sensorSize; offsetX <= settings.sensorSize; offsetX++)
    {
        for(var offsetY: i32 = -settings.sensorSize; offsetY <= settings.sensorSize; offsetY++)
        {
            let samplePos = sensorCentre + vec2i(offsetX, offsetY);
            sum += textureLoad(textureInput, samplePos, 0).b;
        }
    }
    return sum;
}

@compute
@workgroup_size(64, 1, 1)
fn comp_main(@builtin(global_invocation_id) giid: vec3<u32>)
{
    let id = giid.x;
    let s = settings;
    var agent: Agent = agents[id];

    let random = pcg_hash(u32(f32(giid.x) * dot(agent.pos.xyy, agent.pos.yxx)));

    let sensorAngleRad: f32 = s.sensorAngleDegrees * (3.1415 / 180.0);
    let weightForward: f32 = sense(agent, 0.0);
    let weightLeft: f32 = sense(agent, sensorAngleRad);
    let weightRight: f32 = sense(agent, -sensorAngleRad);

    let randomSteerStrength: f32 = scaleToRange01(random);
    let turnSpeed: f32 = s.turnSpeed * 2.0 * 3.1415;

    if (weightForward > weightLeft && weightForward > weightRight) {
		agent.angle += 0.0;
	}
	else if (weightForward < weightLeft && weightForward < weightRight) {
		agent.angle += (randomSteerStrength - 0.5) * 2.0 * turnSpeed * 0.01;
	}
    else if (weightRight > weightLeft) {
		agent.angle -= randomSteerStrength * turnSpeed * 0.01;
	}

	else if (weightLeft > weightRight) {
		agent.angle += randomSteerStrength * turnSpeed * 0.01;
	}

    let direction = vec2f(cos(agent.angle), sin(agent.angle));
    agent.pos += direction * 0.01 * s.moveSpeed;

    if (agent.pos.x < 0.0 || agent.pos.x >= 1000.0 || agent.pos.y < 0.0 || agent.pos.y >= 1000.0) {
		
        agent.angle = scaleToRange01(pcg_hash(random)) * 2.0 * 3.1415;

		agent.pos.x = min(999.0,max(0.0, agent.pos.x));
		agent.pos.y = min(999.0,max(0.0, agent.pos.y));
	}
	else {
		let prev = textureLoad(textureInput, vec2i(agent.pos), 0);
        textureStore(textureTarget, vec2i(agent.pos), min(vec4f(1.0,1.5,2.0,1.0), prev + vec4f(0.1,0.5,0.5,0.0)));
	}
    agents[id] = agent;
}