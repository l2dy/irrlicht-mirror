// Copyright (C) 2002-2006 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#define _IRR_DONT_DO_MEMORY_DEBUGGING_HERE
#include "CD3D9Driver.h"
#include "os.h"
#include "S3DVertex.h"
#include "CD3D9Texture.h"
#include "CImage.h"
#include <stdio.h>

#include "IrrCompileConfig.h"
#ifdef _IRR_WINDOWS_

#ifdef _IRR_COMPILE_WITH_DIRECT3D_9_

#include "CD3D9MaterialRenderer.h"
#include "CD3D9ShaderMaterialRenderer.h"
#include "CD3D9NormalMapRenderer.h"
#include "CD3D9ParallaxMapRenderer.h"
#include "CD3D9HLSLMaterialRenderer.h"

namespace irr
{
namespace video
{


//! constructor
CD3D9Driver::CD3D9Driver(const core::dimension2d<s32>& screenSize, HWND window,
				bool fullscreen, bool stencilbuffer,
				io::IFileSystem* io, bool pureSoftware)
: CNullDriver(io, screenSize), D3DLibrary(0), CurrentRenderMode(ERM_NONE), pID3DDevice(0),
 LastVertexType((video::E_VERTEX_TYPE)-1), ResetRenderStates(true), pID3D(0),
 LastSetLight(-1), Transformation3DChanged(0), StencilBuffer(stencilbuffer),
 DeviceLost(false), Fullscreen(fullscreen), PrevRenderTarget(0), CurrentRendertargetSize(0,0)
{
	#ifdef _DEBUG
	setDebugName("CD3D9Driver");
	#endif

	printVersion();

	for (int i=0; i<4; ++i)
	{
		CurrentTexture[i] = 0;
		LastTextureMipMapsAvailable[i] = false;
	}

	// create sphere map matrix

	SphereMapMatrixD3D9._11 = 0.5f; SphereMapMatrixD3D9._12 = 0.0f;
	SphereMapMatrixD3D9._13 = 0.0f; SphereMapMatrixD3D9._14 = 0.0f;
	SphereMapMatrixD3D9._21 = 0.0f; SphereMapMatrixD3D9._22 =-0.5f;
	SphereMapMatrixD3D9._23 = 0.0f; SphereMapMatrixD3D9._24 = 0.0f;
	SphereMapMatrixD3D9._31 = 0.0f; SphereMapMatrixD3D9._32 = 0.0f;
	SphereMapMatrixD3D9._33 = 1.0f; SphereMapMatrixD3D9._34 = 0.0f;
	SphereMapMatrixD3D9._41 = 0.5f; SphereMapMatrixD3D9._42 = 0.5f;
	SphereMapMatrixD3D9._43 = 0.0f; SphereMapMatrixD3D9._44 = 1.0f;

	core::matrix4 mat;
	UnitMatrixD3D9 = *(D3DMATRIX*)((void*)&mat);

	// init direct 3d is done in the factory function

	MaxLightDistance = sqrtf(FLT_MAX);
}



//! destructor
CD3D9Driver::~CD3D9Driver()
{
	deleteMaterialRenders();

	for (int i=0; i<4; ++i)
		if (CurrentTexture[i])
			CurrentTexture[i]->drop();

	// drop d3d9

	if (pID3DDevice)
		pID3DDevice->Release();

	if (pID3D)
		pID3D->Release();
}



void CD3D9Driver::createMaterialRenderers()
{
	// create D3D9 material renderers

	addAndDropMaterialRenderer(new CD3D9MaterialRenderer_SOLID(pID3DDevice, this));
	addAndDropMaterialRenderer(new CD3D9MaterialRenderer_SOLID_2_LAYER(pID3DDevice, this));

	// add the same renderer for all lightmap types

	CD3D9MaterialRenderer_LIGHTMAP* lmr = new CD3D9MaterialRenderer_LIGHTMAP(pID3DDevice, this);
	addMaterialRenderer(lmr); // for EMT_LIGHTMAP:
	addMaterialRenderer(lmr); // for EMT_LIGHTMAP_ADD:
	addMaterialRenderer(lmr); // for EMT_LIGHTMAP_M2:
	addMaterialRenderer(lmr); // for EMT_LIGHTMAP_M4:
	addMaterialRenderer(lmr); // for EMT_LIGHTMAP_LIGHTING:
	addMaterialRenderer(lmr); // for EMT_LIGHTMAP_LIGHTING_M2:
	addMaterialRenderer(lmr); // for EMT_LIGHTMAP_LIGHTING_M4:
	lmr->drop();

	// add remaining fixed function pipeline material renderers

	addAndDropMaterialRenderer(new CD3D9MaterialRenderer_DETAIL_MAP(pID3DDevice, this));
	addAndDropMaterialRenderer(new CD3D9MaterialRenderer_SPHERE_MAP(pID3DDevice, this));
	addAndDropMaterialRenderer(new CD3D9MaterialRenderer_REFLECTION_2_LAYER(pID3DDevice, this));
	addAndDropMaterialRenderer(new CD3D9MaterialRenderer_TRANSPARENT_ADD_COLOR(pID3DDevice, this));
	addAndDropMaterialRenderer(new CD3D9MaterialRenderer_TRANSPARENT_ALPHA_CHANNEL(pID3DDevice, this));
	addAndDropMaterialRenderer(new CD3D9MaterialRenderer_TRANSPARENT_ALPHA_CHANNEL_REF(pID3DDevice, this));
	addAndDropMaterialRenderer(new CD3D9MaterialRenderer_TRANSPARENT_VERTEX_ALPHA(pID3DDevice, this));
	addAndDropMaterialRenderer(new CD3D9MaterialRenderer_TRANSPARENT_REFLECTION_2_LAYER(pID3DDevice, this));

	// add normal map renderers

	s32 tmp = 0;
	video::IMaterialRenderer* renderer = 0;

	renderer = new CD3D9NormalMapRenderer(pID3DDevice, this, tmp,
		MaterialRenderers[EMT_SOLID].Renderer);
	renderer->drop();

	renderer = new CD3D9NormalMapRenderer(pID3DDevice, this, tmp,
		MaterialRenderers[EMT_TRANSPARENT_ADD_COLOR].Renderer);
	renderer->drop();

	renderer = new CD3D9NormalMapRenderer(pID3DDevice, this, tmp,
		MaterialRenderers[EMT_TRANSPARENT_VERTEX_ALPHA].Renderer);
	renderer->drop();

	// add parallax map renderers

	renderer = new CD3D9ParallaxMapRenderer(pID3DDevice, this, tmp,
		MaterialRenderers[EMT_SOLID].Renderer);
	renderer->drop();

	renderer = new CD3D9ParallaxMapRenderer(pID3DDevice, this, tmp,
		MaterialRenderers[EMT_TRANSPARENT_ADD_COLOR].Renderer);
	renderer->drop();

	renderer = new CD3D9ParallaxMapRenderer(pID3DDevice, this, tmp,
		MaterialRenderers[EMT_TRANSPARENT_VERTEX_ALPHA].Renderer);
	renderer->drop();
}



//! initialises the Direct3D API
bool CD3D9Driver::initDriver(const core::dimension2d<s32>& screenSize, HWND hwnd,
				u32 bits, bool fullScreen, bool pureSoftware,
				bool highPrecisionFPU, bool vsync, bool antiAlias)
{
	HRESULT hr;
	Fullscreen = fullScreen;

	if (!pID3D)
	{
		D3DLibrary = LoadLibrary( "d3d9.dll" );

		if (!D3DLibrary)
		{
			os::Printer::log("Error, could not load d3d9.dll.", ELL_ERROR);
			return false;
		}

		typedef IDirect3D9 * (__stdcall *D3DCREATETYPE)(UINT);
		D3DCREATETYPE d3dCreate = (D3DCREATETYPE) GetProcAddress(D3DLibrary, "Direct3DCreate9");

		if (!d3dCreate)
		{
			os::Printer::log("Error, could not get proc adress of Direct3DCreate9.", ELL_ERROR);
			return false;
		}

		//just like pID3D = Direct3DCreate9(D3D_SDK_VERSION);
		pID3D = (*d3dCreate)(D3D_SDK_VERSION);

		if (!pID3D)
		{
			os::Printer::log("Error initializing D3D.", ELL_ERROR);
			return false;
		}
	}

	// print device information
	D3DADAPTER_IDENTIFIER9 dai;
	if (!FAILED(pID3D->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &dai)))
	{
		char tmp[512];

		s32 Product = HIWORD(dai.DriverVersion.HighPart);
		s32 Version = LOWORD(dai.DriverVersion.HighPart);
		s32 SubVersion = HIWORD(dai.DriverVersion.LowPart);
		s32 Build = LOWORD(dai.DriverVersion.LowPart);

		sprintf(tmp, "%s %s %d.%d.%d.%d", dai.Description, dai.Driver, Product, Version,
			SubVersion, Build);
		os::Printer::log(tmp, ELL_INFORMATION);
	}

	D3DDISPLAYMODE d3ddm;
	hr = pID3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);
	if (FAILED(hr))
	{
		os::Printer::log("Error: Could not get Adapter Display mode.", ELL_ERROR);
		return false;
	}

	ZeroMemory(&present, sizeof(present));

