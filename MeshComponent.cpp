#include "MeshComponent.h"
#include <d3dcompiler.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <cctype>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cfloat>
#include <cmath>
#include <string_view>
#include <cstring>

using namespace DirectX;
namespace megaEngine {

    namespace {

        bool IsBinaryFbx(const std::string& data)
        {
            static const char magic[] = "Kaydara FBX Binary";
            return data.size() >= sizeof(magic) - 1 &&
                data.compare(0, sizeof(magic) - 1, magic) == 0;
        }

        bool ExtractBraceContent(const std::string& text, const char* keyword, size_t searchStart,
            size_t& outStart, size_t& outEnd)
        {
            size_t k = text.find(keyword, searchStart);
            if (k == std::string::npos) return false;
            size_t open = text.find('{', k);
            if (open == std::string::npos) return false;
            int depth = 1;
            size_t i = open + 1;
            for (; i < text.size() && depth > 0; ++i) {
                if (text[i] == '{') ++depth;
                else if (text[i] == '}') --depth;
            }
            if (depth != 0) return false;
            outStart = open + 1;
            outEnd = i - 1;
            return outStart < outEnd;
        }

        void ParseFloatsFromBlock(std::string_view block, std::vector<float>& out)
        {
            const char* p = block.data();
            const char* end = p + block.size();
            while (p < end) {
                while (p < end && std::isspace(static_cast<unsigned char>(*p))) ++p;
                if (p >= end) break;
                char* ep = nullptr;
                float v = std::strtof(p, &ep);
                if (ep == p) {
                    ++p;
                    continue;
                }
                out.push_back(v);
                p = ep;
            }
        }

        void ParseIntsFromBlock(std::string_view block, std::vector<int>& out)
        {
            const char* p = block.data();
            const char* end = p + block.size();
            while (p < end) {
                while (p < end && std::isspace(static_cast<unsigned char>(*p))) ++p;
                if (p >= end) break;
                char* ep = nullptr;
                long v = std::strtol(p, &ep, 10);
                if (ep == p) {
                    ++p;
                    continue;
                }
                out.push_back(static_cast<int>(v));
                p = ep;
            }
        }

        void ComputeNormalsFromIndices(const std::vector<XMFLOAT4>& positions, const std::vector<UINT>& indices,
            std::vector<XMFLOAT3>& outNormals)
        {
            outNormals.assign(positions.size(), XMFLOAT3(0, 0, 0));
            for (size_t t = 0; t + 2 < indices.size(); t += 3) {
                UINT i0 = indices[t], i1 = indices[t + 1], i2 = indices[t + 2];
                XMVECTOR p0 = XMLoadFloat4(&positions[i0]);
                XMVECTOR p1 = XMLoadFloat4(&positions[i1]);
                XMVECTOR p2 = XMLoadFloat4(&positions[i2]);
                XMVECTOR e1 = XMVectorSubtract(p1, p0);
                XMVECTOR e2 = XMVectorSubtract(p2, p0);
                XMVECTOR n = XMVector3Normalize(XMVector3Cross(e1, e2));
                XMFLOAT3 nf;
                XMStoreFloat3(&nf, n);
                outNormals[i0].x += nf.x; outNormals[i0].y += nf.y; outNormals[i0].z += nf.z;
                outNormals[i1].x += nf.x; outNormals[i1].y += nf.y; outNormals[i1].z += nf.z;
                outNormals[i2].x += nf.x; outNormals[i2].y += nf.y; outNormals[i2].z += nf.z;
            }
            for (auto& a : outNormals) {
                XMVECTOR v = XMLoadFloat3(&a);
                if (XMVectorGetX(XMVector3LengthSq(v)) < 1e-20f)
                    a = XMFLOAT3(0, 1, 0);
                else {
                    v = XMVector3Normalize(v);
                    XMStoreFloat3(&a, v);
                }
            }
        }

