// class PhongMaterial extends Material {

// /**
//  * Creates an instance of PhongMaterial.
//  * @param {vec3f} color The material color
//  * @param {Texture} colorMap The texture object of the material
//  * @param {vec3f} specular The material specular coefficient
//  * @param {float} intensity The light intensity
//  * @memberof PhongMaterial
//  */
//     constructor(color, colorMap, specular, intensity) {
//         let textureSample = 0;

//         if (colorMap != null) {
//             textureSample = 1;
//         super({
//             'uTextureSample': { type: '1i', value: textureSample },
//             'uSampler': { type: 'texture', value: colorMap },
//             'uKd': { type: '3fv', value: color },
//             'uKs': { type: '3fv', value: specular },
//             'uLightIntensity': { type: '1f', value: intensity }
//         }, [], PhongVertexShader, PhongFragmentShader);
//         } else {
//         //console.log(color);
//         super({
//         'uTextureSample': { type: '1i', value: textureSample },

//         'uKd': { type: '3fv', value: color },
//         'uKs': { type: '3fv', value: specular },
//         'uLightIntensity': { type: '1f', value: intensity }
//         }, [], PhongVertexShader, PhongFragmentShader);
//         }

//     }
// }

// Load the shader files from file
class PhongMaterial extends Material {
    constructor(uniforms, vertexShader, fragmentShader) {
        super(uniforms, [], vertexShader, fragmentShader);
    }

    /**
     * Creates an instance of PhongMaterial async.
     * @param {vec3f} color
     * @param {Texture} colorMap
     * @param {vec3f} specular
     * @param {float} intensity
     * @returns {Promise<PhongMaterial>}
     */
    static async create(color, colorMap, specular, intensity) {
        const textureSample = colorMap ? 1 : 0;

        const [vertexShader, fragmentShader] = await Promise.all([
            loadShaderFile('src/shaders/phongShader/vertex.glsl'),
            loadShaderFile('src/shaders/phongShader/fragment.glsl')
        ]);
        // console.log('Vertex Shader:', vertexShader.slice(0, 100));
        // console.log('Fragment Shader:', fragmentShader.slice(0, 100));  
        const uniforms = {
            'uTextureSample': { type: '1i', value: textureSample },
            'uKd': { type: '3fv', value: color },
            'uKs': { type: '3fv', value: specular },
            'uLightIntensity': { type: '1f', value: intensity }
        };

        if (colorMap) {
            uniforms['uSampler'] = { type: 'texture', value: colorMap };
        }

        return new PhongMaterial(uniforms, vertexShader, fragmentShader);
    }
}