	present.BackBufferCount		= 1;
	present.EnableAutoDepthStencil	= TRUE;
	if (vsync)
		present.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	else
		present.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	if (fullScreen)
	{
		present.BackBufferWidth = screenSize.Width;
		present.BackBufferHeight = screenSize.Height;
		// request 32bit mode if user specified 32 bit, added by Thomas St�fe
		if (bits == 32 && !StencilBuffer)
			present.BackBufferFormat = D3DFMT_A8R8G8B8;
		else
			present.BackBufferFormat = D3DFMT_R5G6B5;
		present.SwapEffect	= D3DSWAPEFFECT_FLIP;
		present.Windowed	= FALSE;
		present.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	}
	else
	{
		present.BackBufferFormat	= d3ddm.Format;
		present.SwapEffect		= D3DSWAPEFFECT_COPY;
		present.Windowed		= TRUE;
	}

	D3DDEVTYPE devtype = D3DDEVTYPE_HAL;
	#ifndef _IRR_D3D_NO_SHADER_DEBUGGING
	devtype = D3DDEVTYPE_REF;
	#endif

	// enable anti alias if possible and desired
	if (antiAlias)
	{
		DWORD qualityLevels = 0;

		if (SUCCEEDED(pID3D->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT,
		           devtype, present.BackBufferFormat, !fullScreen,
		           D3DMULTISAMPLE_2_SAMPLES, &qualityLevels)))
		{
			// enable multi sampling
			present.MultiSampleType    = D3DMULTISAMPLE_2_SAMPLES;
			present.MultiSampleQuality = qualityLevels-1;
			present.SwapEffect         = D3DSWAPEFFECT_DISCARD;
		}
		else
		if (SUCCEEDED(pID3D->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT,
					devtype, present.BackBufferFormat, !fullScreen,
					D3DMULTISAMPLE_NONMASKABLE, &qualityLevels)))
		{
			// enable non maskable multi sampling
			present.SwapEffect         = D3DSWAPEFFECT_DISCARD;
			present.MultiSampleType    = D3DMULTISAMPLE_NONMASKABLE;
			present.MultiSampleQuality = qualityLevels-1;
		}
		else
		{
			os::Printer::log("Anti aliasing disabled because hardware/driver lacks necessary caps.", ELL_WARNING);
			antiAlias = false;
		}
	}

	// check stencil buffer compatibility
	if (StencilBuffer)
	{
		present.AutoDepthStencilFormat = D3DFMT_D24S8;
		if(FAILED(pID3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, devtype,
			present.BackBufferFormat, D3DUSAGE_DEPTHSTENCIL,
			D3DRTYPE_SURFACE, D3DFMT_D24S8)))
		{
			os::Printer::log("Device does not support stencilbuffer, disabling stencil buffer.", ELL_WARNING);
			StencilBuffer = false;
		}
		else
		if(FAILED(pID3D->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, devtype,
			present.BackBufferFormat, present.BackBufferFormat, D3DFMT_D24S8)))
		{
			os::Printer::log("Depth-stencil format is not compatible with display format, disabling stencil buffer.", ELL_WARNING);
			StencilBuffer = false;
		}
	}

	if (!StencilBuffer)
		present.AutoDepthStencilFormat = D3DFMT_D24X8;

	// create device

	DWORD fpuPrecision = highPrecisionFPU ? D3DCREATE_FPU_PRESERVE : 0;
	if (pureSoftware)
	{
		hr = pID3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hwnd,
				fpuPrecision | D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present, &pID3DDevice);

		if (FAILED(hr))
			os::Printer::log("Was not able to create Direct3D9 software device.", ELL_ERROR);
	}
	else
	{
		hr = pID3D->CreateDevice(D3DADAPTER_DEFAULT, devtype, hwnd,
				fpuPrecision | D3DCREATE_HARDWARE_VERTEXPROCESSING, &present, &pID3DDevice);

		if(FAILED(hr))
		{
			hr = pID3D->CreateDevice(D3DADAPTER_DEFAULT, devtype, hwnd,
					fpuPrecision | D3DCREATE_MIXED_VERTEXPROCESSING , &present, &pID3DDevice);

			if(FAILED(hr))
			{
				hr = pID3D->CreateDevice(D3DADAPTER_DEFAULT, devtype, hwnd,
						fpuPrecision | D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present, &pID3DDevice);

				if (FAILED(hr))
					os::Printer::log("Was not able to create Direct3D9 device.", ELL_ERROR);
			}
		}
	}

	if (!pID3DDevice)
	{
		os::Printer::log("Was not able to create DIRECT3D9 device.", ELL_ERROR);
		return false;
	}

	// get caps
	pID3DDevice->GetDeviceCaps(&Caps);

	// disable stencilbuffer if necessary
	if (StencilBuffer &&
		(!(Caps.StencilCaps & D3DSTENCILCAPS_DECRSAT) ||
		!(Caps.StencilCaps & D3DSTENCILCAPS_INCRSAT) ||
		!(Caps.StencilCaps & D3DSTENCILCAPS_KEEP)))
	{
		os::Printer::log("Device not able to use stencil buffer, disabling stencil buffer.", ELL_WARNING);
		StencilBuffer = false;
	}

	// set default vertex shader
	setVertexShader(EVT_STANDARD);

	// enable antialiasing
	if (antiAlias)
		pID3DDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);

	// set fog mode
	setFog(FogColor, LinearFog, FogStart, FogEnd, FogDensity, PixelFog, RangeFog);

	// set exposed data
	ExposedData.D3D9.D3D9 = pID3D;
	ExposedData.D3D9.D3DDev9 = pID3DDevice;
	ExposedData.D3D9.HWnd =  reinterpret_cast<s32>(hwnd);

	ResetRenderStates = true;

	// create materials
	createMaterialRenderers();

	// set the renderstates
	setRenderStates3DMode();

	// set maximal anisotropy
	pID3DDevice->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, min(16, Caps.MaxAnisotropy));
	pID3DDevice->SetSamplerState(1, D3DSAMP_MAXANISOTROPY, min(16, Caps.MaxAnisotropy));

	// so far so good.
	return true;
}



//! applications must call this method before performing any rendering. returns false if failed.
bool CD3D9Driver::beginScene(bool backBuffer, bool zBuffer, SColor color)
{
	CNullDriver::beginScene(backBuffer, zBuffer, color);
	HRESULT hr;

	if (!pID3DDevice)
		return false;

	if (DeviceLost)
	{
		if(FAILED(hr = pID3DDevice->TestCooperativeLevel()))
		{
			if (hr == D3DERR_DEVICELOST)
				return false;

			if (hr == D3DERR_DEVICENOTRESET)
				reset();
			return false;
		}
	}

	DWORD flags = 0;

	if (backBuffer)
		flags |= D3DCLEAR_TARGET;

	if (zBuffer)
		flags |= D3DCLEAR_ZBUFFER;

	if (StencilBuffer)
		flags |= D3DCLEAR_STENCIL;

	hr = pID3DDevice->Clear( 0, NULL, flags, color.color, 1.0, 0);
	if (FAILED(hr))
		os::Printer::log("DIRECT3D9 clear failed.", ELL_WARNING);

	hr = pID3DDevice->BeginScene();
	if (FAILED(hr))
	{
		os::Printer::log("DIRECT3D9 begin scene failed.", ELL_WARNING);
		return false;
	}

	return true;
}



//! applications must call this method after performing any rendering. returns false if failed.
bool CD3D9Driver::endScene( s32 windowId, core::rect<s32>* sourceRect )
{
	if (DeviceLost)
		return false;

	CNullDriver::endScene();

	HRESULT hr = pID3DDevice->EndScene();
	if (FAILED(hr))
	{
		os::Printer::log("DIRECT3D9 end scene failed.", ELL_WARNING);
		return false;
	}

	RECT* srcRct = 0;
	RECT sourceRectData;
	if ( sourceRect )
	{
		srcRct = &sourceRectData;
		sourceRectData.left = sourceRect->UpperLeftCorner.X;
		sourceRectData.top = sourceRect->UpperLeftCorner.Y;
		sourceRectData.right = sourceRect->LowerRightCorner.X;
		sourceRectData.bottom = sourceRect->LowerRightCorner.Y;
	}

	hr = pID3DDevice->Present(srcRct, NULL, (HWND)windowId, NULL);

	if (hr == D3DERR_DEVICELOST)
	{
		DeviceLost = true;
		os::Printer::log("DIRECT3D9 device lost.", ELL_WARNING);
	}
	else
	if (FAILED(hr) && hr != D3DERR_INVALIDCALL)
	{
		os::Printer::log("DIRECT3D9 present failed.", ELL_WARNING);
		return false;
	}

	return true;
}



