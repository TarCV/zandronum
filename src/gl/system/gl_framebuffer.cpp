/*
** gl_framebuffer.cpp
** Implementation of the non-hardware specific parts of the
** OpenGL frame buffer
**
**---------------------------------------------------------------------------
** Copyright 2000-2007 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "gl/system/gl_system.h"
#include "files.h"
#include "m_swap.h"
#include "r_draw.h"
#include "v_video.h"
#include "r_main.h"
#include "m_png.h"
#include "m_crc32.h"
#include "vectors.h"
#include "v_palette.h"
#include "templates.h"

#include "gl/system/gl_framebuffer.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/data/gl_data.h"
#include "gl/textures/gl_hwtexture.h"
#include "gl/textures/gl_texture.h"
#include "gl/textures/gl_translate.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"
#include "gl/gl_functions.h"
// [BB] Added include.
#ifdef _MSC_VER
#include "../hqnx/hqnx.h"
#endif

// [BB]
CVAR( Bool, cl_disallowfullpitch, false, CVAR_ARCHIVE )

IMPLEMENT_CLASS(OpenGLFrameBuffer)
EXTERN_CVAR (Float, vid_brightness)
EXTERN_CVAR (Float, vid_contrast)
EXTERN_CVAR (Bool, vid_vsync)

void gl_SetupMenu();

FGLRenderer *GLRenderer;

//==========================================================================
//
//
//
//==========================================================================

OpenGLFrameBuffer::OpenGLFrameBuffer(int width, int height, int bits, int refreshHz, bool fullscreen) : 
	Super(width, height, bits, refreshHz, fullscreen) 
{
	GLRenderer = new FGLRenderer(this);
	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
	UpdatePalette ();
	ScreenshotBuffer = NULL;
	LastCamera = NULL;

	InitializeState();
	gl_GenerateGlobalBrightmapFromColormap();
	DoSetGamma();
	needsetgamma = true;
	swapped = false;
	Accel2D = true;
	if (gl.SetVSync!=NULL) gl.SetVSync(vid_vsync);

#ifdef _MSC_VER
	// [BB] Necessary for the hqnx resizing.
	InitLUTs();
#endif
}

OpenGLFrameBuffer::~OpenGLFrameBuffer()
{
	delete GLRenderer;
	GLRenderer = NULL;
}

//==========================================================================
//
// Initializes the GL renderer
//
//==========================================================================

void OpenGLFrameBuffer::InitializeState()
{
	static bool first=true;

	gl.LoadExtensions();
	Super::InitializeState();
	gl_SetupMenu();
	if (first)
	{
		first=false;
		// [BB] For some reason this crashes, if compiled with MinGW and optimization. Has to be investigated.
#ifdef _MSC_VER
		gl.PrintStartupLog();
#endif

		if (gl.flags&RFL_NPOT_TEXTURE)
		{
			Printf("Support for non power 2 textures enabled.\n");
		}
		if (gl.flags&RFL_OCCLUSION_QUERY)
		{
			Printf("Occlusion query enabled.\n");
		}
	}
	gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	gl.ClearDepth(1.0f);
	gl.DepthFunc(GL_LESS);
	gl.ShadeModel(GL_SMOOTH);

	gl.Enable(GL_DITHER);
	gl.Enable(GL_ALPHA_TEST);
	gl.Disable(GL_CULL_FACE);
	gl.Disable(GL_POLYGON_OFFSET_FILL);
	gl.Enable(GL_POLYGON_OFFSET_LINE);
	gl.Enable(GL_BLEND);
	gl.Enable(GL_DEPTH_CLAMP_NV);
	gl.Disable(GL_DEPTH_TEST);
	gl.Enable(GL_TEXTURE_2D);
	gl.Disable(GL_LINE_SMOOTH);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glAlphaFunc(GL_GEQUAL,0.5f);
	gl.Hint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	gl.Hint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	gl.Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	// This was to work around a bug in some older driver. Probably doesn't make sense anymore.
	gl.Enable(GL_FOG);
	gl.Disable(GL_FOG);

	gl.Hint(GL_FOG_HINT, GL_FASTEST);
	gl.Fogi(GL_FOG_MODE, GL_EXP);


	gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	gl.Viewport(0, (GetTrueHeight()-GetHeight())/2, GetWidth(), GetHeight()); 

	Begin2D(false);
	GLRenderer->Initialize();
}

//==========================================================================
//
// Updates the screen
//
//==========================================================================

// Testing only for now. 
CVAR(Bool, gl_draw_sync, true, 0) //false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

void OpenGLFrameBuffer::Update()
{
	if (!CanUpdate()) 
	{
		GLRenderer->Flush();
		return;
	}

	Begin2D(false);

	DrawRateStuff();
	GLRenderer->Flush();

	if (GetTrueHeight() != GetHeight())
	{
		if (GLRenderer != NULL) 
			GLRenderer->ClearBorders();

		Begin2D(false);
	}
	if (gl_draw_sync || !swapped)
	{
		Swap();
	}
	swapped = false;
	Unlock();
	CheckBench();
}


//==========================================================================
//
// Swap the buffers
//
//==========================================================================

void OpenGLFrameBuffer::Swap()
{
	Finish.Reset();
	Finish.Clock();
	gl.Finish();
	if (needsetgamma) 
	{
		//DoSetGamma();
		needsetgamma = false;
	}
	gl.SwapBuffers();
	Finish.Unclock();
	swapped = true;
	FHardwareTexture::UnbindAll();
}

//===========================================================================
//
// DoSetGamma
//
// (Unfortunately Windows has some safety precautions that block gamma ramps
//  that are considered too extreme. As a result this doesn't work flawlessly)
//
//===========================================================================

void OpenGLFrameBuffer::DoSetGamma()
{
	WORD gammaTable[768];

	if (m_supportsGamma)
	{
		// This formula is taken from Doomsday
		float gamma = clamp<float>(Gamma, 0.1f, 4.f);
		float contrast = clamp<float>(vid_contrast, 0.1f, 3.f);
		float bright = clamp<float>(vid_brightness, -0.8f, 0.8f);

		double invgamma = 1 / gamma;
		double norm = pow(255., invgamma - 1);

		for (int i = 0; i < 256; i++)
		{
			double val = i * contrast - (contrast - 1) * 127;
			if(gamma != 1) val = pow(val, invgamma) / norm;
			val += bright * 128;

			gammaTable[i] = gammaTable[i + 256] = gammaTable[i + 512] = (WORD)clamp<double>(val*256, 0, 0xffff);
		}
		SetGammaTable(gammaTable);
	}
}

bool OpenGLFrameBuffer::SetGamma(float gamma)
{
	DoSetGamma();
	return true;
}

bool OpenGLFrameBuffer::SetBrightness(float bright)
{
	DoSetGamma();
	return true;
}

bool OpenGLFrameBuffer::SetContrast(float contrast)
{
	DoSetGamma();
	return true;
}

bool OpenGLFrameBuffer::UsesColormap() const
{
	// The GL renderer has no use for colormaps so let's
	// not create them and save us some time.
	return false;
}

//===========================================================================
//
//
//===========================================================================

void OpenGLFrameBuffer::UpdatePalette()
{
	int rr=0,gg=0,bb=0;
	for(int x=0;x<256;x++)
	{
		rr+=GPalette.BaseColors[x].r;
		gg+=GPalette.BaseColors[x].g;
		bb+=GPalette.BaseColors[x].b;
	}
	rr>>=8;
	gg>>=8;
	bb>>=8;

	palette_brightness = (rr*77 + gg*143 + bb*35)/255;
}

void OpenGLFrameBuffer::GetFlashedPalette (PalEntry pal[256])
{
	memcpy(pal, SourcePalette, 256*sizeof(PalEntry));
}

PalEntry *OpenGLFrameBuffer::GetPalette ()
{
	return SourcePalette;
}

bool OpenGLFrameBuffer::SetFlash(PalEntry rgb, int amount)
{
	Flash = PalEntry(amount, rgb.r, rgb.g, rgb.b);
	return true;
}

void OpenGLFrameBuffer::GetFlash(PalEntry &rgb, int &amount)
{
	rgb = Flash;
	rgb.a = 0;
	amount = Flash.a;
}

int OpenGLFrameBuffer::GetPageCount()
{
	return 1;
}

//==========================================================================
//
// DFrameBuffer :: PrecacheTexture
//
//==========================================================================

void OpenGLFrameBuffer::PrecacheTexture(FTexture *tex, int cache)
{
	if (tex != NULL)
	{
		if (cache)
		{
			tex->PrecacheGL();
		}
		else
		{
			tex->UncacheGL();
		}
	}
}


//==========================================================================
//
// DFrameBuffer :: StateChanged
//
//==========================================================================

void OpenGLFrameBuffer::StateChanged(AActor *actor)
{
	gl_SetActorLights(actor);
}

//===========================================================================
//
// notify the renderer that serialization of the curent level is about to start/end
//
//===========================================================================

void OpenGLFrameBuffer::StartSerialize(FArchive &arc)
{
	gl_DeleteAllAttachedLights();
}

void OpenGLFrameBuffer::EndSerialize(FArchive &arc)
{
	gl_RecreateAllAttachedLights();
}

//===========================================================================
//
// Get max. view angle (renderer specific information so it goes here now)
//
//===========================================================================

EXTERN_CVAR(Float, maxviewpitch)

int OpenGLFrameBuffer::GetMaxViewPitch(bool down)
{
	// [BB]
	if (cl_disallowfullpitch) return Super::GetMaxViewPitch(down);
	else return (down? maxviewpitch : -maxviewpitch) * ANGLE_1;
}

//==========================================================================
//
// DFrameBuffer :: CreatePalette
//
// Creates a native palette from a remap table, if supported.
//
//==========================================================================

FNativePalette *OpenGLFrameBuffer::CreatePalette(FRemapTable *remap)
{
	return GLTranslationPalette::CreatePalette(remap);
}

//==========================================================================
//
//
//
//==========================================================================
bool OpenGLFrameBuffer::Begin2D(bool)
{
	gl.MatrixMode(GL_MODELVIEW);
	gl.LoadIdentity();
	gl.MatrixMode(GL_PROJECTION);
	gl.LoadIdentity();
	gl.Ortho(
		(GLdouble) 0,
		(GLdouble) GetWidth(), 
		(GLdouble) GetHeight(), 
		(GLdouble) 0,
		(GLdouble) -1.0, 
		(GLdouble) 1.0 
		);
	gl.Disable(GL_DEPTH_TEST);
	gl.Disable(GL_MULTISAMPLE);
	if (GLRenderer != NULL) GLRenderer->Begin2D();
	return true;
}

//==========================================================================
//
// Draws a texture
//
//==========================================================================

void STACK_ARGS OpenGLFrameBuffer::DrawTextureV(FTexture *img, double x0, double y0, uint32 tag, va_list tags)
{
	DrawParms parms;

	if (ParseDrawTextureTags(img, x0, y0, tag, tags, &parms, true))
	{
		if (GLRenderer != NULL) GLRenderer->DrawTexture(img, parms);
	}
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::DrawLine(int x1, int y1, int x2, int y2, int palcolor, uint32 color)
{
	if (GLRenderer != NULL) 
		GLRenderer->DrawLine(x1, y1, x2, y2, palcolor, color);
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::DrawPixel(int x1, int y1, int palcolor, uint32 color)
{
	if (GLRenderer != NULL) 
		GLRenderer->DrawPixel(x1, y1, palcolor, color);
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::Dim(PalEntry)
{
	// Unlike in the software renderer the color is being ignored here because
	// view blending only affects the actual view with the GL renderer.
	Super::Dim(0);
}

void OpenGLFrameBuffer::Dim(PalEntry color, float damount, int x1, int y1, int w, int h)
{
	if (GLRenderer != NULL) 
		GLRenderer->Dim(color, damount, x1, y1, w, h);
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::FlatFill (int left, int top, int right, int bottom, FTexture *src, bool local_origin)
{

	if (GLRenderer != NULL) 
		GLRenderer->FlatFill(left, top, right, bottom, src, local_origin);
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::Clear(int left, int top, int right, int bottom, int palcolor, uint32 color)
{
	if (GLRenderer != NULL) 
		GLRenderer->Clear(left, top, right, bottom, palcolor, color);
}

//===========================================================================
// 
//	Takes a screenshot
//
//===========================================================================

void OpenGLFrameBuffer::GetScreenshotBuffer(const BYTE *&buffer, int &pitch, ESSType &color_type)
{
	int w = SCREENWIDTH;
	int h = SCREENHEIGHT;

	ReleaseScreenshotBuffer();
	ScreenshotBuffer = new BYTE[w * h * 3];

	gl.ReadPixels(0,(GetTrueHeight() - GetHeight()) / 2,w,h,GL_RGB,GL_UNSIGNED_BYTE,ScreenshotBuffer);
	pitch = -w*3;
	color_type = SS_RGB;
	buffer = ScreenshotBuffer + w * 3 * (h - 1);
}

//===========================================================================
// 
// Releases the screenshot buffer.
//
//===========================================================================

void OpenGLFrameBuffer::ReleaseScreenshotBuffer()
{
	if (ScreenshotBuffer != NULL) delete [] ScreenshotBuffer;
	ScreenshotBuffer = NULL;
}


void OpenGLFrameBuffer::WriteSavePic (player_t *player, FILE *file, int width, int height)
{
	GLRenderer->WriteSavePic(player, file, width, height);
}

void OpenGLFrameBuffer::RenderView (player_t* player)
{
	GLRenderer->RenderView(player);
}


void OpenGLFrameBuffer::DrawRemainingPlayerSprites()
{
	// not used by hardware renderer
}
