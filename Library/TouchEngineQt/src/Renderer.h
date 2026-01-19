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

// Windows headers must be included before TouchEngine headers
// to define HANDLE and other Windows types
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#endif

#include <TouchEngine/TouchEngine.h>
#include <TouchEngine/TouchObject.h>
#include <rhi/qrhi.h>
#include <vector>
#include <array>
#include <memory>
#include <QQuickWindow>


class Renderer
{
public:
	Renderer();
	Renderer(const Renderer &o) = delete;
	Renderer& operator=(const Renderer& o) = delete;
	virtual ~Renderer() noexcept(false);

    QSharedPointer<QRhi>
    getRhi() const
	{
        return myRhi;
	}

    //virtual const std::wstring& getDeviceName() const = 0;

    virtual bool	setup(QQuickWindow* window);
    virtual bool	configure(TEInstance* instance, QString& error);
	virtual bool	doesInputTextureTransfer() const;
	virtual void	resize(int width, int height);
	virtual void	stop();
    //virtual bool	render() = 0;
	void			setBackgroundColor(float r, float g, float b);

	virtual void		beginImageLayout();
	virtual void		clearInputImages();
	size_t				getRightSideImageCount();
	virtual void		addOutputImage();
	virtual void		endImageLayout();
						
	virtual bool		updateOutputImage(const TouchObject<TEInstance>& instance, size_t index, const std::string& identifier) = 0;
	const TouchObject<TETexture>& getOutputImage(size_t index) const;
    virtual QSharedPointer<QRhiTexture>        getOutputRhiTexture(size_t index)=0;
	virtual void		clearOutputImages(); // TODO: ?
	virtual void		releaseTextures() {} // Release GPU resources before device is destroyed
	virtual void		swapOutputBuffers() {} // Swap double-buffered output images (Vulkan only)
	virtual void		clearRhi() { myRhi.reset(); } // Clear RHI reference when device is lost
	virtual TEGraphicsContext* getTEContext() const = 0;
protected:
	bool				inputDidChange(size_t index) const;
	void				markInputChange(size_t index);
	void				markInputUnchanged(size_t index);
	void				setOutputImage(size_t index, const TouchObject<TETexture>& texture);
	std::array<float, 3>	myBackgroundColor;
	int		myWidth = 0;
	int		myHeight = 0;
    QSharedPointer<QRhi> myRhi = nullptr;
    QQuickWindow* myWindow = nullptr;
private:

	std::vector<TouchObject<TETexture>> myOutputImages;
	std::vector<bool>			myInputImageUpdates;

};