//! queries the features of the driver, returns true if feature is available
bool CD3D9Driver::queryFeature(E_VIDEO_DRIVER_FEATURE feature)
{
	switch (feature)
	{
	case EVDF_BILINEAR_FILTER:
		return true;
	case EVDF_RENDER_TO_TARGET:
		return Caps.NumSimultaneousRTs > 0;
	case EVDF_HARDWARE_TL:
		return (Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;
	case EVDF_MIP_MAP:
		return (Caps.TextureCaps & D3DPTEXTURECAPS_MIPMAP) != 0;
	case EVDF_STENCIL_BUFFER:
		return StencilBuffer &&  Caps.StencilCaps;
	case EVDF_VERTEX_SHADER_1_1:
		return Caps.VertexShaderVersion >= D3DVS_VERSION(1,1);
	case EVDF_VERTEX_SHADER_2_0:
		return Caps.VertexShaderVersion >= D3DVS_VERSION(2,0);
	case EVDF_VERTEX_SHADER_3_0:
		return Caps.VertexShaderVersion >= D3DVS_VERSION(3,0);
	case EVDF_PIXEL_SHADER_1_1:
		return Caps.PixelShaderVersion >= D3DPS_VERSION(1,1);
	case EVDF_PIXEL_SHADER_1_2:
		return Caps.PixelShaderVersion >= D3DPS_VERSION(1,2);
	case EVDF_PIXEL_SHADER_1_3:
		return Caps.PixelShaderVersion >= D3DPS_VERSION(1,3);
	case EVDF_PIXEL_SHADER_1_4:
		return Caps.PixelShaderVersion >= D3DPS_VERSION(1,4);
	case EVDF_PIXEL_SHADER_2_0:
		return Caps.PixelShaderVersion >= D3DPS_VERSION(2,0);
	case EVDF_PIXEL_SHADER_3_0:
		return Caps.PixelShaderVersion >= D3DPS_VERSION(3,0);
	case EVDF_HLSL:
		return Caps.VertexShaderVersion >= D3DVS_VERSION(1,1);
	default:
		return false;
	};
}



//! sets transformation
void CD3D9Driver::setTransform(E_TRANSFORMATION_STATE state, const core::matrix4& mat)
{
	Transformation3DChanged = true;

	switch(state)
	{
	case ETS_VIEW:
		pID3DDevice->SetTransform(D3DTS_VIEW, (D3DMATRIX*)((void*)&mat));
		break;
	case ETS_WORLD:
		pID3DDevice->SetTransform(D3DTS_WORLD, (D3DMATRIX*)((void*)&mat));
		break;
	case ETS_PROJECTION:
		pID3DDevice->SetTransform( D3DTS_PROJECTION, (D3DMATRIX*)((void*)&mat));
		break;
	}

	Matrices[state] = mat;
}



//! sets the current Texture
bool CD3D9Driver::setTexture(s32 stage, video::ITexture* texture)
{
	if (CurrentTexture[stage] == texture)
		return true;

	if (texture && texture->getDriverType() != EDT_DIRECT3D9)
	{
		os::Printer::log("Fatal Error: Tried to set a texture not owned by this driver.", ELL_ERROR);
		return false;
	}

	if (CurrentTexture[stage])
		CurrentTexture[stage]->drop();

	CurrentTexture[stage] = texture;

	if (!texture)
		pID3DDevice->SetTexture(stage, 0);
	else
	{
		pID3DDevice->SetTexture(stage, ((CD3D9Texture*)texture)->getDX9Texture());
		texture->grab();
		pID3DDevice->SetTransform(D3DTS_TEXTURE0+stage, (D3DMATRIX*)((void*)&texture->getTransformation()));
	}
	return true;
}



//! sets a material
void CD3D9Driver::setMaterial(const SMaterial& material)
{
	Material = material;

	setTexture(0, Material.Texture1);
	setTexture(1, Material.Texture2);
	setTexture(2, Material.Texture3);
	setTexture(3, Material.Texture4);
}



//! returns a device dependent texture from a software surface (IImage)
video::ITexture* CD3D9Driver::createDeviceDependentTexture(IImage* surface, const char* name)
{
	return new CD3D9Texture(surface, pID3DDevice,
		TextureCreationFlags, name);
}



//! Enables or disables a texture creation flag.
void CD3D9Driver::setTextureCreationFlag(E_TEXTURE_CREATION_FLAG flag, bool enabled)
{
	if (flag == video::ETCF_CREATE_MIP_MAPS && !queryFeature(EVDF_MIP_MAP))
		enabled = false;

	CNullDriver::setTextureCreationFlag(flag, enabled);
}



//! sets a render target
bool CD3D9Driver::setRenderTarget(video::ITexture* texture,
			bool clearBackBuffer, bool clearZBuffer,
			SColor color)
{
	// check for right driver type

	if (texture && texture->getDriverType() != EDT_DIRECT3D9)
	{
		os::Printer::log("Fatal Error: Tried to set a texture not owned by this driver.", ELL_ERROR);
		return false;
	}

	// check for valid render target

	CD3D9Texture* tex = (CD3D9Texture*)texture;

	if (texture && !tex->isRenderTarget())
	{
		os::Printer::log("Fatal Error: Tried to set a non render target texture as render target.", ELL_ERROR);
		return false;
	}

	if (texture && (tex->getSize().Width > ScreenSize.Width ||
		tex->getSize().Height > ScreenSize.Height ))
	{
		os::Printer::log("Error: Tried to set a render target texture which is bigger than the screen.", ELL_ERROR);
		return false;
	}

	// check if we should set the previous RT back

	bool ret = true;

	if (tex == 0)
	{
		if (PrevRenderTarget)
		{
			if (FAILED(pID3DDevice->SetRenderTarget(0, PrevRenderTarget)))
			{
				os::Printer::log("Error: Could not set back to previous render target.", ELL_ERROR);
				ret = false;
			}

			CurrentRendertargetSize = core::dimension2d<s32>(0,0);
			PrevRenderTarget->Release();
			PrevRenderTarget = 0;
		}
	}
	else
	{
		// we want to set a new target. so do this.

		// store previous target

		if (!PrevRenderTarget)
			if (FAILED(pID3DDevice->GetRenderTarget(0, &PrevRenderTarget)))
			{
				os::Printer::log("Could not get previous render target.", ELL_ERROR);
				return false;
			}

		// set new render target

		if (FAILED(pID3DDevice->SetRenderTarget(0, tex->getRenderTargetSurface())))
		{
			os::Printer::log("Error: Could not set render target.", ELL_ERROR);
			return false;
		}

		CurrentRendertargetSize = tex->getSize();
	}

	if (clearBackBuffer || clearZBuffer)
	{
		DWORD flags = 0;

		if (clearBackBuffer)
			flags |= D3DCLEAR_TARGET;

		if (clearZBuffer)
			flags |= D3DCLEAR_ZBUFFER;

		pID3DDevice->Clear(0, NULL, flags, color.color, 1.0f, 0);
	}

	return ret;
}



//! sets a viewport
void CD3D9Driver::setViewPort(const core::rect<s32>& area)
{
	core::rect<s32> vp = area;
	core::rect<s32> rendert(0,0, ScreenSize.Width, ScreenSize.Height);
	vp.clipAgainst(rendert);

	D3DVIEWPORT9 viewPort;
	viewPort.X = vp.UpperLeftCorner.X;
	viewPort.Y = vp.UpperLeftCorner.Y;
	viewPort.Width = vp.getWidth();
	viewPort.Height = vp.getHeight();
	viewPort.MinZ = 0.0f;
	viewPort.MaxZ = 1.0f;

	HRESULT hr = D3DERR_INVALIDCALL;
	if (vp.getHeight()>0 && vp.getWidth()>0)
		hr = pID3DDevice->SetViewport(&viewPort);

	if (FAILED(hr))
		os::Printer::log("Failed setting the viewport.", ELL_WARNING);

	ViewPort = vp;
}



//! gets the area of the current viewport
const core::rect<s32>& CD3D9Driver::getViewPort() const
{
	return ViewPort;
}



//! draws a vertex primitive list
void CD3D9Driver::drawVertexPrimitiveList(const void* vertices, s32 vertexCount, const u16* indexList, s32 primitiveCount, E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType)
{
	if (!checkPrimitiveCount(primitiveCount))
		return;

	CNullDriver::drawVertexPrimitiveList(vertices, vertexCount, indexList, primitiveCount, vType, pType);

	if (!vertexCount || !primitiveCount)
		return;

	setVertexShader(vType);

	size_t stride=0;
	switch (vType)
	{
		case EVT_STANDARD:
			stride=sizeof(S3DVertex);
			break;
		case EVT_2TCOORDS:
			stride=sizeof(S3DVertex2TCoords);
			break;
		case EVT_TANGENTS:
			stride=sizeof(S3DVertexTangents);
			break;
	}
	if (setRenderStates3DMode())
	{
		switch (pType)
		{
			case scene::EPT_POINTS:
				pID3DDevice->DrawIndexedPrimitiveUP(D3DPT_POINTLIST, 0, vertexCount,
					primitiveCount, indexList, D3DFMT_INDEX16, vertices, stride);
				break;
			case scene::EPT_LINE_STRIP:
				pID3DDevice->DrawIndexedPrimitiveUP(D3DPT_LINESTRIP, 0, vertexCount,
					primitiveCount, indexList, D3DFMT_INDEX16, vertices, stride);
				break;
			case scene::EPT_LINE_LOOP:
			{
				pID3DDevice->DrawIndexedPrimitiveUP(D3DPT_LINESTRIP, 0, vertexCount,
					primitiveCount, indexList, D3DFMT_INDEX16, vertices, stride);
				u16 tmpIndices[] = {0, primitiveCount};
				pID3DDevice->DrawIndexedPrimitiveUP(D3DPT_LINELIST, 0, vertexCount,
					1, tmpIndices, D3DFMT_INDEX16, vertices, stride);
			}
				break;
			case scene::EPT_LINES:
				pID3DDevice->DrawIndexedPrimitiveUP(D3DPT_LINELIST, 0, vertexCount,
					primitiveCount, indexList, D3DFMT_INDEX16, vertices, stride);
				break;
			case scene::EPT_TRIANGLE_STRIP:
				pID3DDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLESTRIP, 0, vertexCount,
					primitiveCount, indexList, D3DFMT_INDEX16, vertices, stride);
				break;
			case scene::EPT_TRIANGLE_FAN:
				pID3DDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLEFAN, 0, vertexCount,
					primitiveCount, indexList, D3DFMT_INDEX16, vertices, stride);
				break;
			case scene::EPT_TRIANGLES:
				pID3DDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, vertexCount,
					primitiveCount, indexList, D3DFMT_INDEX16, vertices, stride);
				break;
		}
	}
}



