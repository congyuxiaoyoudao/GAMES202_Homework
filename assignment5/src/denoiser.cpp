#include "denoiser.h"

Denoiser::Denoiser() : m_useTemportal(false) {}

void Denoiser::Reprojection(const FrameInfo &frameInfo) {
    int height = m_accColor.m_height;
    int width = m_accColor.m_width;
    Matrix4x4 preWorldToScreen =
        m_preFrameInfo.m_matrix[m_preFrameInfo.m_matrix.size() - 1];
    Matrix4x4 preWorldToCamera =
        m_preFrameInfo.m_matrix[m_preFrameInfo.m_matrix.size() - 2];
#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Reproject
            m_valid(x, y) = false;
            m_misc(x, y) = Float3(0.f);

            float id = frameInfo.m_id(x, y);
            // skip ivalid id
            if(id == -1)
                continue;

            Float3 pos = frameInfo.m_position(x, y);
            Matrix4x4 WorldToLocal = Inverse(frameInfo.m_matrix[id]);
            Matrix4x4 preLocalToWorld = m_preFrameInfo.m_matrix[id];
            // P_{i-1}V_{i-1}M_{-1}M_{i}^{-1}
            Matrix4x4 ReprojectionMatrix = preWorldToScreen*preLocalToWorld*WorldToLocal;

            Float3 prePosScreen = ReprojectionMatrix(pos, Float3::EType::Point);
            float pre_x = prePosScreen.x;
            float pre_y = prePosScreen.y;

            // is in viewport
            if(pre_x<0 || pre_x>width-1 || pre_y<0 || pre_y>height-1)
                continue;

            float preId = m_preFrameInfo.m_id(pre_x, pre_y);
            
            if(preId == id){
                m_valid(x, y) = true;
                m_misc(x, y) = m_accColor(pre_x, pre_y);
            }
        }
    }
    std::swap(m_misc, m_accColor);
}

void Denoiser::TemporalAccumulation(const Buffer2D<Float3> &curFilteredColor) {
    int height = m_accColor.m_height;
    int width = m_accColor.m_width;
    int kernelRadius = 3;
    int kernelSize = 2*kernelRadius+1;
    int sampleNum = kernelSize * kernelSize;
#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Temporal clamp
            float alpha = 1.0f;
            // previous frame color
            Float3 color = m_accColor(x, y);

            if(m_valid(x, y)){
                alpha = m_alpha;
                // calculate mu and sigma in 7*7 block
                Float3 mu, mu2 = Float3(0.f);
                Float3 sigma = Float3(0.f);
                for(int i = -kernelRadius; i <= kernelRadius; i++){
                    for(int j = -kernelRadius; j <= kernelRadius; j++){
                        int sampleCoord_x = std::min(std::max(0,x+i),width-1); 
                        int sampleCoord_y = std::min(std::max(0,y+j),height-1);

                        Float3 curSampledColor = curFilteredColor(sampleCoord_x, sampleCoord_y);
                        mu+=curSampledColor;
                        mu2+=Sqr(curSampledColor);
                    }
                }
                mu /= sampleNum;
                mu2 /= sampleNum;
                sigma = SafeSqrt(mu2 - mu*mu);

                color = Clamp(color, mu-sigma*m_colorBoxK, mu+sigma*m_colorBoxK);
            }

            // Exponential moving average
            m_misc(x, y) = Lerp(color, curFilteredColor(x, y), alpha);
        }
    }
    std::swap(m_misc, m_accColor);
}

Buffer2D<Float3> Denoiser::Filter(const FrameInfo &frameInfo) {
    int height = frameInfo.m_beauty.m_height;
    int width = frameInfo.m_beauty.m_width;
    Buffer2D<Float3> filteredImage = CreateBuffer2D<Float3>(width, height);
    int kernelRadius = 16;
#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Joint bilateral filter
            // get current pixel vals
            Float3 color = frameInfo.m_beauty(x, y);
            Float3 normal = frameInfo.m_normal(x, y);
            Float3 pos = frameInfo.m_position(x, y);

            float weight_sum = 0.f;
            Float3 color_sum = Float3(0.f);
            for(int i = -kernelRadius; i <= kernelRadius; i++){
                for(int j = -kernelRadius; j <= kernelRadius; j++){
                    // clamp coordinate
                    int sampleCoord_x = std::min(std::max(0,x+i),width-1); 
                    int sampleCoord_y = std::min(std::max(0,y+j),height-1);
                    
                    // sample sampled pixel vals
                    Float3 sampledColor = frameInfo.m_beauty(sampleCoord_x,sampleCoord_y);
                    Float3 sampledNormal = frameInfo.m_normal(sampleCoord_x,sampleCoord_y);
                    Float3 sampledPos = frameInfo.m_position(sampleCoord_x,sampleCoord_y);

                    // calculate weight
                    float weight = 0.0;
                    float d_coord2 = i*i+j*j;
                    
                    float d_color2 = SqrDistance(color,sampledColor);
                    
                    float cosTheta = Dot(normal,sampledNormal);
                    float d_normal2 = SafeAcos(cosTheta)*SafeAcos(cosTheta);

                    float d_plane = 0.f;
                    float d_pos = Distance(pos, sampledPos);
                    if (d_pos > 0)
                        d_plane = Dot(normal,Normalize(sampledPos-pos));
                    float d_plane2 = d_plane*d_plane;

                    float w_coord = - d_coord2/(2*m_sigmaCoord*m_sigmaCoord);
                    float w_color = - d_color2/(2*m_sigmaColor*m_sigmaColor);
                    float w_normal = - d_normal2/(2*m_sigmaNormal*m_sigmaNormal);
                    float w_plane = - d_plane2/(2*m_sigmaPlane*m_sigmaPlane);

                    weight = exp(w_coord+w_color+w_normal+w_plane);
                    // accumulate weight and color buffer
                    weight_sum += weight;
                    color_sum += sampledColor * weight;
                }
            }
            // protect 0 weight
            if(weight_sum == 0.0f){
                filteredImage(x, y) = color;
            }
            else{
                filteredImage(x, y) = color_sum / weight_sum;
            }
        }
    }
    return filteredImage;
}