        constexpr float kSphereRollAngleScale = 0.28f;

        bool BuildTrianglesFromPolygonIndices(const std::vector<int>& polyIdx, std::vector<UINT>& tri,
            UINT maxVert)
        {
            std::vector<int> face;
            for (int raw : polyIdx) {
                int idx = raw < 0 ? -raw - 1 : raw;
                if (idx < 0 || static_cast<UINT>(idx) >= maxVert)
                    return false;
                face.push_back(idx);
                if (raw < 0) {
                    if (face.size() < 3)
                        return false;
                    for (size_t i = 1; i + 1 < face.size(); ++i) {
                        tri.push_back(static_cast<UINT>(face[0]));
                        tri.push_back(static_cast<UINT>(face[i]));
                        tri.push_back(static_cast<UINT>(face[i + 1]));
                    }
                    face.clear();
                }
            }
            if (!face.empty())
                return false;
            return !tri.empty();
        }

        XMFLOAT4 DefaultLightDirAmbient()
        {
            XMVECTOR L = XMVector3Normalize(XMVectorSet(0.45f, 1.0f, 0.35f, 0.0f));
            XMFLOAT3 ld;
            XMStoreFloat3(&ld, L);
            return XMFLOAT4(ld.x, ld.y, ld.z, 0.28f);
        }

        bool ExtractInnerOfBrace(const std::string& text, size_t openBrace, size_t& innerStart, size_t& innerEnd)
        {
            if (openBrace >= text.size() || text[openBrace] != '{') return false;
            innerStart = openBrace + 1;
            int depth = 1;
            for (size_t i = openBrace + 1; i < text.size(); ++i) {
                if (text[i] == '{') ++depth;
                else if (text[i] == '}') {
                    --depth;
                    if (depth == 0) {
                        innerEnd = i;
                        return true;
                    }
                }
            }
            return false;
        }

        bool FindGeometryBodyEnd(const std::string& text, size_t refPos, size_t& bodyEndExclusive)
        {
            size_t g = text.rfind("Geometry:", refPos);
            if (g == std::string::npos) return false;
            size_t open = text.find('{', g);
            if (open == std::string::npos) return false;
            size_t innerS = 0, innerE = 0;
            if (!ExtractInnerOfBrace(text, open, innerS, innerE)) return false;
            bodyEndExclusive = innerE;
            return true;
        }

        bool GetFbxQuotedStringProp(const std::string& block, const char* key, std::string& out)
        {
            size_t p = block.find(key);
            if (p == std::string::npos) return false;
            p += strlen(key);
            while (p < block.size() && std::isspace(static_cast<unsigned char>(block[p]))) ++p;
            if (p >= block.size() || block[p] != '"') return false;
            ++p;
            size_t q = block.find('"', p);
            if (q == std::string::npos) return false;
            out.assign(block.data() + p, block.data() + q);
            return true;
        }

        bool ParseFirstLayerElementUV(const std::string& text, size_t searchFrom, size_t geomBodyEnd,
            std::string& mapping, std::string& reference,
            std::vector<float>& uvFloats, std::vector<int>& uvIndexInts)
        {
            uvFloats.clear();
            uvIndexInts.clear();
            mapping.clear();
            reference.clear();
            size_t uvKey = text.find("LayerElementUV:", searchFrom);
            if (uvKey == std::string::npos || uvKey >= geomBodyEnd) return false;
            size_t open = text.find('{', uvKey);
            if (open == std::string::npos || open >= geomBodyEnd) return false;
            size_t layerIn0 = 0, layerIn1 = 0;
            if (!ExtractInnerOfBrace(text, open, layerIn0, layerIn1)) return false;
            if (layerIn1 > geomBodyEnd) return false;

            const std::string layer(text.data() + layerIn0, layerIn1 - layerIn0);
            GetFbxQuotedStringProp(layer, "MappingInformationType:", mapping);
            GetFbxQuotedStringProp(layer, "ReferenceInformationType:", reference);

            size_t uvDataStart = 0, uvDataEnd = 0;
            if (!ExtractBraceContent(layer, "UV:", 0, uvDataStart, uvDataEnd)) return false;
            ParseFloatsFromBlock(std::string_view(layer.data() + uvDataStart, uvDataEnd - uvDataStart), uvFloats);
            if (uvFloats.size() < 2) return false;

            size_t uvi0 = 0, uvi1 = 0;
            if (ExtractBraceContent(layer, "UVIndex:", 0, uvi0, uvi1))
                ParseIntsFromBlock(std::string_view(layer.data() + uvi0, uvi1 - uvi0), uvIndexInts);
            return true;
        }