void CD3D9Driver::draw2DImage(video::ITexture* texture, const core::rect<s32>& destRect,
		const core::rect<s32>& sourceRect, const core::rect<s32>* clipRect,
		video::SColor* colors, bool useAlphaChannelOfTexture)
{
	if(!texture)
		return;

	core::rect<s32> trgRect(destRect);

	const core::dimension2d<s32>& ss = texture->getOriginalSize();
	f32 ssw=1.0f/ss.Width;
	f32 ssh=1.0f/ss.Height;

	core::rect<f32> tcoords;
	tcoords.UpperLeftCorner.X = (((f32)sourceRect.UpperLeftCorner.X)+0.5f) * ssw ;
	tcoords.UpperLeftCorner.Y = (((f32)sourceRect.UpperLeftCorner.Y)+0.5f) * ssh;
	tcoords.LowerRightCorner.X = (((f32)sourceRect.UpperLeftCorner.X +0.5f + (f32)sourceRect.getWidth())) * ssw;
	tcoords.LowerRightCorner.Y = (((f32)sourceRect.UpperLeftCorner.Y +0.5f + (f32)sourceRect.getHeight())) * ssh;

	core::dimension2d<s32> renderTargetSize = getCurrentRenderTargetSize();

	s32 xPlus = -(renderTargetSize.Width>>1);
	f32 xFact = 1.0f / (renderTargetSize.Width>>1);

	s32 yPlus = renderTargetSize.Height-(renderTargetSize.Height>>1);
	f32 yFact = 1.0f / (renderTargetSize.Height>>1);

	core::rect<f32> npos;
	npos.UpperLeftCorner.X = (f32)(trgRect.UpperLeftCorner.X+xPlus+0.5f) * xFact;
	npos.UpperLeftCorner.Y = (f32)(yPlus-trgRect.UpperLeftCorner.Y+0.5f) * yFact;
	npos.LowerRightCorner.X = (f32)(trgRect.LowerRightCorner.X+xPlus+0.5f) * xFact;
	npos.LowerRightCorner.Y = (f32)(yPlus-trgRect.LowerRightCorner.Y+0.5f) * yFact;

	video::SColor temp[4] =
	{
		0xFFFFFFFF,
		0xFFFFFFFF,
		0xFFFFFFFF,
		0xFFFFFFFF
	};

	video::SColor* useColor = colors ? colors : temp;

	S3DVertex vtx[4]; // clock wise
	vtx[0] = S3DVertex(npos.UpperLeftCorner.X, npos.UpperLeftCorner.Y , 0.0f, 0.0f, 0.0f, 0.0f, useColor[0], tcoords.UpperLeftCorner.X, tcoords.UpperLeftCorner.Y);
	vtx[1] = S3DVertex(npos.LowerRightCorner.X, npos.UpperLeftCorner.Y , 0.0f, 0.0f, 0.0f, 0.0f, useColor[3], tcoords.LowerRightCorner.X, tcoords.UpperLeftCorner.Y);
	vtx[2] = S3DVertex(npos.LowerRightCorner.X, npos.LowerRightCorner.Y, 0.0f, 0.0f, 0.0f, 0.0f, useColor[2], tcoords.LowerRightCorner.X, tcoords.LowerRightCorner.Y);
	vtx[3] = S3DVertex(npos.UpperLeftCorner.X, npos.LowerRightCorner.Y, 0.0f, 0.0f, 0.0f, 0.0f, useColor[1], tcoords.UpperLeftCorner.X, tcoords.LowerRightCorner.Y);

	s16 indices[6] = {0,1,2,0,2,3};

	setRenderStates2DMode(useColor[0].getAlpha()<255 || useColor[1].getAlpha()<255 || useColor[2].getAlpha()<255 || useColor[3].getAlpha()<255, true, useAlphaChannelOfTexture);

	setTexture(0, texture);

	setVertexShader(EVT_STANDARD);

	pID3DDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, &indices[0],
				D3DFMT_INDEX16,&vtx[0], sizeof(S3DVertex));

}



//! draws a 2d image, using a color (if color is other then Color(255,255,255,255)) and the alpha channel of the texture if wanted.
void CD3D9Driver::draw2DImage(video::ITexture* texture, const core::position2d<s32>& pos,
				 const core::rect<s32>& sourceRect,
				 const core::rect<s32>* clipRect, SColor color,
				 bool useAlphaChannelOfTexture)
{
	if (!texture)
		return;

	if (!sourceRect.isValid())
		return;

	if (!setTexture(0, texture))
		return;

	core::position2d<s32> targetPos = pos;
	core::position2d<s32> sourcePos = sourceRect.UpperLeftCorner;
	core::dimension2d<s32> sourceSize(sourceRect.getSize());

	core::dimension2d<s32> renderTargetSize = getCurrentRenderTargetSize();

	if (clipRect)
	{
		if (targetPos.X < clipRect->UpperLeftCorner.X)
		{
			sourceSize.Width += targetPos.X - clipRect->UpperLeftCorner.X;
			if (sourceSize.Width <= 0)
				return;

			sourcePos.X -= targetPos.X - clipRect->UpperLeftCorner.X;
			targetPos.X = clipRect->UpperLeftCorner.X;
		}

		if (targetPos.X + sourceSize.Width > clipRect->LowerRightCorner.X)
		{
			sourceSize.Width -= (targetPos.X + sourceSize.Width) - clipRect->LowerRightCorner.X;
			if (sourceSize.Width <= 0)
				return;
		}

		if (targetPos.Y < clipRect->UpperLeftCorner.Y)
		{
			sourceSize.Height += targetPos.Y - clipRect->UpperLeftCorner.Y;
			if (sourceSize.Height <= 0)
				return;

			sourcePos.Y -= targetPos.Y - clipRect->UpperLeftCorner.Y;
			targetPos.Y = clipRect->UpperLeftCorner.Y;
		}

		if (targetPos.Y + sourceSize.Height > clipRect->LowerRightCorner.Y)
		{
			sourceSize.Height -= (targetPos.Y + sourceSize.Height) - clipRect->LowerRightCorner.Y;
			if (sourceSize.Height <= 0)
				return;
		}
	}

	// clip these coordinates

	if (targetPos.X<0)
	{
		sourceSize.Width += targetPos.X;
		if (sourceSize.Width <= 0)
			return;

		sourcePos.X -= targetPos.X;
		targetPos.X = 0;
	}

	if (targetPos.X + sourceSize.Width > renderTargetSize.Width)
	{
		sourceSize.Width -= (targetPos.X + sourceSize.Width) - renderTargetSize.Width;
		if (sourceSize.Width <= 0)
			return;
	}

	if (targetPos.Y<0)
	{
		sourceSize.Height += targetPos.Y;
		if (sourceSize.Height <= 0)
			return;

		sourcePos.Y -= targetPos.Y;
		targetPos.Y = 0;
	}

	if (targetPos.Y + sourceSize.Height > renderTargetSize.Height)
	{
		sourceSize.Height -= (targetPos.Y + sourceSize.Height) - renderTargetSize.Height;
		if (sourceSize.Height <= 0)
			return;
	}

	// ok, we've clipped everything.
	// now draw it.

	f32 xPlus = - renderTargetSize.Width / 2.f;
	f32 xFact = 1.0f / (renderTargetSize.Width / 2.f);

	f32 yPlus = renderTargetSize.Height-(renderTargetSize.Height / 2.f);
	f32 yFact = 1.0f / (renderTargetSize.Height / 2.f);

	const core::dimension2d<s32>& sourceSurfaceSize = texture->getOriginalSize();
	core::rect<f32> tcoords;
	tcoords.UpperLeftCorner.X = (((f32)sourcePos.X)+0.5f) / texture->getOriginalSize().Width ;
	tcoords.UpperLeftCorner.Y = (((f32)sourcePos.Y)+0.5f) / texture->getOriginalSize().Height;
	tcoords.LowerRightCorner.X = (((f32)sourcePos.X +0.5f + (f32)sourceSize.Width)) / texture->getOriginalSize().Width;
	tcoords.LowerRightCorner.Y = (((f32)sourcePos.Y +0.5f + (f32)sourceSize.Height)) / texture->getOriginalSize().Height;

	core::rect<s32> poss(targetPos, sourceSize);

	setRenderStates2DMode(color.getAlpha()<255, true, useAlphaChannelOfTexture);

	S3DVertex vtx[4];
	vtx[0] = S3DVertex((f32)(poss.UpperLeftCorner.X+xPlus) * xFact, (f32)(yPlus-poss.UpperLeftCorner.Y ) * yFact , 0.0f, 0.0f, 0.0f, 0.0f, color, tcoords.UpperLeftCorner.X, tcoords.UpperLeftCorner.Y);
	vtx[1] = S3DVertex((f32)(poss.LowerRightCorner.X+xPlus) * xFact, (f32)(yPlus- poss.UpperLeftCorner.Y) * yFact, 0.0f, 0.0f, 0.0f, 0.0f, color, tcoords.LowerRightCorner.X, tcoords.UpperLeftCorner.Y);
	vtx[2] = S3DVertex((f32)(poss.LowerRightCorner.X+xPlus) * xFact, (f32)(yPlus-poss.LowerRightCorner.Y) * yFact, 0.0f, 0.0f, 0.0f, 0.0f, color, tcoords.LowerRightCorner.X, tcoords.LowerRightCorner.Y);
	vtx[3] = S3DVertex((f32)(poss.UpperLeftCorner.X+xPlus) * xFact, (f32)(yPlus-poss.LowerRightCorner.Y) * yFact, 0.0f, 0.0f, 0.0f, 0.0f, color, tcoords.UpperLeftCorner.X, tcoords.LowerRightCorner.Y);

	s16 indices[6] = {0,1,2,0,2,3};

	setVertexShader(EVT_STANDARD);

	pID3DDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, &indices[0],
										D3DFMT_INDEX16,&vtx[0],	sizeof(S3DVertex));
}



