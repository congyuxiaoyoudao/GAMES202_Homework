#include <nori/integrator.h>
#include <nori/scene.h>
#include <nori/ray.h>
#include <filesystem/resolver.h>
#include <sh/spherical_harmonics.h>
#include <sh/default_image.h>
#include <Eigen/Core>
#include <fstream>
#include <random>
#include <stb_image.h>

NORI_NAMESPACE_BEGIN

namespace ProjEnv
{
    std::vector<std::unique_ptr<float[]>>
    LoadCubemapImages(const std::string &cubemapDir, int &width, int &height,
                      int &channel)
    {
        std::vector<std::string> cubemapNames{"negx.jpg", "posx.jpg", "posy.jpg",
                                              "negy.jpg", "posz.jpg", "negz.jpg"};
        std::vector<std::unique_ptr<float[]>> images(6);
        for (int i = 0; i < 6; i++)
        {
            std::string filename = cubemapDir + "/" + cubemapNames[i];
            int w, h, c;
            float *image = stbi_loadf(filename.c_str(), &w, &h, &c, 3);
            if (!image)
            {
                std::cout << "Failed to load image: " << filename << std::endl;
                exit(-1);
            }
            if (i == 0)
            {
                width = w;
                height = h;
                channel = c;
            }
            else if (w != width || h != height || c != channel)
            {
                std::cout << "Dismatch resolution for 6 images in cubemap" << std::endl;
                exit(-1);
            }
            images[i] = std::unique_ptr<float[]>(image);
            int index = (0 * 128 + 0) * channel;
            // std::cout << images[i][index + 0] << "\t" << images[i][index + 1] << "\t"
            //           << images[i][index + 2] << std::endl;
        }
        return images;
    }

    const Eigen::Vector3f cubemapFaceDirections[6][3] = {
        {{0, 0, 1}, {0, -1, 0}, {-1, 0, 0}},  // negx
        {{0, 0, 1}, {0, -1, 0}, {1, 0, 0}},   // posx
        {{1, 0, 0}, {0, 0, -1}, {0, -1, 0}},  // negy
        {{1, 0, 0}, {0, 0, 1}, {0, 1, 0}},    // posy
        {{-1, 0, 0}, {0, -1, 0}, {0, 0, -1}}, // negz
        {{1, 0, 0}, {0, -1, 0}, {0, 0, 1}},   // posz
    };

    float CalcPreArea(const float &x, const float &y)
    {
        return std::atan2(x * y, std::sqrt(x * x + y * y + 1.0));
    }

    float CalcArea(const float &u_, const float &v_, const int &width,
                   const int &height)
    {
        // transform from [0..res - 1] to [- (1 - 1 / res) .. (1 - 1 / res)]
        // ( 0.5 is for texel center addressing)
        float u = (2.0 * (u_ + 0.5) / width) - 1.0;
        float v = (2.0 * (v_ + 0.5) / height) - 1.0;

        // shift from a demi texel, mean 1.0 / size  with u and v in [-1..1]
        float invResolutionW = 1.0 / width;
        float invResolutionH = 1.0 / height;

        // u and v are the -1..1 texture coordinate on the current face.
        // get projected area for this texel
        float x0 = u - invResolutionW;
        float y0 = v - invResolutionH;
        float x1 = u + invResolutionW;
        float y1 = v + invResolutionH;
        float angle = CalcPreArea(x0, y0) - CalcPreArea(x0, y1) -
                      CalcPreArea(x1, y0) + CalcPreArea(x1, y1);

        return angle;
    }

    // template <typename T> T ProjectSH() {}

