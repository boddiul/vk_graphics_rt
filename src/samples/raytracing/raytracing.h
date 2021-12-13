#ifndef VK_GRAPHICS_RT_RAYTRACING_H
#define VK_GRAPHICS_RT_RAYTRACING_H

#include <cstdint>
#include <memory>
#include <iostream>
#include "LiteMath.h"
#include "render/CrossRT.h"
#include "../../../resources/shaders/common.h"
#include <vector>
#include <geom/vk_mesh.h>

class RayTracer
{
public:
  RayTracer(uint32_t a_width, uint32_t a_height) : m_width(a_width), m_height(a_height) {}

  void UpdateView(const LiteMath::float3& a_camPos, const LiteMath::float4x4& a_invProjView ) { m_camPos = to_float4(a_camPos, 1.0f); m_invProjView = a_invProjView; }
  void SetScene(std::shared_ptr<ISceneObject> a_pAccelStruct, std::vector<UniformParams*> a_lights, 
      std::vector<MeshInfo> a_meshInfos,std::shared_ptr<IMeshData> a_pMeshData, 
      std::vector<LiteMath::float4x4> a_instanceMatrices) {
      m_pAccelStruct = a_pAccelStruct;
  m_lights = a_lights;
  m_meshInfos = a_meshInfos;
  m_pMeshData = a_pMeshData;
  m_instanceMatrices = a_instanceMatrices;
  };

  void CastSingleRay(uint32_t tidX, uint32_t tidY, uint32_t* out_color);
  void kernel_InitEyeRay(uint32_t tidX, uint32_t tidY, LiteMath::float4* rayPosAndNear, LiteMath::float4* rayDirAndFar);
  void kernel_RayTrace(uint32_t tidX, uint32_t tidY, const LiteMath::float4* rayPosAndNear, const LiteMath::float4* rayDirAndFar, uint32_t* out_color);
  
  float3 getNormal(CRT_Hit hit);
  float3 kernel_SendRay(const LiteMath::float4* rayPosAndNear, const LiteMath::float4* rayDirAndFar, uint depth);



protected:
  uint32_t m_width;
  uint32_t m_height;

  std::vector<UniformParams*> m_lights;
  std::vector<float*> m_lightIntensity;

  std::vector<MeshInfo> m_meshInfos;
  std::shared_ptr<IMeshData> m_pMeshData;
  std::vector<LiteMath::float4x4> m_instanceMatrices;


  LiteMath::float4   m_camPos;
  LiteMath::float4x4 m_invProjView;

  std::shared_ptr<ISceneObject> m_pAccelStruct;

  static constexpr uint32_t palette_size = 7;
  // color palette to select color for objects based on mesh/instance id
  static constexpr uint32_t m_palette[palette_size] = {
    0xff0082c8, 0xffe1a0f1, 0xffefcdab, 0xff000000, 
    0xfff58231, 0xff911eb4, 0xff46f0f0, /*0xfff032e6,
    0xffd2f53c, 0xfffabebe, 0xff008080, 0xffe6beff,
    0xffaa6e28, 0xfffffac8, 0xff800000, 0xffaaffc3,
    0xff808000, 0xffffd8b1, 0xff000080, 0xff808080*/
  };


  static constexpr uint32_t materials_size = 7;
  static constexpr float m_materials_reflection[materials_size] = {
    0.1,
    0.5,
    0.0,
    1.0, 
    0.0,
    0.0,
    0.0
  };
  static constexpr float m_materials_refraction[materials_size] = {
    0.0,
    0.0,
    0.95,
    0.0,
    0.0,
    0.0,
    0.0
  };
  static constexpr bool m_materials_lambSurf[materials_size] = {
      false,
      false,
      true,
      false,
      true,
      false,
      false
  };
};

#endif// VK_GRAPHICS_RT_RAYTRACING_H