//!Draws a 2d rectangle with a gradient.
void CD3D9Driver::draw2DRectangle(const core::rect<s32>& position,
	SColor colorLeftUp, SColor colorRightUp, SColor colorLeftDown, SColor colorRightDown,
	const core::rect<s32>* clip)
{
	core::rect<s32> pos(position);

	if (clip)
		pos.clipAgainst(*clip);

	if (!pos.isValid())
		return;

	const core::dimension2d<s32>& renderTargetSize = getCurrentRenderTargetSize();

	s32 xPlus = -(renderTargetSize.Width>>1);
	f32 xFact = 1.0f / (renderTargetSize.Width>>1);

	s32 yPlus = renderTargetSize.Height-(renderTargetSize.Height>>1);
	f32 yFact = 1.0f / (renderTargetSize.Height>>1);

	S3DVertex vtx[4];
	vtx[0] = S3DVertex((f32)(pos.UpperLeftCorner.X+xPlus) * xFact, (f32)(yPlus-pos.UpperLeftCorner.Y) * yFact , 0.0f, 0.0f, 0.0f, 0.0f, colorLeftUp, 0.0f, 0.0f);
	vtx[1] = S3DVertex((f32)(pos.LowerRightCorner.X+xPlus) * xFact, (f32)(yPlus- pos.UpperLeftCorner.Y) * yFact, 0.0f, 0.0f, 0.0f, 0.0f, colorRightUp, 0.0f, 1.0f);
	vtx[2] = S3DVertex((f32)(pos.LowerRightCorner.X+xPlus) * xFact, (f32)(yPlus-pos.LowerRightCorner.Y) * yFact, 0.0f, 0.0f, 0.0f, 0.0f, colorRightDown, 1.0f, 0.0f);
	vtx[3] = S3DVertex((f32)(pos.UpperLeftCorner.X+xPlus) * xFact, (f32)(yPlus-pos.LowerRightCorner.Y) * yFact, 0.0f, 0.0f, 0.0f, 0.0f, colorLeftDown, 1.0f, 1.0f);

	s16 indices[6] = {0,1,2,0,2,3};

	setRenderStates2DMode(
		colorLeftUp.getAlpha() < 255 ||
		colorRightUp.getAlpha() < 255 ||
		colorLeftDown.getAlpha() < 255 ||
		colorRightDown.getAlpha() < 255, false, false);

	setTexture(0,0);

	setVertexShader(EVT_STANDARD);

	pID3DDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, &indices[0],
											D3DFMT_INDEX16, &vtx[0], sizeof(S3DVertex));

}



//! Draws a 2d line.
void CD3D9Driver::draw2DLine(const core::position2d<s32>& start,
				const core::position2d<s32>& end,
				SColor color)
{
	// thanks to Vash TheStampede who sent in his implementation

	const core::dimension2d<s32>& renderTargetSize = getCurrentRenderTargetSize();
	const s32 xPlus = -(renderTargetSize.Width>>1);
	const f32 xFact = 1.0f / (renderTargetSize.Width>>1);

	const s32 yPlus = renderTargetSize.Height-(renderTargetSize.Height>>1);
	const f32 yFact = 1.0f / (renderTargetSize.Height>>1);

	S3DVertex vtx[2];
	vtx[0] = S3DVertex((f32)(start.X + xPlus) * xFact,
						(f32)(yPlus - start.Y) * yFact,
						0.0f, // z
						0.0f, 0.0f, 0.0f, // normal
						color,
						0.0f, 0.0f); // texture

	vtx[1] = S3DVertex((f32)(end.X+xPlus) * xFact,
						(f32)(yPlus- end.Y) * yFact,
						0.0f,
						0.0f, 0.0f, 0.0f,
						color,
						0.0f, 0.0f);

	setRenderStates2DMode(color.getAlpha() < 255, false, false);
	setTexture(0,0);

	setVertexShader(EVT_STANDARD);

	pID3DDevice->DrawPrimitiveUP(D3DPT_LINELIST, 1,
					&vtx[0], sizeof(S3DVertex) );
}



//! sets right vertex shader
void CD3D9Driver::setVertexShader(E_VERTEX_TYPE newType)
{
	if (newType != LastVertexType)
	{
		LastVertexType = newType;
		HRESULT hr = 0;

		switch(newType)
		{
		case EVT_STANDARD:
			hr = pID3DDevice->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1);
			break;
		case EVT_2TCOORDS:
			hr = pID3DDevice->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX2);
			break;
		case EVT_TANGENTS:
			hr = pID3DDevice->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX3 |
				D3DFVF_TEXCOORDSIZE2(0) | // real texture coord
				D3DFVF_TEXCOORDSIZE3(1) | // misuse texture coord 2 for tangent
				D3DFVF_TEXCOORDSIZE3(2)   // misuse texture coord 3 for binormal
				);
			break;
		}

		if (FAILED(hr))
		{
			os::Printer::log("Could not set vertex Shader.", ELL_ERROR);
			return;
		}
	}
}



//! sets the needed renderstates
bool CD3D9Driver::setRenderStates3DMode()
{
	if (!pID3DDevice)
		return false;

	if (CurrentRenderMode != ERM_3D)
	{
		// switch back the matrices
		pID3DDevice->SetTransform(D3DTS_VIEW, (D3DMATRIX*)((void*)&Matrices[ETS_VIEW]));
		pID3DDevice->SetTransform(D3DTS_WORLD, (D3DMATRIX*)((void*)&Matrices[ETS_WORLD]));
		pID3DDevice->SetTransform(D3DTS_PROJECTION, (D3DMATRIX*)((void*)&Matrices[ETS_PROJECTION]));

		pID3DDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);

		ResetRenderStates = true;
	}

	if (ResetRenderStates || LastMaterial != Material)
	{
		// unset old material

		if (CurrentRenderMode == ERM_3D &&
			LastMaterial.MaterialType != Material.MaterialType &&
			LastMaterial.MaterialType >= 0 && LastMaterial.MaterialType < (s32)MaterialRenderers.size())
			MaterialRenderers[LastMaterial.MaterialType].Renderer->OnUnsetMaterial();

		// set new material.

		if (Material.MaterialType >= 0 && Material.MaterialType < (s32)MaterialRenderers.size())
			MaterialRenderers[Material.MaterialType].Renderer->OnSetMaterial(
				Material, LastMaterial, ResetRenderStates, this);
	}

	bool shaderOK = true;
	if (Material.MaterialType >= 0 && Material.MaterialType < (s32)MaterialRenderers.size())
		shaderOK = MaterialRenderers[Material.MaterialType].Renderer->OnRender(this, LastVertexType);

	LastMaterial = Material;

	ResetRenderStates = false;

	CurrentRenderMode = ERM_3D;

	return shaderOK;
}



