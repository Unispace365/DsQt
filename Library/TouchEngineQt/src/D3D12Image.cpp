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

#ifdef _WIN32

#include "D3D12Image.h"

D3D12Image::D3D12Image()
{
}

D3D12Image::D3D12Image(D3D12Image&& o) noexcept
    : myTexture(std::move(o.myTexture)), myDirty(o.myDirty)
{
}

D3D12Image&
D3D12Image::operator=(D3D12Image&& o) noexcept
{
    myTexture = std::move(o.myTexture);
    myDirty = o.myDirty;
    return *this;
}

D3D12Image::~D3D12Image()
{
}

bool
D3D12Image::setup()
{
    return true;
}

void
D3D12Image::update(D3D12Texture texture)
{
    myTexture = std::move(texture);
    myDirty = true;
}

#endif // _WIN32
