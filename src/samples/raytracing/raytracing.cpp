#include "raytracing.h"
#include "float.h"

LiteMath::float3 EyeRayDir(float x, float y, float w, float h, LiteMath::float4x4 a_mViewProjInv)
{
  LiteMath::float4 pos = LiteMath::make_float4( 2.0f * (x + 0.5f) / w - 1.0f,
    2.0f * (y + 0.5f) / h - 1.0f,
    0.0f,
    1.0f );

  pos = a_mViewProjInv * pos;
  pos /= pos.w;

  //  pos.y *= (-1.0f);

  return normalize(to_float3(pos));
}

void RayTracer::CastSingleRay(uint32_t tidX, uint32_t tidY, uint32_t* out_color)
{
  LiteMath::float4 rayPosAndNear, rayDirAndFar;
  kernel_InitEyeRay(tidX, tidY, &rayPosAndNear, &rayDirAndFar);

  kernel_RayTrace(tidX, tidY, &rayPosAndNear, &rayDirAndFar, out_color);
}

void RayTracer::kernel_InitEyeRay(uint32_t tidX, uint32_t tidY, LiteMath::float4* rayPosAndNear, LiteMath::float4* rayDirAndFar)
{
  *rayPosAndNear = m_camPos; // to_float4(m_camPos, 1.0f);
  
  const LiteMath::float3 rayDir  = EyeRayDir(float(tidX), float(tidY), float(m_width), float(m_height), m_invProjView);
  *rayDirAndFar  = to_float4(rayDir, FLT_MAX);
}

float3 DecodeNormal(float a_data)
{
    int ti = *reinterpret_cast<uint*>(&a_data);

    const uint a_enc_x = (ti & 0x0000FFFFu);
    const uint a_enc_y = ((ti & 0xFFFF0000u) >> 16);
    const float sign = (a_enc_x & 0x0001u) != 0 ? -1.0f : 1.0f;

    const int usX = int(a_enc_x & 0x0000FFFEu);
    const int usY = int(a_enc_y & 0x0000FFFFu);

    const int sX = (usX <= 32767) ? usX : usX - 65536;
    const int sY = (usY <= 32767) ? usY : usY - 65536;

    const float x = sX * (1.0f / 32767.0f);
    const float y = sY * (1.0f / 32767.0f);
    const float z = sign * sqrt(std::max(1.0f - x * x - y * y, 0.0f));

    return float3(x, y, z);
}

float3 Refraction(const float3 vecI, const float3 vecN, float etaT, float etaI)
{
    float cosi = -std::max(-1.f, std::min(1.f, dot(vecI , vecN)));
    if (cosi < 0)
        return Refraction(vecI, (float3()-vecN), etaI, etaT);
    float eta = etaI / etaT;
    float k = 1 - eta * eta * (1 - cosi * cosi);
    return k < 0 ? float3( 1,0,0 ) : vecI * eta + vecN * (eta * cosi - sqrt(k));

}

float3 RayTracer::getNormal(CRT_Hit hit) {


    uint indexOffset = m_meshInfos[hit.geomId].m_indexOffset;
    uint vertexOffset = m_meshInfos[hit.geomId].m_vertexOffset;

    uint i0 = m_pMeshData->IndexData()[indexOffset + 3 * hit.primId + 0]; 
    uint i1 = m_pMeshData->IndexData()[indexOffset + 3 * hit.primId + 1];
    uint i2 = m_pMeshData->IndexData()[indexOffset + 3 * hit.primId + 2];

    float* v0 = &m_pMeshData->VertexData()[(vertexOffset + i0)*8];
    float* v1 = &m_pMeshData->VertexData()[(vertexOffset + i1)*8];
    float* v2 = &m_pMeshData->VertexData()[(vertexOffset + i2)*8];

        
    float3 vv0 = float3(v0[0], v0[1], v0[2]);
    float3 vv1 = float3(v1[0], v1[1], v1[2]);   
    float3 vv2 = float3(v2[0], v2[1], v2[2]);


    float3 n0 = DecodeNormal(v0[3]);
    float3 n1 = DecodeNormal(v1[3]);
    float3 n2 = DecodeNormal(v2[3]);

    float3 norm = hit.coords[2] * n0 + hit.coords[1] * n1 + hit.coords[0] * n2;
    //norm = (vv1 - vv0) * (vv2 - vv0);

    float3 norm2 = normalize(to_float3(transpose(inverse4x4(m_instanceMatrices[hit.instId])) * to_float4(norm,0.0f)));
   
    return normalize(norm2);
    
}