//! Can be called by an IMaterialRenderer to make its work easier.
void CD3D9Driver::setBasicRenderStates(const SMaterial& material, const SMaterial& lastmaterial,
	bool resetAllRenderstates)
{
	if (resetAllRenderstates ||
		lastmaterial.AmbientColor != material.AmbientColor ||
		lastmaterial.DiffuseColor != material.DiffuseColor ||
		lastmaterial.SpecularColor != material.SpecularColor ||
		lastmaterial.EmissiveColor != material.EmissiveColor ||
		lastmaterial.Shininess != material.Shininess)
	{
		D3DMATERIAL9 mat;
		mat.Diffuse = colorToD3D(material.DiffuseColor);
		mat.Ambient = colorToD3D(material.AmbientColor);
		mat.Specular = colorToD3D(material.SpecularColor);
		mat.Emissive = colorToD3D(material.EmissiveColor);
		mat.Power = material.Shininess;
		pID3DDevice->SetMaterial(&mat);
	}

	// Bilinear and/or trilinear
	if (resetAllRenderstates ||
		lastmaterial.BilinearFilter != material.BilinearFilter ||
		lastmaterial.TrilinearFilter != material.TrilinearFilter ||
		lastmaterial.AnisotropicFilter != material.AnisotropicFilter ||
		!LastTextureMipMapsAvailable[0] ||
		!LastTextureMipMapsAvailable[1])
	{
		if (material.BilinearFilter || material.TrilinearFilter || material.AnisotropicFilter)
		{
			pID3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, material.AnisotropicFilter ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR);
			pID3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, material.AnisotropicFilter ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR);
			pID3DDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, material.TrilinearFilter ? D3DTEXF_LINEAR : D3DTEXF_POINT);

			pID3DDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, material.AnisotropicFilter ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR);
			pID3DDevice->SetSamplerState(1, D3DSAMP_MINFILTER, material.AnisotropicFilter ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR);
			pID3DDevice->SetSamplerState(1, D3DSAMP_MIPFILTER, material.TrilinearFilter ? D3DTEXF_LINEAR : D3DTEXF_POINT);
		}
		else
		{
			pID3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
			pID3DDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
			pID3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

			pID3DDevice->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_POINT);
			pID3DDevice->SetSamplerState(1, D3DSAMP_MIPFILTER,  D3DTEXF_NONE);
			pID3DDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		}
	}

	// fillmode

	if (resetAllRenderstates || lastmaterial.Wireframe != material.Wireframe || lastmaterial.PointCloud != material.PointCloud)
	{
		if (material.Wireframe)
			pID3DDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
		else
		if (material.PointCloud)
			pID3DDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_POINT);
		else
			pID3DDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	}

	// shademode

	if (resetAllRenderstates || lastmaterial.GouraudShading != material.GouraudShading)
	{
		if (material.GouraudShading)
			pID3DDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
		else
			pID3DDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);
	}

	// lighting

	if (resetAllRenderstates || lastmaterial.Lighting != material.Lighting)
	{
		if (material.Lighting)
			pID3DDevice->SetRenderState(D3DRS_LIGHTING, TRUE);
		else
			pID3DDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	}

	// zbuffer

	if (resetAllRenderstates || lastmaterial.ZBuffer != material.ZBuffer)
	{
		if (material.ZBuffer)
			pID3DDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
		else
			pID3DDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	}

	// zwrite
	if (resetAllRenderstates || lastmaterial.ZWriteEnable != material.ZWriteEnable)
	{
		if (material.ZWriteEnable)
			pID3DDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE);
		else
			pID3DDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE);
	}

	// back face culling

	if (resetAllRenderstates || lastmaterial.BackfaceCulling != material.BackfaceCulling)
	{
		if (material.BackfaceCulling)
			pID3DDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW);
		else
			pID3DDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE);
	}

	// fog
	if (resetAllRenderstates || lastmaterial.FogEnable != material.FogEnable)
	{
		pID3DDevice->SetRenderState(D3DRS_FOGENABLE, material.FogEnable);
	}

	// specular highlights
	if (resetAllRenderstates || !core::equals(lastmaterial.Shininess,material.Shininess))
	{
		bool enable = (material.Shininess!=0.0f);
		pID3DDevice->SetRenderState(D3DRS_SPECULARENABLE, enable);
		pID3DDevice->SetRenderState(D3DRS_NORMALIZENORMALS, enable);
		pID3DDevice->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
	}

	// normalization
	if (resetAllRenderstates || lastmaterial.NormalizeNormals != material.NormalizeNormals)
	{
		pID3DDevice->SetRenderState(D3DRS_NORMALIZENORMALS,  material.NormalizeNormals);
	}
}



//! sets the needed renderstates
void CD3D9Driver::setRenderStatesStencilShadowMode(bool zfail)
{
	if ((CurrentRenderMode != ERM_SHADOW_VOLUME_ZFAIL &&
		CurrentRenderMode != ERM_SHADOW_VOLUME_ZPASS) ||
		Transformation3DChanged)
	{
		// switch back the matrices
		pID3DDevice->SetTransform(D3DTS_VIEW, (D3DMATRIX*)((void*)&Matrices[ETS_VIEW]));
		pID3DDevice->SetTransform(D3DTS_WORLD, (D3DMATRIX*)((void*)&Matrices[ETS_WORLD]));
		pID3DDevice->SetTransform(D3DTS_PROJECTION, (D3DMATRIX*)((void*)&Matrices[ETS_PROJECTION]));

		Transformation3DChanged = false;

		setTexture(0,0);
		setTexture(1,0);
		setTexture(2,0);
		setTexture(3,0);

		pID3DDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		pID3DDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
		pID3DDevice->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
		pID3DDevice->SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		pID3DDevice->SetFVF(D3DFVF_XYZ);
		LastVertexType = (video::E_VERTEX_TYPE)(-1);

		pID3DDevice->SetRenderState( D3DRS_ZWRITEENABLE,  FALSE );
		pID3DDevice->SetRenderState( D3DRS_STENCILENABLE, TRUE );
		pID3DDevice->SetRenderState( D3DRS_SHADEMODE, D3DSHADE_FLAT);

		// unset last 3d material
		if (CurrentRenderMode == ERM_3D &&
			Material.MaterialType >= 0 && Material.MaterialType < (s32)MaterialRenderers.size())
			MaterialRenderers[Material.MaterialType].Renderer->OnUnsetMaterial();

		//pID3DDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
		//pID3DDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	}

	if (CurrentRenderMode != ERM_SHADOW_VOLUME_ZPASS && !zfail)
	{
		// USE THE ZPASS METHOD

		pID3DDevice->SetRenderState( D3DRS_STENCILFUNC,  D3DCMP_ALWAYS );
		pID3DDevice->SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
		pID3DDevice->SetRenderState( D3DRS_STENCILFAIL,  D3DSTENCILOP_KEEP );

		pID3DDevice->SetRenderState( D3DRS_STENCILREF,       0x1 );
		pID3DDevice->SetRenderState( D3DRS_STENCILMASK,      0xffffffff );
		pID3DDevice->SetRenderState( D3DRS_STENCILWRITEMASK, 0xffffffff );

		pID3DDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE);
		pID3DDevice->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_ZERO );
		pID3DDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
	}
	else
	if (CurrentRenderMode != ERM_SHADOW_VOLUME_ZFAIL && zfail)
	{
		// USE THE ZFAIL METHOD

		pID3DDevice->SetRenderState( D3DRS_STENCILFUNC,  D3DCMP_ALWAYS );
		pID3DDevice->SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
		pID3DDevice->SetRenderState( D3DRS_STENCILFAIL,  D3DSTENCILOP_KEEP );
		pID3DDevice->SetRenderState( D3DRS_STENCILPASS,  D3DSTENCILOP_KEEP );

		pID3DDevice->SetRenderState( D3DRS_STENCILREF,       0x0 );
		pID3DDevice->SetRenderState( D3DRS_STENCILMASK,      0xffffffff );
		pID3DDevice->SetRenderState( D3DRS_STENCILWRITEMASK, 0xffffffff );

		pID3DDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
		pID3DDevice->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_ZERO );
		pID3DDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
	}

	CurrentRenderMode = zfail ? ERM_SHADOW_VOLUME_ZFAIL : ERM_SHADOW_VOLUME_ZPASS;
}



//! sets the needed renderstates
void CD3D9Driver::setRenderStatesStencilFillMode(bool alpha)
{
	if (CurrentRenderMode != ERM_STENCIL_FILL || Transformation3DChanged)
	{
		core::matrix4 mat;
		pID3DDevice->SetTransform(D3DTS_VIEW, (D3DMATRIX*)((void*)&mat));
		pID3DDevice->SetTransform(D3DTS_WORLD, (D3DMATRIX*)((void*)&mat));
		pID3DDevice->SetTransform(D3DTS_PROJECTION, (D3DMATRIX*)((void*)&mat));

		pID3DDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		pID3DDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
		pID3DDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);

		pID3DDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		pID3DDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		pID3DDevice->SetRenderState( D3DRS_STENCILREF, 0x1);
		pID3DDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_LESSEQUAL);
		//pID3DDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_GREATEREQUAL);
		pID3DDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
		pID3DDevice->SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
		pID3DDevice->SetRenderState( D3DRS_STENCILMASK,      0xffffffff );
		pID3DDevice->SetRenderState( D3DRS_STENCILWRITEMASK, 0xffffffff );

		pID3DDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW);

		Transformation3DChanged = false;

		if (alpha)
		{
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			pID3DDevice->SetTextureStageState (0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
			pID3DDevice->SetTextureStageState (0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );
			pID3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			pID3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			pID3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		}
		else
		{
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			pID3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,  D3DTOP_DISABLE );
			pID3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		}
	}

	CurrentRenderMode = ERM_STENCIL_FILL;
}



