#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RHIResources.h"

struct FCBRColorBufferData{
	float FrameOffset;
	
	float DepthTolerance;

	uint32 Flags;

	FVector4 LinearZTransform;

	FMatrix CurViewProj;

	FMatrix PreInvViewProj;
};



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
	const TRefCountPtr<IPooledRenderTarget>& OutputTarget);

void RenderCBR_DepthResolve(
	FRHICommandListImmediate& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	float zMagic,
	float FrameOffset,
	FRHITexture* InDownSizedInDepth2x0,
	FRHITexture* InDownSizedInDepth2x1,
	const TRefCountPtr<IPooledRenderTarget>& OutputTarget);
