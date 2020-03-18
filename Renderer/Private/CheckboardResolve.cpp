#include "CheckboardResolve.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "Engine/Texture.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "SceneInterface.h"
#include "RHICommandList.h"
#include "Engine/Engine.h"
#include "Shader.h"
#include "SceneRendering.h"
#include "SceneRenderTargets.h"

#define CBR_GROUP_THREAD_COUNTS 4

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FCBRColorBuffer, )
SHADER_PARAMETER(float, FrameOffset)
SHADER_PARAMETER(float, DepthTolerance)
SHADER_PARAMETER(uint32, Flags)
SHADER_PARAMETER(float, _Pad)
SHADER_PARAMETER(FVector4, LinearZTransform)
SHADER_PARAMETER(FMatrix, CurViewProj)
SHADER_PARAMETER(FMatrix, PreInvViewProj)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FCBRColorBuffer, "CBRColorBuffer");

class FCheckboardColorResolveCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCheckboardColorResolveCS, Global)
public:
	FCheckboardColorResolveCS() {}

	FCheckboardColorResolveCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		DownSizedInColor2x0.Bind(Initializer.ParameterMap, TEXT("DownSizedInColor2x0"));
		DownSizedInColor2x1.Bind(Initializer.ParameterMap, TEXT("DownSizedInColor2x1"));
		DownSizedInDepth2x0.Bind(Initializer.ParameterMap, TEXT("DownSizedInDepth2x0"));
		DownSizedInDepth2x1.Bind(Initializer.ParameterMap, TEXT("DownSizedInDepth2x1"));
		TextureSampler.Bind(Initializer.ParameterMap, TEXT("TextureSampler"));
		OutColor.Bind(Initializer.ParameterMap, TEXT("OutColor"));
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Paramers)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("DEBUG_RENDER"), 1);
		OutEnvironment.SetDefine(TEXT("CBR_GROUP_THREAD_COUNTS"), CBR_GROUP_THREAD_COUNTS);
	}

	void SetParameters(FRHICommandListImmediate& RHICmdList, const FCBRColorBufferData& CbrData,
		FTextureRHIRef InDownSizedInColor2x0,
		FTextureRHIRef InDownSizedInColor2x1,
		FTextureRHIRef InDownSizedInDepth2x0,
		FTextureRHIRef InDownSizedInDepth2x1,
		FUnorderedAccessViewRHIRef OutputUAV)
	{
		if (OutColor.IsBound())
			RHICmdList.SetUAVParameter(GetComputeShader(), OutColor.GetBaseIndex(), OutputUAV);

		SetTextureParameter(RHICmdList, GetComputeShader(), DownSizedInColor2x0, DownSizedInColor2x0, TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), InDownSizedInColor2x0);
		SetTextureParameter(RHICmdList, GetComputeShader(), DownSizedInColor2x1, DownSizedInColor2x1, TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), InDownSizedInColor2x1);
		SetTextureParameter(RHICmdList, GetComputeShader(), DownSizedInDepth2x0, TextureSampler, TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), InDownSizedInDepth2x0);
		SetTextureParameter(RHICmdList, GetComputeShader(), DownSizedInDepth2x1, TextureSampler, TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), InDownSizedInDepth2x1);

		FCBRColorBuffer RenderData;
		RenderData.FrameOffset = CbrData.FrameOffset;
		RenderData.DepthTolerance = CbrData.DepthTolerance;
		RenderData.Flags = CbrData.Flags;
		RenderData.LinearZTransform = CbrData.LinearZTransform;
		RenderData.CurViewProj = CbrData.CurViewProj;
		RenderData.PreInvViewProj = CbrData.PreInvViewProj;
		SetUniformBufferParameter(RHICmdList, GetComputeShader(), GetUniformBufferParameter<FCBRColorBuffer>(), FCBRColorBuffer::CreateUniformBuffer(RenderData, EUniformBufferUsage::UniformBuffer_SingleDraw));
	}

	void UnbindUAV(FRHICommandList& RHICmdList)
	{
		if (OutColor.IsBound())
			RHICmdList.SetUAVParameter(GetComputeShader(), OutColor.GetBaseIndex(), nullptr);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DownSizedInColor2x0;
		Ar << DownSizedInColor2x1;
		Ar << DownSizedInDepth2x0;
		Ar << DownSizedInDepth2x1;
		Ar << TextureSampler;
		Ar << OutColor;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter DownSizedInColor2x0;
	FShaderResourceParameter DownSizedInColor2x1;
	FShaderResourceParameter DownSizedInDepth2x0;
	FShaderResourceParameter DownSizedInDepth2x1;
	FShaderResourceParameter TextureSampler;

	FShaderResourceParameter OutColor;
};