//! sets the needed renderstates
void CD3D9Driver::setRenderStates2DMode(bool alpha, bool texture, bool alphaChannel)
{
	if (!pID3DDevice)
		return;

	if (CurrentRenderMode != ERM_2D || Transformation3DChanged)
	{
		core::matrix4 mat;
		pID3DDevice->SetTransform(D3DTS_VIEW, (D3DMATRIX*)((void*)&mat));
		pID3DDevice->SetTransform(D3DTS_WORLD, (D3DMATRIX*)((void*)&mat));
		pID3DDevice->SetTransform(D3DTS_PROJECTION, (D3DMATRIX*)((void*)&mat));

		pID3DDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		//pID3DDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);
		pID3DDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		pID3DDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
		pID3DDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
		pID3DDevice->SetRenderState(D3DRS_SPECULARENABLE, FALSE);

		pID3DDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		pID3DDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		pID3DDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );

		pID3DDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

		pID3DDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
		pID3DDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0);
		pID3DDevice->SetTransform( D3DTS_TEXTURE0, &UnitMatrixD3D9 );

		Transformation3DChanged = false;

		// unset last 3d material
		if (CurrentRenderMode == ERM_3D &&
			Material.MaterialType >= 0 && Material.MaterialType < (s32)MaterialRenderers.size())
			MaterialRenderers[Material.MaterialType].Renderer->OnUnsetMaterial();
	}

	if (texture)
	{
		pID3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		pID3DDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		pID3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

		if (alphaChannel)
		{
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );

			pID3DDevice->SetTextureStageState (0, D3DTSS_ALPHAOP,  D3DTOP_SELECTARG1 );
			pID3DDevice->SetTextureStageState (0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );

			pID3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			//pID3DDevice->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ONE);
			//pID3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			pID3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			pID3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		}
		else
		{
			if (alpha)
			{
				pID3DDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
				pID3DDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
				pID3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				pID3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				pID3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			}
			else
			{
				pID3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
				pID3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
				pID3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
				pID3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,  D3DTOP_DISABLE);
				pID3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			}
		}
	}
	else
	{
		if (alpha)
		{
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			pID3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,  D3DTOP_SELECTARG1);
			pID3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );
			pID3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			pID3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			pID3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		}
		else
		{
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			pID3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			pID3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,  D3DTOP_DISABLE);
			pID3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		}
	}

	CurrentRenderMode = ERM_2D;
}



//! deletes all dynamic lights there are
void CD3D9Driver::deleteAllDynamicLights()
{
	for (s32 i=0; i<LastSetLight+1; ++i)
		pID3DDevice->LightEnable(i, false);

	LastSetLight = -1;

	CNullDriver::deleteAllDynamicLights();
}



//! adds a dynamic light
void CD3D9Driver::addDynamicLight(const SLight& dl)
{
	if ((u32)LastSetLight == Caps.MaxActiveLights-1)
		return;

	CNullDriver::addDynamicLight(dl);

	D3DLIGHT9 light;

	if ( dl.Type == ELT_POINT )
	{
		light.Type = D3DLIGHT_POINT;
		light.Position = *(D3DVECTOR*)((void*)(&dl.Position));
	}
	else
	if ( dl.Type == ELT_DIRECTIONAL )
	{
		light.Type = D3DLIGHT_DIRECTIONAL;
		light.Direction = *(D3DVECTOR*)((void*)(&dl.Position));
	}

	light.Diffuse = *(D3DCOLORVALUE*)((void*)(&dl.DiffuseColor));
	light.Specular = *(D3DCOLORVALUE*)((void*)(&dl.SpecularColor));
	light.Ambient = *(D3DCOLORVALUE*)((void*)(&dl.AmbientColor));
	light.Range = MaxLightDistance;

	light.Attenuation0 = 0.0f;
	light.Attenuation1 = 1.0f / dl.Radius;
	light.Attenuation2 = 0.0f;

	++LastSetLight;
	pID3DDevice->SetLight(LastSetLight, &light);
	pID3DDevice->LightEnable(LastSetLight, true);
}



//! returns the maximal amount of dynamic lights the device can handle
s32 CD3D9Driver::getMaximalDynamicLightAmount()
{
	return Caps.MaxActiveLights;
}



//! Sets the dynamic ambient light color. The default color is
//! (0,0,0,0) which means it is dark.
//! \param color: New color of the ambient light.
void CD3D9Driver::setAmbientLight(const SColorf& color)
{
	if (!pID3DDevice)
		return;

	AmbientLight = color;
	D3DCOLOR col = color.toSColor().color;
	pID3DDevice->SetRenderState(D3DRS_AMBIENT, col);
}



//! \return Returns the name of the video driver. Example: In case of the DIRECT3D9
//! driver, it would return "Direct3D9.1".
const wchar_t* CD3D9Driver::getName()
{
	return L"Direct3D 9.0";
}



//! Draws a shadow volume into the stencil buffer. To draw a stencil shadow, do
//! this: Frist, draw all geometry. Then use this method, to draw the shadow
//! volume. Then, use IVideoDriver::drawStencilShadow() to visualize the shadow.
void CD3D9Driver::drawStencilShadowVolume(const core::vector3df* triangles, s32 count, bool zfail)
{
	if (!StencilBuffer || !count)
		return;

	setRenderStatesStencilShadowMode(zfail);

	if (!zfail)
	{
		// ZPASS Method

		// Draw front-side of shadow volume in stencil/z only
		pID3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW );
		pID3DDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT);
		pID3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, count / 3, triangles, sizeof(core::vector3df));

		// Now reverse cull order so front sides of shadow volume are written.
		pID3DDevice->SetRenderState( D3DRS_CULLMODE,   D3DCULL_CW );
		pID3DDevice->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_DECRSAT);
		pID3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, count / 3, triangles, sizeof(core::vector3df));
	}
	else
	{
		// ZFAIL Method

		// Draw front-side of shadow volume in stencil/z only
		pID3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW );
		pID3DDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_INCRSAT );
		pID3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, count / 3, triangles, sizeof(core::vector3df));

		// Now reverse cull order so front sides of shadow volume are written.
		pID3DDevice->SetRenderState( D3DRS_CULLMODE,   D3DCULL_CCW );
		pID3DDevice->SetRenderState( D3DRS_STENCILZFAIL,  D3DSTENCILOP_DECRSAT );
		pID3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, count / 3, triangles, sizeof(core::vector3df));
	}
}



//! Fills the stencil shadow with color. After the shadow volume has been drawn
//! into the stencil buffer using IVideoDriver::drawStencilShadowVolume(), use this
//! to draw the color of the shadow.
void CD3D9Driver::drawStencilShadow(bool clearStencilBuffer, video::SColor leftUpEdge,
			video::SColor rightUpEdge, video::SColor leftDownEdge, video::SColor rightDownEdge)
{
	if (!StencilBuffer)
		return;

	S3DVertex vtx[4];
	vtx[0] = S3DVertex(1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, leftUpEdge, 0.0f, 0.0f);
	vtx[1] = S3DVertex(1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, rightUpEdge, 0.0f, 1.0f);
	vtx[2] = S3DVertex(-1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, leftDownEdge, 1.0f, 0.0f);
	vtx[3] = S3DVertex(-1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, rightDownEdge, 1.0f, 1.0f);

	s16 indices[6] = {0,1,2,1,3,2};

	setRenderStatesStencilFillMode(
		leftUpEdge.getAlpha() < 255 ||
		rightUpEdge.getAlpha() < 255 ||
		leftDownEdge.getAlpha() < 255 ||
		rightDownEdge.getAlpha() < 255);

	setTexture(0,0);

	setVertexShader(EVT_STANDARD);

	pID3DDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, &indices[0],
										D3DFMT_INDEX16, &vtx[0], sizeof(S3DVertex));

	if (clearStencilBuffer)
		pID3DDevice->Clear( 0, NULL, D3DCLEAR_STENCIL,0, 1.0, 0);
}



//! Returns the maximum amount of primitives (mostly vertices) which
//! the device is able to render with one drawIndexedTriangleList
//! call.
s32 CD3D9Driver::getMaximalPrimitiveCount()
{
	return Caps.MaxPrimitiveCount;
}



//! Sets the fog mode.
void CD3D9Driver::setFog(SColor color, bool linearFog, f32 start,
	f32 end, f32 density, bool pixelFog, bool rangeFog)
{
	CNullDriver::setFog(color, linearFog, start, end, density, pixelFog, rangeFog);

	if (!pID3DDevice)
		return;

	pID3DDevice->SetRenderState(D3DRS_FOGCOLOR, color.color);

	pID3DDevice->SetRenderState(
		pixelFog  ? D3DRS_FOGTABLEMODE : D3DRS_FOGVERTEXMODE,
		linearFog ? D3DFOG_LINEAR : D3DFOG_EXP);

	if(linearFog)
	{
		pID3DDevice->SetRenderState(D3DRS_FOGSTART, *(DWORD*)(&start));
		pID3DDevice->SetRenderState(D3DRS_FOGEND, *(DWORD*)(&end));
	}
	else
		pID3DDevice->SetRenderState(D3DRS_FOGDENSITY, *(DWORD*)(&density));

	if(!pixelFog)
		pID3DDevice->SetRenderState(D3DRS_RANGEFOGENABLE, rangeFog);
}



//! Draws a 3d line.
void CD3D9Driver::draw3DLine(const core::vector3df& start,
	const core::vector3df& end, SColor color)
{
	setVertexShader(EVT_STANDARD);
	setRenderStates3DMode();
	video::S3DVertex v[2];
	v[0].Color = color;
	v[1].Color = color;
	v[0].Pos = start;
	v[1].Pos = end;

	pID3DDevice->DrawPrimitiveUP(D3DPT_LINELIST, 1, v, sizeof(S3DVertex));
}