    template <size_t SHOrder>
    std::vector<Eigen::Array3f> PrecomputeCubemapSH(const std::vector<std::unique_ptr<float[]>> &images,
                                                    const int &width, const int &height,
                                                    const int &channel)
    {
        std::vector<Eigen::Vector3f> cubemapDirs;
        cubemapDirs.reserve(6 * width * height);
        // Compute and store all 6 faces' each pixel direction vector(normalized)  
        for (int i = 0; i < 6; i++)
        {
            Eigen::Vector3f faceDirX = cubemapFaceDirections[i][0];
            Eigen::Vector3f faceDirY = cubemapFaceDirections[i][1];
            Eigen::Vector3f faceDirZ = cubemapFaceDirections[i][2];
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    float u = 2 * ((x + 0.5) / width) - 1;
                    float v = 2 * ((y + 0.5) / height) - 1;
                    Eigen::Vector3f dir = (faceDirX * u + faceDirY * v + faceDirZ).normalized();
                    cubemapDirs.push_back(dir);
                }
            }
        }
        constexpr int SHNum = (SHOrder + 1) * (SHOrder + 1); // SHOrder: l = 2, SHNum: (l+1)^2 = 9
        std::vector<Eigen::Array3f> SHCoeffiecents(SHNum);
        for (int i = 0; i < SHNum; i++)
            SHCoeffiecents[i] = Eigen::Array3f(0);
        float sumWeight = 0;
        // Compute all 6 faces' each pixel's SH coef by projecting cubemap on basis SH functions
        for (int i = 0; i < 6; i++)
        {
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    Eigen::Vector3f dir = cubemapDirs[i * width * height + y * width + x];
                    int index = (y * width + x) * channel;
                    Eigen::Array3f Le(images[i][index + 0], images[i][index + 1],
                                      images[i][index + 2]);

                    // Begin TOP changes Compute SH coef by \sigma_i Le * SH[i] d_\w_i
                    auto dwi = CalcArea(x, y, width, height);
                    for (int l = 0; l <= SHOrder; l++) // each level
                    {
                        for (int m = -l; m <= l; m++) // each level's basis
                        {
                            int SHindex = sh::GetIndex(l, m); // do a remap for index
                            double SHbasis = sh::EvalSH(l, m, dir.cast<double>().normalized()); // get hard encoded SH basis by l, m
                            SHCoeffiecents[SHindex] += Le * SHbasis * dwi; // accumulate each pixel's SH coef
                        }
                        
                    }
                    // End TOP changes
                }
            }
        }
        return SHCoeffiecents;
    }
}

class PRTIntegrator : public Integrator
{
public:
    static constexpr int SHOrder = 2;
    static constexpr int SHCoeffLength = (SHOrder + 1) * (SHOrder + 1);

    enum class Type
    {
        Unshadowed = 0,
        Shadowed = 1,
        Interreflection = 2
    };

    PRTIntegrator(const PropertyList &props)
    {
        /* No parameters this time */
        m_SampleCount = props.getInteger("PRTSampleCount", 100);
        m_CubemapPath = props.getString("cubemap");
        auto type = props.getString("type", "unshadowed");
        if (type == "unshadowed")
        {
            m_Type = Type::Unshadowed;
        }
        else if (type == "shadowed")
        {
            m_Type = Type::Shadowed;
        }
        else if (type == "interreflection")
        {
            m_Type = Type::Interreflection;
            m_Bounce = props.getInteger("bounce", 1);
        }
        else
        {
            throw NoriException("Unsupported type: %s.", type);
        }
    }

