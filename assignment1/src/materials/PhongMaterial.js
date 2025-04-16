class PhongMaterial extends Material {

    // Begin TOP changes Add rotation and light idx for PhongMaterial Ctor
    constructor(color, specular, light, lightIndex, translate, rotate, scale, vertexShader, fragmentShader) {
        let lightMVP = light.CalcLightMVP(translate, rotate, scale);
    // End TOP changes
        let lightIntensity = light.mat.GetIntensity();

        super({
            // Phong
            'uSampler': { type: 'texture', value: color },
            'uKs': { type: '3fv', value: specular },
            'uLightIntensity': { type: '3fv', value: lightIntensity },
            // Shadow
            'uShadowMap': { type: 'texture', value: light.fbo },
            'uLightMVP': { type: 'matrix4fv', value: lightMVP },

            // caution! fifth parameter(frameBuffer) should be null
        }, [], vertexShader, fragmentShader, null, lightIndex);
    }
}

// Begin TOP changes Add rotation and light idx for PhongMaterial builder func
async function buildPhongMaterial(color, specular, light, lightIndex, translate, rotate, scale, vertexPath, fragmentPath) {


    let vertexShader = await getShaderString(vertexPath);
    let fragmentShader = await getShaderString(fragmentPath);

    return new PhongMaterial(color, specular, light, lightIndex, translate, rotate, scale, vertexShader, fragmentShader);
// End TOP changes
}