IMPLEMENT_SHADER_TYPE(, FCheckboardColorResolveCS, TEXT("/Engine/Private/CheckboardResolveCS.usf"), TEXT("MainColorCS"), SF_Compute)

void RenderCBR_ColoResolve(
	FRHICommandListImmediate& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	FViewInfo& ViewInfo,
	float FrameOffset,
	float DepthTolerance,
	uint32 Flags,
	FRHITexture* InDownSizedInColor2x0,
	FRHITexture* InDownSizedInColor2x1,
	FRHITexture* InDownSizedInDepth2x0,
	FRHITexture* InDownSizedInDepth2x1,
	const TRefCountPtr<IPooledRenderTarget>& OutputTarget)

{
	check(IsInRenderingThread());

	FCBRColorBufferData CBRBuffer;
	CBRBuffer.FrameOffset = FrameOffset;
	CBRBuffer.DepthTolerance = DepthTolerance;
	CBRBuffer.Flags = Flags;

	const FMatrix& InvViewProj = ViewInfo.ViewMatrices.GetInvViewProjectionMatrix();

	CBRBuffer.LinearZTransform.X = InvViewProj.M[2][2];
	CBRBuffer.LinearZTransform.Y = InvViewProj.M[3][2];
	CBRBuffer.LinearZTransform.Z = InvViewProj.M[2][3];
	CBRBuffer.LinearZTransform.W = InvViewProj.M[3][3];

	CBRBuffer.CurViewProj = ViewInfo.ViewMatrices.GetViewProjectionMatrix();
	CBRBuffer.PreInvViewProj = ViewInfo.PrevViewInfo.ViewMatrices.GetInvViewProjectionMatrix();

	TShaderMapRef<FCheckboardColorResolveCS> CheckboardColorResolveCS(GetGlobalShaderMap(FeatureLevel));

	RHICmdList.SetComputeShader(CheckboardColorResolveCS->GetComputeShader());

	CheckboardColorResolveCS->SetParameters(RHICmdList, CBRBuffer, InDownSizedInColor2x0, InDownSizedInColor2x1, InDownSizedInDepth2x0, InDownSizedInDepth2x1, OutputTarget->GetRenderTargetItem().UAV);

	//#TODO：长度应该统一对其到4
	DispatchComputeShader(
		RHICmdList,
		*CheckboardColorResolveCS,
		OutputTarget->GetRenderTargetItem().TargetableTexture->GetSizeXYZ().X / CBR_GROUP_THREAD_COUNTS,
		OutputTarget->GetRenderTargetItem().TargetableTexture->GetSizeXYZ().Y / CBR_GROUP_THREAD_COUNTS,
		1ul);

	CheckboardColorResolveCS->UnbindUAV(RHICmdList);
}

//////////////////////////////////////////////////////////////

class FCheckboardDepthResolveCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCheckboardDepthResolveCS, Global)
public:
	FCheckboardDepthResolveCS() {}

	FCheckboardDepthResolveCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		zMagic.Bind(Initializer.ParameterMap, TEXT("zMagic"));
		FrameOffset.Bind(Initializer.ParameterMap, TEXT("FrameOffset"));
		DownSizedInDepth2x0.Bind(Initializer.ParameterMap, TEXT("DownSizedInDepth2x0"));
		DownSizedInDepth2x1.Bind(Initializer.ParameterMap, TEXT("DownSizedInDepth2x1"));
		TextureSampler.Bind(Initializer.ParameterMap, TEXT("TextureSampler"));
		OutDepth.Bind(Initializer.ParameterMap, TEXT("OutDepth"));
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Paramers)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("DEBUG_RENDER"), 0);
		OutEnvironment.SetDefine(TEXT("CBR_GROUP_THREAD_COUNTS"), CBR_GROUP_THREAD_COUNTS);
	}

	void SetParameters(FRHICommandListImmediate& RHICmdList,
		float InzMagic,
		float InFrameOffset,
		FTextureRHIRef InDownSizedInDepth2x0,
		FTextureRHIRef InDownSizedInDepth2x1,
		FUnorderedAccessViewRHIRef OutputUAV)
	{
		if (OutDepth.IsBound())
			RHICmdList.SetUAVParameter(GetComputeShader(), OutDepth.GetBaseIndex(), OutputUAV);

		SetShaderValue(RHICmdList, GetComputeShader(), zMagic, InzMagic);
		SetShaderValue(RHICmdList, GetComputeShader(), FrameOffset, InFrameOffset);
		SetTextureParameter(RHICmdList, GetComputeShader(), DownSizedInDepth2x0, TextureSampler, TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), InDownSizedInDepth2x0);
		SetTextureParameter(RHICmdList, GetComputeShader(), DownSizedInDepth2x1, TextureSampler, TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), InDownSizedInDepth2x1);
	}

	void UnbindUAV(FRHICommandList& RHICmdList)
	{
		if (OutDepth.IsBound())
			RHICmdList.SetUAVParameter(GetComputeShader(), OutDepth.GetBaseIndex(), nullptr);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << zMagic;
		Ar << FrameOffset;
		Ar << DownSizedInDepth2x0;
		Ar << DownSizedInDepth2x1;
		Ar << TextureSampler;
		Ar << OutDepth;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter zMagic;
	FShaderParameter FrameOffset;
	FShaderResourceParameter DownSizedInDepth2x0;
	FShaderResourceParameter DownSizedInDepth2x1;
	FShaderResourceParameter TextureSampler;

	FShaderResourceParameter OutDepth;
};


IMPLEMENT_SHADER_TYPE(, FCheckboardDepthResolveCS, TEXT("/Engine/Private/CheckboardResolveCS.usf"), TEXT("MainDepthCS"), SF_Compute)

void RenderCBR_DepthResolve(
	FRHICommandListImmediate& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	float zMagic,
	float FrameOffset,
	FRHITexture* InDownSizedInDepth2x0,
	FRHITexture* InDownSizedInDepth2x1,
	const TRefCountPtr<IPooledRenderTarget>& OutputTarget)

{
	check(IsInRenderingThread());

	TShaderMapRef<FCheckboardDepthResolveCS> CheckboardDepthResolveCS(GetGlobalShaderMap(FeatureLevel));

	RHICmdList.SetComputeShader(CheckboardDepthResolveCS->GetComputeShader());

	CheckboardDepthResolveCS->SetParameters(RHICmdList, zMagic, FrameOffset, InDownSizedInDepth2x0, InDownSizedInDepth2x1, OutputTarget->GetRenderTargetItem().UAV);

	DispatchComputeShader(RHICmdList, 
		*CheckboardDepthResolveCS, 
		OutputTarget->GetRenderTargetItem().TargetableTexture->GetSizeXYZ().X / CBR_GROUP_THREAD_COUNTS,
		OutputTarget->GetRenderTargetItem().TargetableTexture->GetSizeXYZ().Y / CBR_GROUP_THREAD_COUNTS,
		1ul);

	CheckboardDepthResolveCS->UnbindUAV(RHICmdList);
}