        bool BuildUvByPolygonVertexIndexToDirect(const std::vector<int>& polyIdx, const std::vector<float>& uvFlat,
            const std::vector<int>& uvIndex, UINT maxVert, std::vector<XMFLOAT2>& outUvPerCorner)
        {
            outUvPerCorner.clear();
            std::vector<int> face;
            size_t uvIx = 0;
            for (int raw : polyIdx) {
                int idx = raw < 0 ? -raw - 1 : raw;
                if (idx < 0 || static_cast<UINT>(idx) >= maxVert)
                    return false;
                face.push_back(idx);
                if (raw < 0) {
                    if (face.size() < 3) return false;
                    for (size_t i = 1; i + 1 < face.size(); ++i) {
                        for (int k = 0; k < 3; ++k) {
                            if (uvIx >= uvIndex.size()) return false;
                            int uvi = uvIndex[uvIx++];
                            if (uvi < 0 || static_cast<size_t>(2 * uvi + 1) >= uvFlat.size())
                                outUvPerCorner.push_back(XMFLOAT2(0.f, 0.f));
                            else
                                outUvPerCorner.push_back(XMFLOAT2(uvFlat[static_cast<size_t>(2 * uvi)],
                                    1.f - uvFlat[static_cast<size_t>(2 * uvi + 1)]));
                        }
                    }
                    face.clear();
                }
            }
            if (!face.empty()) return false;
            return uvIx == uvIndex.size();
        }

        bool BuildUvByPolygonVertexDirect(const std::vector<int>& polyIdx, const std::vector<float>& uvFlat,
            UINT maxVert, std::vector<XMFLOAT2>& outUvPerCorner)
        {
            outUvPerCorner.clear();
            std::vector<int> face;
            size_t pairBase = 0;
            for (int raw : polyIdx) {
                int idx = raw < 0 ? -raw - 1 : raw;
                if (idx < 0 || static_cast<UINT>(idx) >= maxVert)
                    return false;
                face.push_back(idx);
                if (raw < 0) {
                    const size_t nc = face.size();
                    if (nc < 3) return false;
                    if (pairBase + 2 * nc > uvFlat.size()) return false;
                    auto cornerUV = [&](size_t fi) {
                        return XMFLOAT2(uvFlat[pairBase + 2 * fi], 1.f - uvFlat[pairBase + 2 * fi + 1]);
                    };
                    for (size_t i = 1; i + 1 < nc; ++i) {
                        outUvPerCorner.push_back(cornerUV(0));
                        outUvPerCorner.push_back(cornerUV(i));
                        outUvPerCorner.push_back(cornerUV(i + 1));
                    }
                    pairBase += 2 * nc;
                    face.clear();
                }
            }
            if (!face.empty()) return false;
            return pairBase == uvFlat.size();
        }

        void BuildUvByVerticeDirect(const std::vector<UINT>& triIndices, const std::vector<float>& uvFlat,
            std::vector<XMFLOAT2>& outUvPerCorner)
        {
            outUvPerCorner.resize(triIndices.size());
            for (size_t k = 0; k < triIndices.size(); ++k) {
                UINT vi = triIndices[k];
                if (static_cast<size_t>(2 * vi + 1) < uvFlat.size())
                    outUvPerCorner[k] = XMFLOAT2(uvFlat[2 * vi], 1.f - uvFlat[2 * vi + 1]);
                else
                    outUvPerCorner[k] = XMFLOAT2(0.f, 0.f);
            }
        }
    }

