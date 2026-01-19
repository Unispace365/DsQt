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

#pragma once

#include <memory>
#include <functional>
#include <TouchEngine/TouchObject.h>
#include <QOpenGLFunctions>
#include <rhi/qrhi.h>

class OpenGLTexture
{
public:
	OpenGLTexture();
    OpenGLTexture(QRhi* rhi,const unsigned char *rgba, size_t bytesPerRow, GLsizei width, GLsizei height);
    OpenGLTexture(QRhi *rhi, const TouchObject<TEOpenGLTexture> &texture);

	GLuint	getName() const;

	constexpr const TouchObject<TEOpenGLTexture> &
		getSource() const
	{
		return mySource;
	}
   QSharedPointer<QRhiTexture> getRhiTexture() const { return myTexture; }
    GLuint getTextureName() const {
        if(myTexture){
            //return myTexture->nativeTextureObject().object;
        }
        return 0;
    }

    void release();

private:
	TouchObject<TEOpenGLTexture> mySource;
	std::shared_ptr<GLuint> myName;
    QSharedPointer<QRhiTexture> myTexture;
};

