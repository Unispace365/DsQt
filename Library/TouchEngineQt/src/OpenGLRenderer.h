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
#include "Renderer.h"
#include <vector>
#include <QOpenGLDebugLogger>
#include "OpenGLImage.h"
//#include "OpenGLProgram.h"
#include <rhi/qrhi.h>


class OpenGLRenderer :
	public Renderer
{
public:
	OpenGLRenderer();
	virtual ~OpenGLRenderer();

	virtual DWORD
	getWindowStyleFlags() const
	{
		return CS_OWNDC | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	}
	HDC
	getDC() const
	{
		return myDC;
	}
	HGLRC
	getRC() const
	{
		return myRenderingContext;
	}
	virtual TEGraphicsContext*
	getTEContext() const override
	{
		return myContext;
	}

    virtual bool	setup(QQuickWindow* window);
    virtual bool	configure(TEInstance* instance, QString& error) override;
	virtual void	resize(int width, int height) override;
	virtual void	stop();
	virtual bool	render();
	virtual void	clearInputImages() override;
	virtual void	addOutputImage() override;
	virtual bool	updateOutputImage(const TouchObject<TEInstance>& instance, size_t index, const std::string& identifier) override;
	virtual void	clearOutputImages() override;
	virtual void	releaseTextures() override;

    virtual QSharedPointer<QRhiTexture>        getOutputRhiTexture(size_t index) override;
    GLuint getOutputName(size_t index) const;
    const QString &getDeviceName() const;
    const QOpenGLFunctions* getGLFunctions() { return m_funcs; }
private:

	static void		textureReleaseCallback(GLuint texture, TEObjectEvent event, void *info);


	GLuint			myVAO = 0;
	GLuint			myVBO = 0;
	GLint			myVAIndex = -1;
	GLint			myTAIndex = -1;
	HGLRC			myRenderingContext = nullptr;
	HDC				myDC = nullptr;
    TouchObject<TEOpenGLContext> myContext=nullptr;
	std::vector<OpenGLImage> myInputImages;
	std::vector<OpenGLImage> myOutputImages;
    QString	myDeviceName;
    QOpenGLDebugLogger logger;
    QOpenGLContext *m_glContextUI=nullptr;
    QOpenGLContext *m_glContextTE=nullptr;
    QOpenGLContext *m_glShareContext=nullptr;

    QOffscreenSurface *m_offscreenSurface=nullptr;
    bool isSetupDone = false;
    QOpenGLFunctions* m_funcs;
};

