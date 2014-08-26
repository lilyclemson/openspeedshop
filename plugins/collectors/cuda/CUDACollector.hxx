////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014 Argo Navis Technologies. All Rights Reserved.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 2.1 of the License, or (at your option)
// any later version.
//
// This library is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
// details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library; if not, write to the Free Software Foundation, Inc.,
// 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
////////////////////////////////////////////////////////////////////////////////

/** @file Declaration of the CUDACollector class. */

#pragma once
 
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Address.hxx"
#include "CollectorAPI.hxx"
#include "PCBuffer.hxx"

#include <set>

namespace OpenSpeedShop { namespace Framework {

    /**
     * CUDA collector.
     *
     * Intercepts all calls to CUDA memory copy/set and kernel executions and
     * records, for each call, the current stack trace and start/end time, as
     * well as additional relevant information depending on the operation. In
     * addition, has the ability to periodically sample hardware event counts
     * via PAPI for both the CPU and GPU.
     */
    class CUDACollector :
        public CollectorImpl
    {
        
    public:
        
        CUDACollector();    
        
        virtual Blob getDefaultParameterValues() const;
        virtual void getParameterValue(const std::string&,
                                       const Blob&, void*) const;
        virtual void setParameterValue(const std::string&,
                                       const void*, Blob&) const;
        
        virtual void startCollecting(const Collector&,
                                     const ThreadGroup&) const;
        virtual void stopCollecting(const Collector&,
                                    const ThreadGroup&) const;
        
        virtual void getMetricValues(const std::string&,
                                     const Collector&, const Thread&,
                                     const Extent&, const Blob&, 
                                     const ExtentGroup&, void*) const;
        
        virtual void getUniquePCValues(const Thread&, const Blob&,
                                       PCBuffer*) const;
        
        virtual void getUniquePCValues(const Thread&, const Blob&,
                                       std::set<Address>&) const;

    }; // class CUDACollector
        
} } // namespace OpenSpeedShop::Framework