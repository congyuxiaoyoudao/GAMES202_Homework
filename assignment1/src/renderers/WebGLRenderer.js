class WebGLRenderer {
    meshes = [];
    shadowMeshes = [];
    lights = [];

    constructor(gl, camera) {
        this.gl = gl;
        this.camera = camera;
    }

    addLight(light) {
        this.lights.push({
            entity: light,
            meshRender: new MeshRender(this.gl, light.mesh, light.mat)
        });
    }
    addMeshRender(mesh) { this.meshes.push(mesh); }
    addShadowMeshRender(mesh) { this.shadowMeshes.push(mesh); }

    updateMVP(mesh, lightEntity){
        let translate = mesh.transform.translate;
        let rotate = mesh.transform.rotate;
        let scale = mesh.transform.scale;
        let MVP = lightEntity.CalcLightMVP(translate, rotate, scale);
        return MVP;
    }

    render(deltaTime) {
        const gl = this.gl;

        gl.clearColor(0.0, 0.0, 0.0, 1.0); // Clear to black, fully opaque
        gl.clearDepth(1.0); // Clear everything
        gl.enable(gl.DEPTH_TEST); // Enable depth testing
        gl.depthFunc(gl.LEQUAL); // Near things obscure far things

        // console.assert(this.lights.length != 0, "No light");
        // console.assert(this.lights.length == 1, "Multiple lights");

        for (let i = 0; i < this.meshes.length; i++) {
            this.meshes[i].mesh.transform.rotate[1] += degreeToRadian(15) * deltaTime;
        }

        for (let l = 0; l < this.lights.length; l++) {
            // Begin TOP changes Clear shadow map each frame or framebuffer will accumulate
            gl.bindFramebuffer(gl.FRAMEBUFFER, this.lights[l].entity.fbo);
            gl.clearColor(1.0, 1.0, 1.0, 1.0);
            gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
            // End TOP changes

            // Draw light
            // Begin TOP changes Add rotation for light
            let lightPos = this.lights[l].entity.lightPos;
            lightPos = vec3.rotateY(lightPos, lightPos, this.lights[l].entity.focalPoint, degreeToRadian(10) * deltaTime);
            this.lights[l].entity.lightPos = lightPos;
            // End TOP changes
            this.lights[l].meshRender.mesh.transform.translate = this.lights[l].entity.lightPos;
            this.lights[l].meshRender.draw(this.camera);

            // Begin TOP changes Update lightMVP each frame
            // Shadow pass
            if (this.lights[l].entity.hasShadowMap == true) {
                for (let i = 0; i < this.shadowMeshes.length; i++) {
                    if(this.shadowMeshes[i].material.lightIndex != l)
                        continue;
                    let lightMVP = this.updateMVP(this.shadowMeshes[i].mesh, this.lights[l].entity);
                    this.shadowMeshes[i].material.uniforms.uLightMVP = { type: 'matrix4fv', value: lightMVP };
                    this.shadowMeshes[i].draw(this.camera);
                }
            }

            if(l != 0)
            {
                gl.enable(gl.BLEND);
                gl.blendFunc(gl.ONE, gl.ONE);
            }

            // Camera pass
            for (let i = 0; i < this.meshes.length; i++) {
                if(this.meshes[i].material.lightIndex != l)
                    continue;
                let lightMVP = this.updateMVP(this.meshes[i].mesh, this.lights[l].entity);
                this.gl.useProgram(this.meshes[i].shader.program.glShaderProgram);
                this.meshes[i].material.uniforms.uLightMVP = { type: 'matrix4fv', value: lightMVP };
                //this.gl.uniform3fv(this.meshes[i].shader.program.uniforms.uLightPos, this.lights[l].entity.lightPos);
                this.meshes[i].material.uniforms.uLightPos = { type: '3fv', value: this.lights[l].entity.lightPos };
                this.meshes[i].draw(this.camera);
            }
            // End TOP changes

            gl.disable(gl.BLEND);
        }
    }
}