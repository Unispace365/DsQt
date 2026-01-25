/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/


#include "OpenGLTexture.h"
#include <TouchEngine/TEOpenGL.h>

OpenGLTexture::OpenGLTexture()
{
}

OpenGLTexture::OpenGLTexture(QRhi* rhi,const unsigned char * rgba, size_t bytesPerRow, GLsizei width, GLsizei height)
{

	if (bytesPerRow != width * 4LL)
	{
		throw "Needs work";
	}

    myTexture.reset(rhi->newTexture(QRhiTexture::Format::RGBA8, QSize(width,height),1));
    myTexture->create();
    myName = std::shared_ptr<GLuint>(new GLuint(myTexture->nativeTexture().object));

}

OpenGLTexture::OpenGLTexture(QRhi* rhi,const TouchObject<TEOpenGLTexture> &source)
    : mySource(source)
{
    auto w = TEOpenGLTextureGetWidth(source);
    auto h = TEOpenGLTextureGetHeight(source);
    myTexture.reset(rhi->newTexture(QRhiTexture::Format::RGBA8, QSize(w,h),1, {QRhiTexture::UsedAsTransferSource || QRhiTexture::RenderTarget}));
    QRhiTexture::NativeTexture src;
    src.layout = 0;
    src.object = TEOpenGLTextureGetName(source);
    myTexture->createFrom(src);
	myName = std::make_shared<GLuint>(TEOpenGLTextureGetName(source));
}

GLuint
OpenGLTexture::getName() const
{
	return *myName;
}

void
OpenGLTexture::release()
{
    myTexture.reset();
    mySource = nullptr;
    myName.reset();
}