Buffer2D<Float3> Denoiser::ATWFilter(const FrameInfo &frameInfo) {
    int height = frameInfo.m_beauty.m_height;
    int width = frameInfo.m_beauty.m_width;
    Buffer2D<Float3> filteredImage = CreateBuffer2D<Float3>(width, height);
    int passes = 5;
#pragma omp parallel for
    // Accelerated by A-Trous Wavelet
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Joint bilateral filter
            // get current pixel vals
            Float3 color = frameInfo.m_beauty(x, y);
            Float3 normal = frameInfo.m_normal(x, y);
            Float3 pos = frameInfo.m_position(x, y);

            float weight_sum = 0.f;
            Float3 color_sum = Float3(0.f);

            for(int pass = 0; pass < passes; pass++){
                int stride = pow(2, pass);
                int kernelRadius = stride * 2;
                for(int i = -kernelRadius; i <= kernelRadius; i+=stride){
                    for(int j = -kernelRadius; j <= kernelRadius; j+=stride){
                        // clamp coordinate
                        int sampleCoord_x = std::min(std::max(0,x+i),width-1); 
                        int sampleCoord_y = std::min(std::max(0,y+j),height-1);
                        
                        // sample sampled pixel vals
                        Float3 sampledColor = frameInfo.m_beauty(sampleCoord_x,sampleCoord_y);
                        Float3 sampledNormal = frameInfo.m_normal(sampleCoord_x,sampleCoord_y);
                        Float3 sampledPos = frameInfo.m_position(sampleCoord_x,sampleCoord_y);

                        // calculate weight
                        float weight = 0.0;
                        float d_coord2 = i*i+j*j;
                        
                        float d_color2 = SqrDistance(color,sampledColor);
                        
                        float cosTheta = Dot(normal,sampledNormal);
                        float d_normal2 = SafeAcos(cosTheta)*SafeAcos(cosTheta);

                        float d_plane = 0.f;
                        float d_pos = Distance(pos, sampledPos);
                        if (d_pos > 0)
                            d_plane = Dot(normal,Normalize(sampledPos-pos));
                        float d_plane2 = d_plane*d_plane;

                        float w_coord = - d_coord2/(2*m_sigmaCoord*m_sigmaCoord);
                        float w_color = - d_color2/(2*m_sigmaColor*m_sigmaColor);
                        float w_normal = - d_normal2/(2*m_sigmaNormal*m_sigmaNormal);
                        float w_plane = - d_plane2/(2*m_sigmaPlane*m_sigmaPlane);

                        weight = exp(w_coord+w_color+w_normal+w_plane);
                        // accumulate weight and color buffer
                        weight_sum += weight;
                        color_sum += sampledColor * weight;
                    }
                }
                // protect 0 weight
                if(weight_sum == 0.0f){
                    filteredImage(x, y) = color;
                }
                else{
                    filteredImage(x, y) = color_sum / weight_sum;
                }
            }
        }
    }
    return filteredImage;
}

void Denoiser::Init(const FrameInfo &frameInfo, const Buffer2D<Float3> &filteredColor) {
    m_accColor.Copy(filteredColor);
    int height = m_accColor.m_height;
    int width = m_accColor.m_width;
    m_misc = CreateBuffer2D<Float3>(width, height);
    m_valid = CreateBuffer2D<bool>(width, height);
}

void Denoiser::Maintain(const FrameInfo &frameInfo) { m_preFrameInfo = frameInfo; }

Buffer2D<Float3> Denoiser::ProcessFrame(const FrameInfo &frameInfo) {
    // Filter current frame
    Buffer2D<Float3> filteredColor;
    // filteredColor = Filter(frameInfo);
    filteredColor = ATWFilter(frameInfo);

    // Reproject previous frame color to current
    if (m_useTemportal) {
        Reprojection(frameInfo);
        TemporalAccumulation(filteredColor);
    } else {
        Init(frameInfo, filteredColor);
    }

    // Maintain
    Maintain(frameInfo);
    if (!m_useTemportal) {
        m_useTemportal = true;
    }
    return m_accColor;
}