    struct CBPerObject {
        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX proj;
        XMFLOAT4 color;
        XMFLOAT4 lightDirAmbient;
    };

    MeshComponent::MeshComponent(Type type, const std::wstring& fbxPath) : type_(type), fbxPath_(fbxPath) {}
    MeshComponent::~MeshComponent() { Shutdown(); }

    bool MeshComponent::CompileShader(const wchar_t* filename, const char* entryPoint,
        const char* target, ID3DBlob** blob, D3D_SHADER_MACRO* macros)
    {
        ID3DBlob* errorBlob = nullptr;
        HRESULT res = D3DCompileFromFile(filename, macros, nullptr, entryPoint, target,
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0, blob, &errorBlob);

        if (FAILED(res)) {
            if (errorBlob) {
                std::cout << static_cast<char*>(errorBlob->GetBufferPointer()) << std::endl;
                errorBlob->Release();
            }
            else {
                std::wcout << L"Shader file not found: " << filename << std::endl;
            }
            return false;
        }
        return true;
    }

    void MeshComponent::CreateGpuMesh(const std::vector<Vertex>& verts, const std::vector<UINT>& indices)
    {
        indexCount_ = static_cast<UINT>(indices.size());
        ID3D11Device* devicePtr = nullptr;
        context_->GetDevice(&devicePtr);

        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * verts.size());
        D3D11_SUBRESOURCE_DATA vbData = { verts.data(), 0, 0 };
        devicePtr->CreateBuffer(&vbDesc, &vbData, vertexBuffer_.GetAddressOf());

        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.Usage = D3D11_USAGE_DEFAULT;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibDesc.ByteWidth = static_cast<UINT>(sizeof(UINT) * indices.size());
        D3D11_SUBRESOURCE_DATA ibData = { indices.data(), 0, 0 };
        devicePtr->CreateBuffer(&ibDesc, &ibData, indexBuffer_.GetAddressOf());
        devicePtr->Release();
    }

    void MeshComponent::CreateBox(float size)
    {
        float s = size * 0.5f;
        std::vector<Vertex> verts;
        const XMFLOAT2 zuv(0.f, 0.f);
        auto V = [&](float x, float y, float z, float nx, float ny, float nz) {
            verts.push_back({ XMFLOAT4(x, y, z, 1.0f), color_, XMFLOAT4(nx, ny, nz, 0.0f), zuv });
        };
        V(-s, -s, s, 0, 0, 1); V(s, -s, s, 0, 0, 1); V(s, s, s, 0, 0, 1); V(-s, s, s, 0, 0, 1);
        V(s, -s, -s, 0, 0, -1); V(-s, -s, -s, 0, 0, -1); V(-s, s, -s, 0, 0, -1); V(s, s, -s, 0, 0, -1);
        V(s, -s, s, 1, 0, 0); V(s, -s, -s, 1, 0, 0); V(s, s, -s, 1, 0, 0); V(s, s, s, 1, 0, 0);
        V(-s, -s, -s, -1, 0, 0); V(-s, -s, s, -1, 0, 0); V(-s, s, s, -1, 0, 0); V(-s, s, -s, -1, 0, 0);
        V(-s, s, s, 0, 1, 0); V(s, s, s, 0, 1, 0); V(s, s, -s, 0, 1, 0); V(-s, s, -s, 0, 1, 0);
        V(-s, -s, -s, 0, -1, 0); V(s, -s, -s, 0, -1, 0); V(s, -s, s, 0, -1, 0); V(-s, -s, s, 0, -1, 0);
        std::vector<UINT> indices = {
            0,1,2, 0,2,3, 4,5,6, 4,6,7, 8,9,10, 8,10,11, 12,13,14, 12,14,15,
            16,17,18, 16,18,19, 20,21,22, 20,22,23
        };
        CreateGpuMesh(verts, indices);
        pickupBoundsRadius_ = 0.5f * sqrtf(3.0f);
    }

    void MeshComponent::CreateSphere(float radius, int slices, int stacks)
    {
        std::vector<Vertex> verts;
        std::vector<UINT> indices;

        for (int y = 0; y <= stacks; ++y) {
            float v = (float)y / stacks;
            float phi = v * XM_PI;
            for (int x = 0; x <= slices; ++x) {
                float u = (float)x / slices;
                float theta = u * XM_2PI;
                float sx = sinf(phi) * cosf(theta);
                float sy = cosf(phi);
                float sz = sinf(phi) * sinf(theta);
                verts.push_back({
                    XMFLOAT4(radius * sx, radius * sy, radius * sz, 1.0f), color_,
                    XMFLOAT4(sx, sy, sz, 0.0f),
                    XMFLOAT2(u, 1.0f - v)
                });
            }
        }

        for (int y = 0; y < stacks; ++y) {
            for (int x = 0; x < slices; ++x) {
                int i0 = y * (slices + 1) + x;
                int i1 = i0 + 1;
                int i2 = i0 + (slices + 1);
                int i3 = i2 + 1;
                indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
                indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
            }
        }

        CreateGpuMesh(verts, indices);
    }

    bool MeshComponent::LoadFbxAsciiFile()
    {
        if (fbxPath_.empty()) {
            std::cout << "MeshComponent FbxFile: empty path.\n";
            return false;
        }
        const std::filesystem::path filePath(fbxPath_);
        if (!std::filesystem::exists(filePath)) {
            std::wcout << L"FBX file not found: " << fbxPath_ << std::endl;
            return false;
        }
        if (!std::filesystem::is_regular_file(filePath)) {
            std::wcout << L"FBX path is not a file: " << fbxPath_ << std::endl;
            return false;
        }
        std::ifstream ifs(filePath, std::ios::binary);
        if (!ifs) {
            std::wcout << L"Failed to open FBX file: " << fbxPath_ << std::endl;
            return false;
        }
        std::string text((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        if (text.empty())
            return false;
        if (IsBinaryFbx(text)) {
            std::cout << "FBX is binary. Re-export as ASCII FBX from Blender, or use Assimp/ufbx.\n";
            return false;
        }

        const size_t vKeyPos = text.find("Vertices:");
        if (vKeyPos == std::string::npos) {
            std::cout << "ASCII FBX: Vertices not found.\n";
            return false;
        }

        size_t vStart = 0, vEnd = 0;
        if (!ExtractBraceContent(text, "Vertices:", 0, vStart, vEnd)) {
            std::cout << "ASCII FBX: Vertices block not found.\n";
            return false;
        }
        std::vector<float> vf;
        ParseFloatsFromBlock(std::string_view(text.data() + vStart, vEnd - vStart), vf);
        if (vf.size() < 9 || vf.size() % 3 != 0) {
            std::cout << "ASCII FBX: invalid Vertices data.\n";
            return false;
        }

        size_t iStart = 0, iEnd = 0;
        if (!ExtractBraceContent(text, "PolygonVertexIndex:", vStart, iStart, iEnd)) {
            std::cout << "ASCII FBX: PolygonVertexIndex block not found.\n";
            return false;
        }
        std::vector<int> poly;
        ParseIntsFromBlock(std::string_view(text.data() + iStart, iEnd - iStart), poly);
        if (poly.empty()) {
            std::cout << "ASCII FBX: empty polygon indices.\n";
            return false;
        }

        const UINT nVerts = static_cast<UINT>(vf.size() / 3);
        std::vector<UINT> indices;
        if (!BuildTrianglesFromPolygonIndices(poly, indices, nVerts)) {
            std::cout << "ASCII FBX: could not build triangle list from polygons.\n";
            return false;
        }

        std::vector<XMFLOAT4> positions;
        positions.reserve(nVerts);
        pickupBoundsRadius_ = 0.0f;
        for (UINT i = 0; i < nVerts; ++i) {
            float x = vf[i * 3], y = vf[i * 3 + 1], z = vf[i * 3 + 2];
            positions.push_back(XMFLOAT4(x, y, z, 1.0f));
            float len = sqrtf(x * x + y * y + z * z);
            if (len > pickupBoundsRadius_) pickupBoundsRadius_ = len;
        }
        if (pickupBoundsRadius_ < 1e-4f) pickupBoundsRadius_ = 0.35f;

        std::vector<XMFLOAT3> norms;
        ComputeNormalsFromIndices(positions, indices, norms);

        size_t geomBodyEnd = text.size();
        FindGeometryBodyEnd(text, vKeyPos, geomBodyEnd);

        std::string mapStr, refStr;
        std::vector<float> uvFlat;
        std::vector<int> uvIndexInts;
        const bool haveUvLayer = ParseFirstLayerElementUV(text, iEnd, geomBodyEnd, mapStr, refStr, uvFlat, uvIndexInts);

        std::vector<XMFLOAT2> cornerUV(indices.size(), XMFLOAT2(0.f, 0.f));
        if (haveUvLayer) {
            std::vector<XMFLOAT2> tmp;
            bool ok = false;
            if (mapStr == "ByPolygonVertex" && refStr == "IndexToDirect" && !uvIndexInts.empty())
                ok = BuildUvByPolygonVertexIndexToDirect(poly, uvFlat, uvIndexInts, nVerts, tmp);
            else if (mapStr == "ByPolygonVertex" && refStr == "Direct")
                ok = BuildUvByPolygonVertexDirect(poly, uvFlat, nVerts, tmp);
            else if ((mapStr == "ByVertice" || mapStr == "ByVertex") && refStr == "Direct"
                && uvFlat.size() >= static_cast<size_t>(2) * nVerts) {
                BuildUvByVerticeDirect(indices, uvFlat, tmp);
                ok = (tmp.size() == indices.size());
            }
            if (ok && tmp.size() == indices.size())
                cornerUV = std::move(tmp);
        }

        std::vector<Vertex> verts;
        verts.reserve(indices.size());
        for (size_t k = 0; k < indices.size(); ++k) {
            UINT vi = indices[k];
            const XMFLOAT3& n = norms[vi];
            verts.push_back({
                positions[vi], color_,
                XMFLOAT4(n.x, n.y, n.z, 0.0f),
                cornerUV[k]
            });
        }

        std::vector<UINT> linearIdx(static_cast<size_t>(indices.size()));
        for (size_t k = 0; k < linearIdx.size(); ++k)
            linearIdx[k] = static_cast<UINT>(k);

        importBasis_ = XMMatrixRotationX(-XM_PIDIV2);

        {
            const XMMATRIX rot = XMMatrixRotationRollPitchYaw(selfAngles_.x, selfAngles_.y, selfAngles_.z);
            const XMMATRIX linear = XMMatrixScaling(scale_, scale_, scale_) * rot * importBasis_;
            float minTy = FLT_MAX;
            for (const auto& p : positions) {
                XMVECTOR t = XMVector4Transform(XMLoadFloat4(&p), linear);
                const float y = XMVectorGetY(t);
                if (y < minTy) minTy = y;
            }
            if (minTy < FLT_MAX && std::isfinite(minTy))
                position_.y -= minTy;
        }

        CreateGpuMesh(verts, linearIdx);
        return indexCount_ > 0;
    }

    bool MeshComponent::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, HWND hwnd)
    {
        context_ = context;
        ID3DBlob* vsBlob = nullptr;
        if (!CompileShader(L"./Shaders/MyVeryFirstShader.hlsl", "VSMain", "vs_5_0", &vsBlob)) return false;
        if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader_))) return false;

        ID3DBlob* psBlob = nullptr;
        if (!CompileShader(L"./Shaders/MyVeryFirstShader.hlsl", "PSMain", "ps_5_0", &psBlob)) { vsBlob->Release(); return false; }
        if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader_))) { vsBlob->Release(); psBlob->Release(); return false; }

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        if (FAILED(device->CreateInputLayout(layout, 4, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout_))) { vsBlob->Release(); psBlob->Release(); return false; }
        vsBlob->Release(); psBlob->Release();

        if (type_ == Type::Box) CreateBox(1.0f);
        else if (type_ == Type::Sphere) CreateSphere(0.5f, 16, 16);
        else {
            if (!LoadFbxAsciiFile()) {
                Shutdown();
                return false;
            }
        }

        CD3D11_RASTERIZER_DESC rsDesc(D3D11_DEFAULT);
        rsDesc.CullMode = D3D11_CULL_NONE;
        if (FAILED(device->CreateRasterizerState(&rsDesc, &rasterizerState_))) return false;

        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.ByteWidth = sizeof(CBPerObject);
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &constantBuffer_))) return false;

        return true;
    }

    void MeshComponent::SetOrbitParams(MeshComponent* parent, float radius, float orbitSpeed, const XMFLOAT4& color)
    {
        parent_ = parent;
        orbitRadius_ = radius;
        orbitSpeed_ = orbitSpeed;
        color_ = color;
    }

    XMMATRIX MeshComponent::GetWorldMatrix() const
    {
        if (stickParent_) {
            const XMMATRIX rel = XMLoadFloat4x4(&stickMeshToBall_);
            const XMMATRIX wb = stickParent_->GetWorldMatrix();
            return XMMatrixMultiply(rel, wb);
        }
        XMMATRIX rot;
        if (type_ == Type::Sphere) {
            XMVECTOR q = XMLoadFloat4(&sphereRollQuat_);
            rot = XMMatrixRotationQuaternion(q);
        }
        else {
            rot = XMMatrixRotationX(selfAngles_.x) * XMMatrixRotationY(selfAngles_.y) * XMMatrixRotationZ(selfAngles_.z);
        }
        return XMMatrixScaling(scale_, scale_, scale_) * rot * importBasis_
            * XMMatrixTranslation(position_.x, position_.y, position_.z);
    }

    void MeshComponent::ApplySphereRollXZ(float dist, float dirX, float dirZ)
    {
        if (type_ != Type::Sphere || dist < 1e-8f) return;
        const float R = 0.5f * scale_;
        if (R < 1e-8f) return;
        const float angle = (dist / R) * kSphereRollAngleScale;
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR moveDir = XMVectorSet(dirX, 0.0f, dirZ, 0.0f);
        XMVECTOR axis = XMVector3Cross(up, moveDir);
        const float axisLenSq = XMVectorGetX(XMVector3Dot(axis, axis));
        if (axisLenSq < 1e-16f) return;
        axis = XMVector3Normalize(axis);
        const XMVECTOR dq = XMQuaternionRotationAxis(axis, angle);
        XMVECTOR q = XMLoadFloat4(&sphereRollQuat_);
        q = XMQuaternionMultiply(q, dq);
        q = XMQuaternionNormalize(q);
        XMStoreFloat4(&sphereRollQuat_, q);
    }

    float MeshComponent::GetCollisionRadius() const
    {
        if (type_ == Type::Sphere)
            return 0.5f * scale_;
        return pickupBoundsRadius_ * scale_;
    }

    bool MeshComponent::IsKatamariPickable() const
    {
        return (type_ == Type::FbxFile || type_ == Type::Box) && stickParent_ == nullptr;
    }

    void MeshComponent::AttachToKatamariBall(MeshComponent* ball)
    {
        if (!ball || stickParent_ != nullptr) return;
        const XMFLOAT3 bp = ball->GetWorldPosition();
        const float Rb = ball->GetCollisionRadius();
        const float Ro = GetCollisionRadius();
        XMVECTOR cb = XMVectorSet(bp.x, bp.y, bp.z, 1.0f);
        XMVECTOR co = XMVectorSet(position_.x, position_.y, position_.z, 1.0f);
        XMVECTOR delta = XMVectorSubtract(co, cb);
        const float dist = XMVectorGetX(XMVector3Length(delta));
        XMVECTOR u = (dist > 1e-5f) ? XMVector3Normalize(delta) : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const float touch = (Rb + Ro) * 0.88f;
        XMVECTOR snapped = XMVectorAdd(cb, XMVectorScale(u, touch));
        position_.x = XMVectorGetX(snapped);
        position_.y = XMVectorGetY(snapped);
        position_.z = XMVectorGetZ(snapped);

        const XMMATRIX wb = ball->GetWorldMatrix();
        const XMMATRIX wc = GetWorldMatrix();
        const XMMATRIX invWb = XMMatrixInverse(nullptr, wb);
        const XMMATRIX rel = XMMatrixMultiply(wc, invWb);
        XMStoreFloat4x4(&stickMeshToBall_, rel);

        stickParent_ = ball;
        parent_ = nullptr;
        orbitRadius_ = 0.0f;
        orbitSpeed_ = 0.0f;
    }

    void MeshComponent::Update(float deltaTime)
    {
        if (stickParent_) {
            const XMMATRIX wChild = GetWorldMatrix();
            const XMVECTOR o = XMVector4Transform(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), wChild);
            position_.x = XMVectorGetX(o);
            position_.y = XMVectorGetY(o);
            position_.z = XMVectorGetZ(o);
            return;
        }

        orbitAngle_ += orbitSpeed_ * deltaTime;

        if (parent_ || fabsf(orbitRadius_) > 1e-6f) {
            XMFLOAT3 center = { 0,0,0 };
            if (parent_) center = parent_->GetWorldPosition();
            position_.x = center.x + orbitRadius_ * cosf(orbitAngle_);
            position_.y = center.y;
            position_.z = center.z + orbitRadius_ * sinf(orbitAngle_);
        }
    }

    XMFLOAT3 MeshComponent::GetWorldPosition() const { return position_; }

    void MeshComponent::Render(ID3D11DeviceContext* context, const XMMATRIX& view, const XMMATRIX& proj)
    {
        context->RSSetState(rasterizerState_.Get());
        context->IASetInputLayout(inputLayout_.Get());
        context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetIndexBuffer(indexBuffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride_, &offset_);
        context->VSSetShader(vertexShader_.Get(), nullptr, 0);
        context->PSSetShader(pixelShader_.Get(), nullptr, 0);

        XMMATRIX world = GetWorldMatrix();
        CBPerObject cb;
        cb.world = XMMatrixTranspose(world);
        cb.view = XMMatrixTranspose(view);
        cb.proj = XMMatrixTranspose(proj);
        cb.color = color_;
        cb.lightDirAmbient = DefaultLightDirAmbient();

        context->UpdateSubresource(constantBuffer_.Get(), 0, nullptr, &cb, 0, 0);
        context->VSSetConstantBuffers(0, 1, constantBuffer_.GetAddressOf());
        context->PSSetConstantBuffers(0, 1, constantBuffer_.GetAddressOf());

        context->DrawIndexed(indexCount_, 0, 0);
    }

    void MeshComponent::Shutdown()
    {
        vertexBuffer_.Reset(); indexBuffer_.Reset(); vertexShader_.Reset(); pixelShader_.Reset(); inputLayout_.Reset(); rasterizerState_.Reset(); constantBuffer_.Reset();
    }

}
