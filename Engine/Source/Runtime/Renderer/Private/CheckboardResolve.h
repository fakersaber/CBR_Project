#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RHIResources.h"

struct FCBRColorBufferData{
	uint32 FrameOffset;
	
	float DepthTolerance;

	uint32 MotionVector;

	float width;

	float height;

	int pad[3];

	FVector4 LinearZTransform;

	//FMatrix CurViewProj;

	//FMatrix PreInvViewProj;
};



void RenderCBR_ColorResolve(
	FRHICommandListImmediate& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	FRHITexture* InDownSizedInColor2x0,
	FRHITexture* InDownSizedInColor2x1,
	FRHITexture* InDownSizedInDepth2x0,
	FRHITexture* InDownSizedInDepth2x1,
	const TRefCountPtr<IPooledRenderTarget>& OutputTarget,
	const FCBRColorBufferData& ConstBuffer
);

void RenderCBR_DepthResolve(
	FRHICommandListImmediate& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	float zMagic,
	float FrameOffset,
	FRHITexture* InDownSizedInDepth2x0,
	FRHITexture* InDownSizedInDepth2x1,
	const TRefCountPtr<IPooledRenderTarget>& OutputTarget);