float3 RayTracer::kernel_SendRay(const LiteMath::float4* rayPosAndNear, const LiteMath::float4* rayDirAndFar,uint depth)
{
    float3 col = float3(0.09, 0.02, 0.4);

    if (depth > 4)
    {
        
        return col;
    }

    const LiteMath::float4 rayPos = *rayPosAndNear;
    const LiteMath::float4 rayDir = *rayDirAndFar;

    CRT_Hit hit = m_pAccelStruct->RayQuery_NearestHit(rayPos, rayDir);





    if (hit.instId != uint32_t(-1))
    {
        int pal_col = m_palette[hit.instId % palette_size];
        col = float3((pal_col & 0xff) / 255.0f, ((pal_col & 0xff00) >> 8) / 255.0f, ((pal_col & 0xff0000) >> 16) / 255.0f);
      


        float refl = m_materials_reflection[hit.instId % materials_size];

        float refr = m_materials_refraction[hit.instId % materials_size];

        bool lambSurf = m_materials_lambSurf[hit.instId % materials_size];


        LiteMath::float3 rayNewPos = to_float3(rayPos) + normalize(to_float3(rayDir)) * hit.t;


        LiteMath::float3 faceNorm = getNormal(hit);



        bool getsLight = false;

        float diffuseLight = 0;

        for (int i = 0; i < m_lights.size(); i++)
        {
            LiteMath::float3 lightPos = to_float3(m_lights[i]->lightPos);

            LiteMath::float3 dirToLight = normalize(lightPos - rayNewPos);



            
            bool getsToLightSource = true;

            LiteMath::float3 lightNewPos = rayNewPos;
            CRT_Hit hitTowardsLight = m_pAccelStruct->RayQuery_NearestHit(to_float4(lightNewPos, 0.001f), to_float4(dirToLight, FLT_MAX));
            
            while (hitTowardsLight.instId != uint32_t(-1))
            {
                

                if (m_materials_refraction[hitTowardsLight.instId % materials_size] == 0.0f)
                {
                    getsToLightSource = false;
                    break;
                }
                lightNewPos = lightNewPos + normalize(dirToLight) * hitTowardsLight.t;

                hitTowardsLight = m_pAccelStruct->RayQuery_NearestHit(to_float4(lightNewPos, 0.001f), to_float4(dirToLight, FLT_MAX));

            }

            
            if (getsToLightSource)
            {
                float lDist = length(lightPos - rayNewPos);
                float intensity = m_lights[i]->lightPos.w / (lDist * lDist);
                float kk = dot(faceNorm, dirToLight);

                diffuseLight +=  (lambSurf ? 1 : intensity * std::max(0.0f, kk));
                getsLight = true;
            }






        }
        
        if (getsLight)
        {
            col = col * std::min(diffuseLight, 1.0f);


            if (refl > 0)
            {

                float3 reflectDir = normalize(reflect(to_float3(rayDir), faceNorm));
                float3 reflectOrig = rayNewPos;
                
                float4 newRayPosAndNear = to_float4(reflectOrig,0.001f);
                float4 newRayDirAndFar = to_float4(reflectDir,FLT_MAX);

                
                float3 reflectColor = kernel_SendRay(&newRayPosAndNear, &newRayDirAndFar, depth + 1);


                col+=reflectColor*refl;
                
            }
            if (refr > 0)
            {
                


                float3 refractDir = normalize(Refraction(to_float3(rayDir), faceNorm,1.5,1.0));
                float3 refractOrig = rayNewPos;

                float4 newRayPosAndNear = to_float4(refractOrig, 0.001f);
                float4 newRayDirAndFar = to_float4(refractDir, FLT_MAX);




                float3 refractColor = kernel_SendRay(&newRayPosAndNear, &newRayDirAndFar, depth + 1);

                col = refractColor +(1-refr)*col;

            }
        }
        else
        {
            col = float3(0, 0, 0);
        }




        float mm = std::max(col.x, std::max(col.y, col.z));
        if (mm > 1)
            col = col * (1.0 / mm);
    }


    return col;
}
void RayTracer::kernel_RayTrace(uint32_t tidX, uint32_t tidY, const LiteMath::float4* rayPosAndNear, const LiteMath::float4* rayDirAndFar, uint32_t* out_color)
{


  float3 col = kernel_SendRay(rayPosAndNear, rayDirAndFar, 0);
  out_color[tidY * m_width + tidX] = 0xff000000 + (((int)(col.z * 255)) << 16) + (((int)(col.y * 255)) << 8) + (((int)(col.x * 255)));
 
}