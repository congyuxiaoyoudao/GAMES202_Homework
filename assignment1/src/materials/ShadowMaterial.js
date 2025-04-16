class ShadowMaterial extends Material {

    // Begin TOP changes Add rotation and light idx for ShadowMaterial Ctor
    constructor(light, lightIndex, translate, rotate, scale, vertexShader, fragmentShader) {
        let lightMVP = light.CalcLightMVP(translate, rotate, scale);
    // End TOP changes
        super({
            'uLightMVP': { type: 'matrix4fv', value: lightMVP }
        }, [], vertexShader, fragmentShader, light.fbo, lightIndex);
    }
}

// Begin TOP changes Add rotation and light idx for ShadowMaterial builder func
async function buildShadowMaterial(light, lightIndex, translate, rotate, scale, vertexPath, fragmentPath) {


    let vertexShader = await getShaderString(vertexPath);
    let fragmentShader = await getShaderString(fragmentPath);

    return new ShadowMaterial(light, lightIndex, translate, rotate, scale, vertexShader, fragmentShader);
// End TOP changes
}