class ShadowMaterial extends Material {

    // Begin TOP changes Add rotation for ShadowMaterial Ctor
    constructor(light, translate, rotate, scale, vertexShader, fragmentShader) {
        let lightMVP = light.CalcLightMVP(translate, rotate, scale);
    // End TOP changes
        super({
            'uLightMVP': { type: 'matrix4fv', value: lightMVP }
        }, [], vertexShader, fragmentShader, light.fbo);
    }
}

// Begin TOP changes Add rotation for ShadowMaterial builder func
async function buildShadowMaterial(light, translate, rotate, scale, vertexPath, fragmentPath) {


    let vertexShader = await getShaderString(vertexPath);
    let fragmentShader = await getShaderString(fragmentPath);

    return new ShadowMaterial(light, translate,rotate, scale, vertexShader, fragmentShader);
// End TOP changes
}