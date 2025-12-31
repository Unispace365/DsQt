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


#include "OpenGLRenderer.h"
#include <TouchEngine/TouchEngine.h>
#include <TouchEngine/TEOpenGL.h>
#include <QOpenGLFunctions_4_4_Core>
#include <QOffscreenSurface>
#include <touchenginemanager.h>



OpenGLRenderer::OpenGLRenderer()
{
}


OpenGLRenderer::~OpenGLRenderer()
{
}

bool
OpenGLRenderer::setup(QQuickWindow *window)
{


    bool success = Renderer::setup(window);

    auto currentContext = TouchEngineManager::inst()->getShareContext();
    //auto rContext = TouchEngineManager::inst()->getRenderContext();
    QRhiGles2InitParams initParams;
    initParams.shareContext = currentContext;
    initParams.fallbackSurface = initParams.newFallbackSurface(currentContext->format());
    initParams.format = currentContext->format();

    myRhi.reset(QRhi::create(QRhi::OpenGLES2, &initParams));

    if (success && currentContext)
	{
        if(!m_glContextUI){
            qDebug()<<"Creating shared OpenGL context for TouchEngine OpenGLRenderer";
            m_glContextUI = new QOpenGLContext();
            m_glContextUI->setShareContext(currentContext);
            m_glContextUI->setFormat(currentContext->format());
            success = m_glContextUI->create();

            m_offscreenSurface = new QOffscreenSurface();
            m_offscreenSurface->setFormat(m_glContextUI->format());
            m_offscreenSurface->create();
        }
        if(window){

            myRenderingContext = currentContext->nativeInterface<QNativeInterface::QWGLContext>()->nativeContext();
            success = true;
        } else {
            success = false;
        }
        if (success)
        {
            m_glContextUI->makeCurrent(m_offscreenSurface);
            m_funcs = m_glContextUI->functions();
            const GLubyte* render = m_funcs->glGetString(GL_RENDERER);
            myDeviceName = QString(reinterpret_cast<const char *>(render));
        }
        if(success){
            //get DC
            myDC = GetDC(reinterpret_cast<HWND>(window->winId()));
        }


	}
	if (success)
	{
        //if (glewInit() != GLEW_OK)
        //{
        //	success = FALSE;
        //}
	}
	if (success)
	{
        //QOpenGLFunctions* f = currentContext->functions();
        m_glContextUI->makeCurrent(m_offscreenSurface);
        success = logger.initialize();
        m_glContextUI->makeCurrent(nullptr);
	}
	if (success)
	{
        if (TEOpenGLContextCreate(myDC, myRenderingContext, myContext.take()) != TEResultSuccess)
		{
			success = false;
		}
	}
	return success;
}

bool
OpenGLRenderer::configure(TEInstance* instance, QString& error)
{
	if (TEOpenGLContextSupportsTexturesForInstance(myContext, instance))
	{
		return true;
	}
    error = "OpenGL is not supported. The selected GPU does not have needed features.";
    error += "\nThe selected GPU is: ";
	error += myDeviceName;
	return false;
}

void
OpenGLRenderer::resize(int width, int height)
{
	Renderer::resize(width, height);
}

void
OpenGLRenderer::stop()
{


	Renderer::stop();

}

bool
OpenGLRenderer::render()
{

	return true;
}


QSharedPointer<QRhiTexture> OpenGLRenderer::getOutputRhiTexture(size_t index)
{
    if(index >= 0 && index < myOutputImages.size()){
        return myOutputImages[index].getTexture().getRhiTexture();
    }
    return QSharedPointer<QRhiTexture>();
}

GLuint OpenGLRenderer::getOutputName(size_t index) const
{
    if(index > 0 && index < myOutputImages.size()){
        return myOutputImages[index].getTexture().getName();
    }
    return 0;
}

const QString& OpenGLRenderer::getDeviceName() const
{
	return myDeviceName;
}

void
OpenGLRenderer::clearInputImages()
{
	myInputImages.clear();
	Renderer::clearInputImages();
}

void
OpenGLRenderer::addOutputImage()
{
   // m_glContextUI->doneCurrent();
    //m_glContextUI->makeCurrent(myWindow);

	myOutputImages.emplace_back();
	myOutputImages.back().setup(myVAIndex, myTAIndex);

    //m_glContextUI->makeCurrent(nullptr);

	Renderer::addOutputImage();
}

bool OpenGLRenderer::updateOutputImage(const TouchObject<TEInstance>& instance, size_t index, const std::string& identifier)
{
	bool success = false;

	if (index < myOutputImages.size())
	{
		const auto& source = myOutputImages.at(index).getTexture().getSource();
		if (source)
		{
			TEOpenGLTextureUnlock(source);
		}
	}
	TouchObject<TETexture> texture;
	TEResult result = TEInstanceLinkGetTextureValue(instance, identifier.c_str(), TELinkValueCurrent, texture.take());
	if (result == TEResultSuccess)
	{

		setOutputImage(index, texture);
		if (texture && TETextureGetType(texture) == TETextureTypeD3DShared)
		{
			TouchObject<TEOpenGLTexture> created;
			if (TEOpenGLContextGetTexture(myContext, static_cast<TED3DSharedTexture*>(texture.get()), created.take()) == TEResultSuccess)
			{
				if (TEOpenGLTextureLock(created) == TEResultSuccess)
				{
                    //m_renderContext->doneCurrent();
                    //m_glContextUI->makeCurrent(m_offscreenSurface);

                    myOutputImages.at(index).update(OpenGLTexture(myWindow->rhi(),created));

                    success = true;
				}
			}
		}
	}

	if (!success)
	{
		myOutputImages.at(index).update(OpenGLTexture());
		setOutputImage(index, nullptr);
	}
	return success;
}

void
OpenGLRenderer::clearOutputImages()
{
	Renderer::clearOutputImages();
}

void
OpenGLRenderer::textureReleaseCallback(GLuint texture, TEObjectEvent event, void *info)
{
	// TODO: might come from another thread
	// Delete our reference to the texture (and the texture itself if we are the last reference)
	if (event == TEObjectEventRelease)
	{
		delete reinterpret_cast<OpenGLTexture*>(info);
	}
}