    virtual void preprocess(const Scene *scene) override
    {

        // Here only compute one mesh
        const auto mesh = scene->getMeshes()[0];
        // Projection environment
        auto cubePath = getFileResolver()->resolve(m_CubemapPath);
        auto lightPath = cubePath / "light.txt";
        auto transPath = cubePath / "transport.txt";
        std::ofstream lightFout(lightPath.str());
        std::ofstream fout(transPath.str());
        int width, height, channel;
        std::vector<std::unique_ptr<float[]>> images =
            ProjEnv::LoadCubemapImages(cubePath.str(), width, height, channel);
        auto envCoeffs = ProjEnv::PrecomputeCubemapSH<SHOrder>(images, width, height, channel);
        m_LightCoeffs.resize(3, SHCoeffLength);
        for (int i = 0; i < envCoeffs.size(); i++)
        {
            lightFout << (envCoeffs)[i].x() << " " << (envCoeffs)[i].y() << " " << (envCoeffs)[i].z() << std::endl;
            m_LightCoeffs.col(i) = (envCoeffs)[i];
        }
        std::cout << "Computed light sh coeffs from: " << cubePath.str() << " to: " << lightPath.str() << std::endl;
        // Projection transport
        m_TransportSHCoeffs.resize(SHCoeffLength, mesh->getVertexCount());
        fout << mesh->getVertexCount() << std::endl;
        for (int i = 0; i < mesh->getVertexCount(); i++)
        {
            const Point3f &v = mesh->getVertexPositions().col(i);
            const Normal3f &n = mesh->getVertexNormals().col(i);
            // Begin TOP changes proj function lambda
            auto shFunc = [&](double phi, double theta) -> double {
                Eigen::Array3d d = sh::ToVector(phi, theta);
                const auto wi = Vector3f(d.x(), d.y(), d.z());
                float H = wi.dot(n);
                if (m_Type == Type::Unshadowed)
                {
                    return H > 0.0 ? H : 0;
                }
                else
                {
                    // Visibility term : if incident ray is occluded by scene
                    float visibility = scene->rayIntersect(Ray3f(v, wi))? 0.0 : 1.0;
                    return std::max(H,0.0f) * visibility;
                }
            };
            // End TOP changes
            auto shCoeff = sh::ProjectFunction(SHOrder, shFunc, m_SampleCount);
            for (int j = 0; j < shCoeff->size(); j++)
            {
                m_TransportSHCoeffs.col(i).coeffRef(j) = (*shCoeff)[j] / M_PI;// devide by pi assume that rho = 1
            }
        }
        if (m_Type == Type::Interreflection)
        {
            // for each vertex cast a ray to the scene
            // if occluded return interpolated sh coef
            // then recursively call this function
            for(int i = 0;i < mesh->getVertexCount(); i++)
            {
                const Point3f &v = mesh->getVertexPositions().col(i);
                const Normal3f &n = mesh->getVertexNormals().col(i);

                // iterate lambda
                // need to pass pos, normal, original coeff and current bounce
                std::function<std::unique_ptr<std::vector<double>>(Eigen::MatrixXf*, const Point3f&, const Normal3f&, const Scene*, int)> iterateSHFunc;
                iterateSHFunc = [&](Eigen::MatrixXf* TransportSHCoeffs, const Point3f &pos, const Normal3f &normal, const Scene* s, int bounce) -> std::unique_ptr<std::vector<double>>
                {
                    // Step 1: allocate and assign 0 to coeffs
                    std::unique_ptr<std::vector<double>> coeffs(new std::vector<double>());
                    coeffs->assign(SHCoeffLength, 0.0);
                
                    // Step 2: judge by bounce to terminate
                    if (bounce > m_Bounce)
                        return coeffs;
                
                    // Step 3: generate sample_side^2 uniformly and stratified samples over the sphere
                    const int sample_side = static_cast<int>(floor(sqrt(m_SampleCount)));
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_real_distribution<> rng(0.0, 1.0);
                    for (int t = 0; t < sample_side; t++)
                    {
                        for (int p = 0; p < sample_side; p++)
                        {
                            double alpha = (t + rng(gen)) / sample_side;
                            double beta = (p + rng(gen)) / sample_side;
                            double phi = 2.0 * M_PI * beta;
                            double theta = acos(2.0 * alpha - 1.0);
                
                            // Step 4: for each wi cast a ray to the scene
                            Eigen::Array3d d = sh::ToVector(phi, theta);
                            const auto wi = Vector3f(d.x(), d.y(), d.z());
                            float H = wi.dot(normal); 
                
                            Ray3f ray = Ray3f(pos, wi);
                            Intersection its;
                            if(H > 0.0 && s->rayIntersect(ray, its))
                            {
                                // Step 5: if occluded, get the intersected point and its normal
                                Point3f intersectPos = its.p;
                                Vector3f baryCoord = its.bary;
                                Point3f idx = its.tri_index;
                                MatrixXf normals = mesh->getVertexNormals();
                                Normal3f intersectNormal = Normal3f(normals.col(idx.x()).normalized() * baryCoord.x() +
                                                                        normals.col(idx.y()).normalized() * baryCoord.y() +
                                                                        normals.col(idx.z()).normalized() * baryCoord.z()).normalized();
                                auto nextBounceCoeffs = iterateSHFunc(TransportSHCoeffs, intersectPos, intersectNormal, scene, bounce + 1);
                
                                // Step 6: accumulate the coeffs
                                for (int k = 0; k < SHCoeffLength; k++)
                                {
                                    auto interpolateSH = (TransportSHCoeffs->col(idx.x()).coeffRef(k) * baryCoord.x() +
                                                                TransportSHCoeffs->col(idx.y()).coeffRef(k) * baryCoord.y() +
                                                                TransportSHCoeffs->col(idx.z()).coeffRef(k) * baryCoord.z());
                
                                    (*coeffs)[k] += (interpolateSH + (*nextBounceCoeffs)[k]) * H;
                                }
                            }
                        }
                    }
                    // Step 7: return the coeffs
                    double weight = (sample_side * sample_side);
                    for (unsigned int c = 0; c < coeffs->size(); c++) {
                        (*coeffs)[c] /= weight;
                    }
                
                    return coeffs;
                };
                
                // Step 8: call the iterateSHFunc
                auto interreflectCoeffs = iterateSHFunc(&m_TransportSHCoeffs, v, n, scene, 1);
                for (int j = 0; j < SHCoeffLength; j++)
                {
                    m_TransportSHCoeffs.col(i).coeffRef(j) += (*interreflectCoeffs)[j];
                }
            }
        }

        // Save in face format
        for (int f = 0; f < mesh->getTriangleCount(); f++)
        {
            const MatrixXu &F = mesh->getIndices();
            uint32_t idx0 = F(0, f), idx1 = F(1, f), idx2 = F(2, f);
            for (int j = 0; j < SHCoeffLength; j++)
            {
                fout << m_TransportSHCoeffs.col(idx0).coeff(j) << " ";
            }
            fout << std::endl;
            for (int j = 0; j < SHCoeffLength; j++)
            {
                fout << m_TransportSHCoeffs.col(idx1).coeff(j) << " ";
            }
            fout << std::endl;
            for (int j = 0; j < SHCoeffLength; j++)
            {
                fout << m_TransportSHCoeffs.col(idx2).coeff(j) << " ";
            }
            fout << std::endl;
        }
        std::cout << "Computed SH coeffs"
                  << " to: " << transPath.str() << std::endl;
    }

    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const
    {
        Intersection its;
        if (!scene->rayIntersect(ray, its))
            return Color3f(0.0f);

        const Eigen::Matrix<Vector3f::Scalar, SHCoeffLength, 1> sh0 = m_TransportSHCoeffs.col(its.tri_index.x()),
                                                                sh1 = m_TransportSHCoeffs.col(its.tri_index.y()),
                                                                sh2 = m_TransportSHCoeffs.col(its.tri_index.z());
        const Eigen::Matrix<Vector3f::Scalar, SHCoeffLength, 1> rL = m_LightCoeffs.row(0), gL = m_LightCoeffs.row(1), bL = m_LightCoeffs.row(2);

        Color3f c0 = Color3f(rL.dot(sh0), gL.dot(sh0), bL.dot(sh0)),
                c1 = Color3f(rL.dot(sh1), gL.dot(sh1), bL.dot(sh1)),
                c2 = Color3f(rL.dot(sh2), gL.dot(sh2), bL.dot(sh2));

        const Vector3f &bary = its.bary;
        Color3f c = bary.x() * c0 + bary.y() * c1 + bary.z() * c2;
        return c;
    }

    std::string toString() const
    {
        return "PRTIntegrator[]";
    }

private:
    Type m_Type;
    int m_Bounce = 1;
    int m_SampleCount = 100;
    std::string m_CubemapPath;
    Eigen::MatrixXf m_TransportSHCoeffs;
    Eigen::MatrixXf m_LightCoeffs;
};

NORI_REGISTER_CLASS(PRTIntegrator, "prt");
NORI_NAMESPACE_END