//! resets the device
bool CD3D9Driver::reset()
{
	// reset
	os::Printer::log("Resetting D3D9 device.", ELL_INFORMATION);
	if (FAILED(pID3DDevice->Reset(&present)))
	{
		os::Printer::log("Resetting failed.", ELL_WARNING);
		return false;
	}

	DeviceLost = false;
	ResetRenderStates = true;
	LastVertexType = (E_VERTEX_TYPE)-1;

	for (int i=0; i<4; ++i)
	{
		if (CurrentTexture[i])
			CurrentTexture[i]->drop();

		CurrentTexture[i] = 0;
	}

	setVertexShader(EVT_STANDARD);
	setRenderStates3DMode();
	setFog(FogColor, LinearFog, FogStart, FogEnd, FogDensity, PixelFog, RangeFog);
	setAmbientLight(AmbientLight);

	return true;
}



void CD3D9Driver::OnResize(const core::dimension2d<s32>& size)
{
	if (!pID3DDevice)
		return;

	CNullDriver::OnResize(size);
	present.BackBufferWidth = size.Width;
	present.BackBufferHeight = size.Height;

	reset();
}



//! Returns type of video driver
E_DRIVER_TYPE CD3D9Driver::getDriverType()
{
	return EDT_DIRECT3D9;
}



//! Returns the transformation set by setTransform
const core::matrix4& CD3D9Driver::getTransform(E_TRANSFORMATION_STATE state)
{
	return Matrices[state];
}



//! Sets a vertex shader constant.
void CD3D9Driver::setVertexShaderConstant(const f32* data, s32 startRegister, s32 constantAmount)
{
	if (data)
		pID3DDevice->SetVertexShaderConstantF(startRegister, data, constantAmount);
}

//! Sets a pixel shader constant.
void CD3D9Driver::setPixelShaderConstant(const f32* data, s32 startRegister, s32 constantAmount)
{
	if (data)
		pID3DDevice->SetPixelShaderConstantF(startRegister, data, constantAmount);
}

//! Sets a constant for the vertex shader based on a name.
bool CD3D9Driver::setVertexShaderConstant(const c8* name, const f32* floats, int count)
{
	if (Material.MaterialType >= 0 && Material.MaterialType < (s32)MaterialRenderers.size())
	{
		CD3D9MaterialRenderer* r = (CD3D9MaterialRenderer*)MaterialRenderers[Material.MaterialType].Renderer;
		return r->setVariable(true, name, floats, count);
	}

	return false;
}

//! Sets a constant for the pixel shader based on a name.
bool CD3D9Driver::setPixelShaderConstant(const c8* name, const f32* floats, int count)
{
	if (Material.MaterialType >= 0 && Material.MaterialType < (s32)MaterialRenderers.size())
	{
		CD3D9MaterialRenderer* r = (CD3D9MaterialRenderer*)MaterialRenderers[Material.MaterialType].Renderer;
		return r->setVariable(false, name, floats, count);
	}

	return false;
}

//! Returns pointer to the IGPUProgrammingServices interface.
IGPUProgrammingServices* CD3D9Driver::getGPUProgrammingServices()
{
	return this;
}

//! Adds a new material renderer to the VideoDriver, using pixel and/or
//! vertex shaders to render geometry.
s32 CD3D9Driver::addShaderMaterial(const c8* vertexShaderProgram,
	const c8* pixelShaderProgram,
	IShaderConstantSetCallBack* callback,
	E_MATERIAL_TYPE baseMaterial, s32 userData)
{
	s32 nr = -1;
	CD3D9ShaderMaterialRenderer* r = new CD3D9ShaderMaterialRenderer(
		pID3DDevice, this, nr, vertexShaderProgram, pixelShaderProgram,
		callback, getMaterialRenderer(baseMaterial), userData);

	r->drop();
	return nr;
}


//! Adds a new material renderer to the VideoDriver, based on a high level shading
//! language.
s32 CD3D9Driver::addHighLevelShaderMaterial(
	const c8* vertexShaderProgram,
	const c8* vertexShaderEntryPointName,
	E_VERTEX_SHADER_TYPE vsCompileTarget,
	const c8* pixelShaderProgram,
	const c8* pixelShaderEntryPointName,
	E_PIXEL_SHADER_TYPE psCompileTarget,
	IShaderConstantSetCallBack* callback,
	E_MATERIAL_TYPE baseMaterial,
	s32 userData)
{
	s32 nr = -1;

	CD3D9HLSLMaterialRenderer* hlsl = new CD3D9HLSLMaterialRenderer(
		pID3DDevice, this, nr,
		vertexShaderProgram,
		vertexShaderEntryPointName,
		vsCompileTarget,
		pixelShaderProgram,
		pixelShaderEntryPointName,
		psCompileTarget,
		callback,
		getMaterialRenderer(baseMaterial),
		userData);

	hlsl->drop();
	return nr;
}


//! Returns a pointer to the IVideoDriver interface. (Implementation for
//! IMaterialRendererServices)
IVideoDriver* CD3D9Driver::getVideoDriver()
{
	return this;
}


//! Creates a render target texture.
ITexture* CD3D9Driver::createRenderTargetTexture(core::dimension2d<s32> size)
{
	return new CD3D9Texture(pID3DDevice, size, 0);
}


//! Clears the ZBuffer.
void CD3D9Driver::clearZBuffer()
{
	HRESULT hr = pID3DDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0, 0);

	if (FAILED(hr))
		os::Printer::log("CD3D9Driver clearZBuffer() failed.", ELL_WARNING);
}


//! Returns an image created from the last rendered frame.
IImage* CD3D9Driver::createScreenShot()
{
	HRESULT hr;

	// query the screen dimensions of the current adapter
	D3DDISPLAYMODE displayMode;
	pID3DDevice->GetDisplayMode(0, &displayMode);

	// create the image surface to store the front buffer image [always A8R8G8B8]
	LPDIRECT3DSURFACE9 lpSurface;
	if (FAILED(hr = pID3DDevice->CreateOffscreenPlainSurface(displayMode.Width, displayMode.Height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &lpSurface, 0)))
		return 0;

	// read the front buffer into the image surface
	if (FAILED(hr = pID3DDevice->GetFrontBufferData(0, lpSurface)))
	{
		lpSurface->Release();
		return 0;
	}

	RECT clientRect;
	{
		POINT clientPoint;
		clientPoint.x = 0;
		clientPoint.y = 0;

		ClientToScreen((HWND)getExposedVideoData().D3D8.HWnd, &clientPoint);

		clientRect.left   = clientPoint.x;
		clientRect.top    = clientPoint.y;
		clientRect.right  = clientRect.left + ScreenSize.Width;
		clientRect.bottom = clientRect.top  + ScreenSize.Height;
	}

	// lock our area of the surface
	D3DLOCKED_RECT lockedRect;
	if (FAILED(lpSurface->LockRect(&lockedRect, &clientRect, D3DLOCK_READONLY)))
	{
		lpSurface->Release();
		return 0;
	}

	// this could throw, but we aren't going to worry about that case very much
	IImage* newImage = new CImage(ECF_A8R8G8B8, ScreenSize);

	// d3d pads the image, so we need to copy the correct number of bytes
	u32* pPixels = (u32*)newImage->lock();
	if (pPixels)
	{
		u8 * sP = (u8 *)lockedRect.pBits;
		u32* dP = (u32*)pPixels;

		s32 y;
		for (y = 0; y < ScreenSize.Height; ++y)
		{
			memcpy(dP, sP, ScreenSize.Width * 4);

			sP += lockedRect.Pitch;
			dP += ScreenSize.Width;
		}

		newImage->unlock();
	}

	// we can unlock and release the surface
	lpSurface->UnlockRect();

	// release the image surface
	lpSurface->Release();

	// return status of save operation to caller
	return newImage;
}


// returns the current size of the screen or rendertarget
core::dimension2d<s32> CD3D9Driver::getCurrentRenderTargetSize()
{
	if ( CurrentRendertargetSize.Width == 0 )
		return ScreenSize;
	else
		return CurrentRendertargetSize;
}



} // end namespace video
} // end namespace irr

#endif // _IRR_COMPILE_WITH_DIRECT3D_9_
#endif // _IRR_WINDOWS_




namespace irr
{
namespace video
{

#if (defined(_IRR_WINDOWS_) || defined(_XBOX))
//! creates a video driver
IVideoDriver* createDirectX9Driver(const core::dimension2d<s32>& screenSize, HWND window,
				u32 bits, bool fullscreen, bool stencilbuffer,
				io::IFileSystem* io, bool pureSoftware, bool highPrecisionFPU,
				bool vsync, bool antiAlias)
{
	#ifdef _IRR_COMPILE_WITH_DIRECT3D_9_
	CD3D9Driver* dx9 =  new CD3D9Driver(screenSize, window, fullscreen, stencilbuffer, io, pureSoftware);
	if (!dx9->initDriver(screenSize, window, bits, fullscreen, pureSoftware, highPrecisionFPU, vsync, antiAlias))
	{
		dx9->drop();
		dx9 = 0;
	}

	return dx9;

	#else

	return 0;

	#endif // _IRR_COMPILE_WITH_DIRECT3D_9_
}
#endif

} // end namespace video
} // end namespace irr

