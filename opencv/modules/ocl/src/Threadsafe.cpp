/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2010-2012, Institute Of Software Chinese Academy Of Science, all rights reserved.
// Copyright (C) 2010-2012, Advanced Micro Devices, Inc., all rights reserved.
// Copyright (C) 2010-2012, MulticoreWare Inc. all rights reserved.
// Third party copyrights are property of their respective owners.
//
// @Authors
//    Niko Li, Niko.li@amd.com
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other oclMaterials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#include "opencv2/ocl/Threadsafe.h"


CriticalSection::CriticalSection()
{
#if defined WIN32 || defined _WIN32
	InitializeCriticalSection(&m_CritSec);
#else
	pthread_mutex_init(&m_CritSec, NULL);
#endif
}

CriticalSection::~CriticalSection()
{
#if defined WIN32 || defined _WIN32
	DeleteCriticalSection(&m_CritSec);
#else
	pthread_mutex_destroy(&m_CritSec);
#endif
}

void CriticalSection::Lock()
{
#if defined WIN32 || defined _WIN32
	EnterCriticalSection(&m_CritSec);
#else
	pthread_mutex_lock(&m_CritSec);
#endif
}

void CriticalSection::Unlock()
{
#if defined WIN32 || defined _WIN32
	LeaveCriticalSection(&m_CritSec);
#else
	pthread_mutex_unlock(&m_CritSec);
#endif
}
